// ------------------------------------------------------------------------
// Pion is a development platform for building Reactors that process Events
// ------------------------------------------------------------------------
// Copyright (C) 2007-2008 Atomic Labs, Inc.  (http://www.atomiclabs.com)
//
// Pion is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// Pion is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License for
// more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with Pion.  If not, see <http://www.gnu.org/licenses/>.
//

#include "LogCodec.hpp"
#include <pion/platform/ConfigManager.hpp>

using namespace pion::platform;


namespace pion {		// begin namespace pion
namespace plugins {		// begin namespace plugins


// static members of LogCodec
const std::string			LogCodec::CONTENT_TYPE = "text/ascii";
const std::string			LogCodec::FLUSH_ELEMENT_NAME = "Flush";
const std::string			LogCodec::HEADERS_ELEMENT_NAME = "Headers";	// this means ELF
const std::string			LogCodec::TIME_OFFSET_ELEMENT_NAME = "TimeOffset";
const std::string			LogCodec::FIELD_ELEMENT_NAME = "Field";
const std::string			LogCodec::TERM_ATTRIBUTE_NAME = "term";
const std::string			LogCodec::START_ATTRIBUTE_NAME = "start";
const std::string			LogCodec::END_ATTRIBUTE_NAME = "end";
const std::string			LogCodec::OPTIONAL_ATTRIBUTE_NAME = "optional";
const std::string			LogCodec::URLENCODE_ATTRIBUTE_NAME = "urlencode";
const std::string			LogCodec::ESCAPE_ATTRIBUTE_NAME = "escape";
const std::string			LogCodec::EMPTY_ATTRIBUTE_NAME = "empty";
const std::string			LogCodec::EVENTS_ELEMENT_NAME = "Events";
const std::string			LogCodec::FIELDS_ELEMENT_NAME = "Fields";
const std::string			LogCodec::SPLIT_ATTRIBUTE_NAME = "split";
const std::string			LogCodec::JOIN_ATTRIBUTE_NAME = "join";
const std::string			LogCodec::COMMENT_ATTRIBUTE_NAME = "comment";
const std::string			LogCodec::CONSUME_ATTRIBUTE_NAME = "consume";
const unsigned int			LogCodec::READ_BUFFER_SIZE = 1024 * 128;	// 128KB

// defaults for various settings (tuned for ELF-like formats)
const std::string			LogCodec::EVENT_SPLIT_SET = "\r\n";
const std::string			LogCodec::EVENT_JOIN_STRING = OSEOL;
const std::string			LogCodec::COMMENT_CHAR_SET = "#";
// NOTE: If Headers is true, these defaults CANNOT be overridden
const std::string			LogCodec::FIELD_SPLIT_SET = " \t";
const std::string			LogCodec::FIELD_JOIN_STRING = " ";
const bool					LogCodec::CONSUME_DELIMS_FLAG = true;

// special support for ELF (when Headers is true)
const std::string			LogCodec::VERSION_ELF_HEADER = "#Version:";
const std::string			LogCodec::DATE_ELF_HEADER = "#Date:";
const std::string			LogCodec::SOFTWARE_ELF_HEADER = "#Software:";
const std::string			LogCodec::FIELDS_ELF_HEADER = "#Fields:";


// LogCodec member functions

CodecPtr LogCodec::clone(void) const
{
	LogCodec *new_codec(new LogCodec());
	new_codec->copyCodec(*this);
	new_codec->m_flush_after_write = m_flush_after_write;
	new_codec->m_handle_elf_headers = m_handle_elf_headers;
	new_codec->m_wrote_elf_headers = false;	// Important!
	new_codec->m_time_offset = m_time_offset;
	new_codec->m_event_split = m_event_split;
	new_codec->m_event_join = m_event_join;
	new_codec->m_comment_chars = m_comment_chars;
	new_codec->m_field_split = m_field_split;
	new_codec->m_field_join = m_field_join;
	new_codec->m_consume_delims = m_consume_delims;
	for (CurrentFormat::const_iterator i = m_format.begin(); i != m_format.end(); ++i) {
		new_codec->mapFieldToTerm((*i)->log_field, (*i)->log_term, (*i)->log_delim_start, (*i)->log_delim_end,
								  (*i)->log_opt_delims, (*i)->log_urlencode,
								  (*i)->log_escape_char, (*i)->log_empty_val,
								  (*i)->log_do_time_offset, (*i)->log_time_offset);
	}
	return CodecPtr(new_codec);
}

void LogCodec::write(std::ostream& out, const Event& e)
{
	const Event::ParameterValue *value_ptr;

	// write the ELF headers if necessary
	if (m_handle_elf_headers && !m_wrote_elf_headers) {
		writeELFHeaders(out);
		m_wrote_elf_headers = true;
	}

	// iterate through each field in the current format
	CurrentFormat::const_iterator i = m_format.begin();
	while (i != m_format.end()) {
		// get the value for the field
		value_ptr = e.getPointer((*i)->log_term.term_ref);

		// check if the value is undefined
		if (value_ptr == NULL)
			(*i)->writeEmptyValue(out);
		else
			(*i)->write(out, *value_ptr);

		// iterate to the next field
		++i;
		// add field-join between all fields
		if (i != m_format.end())
			out << m_field_join;
	}

	// write event-join for each event record
	out << m_event_join;

	// flush the output stream
	if (m_flush_after_write)
		out.flush();
}

bool LogCodec::read(std::istream& input_stream, Event& e)
{
	if (e.getType() != getEventType())
		throw WrongEventTypeException();
	e.clear();

	streambuf_type *buf_ptr = input_stream.rdbuf();
	// skip "empty events" (i.e. records with no data) and comments
	int_type c = consumeVoidsAndComments(buf_ptr);
	// if nothing left...punt with no event generated
	if (traits_type::eq_int_type(c, traits_type::eof())) {
		input_stream.setstate(std::ios::eofbit);
		return false;
	}

	// iterate through each field in the format
	CurrentFormat::const_iterator i;
	char delim_start;
	char delim_end;
	char escape_char;
	char * const read_buf = m_read_buf.get();
	char * read_ptr;

	for (i = m_format.begin(); !traits_type::eq_int_type(c, traits_type::eof()) &&
		   m_event_split.find(c) == std::string::npos && i != m_format.end(); ++i)
	{
		delim_start = (*i)->log_delim_start;
		delim_end = (*i)->log_delim_end;
		escape_char = (*i)->log_escape_char;

		if (delim_start != '\0') {
			if (c == delim_start)
				c = buf_ptr->snextc();			// skip over start-delimiter
			else if (!(*i)->log_opt_delims)
				break;							// missing start-delimiter is an error, gotta punt
			else
				delim_start = delim_end = '\0';	// didn't find start-delimiter, treat as optional
		}

		// parse the field contents
		read_ptr = read_buf;
		do {
			if ((delim_end != '\0' && c == delim_end) && read_ptr > read_buf && read_ptr[-1] == escape_char)
				--read_ptr;	// escaped end-delimiter...overwrite the escape-character
			else if ((delim_end != '\0' && c == delim_end) ||
					 (delim_end == '\0' && (m_field_split.find(c) != std::string::npos ||
											m_event_split.find(c) != std::string::npos)))
			{
				// we've reached the end of the field contents
				if (delim_end != '\0' && c == delim_end)
					c = buf_ptr->snextc();	// skip over end-delimiter
				break;
			}
			if (read_ptr < m_read_end)
				*(read_ptr++) = c;
			c = buf_ptr->snextc();
		} while (!traits_type::eq_int_type(c, traits_type::eof()));
		*read_ptr = '\0';

		// only parse-and-set values that are not null/empty
		if (read_ptr != read_buf && *read_buf != '\0' && read_buf != (*i)->log_empty_val)
			(*i)->read(read_buf, e);

		// if EOF or end-of-record or not-a-field-delim, gotta punt
		if (traits_type::eq_int_type(c, traits_type::eof()) ||
			m_event_split.find(c) != std::string::npos || m_field_split.find(c) == std::string::npos)
			break;

		do {
			// skip delimiter(s) between fields
			c = buf_ptr->snextc();
			if (!m_consume_delims)
				break;
		} while (!traits_type::eq_int_type(c, traits_type::eof()) && m_field_split.find(c) != std::string::npos);
	}

	// skip the rest of the record...if there's something left
	while (!traits_type::eq_int_type(c, traits_type::eof())) {
		if (m_event_split.find(c) != std::string::npos)
			break;
		c = buf_ptr->snextc();
	}

	// skip "empty events" (i.e. records with no data)
	while (!traits_type::eq_int_type(c, traits_type::eof())) {
		if (m_event_split.find(c) == std::string::npos)
			break;
		c = buf_ptr->snextc();
	}

	if (traits_type::eq_int_type(c, traits_type::eof()))
		input_stream.setstate(std::ios::eofbit);
	return true;
}

void LogCodec::setConfig(const Vocabulary& v, const xmlNodePtr config_ptr)
{
	// first set config options for the Codec base class
	reset();
	Codec::setConfig(v, config_ptr);

	// check if the Codec should flush the output stream after each write
	m_flush_after_write = false;
	std::string flush_option;
	if (ConfigManager::getConfigOption(FLUSH_ELEMENT_NAME, flush_option, config_ptr)) {
		if (flush_option == "true")
			m_flush_after_write = true;
	}

	// check if the Codec should include headers when writing output
	m_handle_elf_headers = false;
	std::string headers_option;
	if (ConfigManager::getConfigOption(HEADERS_ELEMENT_NAME, headers_option, config_ptr)) {
		if (headers_option == "true")
			m_handle_elf_headers = true;
	}

	// check if the Codec should apply an offset when reading and writing dates and times
	bool do_time_offset = false;
	PionDateTime::time_duration_type time_offset(0, 0, 0);
	std::string time_offset_option;
	if (ConfigManager::getConfigOption(TIME_OFFSET_ELEMENT_NAME, time_offset_option, config_ptr)) {
		m_time_offset = boost::lexical_cast<std::int32_t>(time_offset_option);
		if (m_time_offset != 0) {
			do_time_offset = true;
			time_offset = PionDateTime::time_duration_type(0, m_time_offset, 0);
		}
	}

	// next, map the fields to Terms
	xmlNodePtr codec_field_node = config_ptr;
	while ((codec_field_node = ConfigManager::findConfigNodeByName(FIELD_ELEMENT_NAME, codec_field_node)) != NULL) {
		// start with the name of the field (element content)
		xmlChar *xml_char_ptr = xmlNodeGetContent(codec_field_node);
		if (xml_char_ptr == NULL || xml_char_ptr[0] == '\0') {
			if (xml_char_ptr != NULL)
				xmlFree(xml_char_ptr);
			throw EmptyFieldException(getId());
		}
		const std::string field_name(reinterpret_cast<char*>(xml_char_ptr));
		xmlFree(xml_char_ptr);

		// next get the Term we want to map to
		xml_char_ptr = xmlGetProp(codec_field_node, reinterpret_cast<const xmlChar*>(TERM_ATTRIBUTE_NAME.c_str()));
		if (xml_char_ptr == NULL || xml_char_ptr[0] == '\0') {
			if (xml_char_ptr != NULL)
				xmlFree(xml_char_ptr);
			throw EmptyTermException(getId());
		}
		const std::string term_id(reinterpret_cast<char*>(xml_char_ptr));
		xmlFree(xml_char_ptr);

		// make sure that the Term is valid
		const Vocabulary::TermRef term_ref = v.findTerm(term_id);
		if (term_ref == Vocabulary::UNDEFINED_TERM_REF)
			throw UnknownTermException(term_id);

		// get the starting delimiter (if any)
		char delim_start = '\0';
		xml_char_ptr = xmlGetProp(codec_field_node, reinterpret_cast<const xmlChar*>(START_ATTRIBUTE_NAME.c_str()));
		if (xml_char_ptr != NULL) {
			delim_start = cstyle(reinterpret_cast<char*>(xml_char_ptr))[0];
			xmlFree(xml_char_ptr);
		}

		// get the ending delimiter (if any)
		char delim_end = '\0';
		xml_char_ptr = xmlGetProp(codec_field_node, reinterpret_cast<const xmlChar*>(END_ATTRIBUTE_NAME.c_str()));
		if (xml_char_ptr != NULL) {
			delim_end = cstyle(reinterpret_cast<char*>(xml_char_ptr))[0];
			xmlFree(xml_char_ptr);
		}

		// if only one delimiter exists, use it for both
		if (delim_start == '\0' && delim_end != '\0')
			delim_start = delim_end;
		else if (delim_start != '\0' && delim_end == '\0')
			delim_end = delim_start;

		// check if start/end delimiters are optional
		// default is false
		bool opt_delims = false;
		xml_char_ptr = xmlGetProp(codec_field_node, reinterpret_cast<const xmlChar*>(OPTIONAL_ATTRIBUTE_NAME.c_str()));
		if (xml_char_ptr != NULL) {
			const std::string opt_option(reinterpret_cast<char*>(xml_char_ptr));
			if (opt_option == "true")
				opt_delims = true;
			xmlFree(xml_char_ptr);
		}
		
		// check if field is urlencoded
		// default is false
		bool urlencode = false;
		xml_char_ptr = xmlGetProp(codec_field_node, reinterpret_cast<const xmlChar*>(URLENCODE_ATTRIBUTE_NAME.c_str()));
		if (xml_char_ptr != NULL) {
			const std::string urlencode_str(reinterpret_cast<char*>(xml_char_ptr));
			xmlFree(xml_char_ptr);
			if (urlencode_str == "true")
				urlencode = true;
		}

		// get the escape character (if any)
		// default is "\"
		char escape_char = '\\';
		xml_char_ptr = xmlGetProp(codec_field_node, reinterpret_cast<const xmlChar*>(ESCAPE_ATTRIBUTE_NAME.c_str()));
		if (xml_char_ptr != NULL) {
			escape_char = cstyle(reinterpret_cast<char*>(xml_char_ptr))[0];
			xmlFree(xml_char_ptr);
		}

		// get the empty value (if any)
		// default is "-" if there are no delimiters
		std::string empty_val = (delim_start == '\0') ? "-" : "";
		xml_char_ptr = xmlGetProp(codec_field_node, reinterpret_cast<const xmlChar*>(EMPTY_ATTRIBUTE_NAME.c_str()));
		if (xml_char_ptr != NULL) {
			empty_val = cstyle(reinterpret_cast<char*>(xml_char_ptr));
			xmlFree(xml_char_ptr);
		}

		// add the field mapping
		mapFieldToTerm(field_name, v[term_ref], delim_start, delim_end, opt_delims, urlencode, escape_char, empty_val, do_time_offset, time_offset);

		// step to the next field mapping
		codec_field_node = codec_field_node->next;
	}

	// initialize event specifications
	m_event_split = EVENT_SPLIT_SET;
	m_event_join = EVENT_JOIN_STRING;
	m_comment_chars = COMMENT_CHAR_SET;

	// handle event specifications
	xmlNodePtr events_node = ConfigManager::findConfigNodeByName(EVENTS_ELEMENT_NAME, config_ptr);
	if (events_node != NULL) {
		xmlChar *xml_char_ptr;
		// get the split set (if any)
		xml_char_ptr = xmlGetProp(events_node, reinterpret_cast<const xmlChar*>(SPLIT_ATTRIBUTE_NAME.c_str()));
		if (xml_char_ptr != NULL) {
			if (cstyle(reinterpret_cast<char*>(xml_char_ptr))[0] != '\0')
				m_event_split = reinterpret_cast<char*>(xml_char_ptr);
			xmlFree(xml_char_ptr);
		}
		// get the join string (if any)
		xml_char_ptr = xmlGetProp(events_node, reinterpret_cast<const xmlChar*>(JOIN_ATTRIBUTE_NAME.c_str()));
		if (xml_char_ptr != NULL) {
			if (cstyle(reinterpret_cast<char*>(xml_char_ptr))[0] != '\0')
				m_event_join = reinterpret_cast<char*>(xml_char_ptr);
			xmlFree(xml_char_ptr);
		}
		// get the comment chars (if any)
		xml_char_ptr = xmlGetProp(events_node, reinterpret_cast<const xmlChar*>(COMMENT_ATTRIBUTE_NAME.c_str()));
		if (xml_char_ptr != NULL) {
			if (cstyle(reinterpret_cast<char*>(xml_char_ptr))[0] != '\0')
				m_comment_chars = reinterpret_cast<char*>(xml_char_ptr);
			xmlFree(xml_char_ptr);
		}
	}

	// initialize field specifications
	m_field_split = FIELD_SPLIT_SET;
	m_field_join = FIELD_JOIN_STRING;
	m_consume_delims = CONSUME_DELIMS_FLAG;

	// if this is ELF data, use defaults only!
	if (m_handle_elf_headers)
		return;

	// handle field specifications
	xmlNodePtr fields_node = ConfigManager::findConfigNodeByName(FIELDS_ELEMENT_NAME, config_ptr);
	if (fields_node != NULL) {
		xmlChar *xml_char_ptr;
		// get the split set (if any)
		xml_char_ptr = xmlGetProp(fields_node, reinterpret_cast<const xmlChar*>(SPLIT_ATTRIBUTE_NAME.c_str()));
		if (xml_char_ptr != NULL) {
			if (cstyle(reinterpret_cast<char*>(xml_char_ptr))[0] != '\0')
				m_field_split = reinterpret_cast<char*>(xml_char_ptr);
			xmlFree(xml_char_ptr);
		}
		// get the join string (if any)
		xml_char_ptr = xmlGetProp(fields_node, reinterpret_cast<const xmlChar*>(JOIN_ATTRIBUTE_NAME.c_str()));
		if (xml_char_ptr != NULL) {
			if (cstyle(reinterpret_cast<char*>(xml_char_ptr))[0] != '\0')
				m_field_join = reinterpret_cast<char*>(xml_char_ptr);
			xmlFree(xml_char_ptr);
		}
		// check if the Codec should consume consecutive field delimiters
		xml_char_ptr = xmlGetProp(fields_node, reinterpret_cast<const xmlChar*>(CONSUME_ATTRIBUTE_NAME.c_str()));
		if (xml_char_ptr != NULL) {
			const std::string consume_option(reinterpret_cast<char*>(xml_char_ptr));
			if (consume_option == "false")
				m_consume_delims = false;
			else if (consume_option == "true")
				m_consume_delims = true;
			xmlFree(xml_char_ptr);
		}
	}
}

void LogCodec::updateVocabulary(const Vocabulary& v)
{
	// first update anything in the Codec base class that might be needed
	Codec::updateVocabulary(v);

	/// copy Term data over from the updated Vocabulary
	for (CurrentFormat::iterator i = m_format.begin(); i != m_format.end(); ++i) {
		// refresh term 
		v.refreshTerm((*i)->log_term);

		// for date/time types, update log_time_facet
		switch ((*i)->log_term.term_type) {
			case pion::platform::Vocabulary::TYPE_DATE_TIME:
			case pion::platform::Vocabulary::TYPE_DATE:
			case pion::platform::Vocabulary::TYPE_TIME:
				(*i)->log_time_facet.setFormat((*i)->log_term.term_format);
				break;
			default:
				break; // do nothing
		}
	}
}

}	// end namespace plugins
}	// end namespace pion


/// creates new LogCodec objects
extern "C" PION_PLUGIN_API pion::platform::Codec *pion_create_LogCodec(void) {
	return new pion::plugins::LogCodec();
}

/// destroys LogCodec objects
extern "C" PION_PLUGIN_API void pion_destroy_LogCodec(pion::plugins::LogCodec *codec_ptr) {
	delete codec_ptr;
}

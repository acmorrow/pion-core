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

#include <yajl/yajl_parse.h>
#include <yajl/yajl_gen.h>
#include <pion/platform/ConfigManager.hpp>
#include <pion/platform/Event.hpp>
#include <pion/platform/Vocabulary.hpp>
#include "JSONCodec.hpp"

using namespace pion::platform;


namespace pion {		// begin namespace pion
namespace plugins {		// begin namespace plugins


// static members of JSONCodec
const std::string			JSONCodec::CONTENT_TYPE = "text/json";
const std::string			JSONCodec::FIELD_ELEMENT_NAME = "Field"; // TODO: Shouldn't this really be Codec::FIELD_ELEMENT_NAME?
const std::string			JSONCodec::TERM_ATTRIBUTE_NAME = "term"; // TODO: Shouldn't this really be Codec::TERM_ATTRIBUTE_NAME?
const unsigned int			JSONCodec::READ_BUFFER_SIZE = 4096;	     // TODO: What should this be?
std::string					JSONCodec::INDENT_STRING = "\t";


// JSONCodec member functions

CodecPtr JSONCodec::clone(void) const
{
	JSONCodec *new_codec(new JSONCodec());
	new_codec->copyCodec(*this);
	for (CurrentFormat::const_iterator i = m_format.begin(); i != m_format.end(); ++i) {
		new_codec->mapFieldToTerm((*i)->field_name, (*i)->term);
	}
	new_codec->m_JSON_field_ptr_map = m_JSON_field_ptr_map;
	new_codec->m_no_events_written = true;
	return CodecPtr(new_codec);
}

void JSONCodec::write(std::ostream& out, const Event& e)
{
	if (m_no_events_written) {
		yajl_gen_config conf;
		conf.beautify = 1;
		conf.indentString = INDENT_STRING.c_str();
		m_yajl_generator = yajl_gen_alloc(&conf, NULL);
		yajl_gen_array_open(m_yajl_generator);
		m_no_events_written = false;
	}

	// output '{' to mark the beginning of the event
	yajl_gen_map_open(m_yajl_generator);

	// iterate through each field in the current format
	CurrentFormat::const_iterator i;
	std::string value_str;
	const pion::platform::Event::BlobType* ss;
	for (i = m_format.begin(); i != m_format.end(); ++i) {
		// get the range of values for the field Term
		pion::platform::Vocabulary::TermRef term_ref = (*i)->term.term_ref;
		Event::ValuesRange range = e.equal_range(term_ref);

		// generate a JSON name/value pair for each value
		for (Event::ConstIterator i2 = range.first; i2 != range.second; ++i2) {
			yajl_gen_string(m_yajl_generator, (unsigned char*)(*i)->field_name.c_str(), (*i)->field_name.size());
			switch ((*i)->term.term_type) {
				case pion::platform::Vocabulary::TYPE_NULL:
				case pion::platform::Vocabulary::TYPE_OBJECT:
					// TODO: should we output an empty string instead of nothing?
					break;
				case pion::platform::Vocabulary::TYPE_INT8:
				case pion::platform::Vocabulary::TYPE_INT16:
				case pion::platform::Vocabulary::TYPE_INT32:
					yajl_gen_integer(m_yajl_generator, boost::get<std::int32_t>(i2->value));
					break;
				case pion::platform::Vocabulary::TYPE_INT64:
					value_str = boost::lexical_cast<std::string>(boost::get<std::int64_t>(i2->value));
					yajl_gen_string(m_yajl_generator, (unsigned char*)value_str.c_str(), value_str.size());
					break;
				case pion::platform::Vocabulary::TYPE_UINT8:
				case pion::platform::Vocabulary::TYPE_UINT16:
				case pion::platform::Vocabulary::TYPE_UINT32:
					yajl_gen_integer(m_yajl_generator, boost::get<std::uint32_t>(i2->value));
					break;
				case pion::platform::Vocabulary::TYPE_UINT64:
					value_str = boost::lexical_cast<std::string>(boost::get<std::uint64_t>(i2->value));
					yajl_gen_string(m_yajl_generator, (unsigned char*)value_str.c_str(), value_str.size());
					break;
				case pion::platform::Vocabulary::TYPE_FLOAT:
					yajl_gen_double(m_yajl_generator, boost::get<float>(i2->value));
					break;
				case pion::platform::Vocabulary::TYPE_DOUBLE:
					// using boost::lexical_cast<std::string> ensures precision appropriate to type double
					value_str = boost::lexical_cast<std::string>(boost::get<double>(i2->value));
					yajl_gen_string(m_yajl_generator, (unsigned char*)value_str.c_str(), value_str.size());
					break;
				case pion::platform::Vocabulary::TYPE_LONG_DOUBLE:
					// using boost::lexical_cast<std::string> ensures precision appropriate to type long double
					value_str = boost::lexical_cast<std::string>(boost::get<long double>(i2->value));
					yajl_gen_string(m_yajl_generator, (unsigned char*)value_str.c_str(), value_str.size());
					break;
				case pion::platform::Vocabulary::TYPE_SHORT_STRING:
				case pion::platform::Vocabulary::TYPE_STRING:
				case pion::platform::Vocabulary::TYPE_LONG_STRING:
				case pion::platform::Vocabulary::TYPE_BLOB:
				case pion::platform::Vocabulary::TYPE_ZBLOB:
					ss = &boost::get<const pion::platform::Event::BlobType&>(i2->value);
					yajl_gen_string(m_yajl_generator, (unsigned char*)ss->get(), ss->size());
					break;
				case pion::platform::Vocabulary::TYPE_CHAR:
					ss = &boost::get<const pion::platform::Event::BlobType&>(i2->value);
					yajl_gen_string(m_yajl_generator, (unsigned char*)ss->get(),
									ss->size() < (*i)->term.term_size? ss->size() : (*i)->term.term_size);
					break;
				case pion::platform::Vocabulary::TYPE_DATE_TIME:
				case pion::platform::Vocabulary::TYPE_DATE:
				case pion::platform::Vocabulary::TYPE_TIME:
					(*i)->time_facet.toString(value_str, boost::get<const PionDateTime&>(i2->value));
					yajl_gen_string(m_yajl_generator, (unsigned char*)value_str.c_str(), value_str.size());
					break;
			}
		}
	}

	// output '}' to mark the end of the event
	yajl_gen_map_close(m_yajl_generator);

	const unsigned char* buf;
	unsigned int len;
	yajl_gen_get_buf(m_yajl_generator, &buf, &len);
	out.write((char*)buf, len);
	yajl_gen_clear(m_yajl_generator);

	// flush the output stream
	if (m_flush_after_write)
		out.flush();
}

void JSONCodec::finish(std::ostream& out)
{
	if (m_no_events_written)
		return;

	// write the JSON array end token ']'
	yajl_gen_array_close(m_yajl_generator);
	const unsigned char* buf;
	unsigned int len;
	yajl_gen_get_buf(m_yajl_generator, &buf, &len);
	out.write((char*)buf, len);
	yajl_gen_clear(m_yajl_generator);

	// we're done with the generator, so release it
	yajl_gen_free(m_yajl_generator);
	m_yajl_generator = NULL;
}

int number_handler(void* ctx, const char* number_char_ptr, unsigned int number_len)
{
	Context* c = (Context*)ctx;
	JSONCodec::JSONObject& json_object = *c->json_object_ptr;
	json_object.insert(std::make_pair(c->term_ref, std::string(number_char_ptr, number_len)));

	return 1;
}

int string_handler(void* ctx, const unsigned char* string_val, unsigned int string_len)
{
	Context* c = (Context*)ctx;
	JSONCodec::JSONObject& json_object = *c->json_object_ptr;
	json_object.insert(std::make_pair(c->term_ref, std::string((char*)string_val, string_len)));

	return 1;
}

int map_key_handler(void* ctx, const unsigned char* stringVal, unsigned int stringLen)
{
	Context* c = (Context*)ctx;
	JSONCodec::FieldMap::const_iterator i = c->field_map.find(std::string((char*)stringVal, stringLen));
	c->term_ref = i->second->term.term_ref;

	return 1;
}

int start_map_handler(void* ctx)
{
	Context* c = (Context*)ctx;
	c->json_object_ptr = JSONCodec::JSONObjectPtr(new JSONCodec::JSONObject);
	
	return 1;
}

int end_map_handler(void* ctx)
{
	Context* c = (Context*)ctx;
	c->json_object_queue.push(c->json_object_ptr);

	return 1;
}

int start_array_handler(void* ctx)
{
	Context* c = (Context*)ctx;
	c->m_array_started = true;

	return 1;
}

int end_array_handler(void* ctx)
{
	Context* c = (Context*)ctx;
	c->m_array_ended = true;

	return 1;
}

static yajl_callbacks callbacks = {
	NULL,
	NULL,
	NULL,
	NULL,
	number_handler,
	string_handler,
	start_map_handler,
	map_key_handler,
	end_map_handler,
	start_array_handler,
	end_array_handler
};


bool JSONCodec::read(std::istream& in, Event& e)
{
	if (e.getType() != getEventType())
		throw WrongEventTypeException();

	e.clear();

	if (m_first_read_attempt) {
		yajl_parser_config cfg = { 1, 1 };
		m_context = std::shared_ptr<Context>(new Context(m_field_map, m_json_object_queue));
		m_yajl_handle = yajl_alloc(&callbacks, &cfg, NULL, (void*)m_context.get());
		m_first_read_attempt = false;
	}

	if (!m_context->m_array_started) {

		// consume white space
		streambuf_type* buf_ptr = in.rdbuf();
		int_type c = buf_ptr->sgetc();
		while (1) {
			if (traits_type::eq_int_type(c, traits_type::eof()))
				return false;
			if (c == ' ' || c == '\t' || c == '\x0A' || c == '\x0D') {
				c = buf_ptr->snextc();
			} else {
				break;
			}
		}

		// first non-whitespace char must be '['
		if (c != '[')
			throw PionException("input stream is not a JSON array");

		// Since c has not been consumed, m_context->m_array_started will be set to true
		// the first time yajl_parse is called.
	}

	while (m_json_object_queue.empty()) {
		if (m_context->m_array_ended) {
			// Setting eofbit gives the caller a way to distinguish between the case where there is
			// currently not enough data in the stream to parse an Event and the case where the
			// end marker of the Event sequence has been reached (and no more Events are in the queue).
			in.setstate(std::ios::eofbit);

			return false;
		}

		// read up to READ_BUFFER_SIZE bytes into a buffer from the input stream
		char data[READ_BUFFER_SIZE];
		streambuf_type* buf_ptr = in.rdbuf();
		char* p = data;
		std::streamsize num_bytes_read;
		for (num_bytes_read = 0; num_bytes_read < static_cast<std::streamsize>(READ_BUFFER_SIZE); ++num_bytes_read) {
			*p = buf_ptr->sbumpc();
			if (traits_type::eq_int_type(*p, traits_type::eof()))
				break;
			++p;
		}

		if (num_bytes_read == 0)
			return false;

		yajl_status stat = yajl_parse(m_yajl_handle, (unsigned char*)data, num_bytes_read);

		if (stat == yajl_status_ok) {
			// The end of the JSON array was parsed.  Events might have been added to the queue
			// before the end was reached, so continue.
			PION_ASSERT(m_context->m_array_ended);
		} else if (stat == yajl_status_insufficient_data) {
			// The queue might or might not have an Event now, so continue.
			// If the queue is still empty, more data will be read in.
		} else {
			// TODO: handle yajl_status_client_canceled and yajl_status_error
			return false;
		}
	}

	JSONCodec::JSONObjectPtr json_object_ptr = m_json_object_queue.front();
	m_json_object_queue.pop();
	const JSONCodec::JSONObject& json_object = *json_object_ptr;

	for (JSONCodec::JSONObject::const_iterator i = json_object.begin(); i != json_object.end(); ++i) {
		const pion::platform::Vocabulary::TermRef term_ref = i->first;
		const std::string& value_str = i->second;
		const pion::platform::Vocabulary::Term& term = m_JSON_field_ptr_map[term_ref]->term;
		switch (term.term_type) {
			case pion::platform::Vocabulary::TYPE_NULL:
			case pion::platform::Vocabulary::TYPE_OBJECT:
				// do nothing
				break;
			case pion::platform::Vocabulary::TYPE_INT8:
			case pion::platform::Vocabulary::TYPE_INT16:
			case pion::platform::Vocabulary::TYPE_INT32:
				e.setInt(term_ref, boost::lexical_cast<std::int32_t>(value_str));
				break;
			case pion::platform::Vocabulary::TYPE_INT64:
				e.setBigInt(term_ref, boost::lexical_cast<std::int64_t>(value_str));
				break;
			case pion::platform::Vocabulary::TYPE_UINT8:
			case pion::platform::Vocabulary::TYPE_UINT16:
			case pion::platform::Vocabulary::TYPE_UINT32:
				e.setUInt(term_ref, boost::lexical_cast<std::uint32_t>(value_str));
				break;
			case pion::platform::Vocabulary::TYPE_UINT64:
				e.setUBigInt(term_ref, boost::lexical_cast<std::uint64_t>(value_str));
				break;
			case pion::platform::Vocabulary::TYPE_FLOAT:
				e.setFloat(term_ref, boost::lexical_cast<float>(value_str));
				break;
			case pion::platform::Vocabulary::TYPE_DOUBLE:
				e.setDouble(term_ref, boost::lexical_cast<double>(value_str));
				break;
			case pion::platform::Vocabulary::TYPE_LONG_DOUBLE:
				e.setLongDouble(term_ref, boost::lexical_cast<long double>(value_str));
				break;
			case pion::platform::Vocabulary::TYPE_SHORT_STRING:
			case pion::platform::Vocabulary::TYPE_STRING:
			case pion::platform::Vocabulary::TYPE_LONG_STRING:
				e.setString(term_ref, value_str);
				break;
			case pion::platform::Vocabulary::TYPE_BLOB:
			case pion::platform::Vocabulary::TYPE_ZBLOB:
				e.setBlob(term_ref, value_str);
				break;
			case pion::platform::Vocabulary::TYPE_CHAR:
				if (value_str.size() > term.term_size) {
					e.setString(term_ref, std::string(value_str, 0, term.term_size));
				} else {
					e.setString(term_ref, value_str);
				}
				break;
			case pion::platform::Vocabulary::TYPE_DATE_TIME:
			case pion::platform::Vocabulary::TYPE_DATE:
			case pion::platform::Vocabulary::TYPE_TIME:
			{
				PionDateTime dt;
				m_JSON_field_ptr_map[term_ref]->time_facet.fromString(value_str, dt);
				e.setDateTime(term_ref, dt);
				break;
			}
		}
	}
	return true;
}

void JSONCodec::setConfig(const Vocabulary& v, const xmlNodePtr config_ptr)
{
	// first set config options for the Codec base class
	reset();
	Codec::setConfig(v, config_ptr);

	// TODO: options

	// next, map the fields to Terms
	xmlNodePtr codec_field_node = config_ptr;
	while ( (codec_field_node = ConfigManager::findConfigNodeByName(FIELD_ELEMENT_NAME, codec_field_node)) != NULL) {
		// parse new field mapping

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

		// add the field mapping
		mapFieldToTerm(field_name, v[term_ref]);

		// step to the next field mapping
		codec_field_node = codec_field_node->next;
	}

	m_JSON_field_ptr_map.clear();
	for (FieldMap::const_iterator i = m_field_map.begin(); i != m_field_map.end(); ++i) {
		if (m_JSON_field_ptr_map.find(i->second->term.term_ref) != m_JSON_field_ptr_map.end())
			throw PionException("Duplicate Field Term");
		m_JSON_field_ptr_map[i->second->term.term_ref] = i->second;
	}
}

void JSONCodec::updateVocabulary(const Vocabulary& v)
{
	// first update anything in the Codec base class that might be needed
	Codec::updateVocabulary(v);

	/// copy Term data over from the updated Vocabulary
	for (CurrentFormat::iterator i = m_format.begin(); i != m_format.end(); ++i) {
		// refresh term 
		v.refreshTerm((*i)->term);

		// for date/time types, update time_facet
		switch ((*i)->term.term_type) {
			case pion::platform::Vocabulary::TYPE_DATE_TIME:
			case pion::platform::Vocabulary::TYPE_DATE:
			case pion::platform::Vocabulary::TYPE_TIME:
				(*i)->time_facet.setFormat((*i)->term.term_format);
				break;
			default:
				break; // do nothing
		}
	}
}


}	// end namespace plugins
}	// end namespace pion


/// creates new JSONCodec objects
extern "C" PION_PLUGIN_API pion::platform::Codec *pion_create_JSONCodec(void) {
	return new pion::plugins::JSONCodec();
}

/// destroys JSONCodec objects
extern "C" PION_PLUGIN_API void pion_destroy_JSONCodec(pion::plugins::JSONCodec *codec_ptr) {
	delete codec_ptr;
}

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

#include <libxml/tree.h>
#include <libxml/xmlwriter.h>
#include <libxml/xmlreader.h>
#include <pion/platform/ConfigManager.hpp>
#include <pion/platform/Event.hpp>
#include <pion/platform/Vocabulary.hpp>
#include "XMLCodec.hpp"

using namespace pion::platform;


namespace pion {		// begin namespace pion
namespace plugins {		// begin namespace plugins


// static members of XMLCodec
const std::string			XMLCodec::CONTENT_TYPE = "text/xml";
const std::string			XMLCodec::EVENT_TAG_ELEMENT_NAME = "EventTag";
const std::string			XMLCodec::EVENT_CONTAINER_TAG_ELEMENT_NAME = "EventContainerTag";
const std::string			XMLCodec::FIELD_ELEMENT_NAME = "Field";
const std::string			XMLCodec::TERM_ATTRIBUTE_NAME = "term";
const std::string			XMLCodec::DEFAULT_EVENT_TAG = "Event";
const std::string			XMLCodec::DEFAULT_EVENT_CONTAINER_TAG = "Events";


// XMLCodec member functions

CodecPtr XMLCodec::clone(void) const
{
	XMLCodec *new_codec(new XMLCodec());
	new_codec->copyCodec(*this);
	for (CurrentFormat::const_iterator i = m_format.begin(); i != m_format.end(); ++i) {
		new_codec->mapFieldToTerm((*i)->field_name, (*i)->term);
	}
	new_codec->m_XML_field_ptr_map = m_XML_field_ptr_map;
	new_codec->m_event_container_tag = m_event_container_tag;
	new_codec->m_event_tag = m_event_tag;
	new_codec->m_no_events_written = true;
	return CodecPtr(new_codec);
}

void XMLCodec::write(std::ostream& out, const Event& e)
{
	if (m_no_events_written) {
		m_buf = xmlBufferCreate();
		if (m_buf == NULL)
			throw PionException("Error creating xmlBuffer");

		m_xml_writer = xmlNewTextWriterMemory(m_buf, 0 /* no compression */);
		if (m_xml_writer == NULL) 
			throw PionException("Error creating xmlTextWriter");

		// Initialize indentation.
		if (xmlTextWriterSetIndent(m_xml_writer, 1) < 0)
			throw PionException("xmlTextWriter failed to turn on indentation");
		if (xmlTextWriterSetIndentString(m_xml_writer, (xmlChar*)"\t") < 0)
			throw PionException("xmlTextWriter failed to set the indent string");
		
		// Write an XML header.
		if (xmlTextWriterStartDocument(m_xml_writer, NULL, "UTF-8", NULL) < 0)
			throw PionException("xmlTextWriter failed to write the start of the document");

		if (! m_event_container_tag.empty()) {
			// Start the root element, named "Events".
			if (xmlTextWriterStartElement(m_xml_writer, (xmlChar*)m_event_container_tag.c_str()) < 0)
				throw PionException("xmlTextWriter failed to write a start-tag");
		}
		m_no_events_written = false;
	}

	if (xmlTextWriterStartElement(m_xml_writer, (xmlChar*)m_event_tag.c_str()) < 0)
		throw PionException("xmlTextWriter failed to write an Event start-tag");

	// Iterate through each field in the current format.
	CurrentFormat::const_iterator i;
	std::string value_str;
	const pion::platform::Event::BlobType* ss;
	for (i = m_format.begin(); i != m_format.end(); ++i) {
		// Get the range of values for the field Term.
		pion::platform::Vocabulary::TermRef term_ref = (*i)->term.term_ref;
		Event::ValuesRange range = e.equal_range(term_ref);
		xmlChar* field_name = (xmlChar*)(*i)->field_name.c_str();

		// Generate an XML element for each value.
		for (Event::ConstIterator i2 = range.first; i2 != range.second; ++i2) {
			int rc = 0;
			switch ((*i)->term.term_type) {
				case pion::platform::Vocabulary::TYPE_NULL:
				case pion::platform::Vocabulary::TYPE_OBJECT:
					// TODO: should we output an empty element instead of nothing?
					break;
				case pion::platform::Vocabulary::TYPE_INT8:
				case pion::platform::Vocabulary::TYPE_INT16:
				case pion::platform::Vocabulary::TYPE_INT32:
					rc = xmlTextWriterWriteFormatElement(m_xml_writer, field_name, "%d", boost::get<std::int32_t>(i2->value));
					break;
				case pion::platform::Vocabulary::TYPE_INT64:
					value_str = boost::lexical_cast<std::string>(boost::get<std::int64_t>(i2->value));
					rc = xmlTextWriterWriteElement(m_xml_writer, field_name, (xmlChar*)value_str.c_str());
					break;
				case pion::platform::Vocabulary::TYPE_UINT8:
				case pion::platform::Vocabulary::TYPE_UINT16:
				case pion::platform::Vocabulary::TYPE_UINT32:
					rc = xmlTextWriterWriteFormatElement(m_xml_writer, field_name, "%d", boost::get<std::uint32_t>(i2->value));
					break;
				case pion::platform::Vocabulary::TYPE_UINT64:
					value_str = boost::lexical_cast<std::string>(boost::get<std::uint64_t>(i2->value));
					rc = xmlTextWriterWriteElement(m_xml_writer, field_name, (xmlChar*)value_str.c_str());
					break;
				case pion::platform::Vocabulary::TYPE_FLOAT:
					rc = xmlTextWriterWriteFormatElement(m_xml_writer, field_name, "%g", boost::get<float>(i2->value));
					break;
				case pion::platform::Vocabulary::TYPE_DOUBLE:
					// Using boost::lexical_cast<std::string> ensures precision appropriate to type double.
					value_str = boost::lexical_cast<std::string>(boost::get<double>(i2->value));
					rc = xmlTextWriterWriteElement(m_xml_writer, field_name, (xmlChar*)value_str.c_str());
					break;
				case pion::platform::Vocabulary::TYPE_LONG_DOUBLE:
					// Using boost::lexical_cast<std::string> ensures precision appropriate to type long double.
					value_str = boost::lexical_cast<std::string>(boost::get<long double>(i2->value));
					rc = xmlTextWriterWriteElement(m_xml_writer, field_name, (xmlChar*)value_str.c_str());
					break;

				case pion::platform::Vocabulary::TYPE_SHORT_STRING:
				case pion::platform::Vocabulary::TYPE_STRING:
				case pion::platform::Vocabulary::TYPE_LONG_STRING:
				case pion::platform::Vocabulary::TYPE_BLOB:
				case pion::platform::Vocabulary::TYPE_ZBLOB:
					ss = &boost::get<const pion::platform::Event::BlobType&>(i2->value);
					rc = xmlTextWriterWriteElement(m_xml_writer, field_name, (xmlChar*)ss->get());
					break;
				case pion::platform::Vocabulary::TYPE_CHAR:
					ss = &boost::get<const pion::platform::Event::BlobType&>(i2->value);
					if (ss->size() <= (*i)->term.term_size) {
						rc = xmlTextWriterWriteElement(m_xml_writer, field_name, (xmlChar*)ss->get());
					} else {
						std::string trunc_str(ss->get(), (*i)->term.term_size);
						rc = xmlTextWriterWriteElement(m_xml_writer, field_name, (xmlChar*)trunc_str.c_str());
					}
					break;

				case pion::platform::Vocabulary::TYPE_DATE_TIME:
				case pion::platform::Vocabulary::TYPE_DATE:
				case pion::platform::Vocabulary::TYPE_TIME:
					(*i)->time_facet.toString(value_str, boost::get<const PionDateTime&>(i2->value));
					rc = xmlTextWriterWriteElement(m_xml_writer, field_name, (xmlChar*)value_str.c_str());
					break;
			}
			if (rc < 0)
				throw PionException("xmlTextWriter failed to write a Term element");
		}
	}

	if (xmlTextWriterEndElement(m_xml_writer) < 0)
		throw PionException("xmlTextWriter failed to write an Event end-tag");

	if (xmlTextWriterFlush(m_xml_writer) < 0)
		throw PionException("Error flushing XML writer");
	out.write((char*)m_buf->content, m_buf->use);

	xmlBufferEmpty(m_buf);

	if (m_flush_after_write)
		out.flush();
}

void XMLCodec::finish(std::ostream& out)
{
	if (m_no_events_written)
		return;

	// Write the 'Events' end-tag.
    if (xmlTextWriterEndDocument(m_xml_writer) < 0)
		throw PionException("xmlTextWriter failed to write end of document");

	// Send all remaining data to the output stream.
	if (xmlTextWriterFlush(m_xml_writer) < 0)
		throw PionException("Error flushing XML writer");
	out.write((char*)m_buf->content, m_buf->use);

	// We're done with the writer, so release it and the buffer it uses.
	xmlFreeTextWriter(m_xml_writer);
	m_xml_writer = NULL;
    xmlBufferFree(m_buf);
	m_buf = NULL;
}

int	XMLCodec::xmlInputReadCallback(void* context, char* buffer, int len)
{
	// Read up to len bytes into a buffer from the input stream.
	streambuf_type* buf_ptr = ((std::istream*)context)->rdbuf();
	char* p = buffer;
	std::streamsize num_bytes_read;
	for (num_bytes_read = 0; num_bytes_read < len; ++num_bytes_read) {
		*p = buf_ptr->sbumpc();
		if (traits_type::eq_int_type(*p, traits_type::eof()))
			break;
		++p;
	}

	return num_bytes_read;
}

int	XMLCodec::xmlInputCloseCallback(void* context)
{
	return 0;
}

bool XMLCodec::read(std::istream& in, Event& e)
{
	if (e.getType() != getEventType())
		throw WrongEventTypeException();

	e.clear();
	
	if (m_first_read_attempt) {
		// verify that field mappings are defined
		if (m_field_map.empty())
			throw PionException("Codec is not configured yet.");

		// initialize xml reader to use
	    m_xml_reader = xmlReaderForIO(xmlInputReadCallback, xmlInputCloseCallback, (void*)(&in), NULL, NULL, XML_PARSE_NOBLANKS);
		if (!m_xml_reader)
			throw PionException("failed to create XML parser");

		if (! m_event_container_tag.empty()) {
			// parse the 'Events' start-tag
			while (1) {
				int ret = xmlTextReaderRead(m_xml_reader);
				if (ret == 0) {
					return false;
				} else if (ret < 0) {
					throw PionException("XML parser error");
				}
		
				if (xmlTextReaderNodeType(m_xml_reader) == 1) {
					const xmlChar* name = xmlTextReaderConstName(m_xml_reader);
					if (name == NULL)
						throw PionException("XML parser error");
					if (strcmp((char*)name, m_event_container_tag.c_str()) == 0)
						break;
				}
			}
		}

		m_first_read_attempt = false;
	}
	
	// parse the input until the next 'Event' start-tag is found
	while (1) {
		int ret = xmlTextReaderRead(m_xml_reader);
		if (ret == 0) {
			// set eof bit (finished ok) if no outside container
			if (m_event_container_tag.empty())
				in.setstate(std::ios::eofbit);
			return false;
		} else if (ret < 0) {
			throw PionException("XML parser error");
		}
		const xmlChar* name = xmlTextReaderConstName(m_xml_reader);
		if (xmlTextReaderNodeType(m_xml_reader) == 1) {	// start xml element
			if (name == NULL)
				throw PionException("XML parser error");
			if (strcmp((char*)name, m_event_tag.c_str()) == 0)
				break;
		} else if (xmlTextReaderNodeType(m_xml_reader) == 15) {	// end xml element
			if (name == NULL)
				throw PionException("XML parser error");
			// check for </Event>
			if (strcmp((char*)name, m_event_tag.c_str()) == 0) {
				return true;	// empty event !
			}
			// check for </Events>
			if (!m_event_container_tag.empty() && strcmp((char*)name, m_event_container_tag.c_str()) == 0) {
				// Setting eofbit gives the caller a way to distinguish between the case where there is
				// currently not enough data in the stream to parse an Event and the case where the
				// Events end-tag has been reached.
				in.setstate(std::ios::eofbit);
				return false;
			}
		}
	}

	// parse the input until the 'Event' end-tag is found
	std::string open_element_name;
	while (1) {
		if (xmlTextReaderRead(m_xml_reader) != 1)
			throw PionException("XML parser error");

		// check for start or end XML element
		const xmlChar* name = xmlTextReaderConstName(m_xml_reader);
		if (xmlTextReaderNodeType(m_xml_reader) == 15) {		// end xml element
			if (name == NULL)
				throw PionException("XML parser error");
			if (strcmp((char*)name, m_event_tag.c_str()) == 0)
				break;	// found </Event> - done parsing current event
			if (!m_event_container_tag.empty() && strcmp((char*)name, m_event_container_tag.c_str()) == 0) {
				// assume </Event> before </Events>
				in.setstate(std::ios::eofbit);
				return true;
			}
			if (strcmp((char*)name, open_element_name.c_str()) == 0)
				open_element_name.clear();	// found closing node for ignored element
		} else if (xmlTextReaderNodeType(m_xml_reader) == 1) {	// start xml element
			// check for field element
			if (name == NULL)
				throw PionException("XML parser error");
			if (! open_element_name.empty())
				continue;	// skip contents if inside of an ignored element
			if (xmlTextReaderIsEmptyElement(m_xml_reader) == 1)
				continue;	// no element contents to extract/assign and attributes not supported yet
			open_element_name = (char*)name;

			// get the Term corresponding to the tag
			XMLCodec::FieldMap::const_iterator i = m_field_map.find(open_element_name);
			if (i == m_field_map.end())
				continue;	// ignore current element
			const pion::platform::Vocabulary::Term& term = i->second->term;

			// get the value of the Term from the contents of the element
			std::string value_str;
			bool found_end_of_field = false;
			while (!found_end_of_field) {
				if (xmlTextReaderRead(m_xml_reader) != 1)
					throw PionException("XML parser error");
				switch (xmlTextReaderNodeType(m_xml_reader)) {
				case 1:	// start element
					name = xmlTextReaderConstName(m_xml_reader);
					if (name) {
						value_str += '<';
						value_str += (char*) name;
						value_str += '>';
					}
					break;
						
				case 15:	// end element
					name = xmlTextReaderConstName(m_xml_reader);
					if (name) {
						if (strcmp((char*)name, open_element_name.c_str()) == 0) {
							// found end of data field
							open_element_name.clear();
							found_end_of_field = true;
							break;
						} else {
							value_str += "</";
							value_str += (char*) name;
							value_str += '>';
						}
					}
					break;
					
				case 3:		// text node
				case 4:		// CDATA node
					name = xmlTextReaderValue(m_xml_reader);
					if (name) {
						value_str += (char*) name;
					}
					break;

				default:
					break;
				}
			}	// done getting field value
					
			switch (term.term_type) {
				case pion::platform::Vocabulary::TYPE_NULL:
				case pion::platform::Vocabulary::TYPE_OBJECT:
					// do nothing
					break;
				case pion::platform::Vocabulary::TYPE_INT8:
				case pion::platform::Vocabulary::TYPE_INT16:
				case pion::platform::Vocabulary::TYPE_INT32:
					e.setInt(term.term_ref, boost::lexical_cast<std::int32_t>(value_str));
					break;
				case pion::platform::Vocabulary::TYPE_INT64:
					e.setBigInt(term.term_ref, boost::lexical_cast<std::int64_t>(value_str));
					break;
				case pion::platform::Vocabulary::TYPE_UINT8:
				case pion::platform::Vocabulary::TYPE_UINT16:
				case pion::platform::Vocabulary::TYPE_UINT32:
					e.setUInt(term.term_ref, boost::lexical_cast<std::uint32_t>(value_str));
					break;
				case pion::platform::Vocabulary::TYPE_UINT64:
					e.setUBigInt(term.term_ref, boost::lexical_cast<std::uint64_t>(value_str));
					break;
				case pion::platform::Vocabulary::TYPE_FLOAT:
					e.setFloat(term.term_ref, boost::lexical_cast<float>(value_str));
					break;
				case pion::platform::Vocabulary::TYPE_DOUBLE:
					e.setDouble(term.term_ref, boost::lexical_cast<double>(value_str));
					break;
				case pion::platform::Vocabulary::TYPE_LONG_DOUBLE:
					e.setLongDouble(term.term_ref, boost::lexical_cast<long double>(value_str));
					break;
				case pion::platform::Vocabulary::TYPE_SHORT_STRING:
				case pion::platform::Vocabulary::TYPE_STRING:
				case pion::platform::Vocabulary::TYPE_LONG_STRING:
					e.setString(term.term_ref, value_str);
					break;
				case pion::platform::Vocabulary::TYPE_BLOB:
				case pion::platform::Vocabulary::TYPE_ZBLOB:
					e.setBlob(term.term_ref, value_str);
					break;
				case pion::platform::Vocabulary::TYPE_CHAR:
					if (value_str.size() > term.term_size) {
						e.setString(term.term_ref, std::string(value_str, 0, term.term_size));
					} else {
						e.setString(term.term_ref, value_str);
					}
					break;
				case pion::platform::Vocabulary::TYPE_DATE_TIME:
				case pion::platform::Vocabulary::TYPE_DATE:
				case pion::platform::Vocabulary::TYPE_TIME:
				{
					PionDateTime dt;
					m_XML_field_ptr_map[term.term_ref]->time_facet.fromString(value_str, dt);
					e.setDateTime(term.term_ref, dt);
					break;
				}
			}
		}
	}

	return true;
}

void XMLCodec::setConfig(const Vocabulary& v, const xmlNodePtr config_ptr)
{
	// first set config options for the Codec base class
	reset();
	Codec::setConfig(v, config_ptr);

	// Read in the tags to use for Events and Event containers, or use the defaults if no tags are specified.
	if (! ConfigManager::getConfigOption(EVENT_TAG_ELEMENT_NAME, m_event_tag, config_ptr)) {
		m_event_tag = DEFAULT_EVENT_TAG;
	}
	if (! ConfigManager::getConfigOption(EVENT_CONTAINER_TAG_ELEMENT_NAME, m_event_container_tag, config_ptr)) {
		if (ConfigManager::findConfigNodeByName(EVENT_CONTAINER_TAG_ELEMENT_NAME, config_ptr)) {
			m_event_container_tag = "";
		} else {
			m_event_container_tag = DEFAULT_EVENT_CONTAINER_TAG;
		}
	}

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

	m_XML_field_ptr_map.clear();
	for (FieldMap::const_iterator i = m_field_map.begin(); i != m_field_map.end(); ++i) {
		if (m_XML_field_ptr_map.find(i->second->term.term_ref) != m_XML_field_ptr_map.end())
			throw PionException("Duplicate Field Term");
		m_XML_field_ptr_map[i->second->term.term_ref] = i->second;
	}
}

void XMLCodec::updateVocabulary(const Vocabulary& v)
{
	// First update anything in the Codec base class that might be needed.
	Codec::updateVocabulary(v);

	/// Copy Term data over from the updated Vocabulary.
	for (CurrentFormat::iterator i = m_format.begin(); i != m_format.end(); ++i) {
		// refresh term 
		v.refreshTerm((*i)->term);

		// For date/time types, update time_facet.
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


/// creates new XMLCodec objects
extern "C" PION_PLUGIN_API pion::platform::Codec *pion_create_XMLCodec(void) {
	return new pion::plugins::XMLCodec();
}

/// destroys XMLCodec objects
extern "C" PION_PLUGIN_API void pion_destroy_XMLCodec(pion::plugins::XMLCodec *codec_ptr) {
	delete codec_ptr;
}

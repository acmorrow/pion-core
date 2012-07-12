// ------------------------------------------------------------------------
// Pion is a development platform for building Reactors that process Events
// ------------------------------------------------------------------------
// Copyright (C) 2007-2009 Atomic Labs, Inc.  (http://www.atomiclabs.com)
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

#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/array.hpp>
#include <pion/platform/ConfigManager.hpp>
#include <pion/platform/CodecFactory.hpp>
#include "FissionReactor.hpp"

using namespace pion::platform;


namespace pion {		// begin namespace pion
namespace plugins {		// begin namespace plugins


// static members of FissionReactor
	
const std::string			FissionReactor::COPY_TERM_ELEMENT_NAME = "CopyTerm";
const std::string			FissionReactor::COPY_ALL_TERMS_ELEMENT_NAME = "CopyAllTerms";
const std::string			FissionReactor::INPUT_EVENT_TYPE_ELEMENT_NAME = "InputEventType";
const std::string			FissionReactor::INPUT_EVENT_TERM_ELEMENT_NAME = "InputEventTerm";
const std::string			FissionReactor::CODEC_ELEMENT_NAME = "Codec";


// FissionReactor member functions

void FissionReactor::setConfig(const Vocabulary& v, const xmlNodePtr config_ptr)
{
	// first set config options for the Reactor base class
	ConfigWriteLock cfg_lock(*this);
	Reactor::setConfig(v, config_ptr);

	// get the input event type
	std::string config_str;
	if (! ConfigManager::getConfigOption(INPUT_EVENT_TYPE_ELEMENT_NAME, config_str, config_ptr))
		throw EmptyInputEventTypeException(getId());

	// find vocabulary term for input event type
	Vocabulary::TermRef term_ref = v.findTerm(config_str);
	if (term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(config_str);
	m_input_event_type = v[term_ref];

	// make sure that term is object/event type
	if (m_input_event_type.term_type != Vocabulary::TYPE_OBJECT)
		throw NotAnObjectException(config_str);

	// get the input event term
	if (! ConfigManager::getConfigOption(INPUT_EVENT_TERM_ELEMENT_NAME, config_str, config_ptr))
		throw EmptyInputEventTermException(getId());

	// find vocabulary term for input event term
	term_ref = v.findTerm(config_str);
	if (term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(config_str);
	m_input_event_term = v[term_ref];

	// only string types are currently supported for input event term
	switch (m_input_event_term.term_type) {
	case Vocabulary::TYPE_NULL:
	case Vocabulary::TYPE_OBJECT:
	case Vocabulary::TYPE_INT8:
	case Vocabulary::TYPE_INT16:
	case Vocabulary::TYPE_INT32:
	case Vocabulary::TYPE_UINT8:
	case Vocabulary::TYPE_UINT16:
	case Vocabulary::TYPE_UINT32:
	case Vocabulary::TYPE_INT64:
	case Vocabulary::TYPE_UINT64:
	case Vocabulary::TYPE_FLOAT:
	case Vocabulary::TYPE_DOUBLE:
	case Vocabulary::TYPE_LONG_DOUBLE:
	case Vocabulary::TYPE_DATE_TIME:
	case Vocabulary::TYPE_DATE:
	case Vocabulary::TYPE_TIME:
		throw TermNotStringException(config_str);
		break;
	case Vocabulary::TYPE_SHORT_STRING:
	case Vocabulary::TYPE_STRING:
	case Vocabulary::TYPE_LONG_STRING:
	case Vocabulary::TYPE_CHAR:
	case Vocabulary::TYPE_BLOB:
	case Vocabulary::TYPE_ZBLOB:
		break;	// these are all OK
	}

	// get the codec to use
	std::unique_lock<std::mutex> codec_lock(m_codec_mutex);
	if (! ConfigManager::getConfigOption(CODEC_ELEMENT_NAME, m_codec_id, config_ptr))
		throw EmptyCodecException(getId());
	getCodecPlugin(m_codec_ptr, m_codec_id);
	PION_ASSERT(m_codec_ptr);
	codec_lock.unlock();

	// check if we should copy all terms from original event
	m_copy_all_terms = false;
	std::string copy_all_terms_str;
	if (ConfigManager::getConfigOption(COPY_ALL_TERMS_ELEMENT_NAME,
									   copy_all_terms_str, config_ptr))
	{
		if (copy_all_terms_str == "true")
			m_copy_all_terms = true;
	}

	// get list of terms to copy from original event
	m_copy_terms.clear();
	xmlNodePtr copy_term_node = config_ptr;
	while ((copy_term_node = ConfigManager::findConfigNodeByName(COPY_TERM_ELEMENT_NAME, copy_term_node)) != NULL) {
		xmlChar *xml_char_ptr = xmlNodeGetContent(copy_term_node);
		if (xml_char_ptr != NULL) {
			const std::string copy_term_str(reinterpret_cast<char*>(xml_char_ptr));
			xmlFree(xml_char_ptr);
			if (! copy_term_str.empty()) {
				// find the term in the Vocabulary
				term_ref = v.findTerm(copy_term_str);
				if (term_ref == Vocabulary::UNDEFINED_TERM_REF)
					throw UnknownTermException(copy_term_str);

				// add it to the copy terms collection
				m_copy_terms.push_back(v[term_ref]);
			}
		}

		// step to the next copy term
		copy_term_node = copy_term_node->next;
	}
}
	
void FissionReactor::updateVocabulary(const Vocabulary& v)
{
	// first update anything in the Reactor base class that might be needed
	ConfigWriteLock cfg_lock(*this);
	Reactor::updateVocabulary(v);

	v.refreshTerm(m_input_event_type);
	v.refreshTerm(m_input_event_term);

	std::lock_guard<std::mutex> codec_lock(m_codec_mutex);
	if (m_codec_ptr)
		m_codec_ptr->updateVocabulary(v);
}

void FissionReactor::updateCodecs(void)
{
	// check if the codec was deleted (if so, stop now!)
	if (! hasCodecPlugin(m_codec_id)) {
		stop();
		std::lock_guard<std::mutex> codec_lock(m_codec_mutex);
		m_codec_ptr.reset();
	} else {
		// update the codec pointer
		std::lock_guard<std::mutex> codec_lock(m_codec_mutex);
		getCodecPlugin(m_codec_ptr, m_codec_id);
		PION_ASSERT(m_codec_ptr);
	}
}
	
void FissionReactor::process(const EventPtr& e)
{
	if (e->getType() == m_input_event_type.term_ref) {
		// used for generating new events
		EventFactory event_factory;
		EventPtr new_ptr;
		bool read_result;

		// iterate through each value defined for the input event term
		Event::ValuesRange range = e->equal_range(m_input_event_term.term_ref);
		for (Event::ConstIterator it = range.first; it != range.second; ++it) {

			// create an input stream based upon the term value
			const Event::BlobType& ss = boost::get<const Event::BlobType&>(it->value);
			boost::iostreams::stream<boost::iostreams::array_source> input_stream(ss.get(), ss.size());

			// read Event(s) from the input stream
			while ( isRunning() && !input_stream.eof() ) {

				// only allow one thread to use the codec at a time
				std::unique_lock<std::mutex> codec_lock(m_codec_mutex);
				event_factory.create(new_ptr, m_codec_ptr->getEventType());
				read_result = m_codec_ptr->read(input_stream, *new_ptr);
				codec_lock.unlock();
				if (! read_result)
					break;

				// copy terms from original event ?
				if (m_copy_all_terms) {
					// copy all terms from original event
					*new_ptr += *e;
				} else if (! m_copy_terms.empty() ) {
					// copy some terms from original event
					for (TermVector::const_iterator term_it = m_copy_terms.begin();
						term_it != m_copy_terms.end(); ++term_it)
					{
						new_ptr->copyValues(*e, term_it->term_ref);
					}
				}

				// deliver the event
				deliverEvent(new_ptr);
			}
		}
	}
}

	
}	// end namespace plugins
}	// end namespace pion


/// creates new FissionReactor objects
extern "C" PION_PLUGIN_API pion::platform::Reactor *pion_create_FissionReactor(void) {
	return new pion::plugins::FissionReactor();
}

/// destroys FissionReactor objects
extern "C" PION_PLUGIN_API void pion_destroy_FissionReactor(pion::plugins::FissionReactor *reactor_ptr) {
	delete reactor_ptr;
}

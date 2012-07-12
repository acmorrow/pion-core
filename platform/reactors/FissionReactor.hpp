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

#ifndef __PION_FISSIONREACTOR_HEADER__
#define __PION_FISSIONREACTOR_HEADER__

#include <string>
#include <vector>
#include <mutex>
#include <pion/PionConfig.hpp>
#include <pion/PionException.hpp>
#include <pion/platform/Codec.hpp>
#include <pion/platform/Reactor.hpp>
#include <pion/platform/Vocabulary.hpp>


namespace pion {		// begin namespace pion
namespace plugins {		// begin namespace plugins


///
/// FissionReactor: used to extract a collection of events derived from a single one
///
class FissionReactor :
	public pion::platform::Reactor
{
public:	
	
	/// exception thrown if the Reactor configuration uses an unknown term in a field mapping
	class UnknownTermException : public PionException {
	public:
		UnknownTermException(const std::string& term_id)
			: PionException("FissionReactor configuration references an unknown term: ", term_id) {}
	};

	/// exception thrown if the Reactor configuration uses a Term that is not an object as the input event type
	class NotAnObjectException : public PionException {
	public:
		NotAnObjectException(const std::string& event_type)
			: PionException("FissionReactor configuration defines a non-object input event type: ", event_type) {}
	};

	/// exception thrown if the term specified for extraction is not string type
	class TermNotStringException : public PionException {
	public:
		TermNotStringException(const std::string& term_id)
			: PionException("FissionReactor non-string input event term specified: ", term_id) {}
	};

	/// exception thrown if the Reactor configuration does not define a Codec
	class EmptyCodecException : public PionException {
	public:
		EmptyCodecException(const std::string& reactor_id)
			: PionException("FissionReactor configuration is missing a required Codec parameter: ", reactor_id) {}
	};

	/// exception thrown if the Reactor configuration does not define a InputEventType
	class EmptyInputEventTypeException : public PionException {
	public:
		EmptyInputEventTypeException(const std::string& reactor_id)
			: PionException("FissionReactor configuration is missing a required InputEventType parameter: ", reactor_id) {}
	};

	/// exception thrown if the Reactor configuration does not define a EmptyInputEventTermException
	class EmptyInputEventTermException : public PionException {
	public:
		EmptyInputEventTermException(const std::string& reactor_id)
			: PionException("FissionReactor configuration is missing a required InputEventTerm parameter: ", reactor_id) {}
	};


	/// constructs a new FissionReactor object
	FissionReactor(void)
		: Reactor(TYPE_PROCESSING), m_copy_all_terms(false),
		m_input_event_type(""), m_input_event_term("")
	{}
	
	/// virtual destructor: this class is meant to be extended
	virtual ~FissionReactor() { stop(); }
	
	/**
	 * sets configuration parameters for this Reactor
	 *
	 * @param v the Vocabulary that this Reactor will use to describe Terms
	 * @param config_ptr pointer to a list of XML nodes containing Reactor
	 *                   configuration parameters
	 */
	virtual void setConfig(const pion::platform::Vocabulary& v, const xmlNodePtr config_ptr);
	
	/**
	 * this updates the Vocabulary information used by this Reactor; it should
	 * be called whenever the global Vocabulary is updated
	 *
	 * @param v the Vocabulary that this Reactor will use to describe Terms
	 */
	virtual void updateVocabulary(const pion::platform::Vocabulary& v);

	/**
	 * this updates the Codecs that are used by this Reactor; it should
	 * be called whenever any Codec's configuration is updated
	 */
	virtual void updateCodecs(void);

	/**
	 * Processes an Event by filtering it out if it's not of the input event type,
	 * otherwise extracting from it the value(s) of the input event term and 
	 * parsing those values into output Events using the Codec.
	 * If any 'copy terms' are specified, the values of those Terms are copied
	 * from the input Event to every resulting output Event.
	 *
	 * @param e pointer to the Event to process
	 */
	virtual void process(const pion::platform::EventPtr& e);
	
	
private:

	/// data type for a collection of Vocabulary terms
	typedef std::vector<pion::platform::Vocabulary::Term>	TermVector;


	/// name of the CopyTerm element for Pion XML config files
	static const std::string			COPY_TERM_ELEMENT_NAME;

	/// name of the CopyAllTerms element for Pion XML config files
	static const std::string			COPY_ALL_TERMS_ELEMENT_NAME;

	/// name of the InputEventType element for Pion XML config files
	static const std::string			INPUT_EVENT_TYPE_ELEMENT_NAME;

	/// name of the InputEventTerm element for Pion XML config files
	static const std::string			INPUT_EVENT_TERM_ELEMENT_NAME;

	/// name of the Codec element for Pion XML config files
	static const std::string			CODEC_ELEMENT_NAME;


	/// if true, copy all terms from the original event into derived events
	bool								m_copy_all_terms;

	/// terms from the original event that should be copied into derived events
	TermVector							m_copy_terms;

	/// only generate new events derived from this base event type
	pion::platform::Vocabulary::Term	m_input_event_type;

	/// generate new events derived from the contents of this term
	pion::platform::Vocabulary::Term	m_input_event_term;

	/// the codec to use for generating new events
	pion::platform::CodecPtr			m_codec_ptr;

	/// unique identifier of the Codec that is used for extracting Events
	std::string							m_codec_id;

	/// used to protect access to the codec pointer
	std::mutex						m_codec_mutex;
};


}	// end namespace plugins
}	// end namespace pion

#endif

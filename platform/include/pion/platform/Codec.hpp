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

#ifndef __PION_CODEC_HEADER__
#define __PION_CODEC_HEADER__

#include <istream>
#include <memory>
#include <ostream>
#include <libxml/tree.h>
#include <pion/PionConfig.hpp>
#include <pion/PionException.hpp>
#include <pion/platform/Event.hpp>
#include <pion/platform/Vocabulary.hpp>
#include <pion/platform/PlatformPlugin.hpp>


namespace pion {		// begin namespace pion
namespace platform {	// begin namespace platform (Pion Platform Library)


///
/// Codec: used to encode and decode Events using a particular format
///
class PION_PLATFORM_API Codec
	: public PlatformPlugin
{
public:

	/// exception thrown if the Codec configuration does not define an event type
	class EmptyEventException : public PionException {
	public:
		EmptyEventException(const std::string& codec_id)
			: PionException("Codec configuration does not define an event type: ", codec_id) {}
	};

	/// exception thrown if the Codec configuration references an unknown event type
	class UnknownTermException : public PionException {
	public:
		UnknownTermException(const std::string& event_type)
			: PionException("Codec configuration references an unknown event type: ", event_type) {}
	};

	/// exception thrown if the Codec configuration uses an event type for a Term that is not an object
	class NotAnObjectException : public PionException {
	public:
		NotAnObjectException(const std::string& event_type)
			: PionException("Codec configuration defines a non-object event type: ", event_type) {}
	};

	/// exception thrown if a Codec tries to read into an Event of a type different from what it's configured for
	class WrongEventTypeException : public PionException {
	public:
		WrongEventTypeException(void)
			: PionException("Event is not of the expected type") {}
	};

	/// constructs a new Codec object
	Codec(void) {}
	
	/// virtual destructor: this class is meant to be extended
	virtual ~Codec() {}
	
	/// returns an HTTP content type that is used by this Codec
	virtual const std::string& getContentType(void) const = 0;
	
	/**
	 * clones the codec, returning a pointer to the cloned copy
	 *
	 * @return CodecPtr pointer to the cloned copy of the codec
	 */
	virtual std::shared_ptr<Codec> clone(void) const = 0;

	/**
	 * writes an Event to an output stream
	 *
	 * @param out the output stream to which the Event will be written
	 * @param e the Event to write to the output stream
	 */
	virtual void write(std::ostream& out, const Event& e) = 0;

	/**
	 * writes any needed footers (e.g. closing tag, JSON array end token, etc.) to an output stream
	 * and does any related cleanup
	 *
	 * @param out the output stream to which the footers will be written
	 */
	virtual void finish(std::ostream& out) = 0;

	/**
	 * reads an Event from an input stream
	 *
	 * @param in the input stream to read the Event from
	 * @param e the Event read, if any; null if error
	 * @return true if successful, false otherwise
	 */
	virtual bool read(std::istream& in, Event& e) = 0;

	/**
	 * sets configuration parameters for this Codec
	 *
	 * @param v the Vocabulary that this Codec will use to describe Terms
	 * @param config_ptr pointer to a list of XML nodes containing Codec
	 *                   configuration parameters
	 */
	virtual void setConfig(const Vocabulary& v, const xmlNodePtr config_ptr);
	
	/**
	 * this updates the Vocabulary information used by this Codec;
	 * it should be called whenever the global Vocabulary is updated
	 *
	 * @param v the Vocabulary that this Codec will use to describe Terms
	 */
	virtual void updateVocabulary(const Vocabulary& v);

	/**
	 * reads an Event from an input stream
	 *
	 * @param in the input stream to read the Event from
	 * @param f the factory to use to create new Events
	 *
	 * @return EventPtr& pointer to the event read, if any; null if error
	 */
	inline EventPtr read(std::istream& in, EventFactory& f) {
		EventPtr event_ptr(f.create(getEventType()));
		if (! read(in, *event_ptr))
			event_ptr.reset();
		return event_ptr;
	}
	
	/// returns the type of Event that is used by this Codec
	inline Event::EventType getEventType(void) const { return m_event_term.term_ref; }

	
protected:
	
	/// protected copy function (use clone() instead)
	inline void copyCodec(const Codec& c) {
		copyPlugin(c);
		m_event_term = c.m_event_term;
	}

	
private:
	
	/// name of the event type element for Pion XML config files
	static const std::string		EVENT_ELEMENT_NAME;

	
	/// the type of Events used by this Codec (TermRef maps to Terms of type OBJECT)
	Vocabulary::Term				m_event_term;
};


/// data type used for Codec smart pointers
typedef std::shared_ptr<Codec>	CodecPtr;
	
	
//
// The following symbols must be defined for any Codecs that you would
// like to be able to load dynamically using the CodecFactory::loadCodec()
// function.  These are not required for any Codecs that you only want to link
// directly into your programs.
//
// Make sure that you replace "CODEC" with the name of your derived class.
// This name must also match the name of the object file (excluding the
// extension).  These symbols must be linked into your Codec's object file,
// not included in any headers that it may use (declarations are OK in headers
// but not the definitions).
//
// The "pion_create" function is used to create new instances of your Codec.
// The "pion_destroy" function is used to destroy instances of your Codec.
//
// extern "C" PION_PLUGIN_API Codec *pion_create_CODEC(void) {
//		return new CODEC;
// }
//
// extern "C" PION_PLUGIN_API void pion_destroy_CODEC(CODEC *codec_ptr) {
//		delete codec_ptr;
// }
//

	
}	// end namespace platform
}	// end namespace pion

#endif

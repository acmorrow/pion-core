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

#ifndef __PION_PROTOCOL_HEADER__
#define __PION_PROTOCOL_HEADER__

#include <istream>
#include <memory>
#include <ostream>
#include <libxml/tree.h>
#include <boost/logic/tribool.hpp>
#include <boost/system/error_code.hpp>
#include <pion/PionConfig.hpp>
#include <pion/PionException.hpp>
#include <pion/platform/Event.hpp>
#include <pion/platform/Vocabulary.hpp>
#include <pion/platform/PlatformPlugin.hpp>


namespace pion {		// begin namespace pion
namespace platform {	// begin namespace platform (Pion Platform Library)


///
/// Protocol: used to encode and decode Events using a particular format
///
class PION_PLATFORM_API Protocol
	: public PlatformPlugin
{
public:

	/// exception thrown if the Protocol configuration does not define an event type
	class EmptyEventException : public PionException {
	public:
		EmptyEventException(const std::string& protocol_id)
			: PionException("Protocol configuration does not define an event type: ", protocol_id) {}
	};

	/// exception thrown if the Protocol configuration references an unknown event type
	class UnknownTermException : public PionException {
	public:
		UnknownTermException(const std::string& term)
			: PionException("Protocol configuration references an unknown term: ", term) {}
	};

	/// exception thrown if the Protocol configuration uses an event type for a Term that is not an object
	class NotAnObjectException : public PionException {
	public:
		NotAnObjectException(const std::string& event_type)
			: PionException("Protocol configuration defines a non-object event type: ", event_type) {}
	};

	/// constructs a new Protocol object
	Protocol(void) {}
	
	/// virtual destructor: this class is meant to be extended
	virtual ~Protocol() {}
		
	/// resets the Protocol to its initial state
	virtual void reset(void) = 0;
	
	/**
	 * clones the Protocol, returning a pointer to the cloned copy
	 *
	 * @return ProtocolPtr pointer to the cloned copy of the codec
	 */
	virtual std::shared_ptr<Protocol> clone(void) const = 0;

	/**
	 * called to close the protocol parsing.  An event may be returned
	 * if there is data remaining (i.e. if closed prematurely)
	 *
	 * @param event_ptr_ref refererence to an event object returned if the call resulted in event generation
	 * @param client_reset will be true if the client reset the connection
	 * @param server_reset will be true if the server reset the connection
	 *
	 * @return true if the request or response parsing was finished prematurely
	 */
	virtual bool close(pion::platform::EventPtr& event_ptr_ref,
		bool client_reset, bool server_reset) = 0;

	/**
	 * parses the next portion of the network data
	 * 
	 * @param request direction flag (true if request, false if response)
	 * @param ptr pointer to data (may be NULL if data packet was missing)
	 * @param len length in bytes of the network data
	 * @param data_timestamp data frame timestamp
	 * @param ack_timestamp timestamp for acknowlegement of receipt of data frame
	 * @param events one or more events generated from network data
	 * @param ec error_code contains additional information for parsing errors
	 *
	 * @return true if the current data chunk completes a new event, indeterminate if the event parsing is not 
	 *		   yet complete, false if an error encountered during the parsing
	 */
	virtual boost::tribool readNext(bool request, const char* ptr, size_t len, 
			boost::posix_time::ptime data_timestamp, boost::posix_time::ptime ack_timestamp,
			pion::platform::EventContainer& events, boost::system::error_code& ec) = 0;

	/**
	 * called when parsing previously failed.  should return true if the current packet
	 * allows parsing to recover by starting over at the current point in the data stream.
	 * 
	 * @param request direction flag (true if request, false if response)
	 * @param ptr pointer to data (may be NULL if data packet was missing)
	 * @param len length in bytes of the network data
	 *
	 * @return true if the current packet allows parsing to recover
	 */
	virtual bool checkRecoveryPacket(bool request, const char* ptr, size_t len);

	/**
	 * sets configuration parameters for this Protocol
	 *
	 * @param v the Vocabulary that this Protocol will use to describe Terms
	 * @param config_ptr pointer to a list of XML nodes containing Protocol
	 *                   configuration parameters
	 */
	virtual void setConfig(const Vocabulary& v, const xmlNodePtr config_ptr);
	
	/**
	 * this updates the Vocabulary information used by this Protocol;
	 * it should be called whenever the global Vocabulary is updated
	 *
	 * @param v the Vocabulary that this Protocol will use to describe Terms
	 */
	virtual void updateVocabulary(const Vocabulary& v);
	
	/// returns the type of Event that is used by this Protocol
	inline Event::EventType getEventType(void) const { return m_event_term.term_ref; }

	
protected:
	
	/// protected copy function (use clone() instead)
	inline void copyProtocol(const Protocol& c) {
		copyPlugin(c);
		m_event_term = c.m_event_term;
	}

	/// used to efficiently generate new Events
	EventFactory					m_event_factory;


private:
	
	/// name of the event type element for Pion XML config files
	static const std::string		EVENT_ELEMENT_NAME;

	
	/// the type of Events used by this Protocol (TermRef maps to Terms of type OBJECT)
	Vocabulary::Term				m_event_term;
};


/// data type used for Protocol smart pointers
typedef std::shared_ptr<Protocol>	ProtocolPtr;

	
}	// end namespace platform
}	// end namespace pion

#endif

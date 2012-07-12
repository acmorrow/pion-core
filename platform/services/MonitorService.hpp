// ------------------------------------------------------------------------
// Pion is a development platform for building Reactors that process Events
// ------------------------------------------------------------------------
// Copyright (C) 2010 Atomic Labs, Inc.  (http://www.atomiclabs.com)
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

#ifndef __PION_MONITORSERVICE_HEADER__
#define __PION_MONITORSERVICE_HEADER__

#include <memory>
#include <string>
#include <iosfwd>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/circular_buffer.hpp>
#include <mutex>
#include <boost/unordered_map.hpp>
#include <pion/net/HTTPTypes.hpp>
#include <pion/PionConfig.hpp>
#include <pion/PionLogger.hpp>
#include <pion/net/TCPStream.hpp>
#include <pion/platform/Codec.hpp>
#include <pion/platform/Reactor.hpp>
#include <pion/platform/ReactionEngine.hpp>
#include "PlatformService.hpp"


namespace pion {		// begin namespace pion
namespace plugins {		// begin namespace plugins


///
/// MonitorWriter: helper class, capturing packets, storing them into an array, streaming into XML
///
class MonitorWriter
	: public std::enable_shared_from_this<MonitorWriter>
{
	typedef std::set<pion::platform::Vocabulary::TermRef>	TermRefSet;

	typedef	boost::circular_buffer<pion::platform::EventPtr> EventBuffer;

private:
	/// reference to ReactionEngine used to send Events to Reactors
	pion::platform::ReactionEngine &	m_reaction_engine;

	/// primary logging interface used by this class
	PionLogger							m_logger;	

	/// unique identifier for this particular connection (generated when constructed)
	const std::string					m_connection_id;

	/// unique identifier for the Reactor that this handler interacts with
	const std::string					m_reactor_id;

	/// mutex used to protect the MonitorHandler's data
	mutable std::mutex				m_mutex;

	/// Circular buffer to capture scrolling window of events
	EventBuffer							m_event_buffer;

	/// Size of scroll buffer
	unsigned							m_size;

	/// Scroll or Capture/Stop
	bool								m_scroll;

	/// copy of the universal Term Vocabular
	platform::VocabularyPtr				m_vocab_ptr;

	/// At what length to truncate strings
	unsigned							m_truncate;

	/// Is the MonitorWriter still collecting events?
	volatile bool						m_stopped;

	/// HideAll is an opt-in mode
	bool								m_hide_all;

	/// Which terms to show in opt-in mode
	TermRefSet							m_show_terms;

	/// Suppressed termref's
	TermRefSet							m_suppressed_terms;

	/// All terms seen (ever)
	TermRefSet							m_terms_seen;

	/// All event types seen (ever)
	TermRefSet							m_events_seen;

	/// Events that are filtered
	TermRefSet							m_filtered_events;

	/// Counters for visual changes (to help UI)
	boost::uint32_t						m_event_counter;
	boost::uint32_t						m_change_counter;

	/// Age (last call by a browser)
	boost::posix_time::ptime			m_age;

public:

	typedef boost::unordered_map<pion::platform::Vocabulary::TermRef, unsigned> TermCol;
	
	/// destructor
	~MonitorWriter()
	{
		stop();
	}

	/**
	 * constructs a new MonitorWriter object
	 *
	 * @param reaction_engine reference to reaction engine
	 * @param vptr pointer to vocabulary
	 * @param reactor_id unique identifier for the Reactor that this handler interacts with
	 * @param size size of circular buffer to use for capture
	 * @param scroll boolean, whether to use scroll or capture&stop
	 * @param logger PionLogger to use for logging messages
	 */
	MonitorWriter(pion::platform::ReactionEngine &reaction_engine, platform::VocabularyPtr& vptr,
			   const std::string& reactor_id, unsigned size, bool scroll, PionLogger logger)
		: m_reaction_engine(reaction_engine), m_logger(logger), m_connection_id(PionId().to_string()), m_reactor_id(reactor_id),
		m_event_buffer(size), m_size(size), m_scroll(scroll), m_vocab_ptr(vptr), m_truncate(100), m_stopped(false),
		m_hide_all(false), m_event_counter(0), m_change_counter(0)
	{ }
	
	/**
	 * sends an Event over the TCP connection and cleans up if the
	 * connection has closed since we last sent data
	 *
	 * @param e the Event to possibly (filter) add to the circular buffer
	 */
	void writeEvent(pion::platform::EventPtr e);

	/// "set method" for MonitorWriters parameters from query parameters
	void setQP(const pion::net::HTTPTypes::QueryParams& qp);
	
	/// starts the MonitorWriter
	void start(const pion::net::HTTPTypes::QueryParams& qp);

	/**
	 * stop MonitorWriter from collecting more data
	 *
	 * @param Stop (default true) -- stop from collecting; don't use if reactor died
	 * @param Flush (default false) -- flush events; if aged out
	 */
	void stop(bool Stop = true, bool Flush = false) {
		std::lock_guard<std::mutex> writer_lock(m_mutex);
		if (!m_stopped) {
			PION_LOG_INFO(m_logger, "Stopping output feed to " << getConnectionId());
			if (Stop) {
				PION_LOG_DEBUG(m_logger, "Removing connection to " << getConnectionId());
				m_reaction_engine.removeTempConnection(getConnectionId());
			}
			m_stopped = true;
		}
		if (Flush) {
			PION_LOG_DEBUG(m_logger, "Clearing events from " << getConnectionId());
			m_event_buffer.clear();
			m_age = boost::date_time::not_a_date_time;
		}
	}

	/**
	 * serializes event Terms to an XML output stream
	 * (for use by Event::for_each)
	 *
	 * @param tref ParameterNode TermRef
	 * @param value ParameterNode ParameterValue
	 * @param xml output stream
	 */
	void SerializeXML(pion::platform::Vocabulary::TermRef tref,
		const pion::platform::Event::ParameterValue& value,
		std::ostream& xml, TermCol& cols);

	/// returns the unique identifier for this particular connection
	inline const std::string& getConnectionId(void) const { return m_connection_id; }

	/// returns the unique identifier for the Reactor that this handler interacts with
	inline const std::string& getReactorId(void) const { return m_reactor_id; }

	/// parse all (possible,optional) query parameters and set flags appropriately
	std::string getStatus(const pion::net::HTTPTypes::QueryParams& qp);

	/// getAge/setAge
	boost::posix_time::ptime getAge(void) const { return m_age; }
	void setAge(void) { m_age = boost::posix_time::second_clock::local_time(); }
};

/// data type used for MonitorWriter smart pointers
typedef std::shared_ptr<MonitorWriter>	MonitorWriterPtr;



///
/// MonitorService: Platform WebService used to send and receive Event streams
///
class MonitorService
	: public pion::server::PlatformService
{
	/// primary logging interface used by this class
	PionLogger							m_logger;	

	/// A vector of currently active MonitorWriters
	std::vector<MonitorWriterPtr>		m_writers;

	/// mutex used to protect the MonitorService's data
	mutable std::mutex				m_mutex;

public:
	
	/// constructs a new MonitorService object
	MonitorService(void)
		: PlatformService("pion.MonitorService"),
		m_logger(PION_GET_LOGGER("pion.MonitorService")),
		m_writers(WRITERS) 								// a default of ten simultaneous monitors allowed
	{ }
	
	/// virtual destructor -- stop all the running captures
	virtual ~MonitorService()
	{
		PION_LOG_INFO(m_logger, "shutdown - clearing all writers");
		std::lock_guard<std::mutex> service_lock(m_mutex);
		m_writers.clear();
	}
	
	/**
	 * attempts to handle a new HTTP request
	 *
	 * @param request the new HTTP request to handle
	 * @param tcp_conn the TCP connection that has the new request
	 */
	virtual void operator()(pion::net::HTTPRequestPtr& request,
							pion::net::TCPConnectionPtr& tcp_conn);

	/// returns the type attribute used for an XML Permission node pertaining to MonitorService
	std::string getPermissionType(void) const { return MONITOR_SERVICE_PERMISSION_TYPE; }

private:

	static const std::string			MONITOR_SERVICE_PERMISSION_TYPE;
	static const unsigned				WRITERS;
};

	
}	// end namespace plugins
}	// end namespace pion

#endif

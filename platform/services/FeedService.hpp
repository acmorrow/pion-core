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

#ifndef __PION_FEEDSERVICE_HEADER__
#define __PION_FEEDSERVICE_HEADER__

#include <string>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
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
/// FeedHandler: base helper class for sending and receiving Events
///
class FeedHandler
{
public:

	/// virtual destructor
	virtual ~FeedHandler() { m_tcp_conn->finish(); }
	
	/**
	 * constructs a new FeedHandler object
	 *
	 * @param reaction_engine reference to the ReactionEngine
	 * @param reactor_id unique identifier for the Reactor that this handler interacts with
	 * @param codec_ptr pointer to the Codec that this handler will use
	 * @param tcp_conn TCP connection that will be used to send/receive Events
	 */
	FeedHandler(pion::platform::ReactionEngine &reaction_engine,
				const std::string& reactor_id, pion::platform::CodecPtr& codec_ptr,
				pion::net::TCPConnectionPtr& tcp_conn);
	
	/// starts the FeedHandler
	virtual void start(void) = 0;
	
	/**
	 * sends an HTTP response over the TCP connection
	 *
	 * @return true if the response was sent successfully, or false if not
	 */
	bool sendResponse(void);
	
	/**
	 * generates a string describing information for a TCP connection
	 *
	 * @param tcp_conn TCP connection that will be used to send/receive Events
	 * @return std::string contains information describing this connection
	 */
	static std::string createConnectionInfo(pion::net::TCPConnectionPtr& tcp_conn);
	
	/// returns the unique identifier for this particular connection
	inline const std::string& getConnectionId(void) const { return m_connection_id; }
	
	/// returns information describing this particular connection
	inline const std::string& getConnectionInfo(void) const { return m_connection_info; }
	
	/// returns the unique identifier for the Reactor that this handler interacts with
	inline const std::string& getReactorId(void) const { return m_reactor_id; }

	/// returns the unique identifier for the Codec that this handler uses
	inline const std::string& getCodecId(void) const { return m_codec_ptr->getId(); }

	
protected:
	
	/// reference to ReactionEngine used to send Events to Reactors
	pion::platform::ReactionEngine &	m_reaction_engine;
	
	/// primary logging interface used by this class
	PionLogger							m_logger;	

	/// unique identifier for this particular connection (generated when constructed)
	const std::string					m_connection_id;
	
	/// information describing this particular connection (generated when constructed)
	const std::string					m_connection_info;
	
	/// unique identifier for the Reactor that this handler interacts with
	const std::string					m_reactor_id;
	
	/// pointer to the Codec used to write outgoing Event streams
	pion::platform::CodecPtr			m_codec_ptr;
	
	/// TCP connection for sending outgoing Event streams
	pion::net::TCPConnectionPtr			m_tcp_conn;
	
	/// Wrapper on top of the TCP connection for synchronous stream operations
	pion::net::TCPStream				m_tcp_stream;
	
	/// mutex used to protect the FeedHandler's data
	mutable boost::mutex				m_mutex;	
};			

	
///
/// FeedWriter: helper class used to send Events over a TCP connection
///
class FeedWriter
	: public FeedHandler,
	public boost::enable_shared_from_this<FeedWriter>
{
public:
	
	/// virtual destructor
	virtual ~FeedWriter();

	/**
	 * constructs a new FeedWriter object
	 *
	 * @param reactor_id unique identifier for the Reactor that this handler interacts with
	 * @param codec_ptr pointer to the Codec that this handler will use
	 * @param tcp_conn TCP connection that will be used to send/receive Events
	 */
	FeedWriter(pion::platform::ReactionEngine &reaction_engine,
			   const std::string& reactor_id, pion::platform::CodecPtr& codec_ptr,
			   pion::net::TCPConnectionPtr& tcp_conn);
	
	/**
	 * sends an Event over the TCP connection and cleans up if the
	 * connection has closed since we last sent data
	 *
	 * @param e the Event to send over the TCPConnection
	 */
	void writeEvent(pion::platform::EventPtr e);
	
	/// starts the FeedWriter
	virtual void start(void);
};

/// data type used for FeedWriter smart pointers
typedef boost::shared_ptr<FeedWriter>	FeedWriterPtr;

	
///
/// FeedReader: helper class used to read Events from a TCP connection
///
class FeedReader
	: public FeedHandler,
	public boost::enable_shared_from_this<FeedReader>
{
public:
	
	/// virtual destructor
	virtual ~FeedReader();
	
	/**
	 * constructs a new FeedReader object
	 *
	 * @param reaction_engine reference to the ReactionEngine
	 * @param reactor_id unique identifier for the Reactor that this handler interacts with
	 * @param codec_ptr pointer to the Codec that this handler will use
	 * @param tcp_conn TCP connection that will be used to send/receive Events
	 */
	FeedReader(pion::platform::ReactionEngine &reaction_engine,
			   const std::string& reactor_id, pion::platform::CodecPtr& codec_ptr,
			   pion::net::TCPConnectionPtr& tcp_conn);
	
	/// called when the Reactor we are sending Events to is being removed
	void reactorWasRemoved(void);
	
	/// starts the FeedReader
	virtual void start(void);

	
private:	

	/// pointer to the Reactor that Events are sent to
	pion::platform::Reactor *		m_reactor_ptr;
};

/// data type used for FeedReader smart pointers
typedef boost::shared_ptr<FeedReader>	FeedReaderPtr;

	
///
/// FeedService: Platform WebService used to send and receive Event streams
///
class FeedService
	: public pion::server::PlatformService
{
public:
	
	/// constructs a new FeedService object
	FeedService(void) : PlatformService("pion.FeedService") {}
	
	/// virtual destructor
	virtual ~FeedService() {}
	
	/**
	 * attempts to handle a new HTTP request
	 *
	 * @param request the new HTTP request to handle
	 * @param tcp_conn the TCP connection that has the new request
	 */
	virtual void operator()(pion::net::HTTPRequestPtr& request,
							pion::net::TCPConnectionPtr& tcp_conn);
};

	
}	// end namespace plugins
}	// end namespace pion

#endif

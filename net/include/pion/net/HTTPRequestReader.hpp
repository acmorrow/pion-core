// ------------------------------------------------------------------
// pion-net: a C++ framework for building lightweight HTTP interfaces
// ------------------------------------------------------------------
// Copyright (C) 2007-2008 Atomic Labs, Inc.  (http://www.atomiclabs.com)
//
// Distributed under the Boost Software License, Version 1.0.
// See http://www.boost.org/LICENSE_1_0.txt
//

#ifndef __PION_HTTPREQUESTREADER_HEADER__
#define __PION_HTTPREQUESTREADER_HEADER__

#include <functional>
#include <memory>
#include <boost/asio.hpp>
#include <pion/PionConfig.hpp>
#include <pion/net/HTTPRequest.hpp>
#include <pion/net/HTTPReader.hpp>


namespace pion {	// begin namespace pion
namespace net {		// begin namespace net (Pion Network Library)


///
/// HTTPRequestReader: asynchronously reads and parses HTTP requests
///
class HTTPRequestReader :
	public HTTPReader,
	public std::enable_shared_from_this<HTTPRequestReader>
{

public:

	/// function called after the HTTP message has been parsed
	typedef std::function<void(HTTPRequestPtr&, TCPConnectionPtr&,
		const boost::system::error_code&)>	FinishedHandler;

	
	// default destructor
	virtual ~HTTPRequestReader() {}
	
	/**
	 * creates new HTTPRequestReader objects
	 *
	 * @param tcp_conn TCP connection containing a new message to parse
	 * @param handler function called after the message has been parsed
	 */
	static inline std::shared_ptr<HTTPRequestReader>
		create(TCPConnectionPtr& tcp_conn, FinishedHandler handler)
	{
		return std::shared_ptr<HTTPRequestReader>
			(new HTTPRequestReader(tcp_conn, handler));
	}

	
protected:

	/**
	 * protected constructor restricts creation of objects (use create())
	 *
	 * @param tcp_conn TCP connection containing a new message to parse
	 * @param handler function called after the message has been parsed
	 */
	HTTPRequestReader(TCPConnectionPtr& tcp_conn, FinishedHandler handler)
		: HTTPReader(true, tcp_conn), m_http_msg(new HTTPRequest),
		m_finished(handler)
	{
		m_http_msg->setRemoteIp(tcp_conn->getRemoteIp());
		setLogger(PION_GET_LOGGER("pion.net.HTTPRequestReader"));
	}
		
	/// Reads more bytes from the TCP connection
	virtual void readBytes(void) {
		void(HTTPRequestReader::* pconsumeBytes)(const boost::system::error_code&, std::size_t) = &HTTPRequestReader::consumeBytes;
		getTCPConnection()->async_read_some(std::bind(pconsumeBytes,
														shared_from_this(),
														std::placeholders::_1,
														std::placeholders::_2));
	}

	/// Called after we have finished reading/parsing the HTTP message
	virtual void finishedReading(const boost::system::error_code& ec) {
		// call the finished handler with the finished HTTP message
		if (m_finished) m_finished(m_http_msg, getTCPConnection(), ec);
	}
	
	/// Returns a reference to the HTTP message being parsed
	virtual HTTPMessage& getMessage(void) { return *m_http_msg; }

	/// The new HTTP message container being created
	HTTPRequestPtr				m_http_msg;

	/// function called after the HTTP message has been parsed
	FinishedHandler				m_finished;
};


/// data type for a HTTPRequestReader pointer
typedef std::shared_ptr<HTTPRequestReader>	HTTPRequestReaderPtr;


}	// end namespace net
}	// end namespace pion

#endif
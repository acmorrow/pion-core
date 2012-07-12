// ------------------------------------------------------------------
// pion-net: a C++ framework for building lightweight HTTP interfaces
// ------------------------------------------------------------------
// Copyright (C) 2007-2008 Atomic Labs, Inc.  (http://www.atomiclabs.com)
//
// Distributed under the Boost Software License, Version 1.0.
// See http://www.boost.org/LICENSE_1_0.txt
//

#ifndef __PION_HTTPRESPONSEREADER_HEADER__
#define __PION_HTTPRESPONSEREADER_HEADER__

#include <functional>
#include <memory>
#include <boost/asio.hpp>
#include <pion/PionConfig.hpp>
#include <pion/net/HTTPResponse.hpp>
#include <pion/net/HTTPReader.hpp>


namespace pion {	// begin namespace pion
namespace net {		// begin namespace net (Pion Network Library)


///
/// HTTPResponseReader: asynchronously reads and parses HTTP responses
///
class HTTPResponseReader :
	public HTTPReader,
	public std::enable_shared_from_this<HTTPResponseReader>
{

public:

	/// function called after the HTTP message has been parsed
	typedef std::function<void(HTTPResponsePtr&, TCPConnectionPtr&,
		const boost::system::error_code&)>	FinishedHandler;

	
	// default destructor
	virtual ~HTTPResponseReader() {}
	
	/**
	 * creates new HTTPResponseReader objects
	 *
	 * @param tcp_conn TCP connection containing a new message to parse
     * @param http_request the request we are responding to
	 * @param handler function called after the message has been parsed
	 */
	static inline std::shared_ptr<HTTPResponseReader>
		create(TCPConnectionPtr& tcp_conn, const HTTPRequest& http_request,
			   FinishedHandler handler)
	{
		return std::shared_ptr<HTTPResponseReader>
			(new HTTPResponseReader(tcp_conn, http_request, handler));
	}

	
protected:

	/**
	 * protected constructor restricts creation of objects (use create())
	 *
	 * @param tcp_conn TCP connection containing a new message to parse
     * @param http_request the request we are responding to
	 * @param handler function called after the message has been parsed
	 */
	HTTPResponseReader(TCPConnectionPtr& tcp_conn, const HTTPRequest& http_request,
					   FinishedHandler handler)
		: HTTPReader(false, tcp_conn), m_http_msg(new HTTPResponse(http_request)),
		m_finished(handler)
	{
		m_http_msg->setRemoteIp(tcp_conn->getRemoteIp());
		setLogger(PION_GET_LOGGER("pion.net.HTTPResponseReader"));
	}
		
	/// Reads more bytes from the TCP connection
	virtual void readBytes(void) {
		void(HTTPResponseReader::* pconsumeBytes)(const boost::system::error_code&, std::size_t) = &HTTPResponseReader::consumeBytes;
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
	HTTPResponsePtr				m_http_msg;

	/// function called after the HTTP message has been parsed
	FinishedHandler				m_finished;
};


/// data type for a HTTPResponseReader pointer
typedef std::shared_ptr<HTTPResponseReader>	HTTPResponseReaderPtr;


}	// end namespace net
}	// end namespace pion

#endif

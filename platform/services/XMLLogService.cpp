// ------------------------------------------------------------------
// pion-net: a C++ framework for building lightweight HTTP interfaces
// ------------------------------------------------------------------
// Copyright (C) 2010-2011 Atomic Labs, Inc.  (http://www.atomiclabs.com)
//
// Distributed under the Boost Software License, Version 1.0.
// See http://www.boost.org/LICENSE_1_0.txt
//

#include "XMLLogService.hpp"

// This only works with LOG4CPLUS
#include <log4cplus/spi/loggingevent.h>
#include <boost/lexical_cast.hpp>

#include <pion/net/HTTPResponseWriter.hpp>
#include <pion/PionLogger.hpp>

using namespace pion;
using namespace pion::net;
using namespace pion::platform;

namespace pion {		// begin namespace pion
namespace plugins {		// begin namespace plugins


// static members of XMLLogServiceAppender

const unsigned int		XMLLogServiceAppender::DEFAULT_MAX_EVENTS = 100;
const unsigned int		XMLLogServiceAppender::DEFAULT_TRUNCATION_LENGTH = 100;
boost::uint32_t			XMLLogServiceAppender::m_event_count = 0;


// XMLLogServiceAppender member functions

XMLLogServiceAppender::XMLLogServiceAppender(void)
	: m_max_events(DEFAULT_MAX_EVENTS), m_num_events(0), m_truncate(DEFAULT_TRUNCATION_LENGTH)
{
	// Append any log events that have already occurred.
	log4cplus::SharedAppenderPtr cba_ptr = log4cplus::Logger::getRoot().getAppender("CircularBufferAppender");
	const CircularBufferAppender& cba = dynamic_cast<const CircularBufferAppender&>(*cba_ptr.get());
	const CircularBufferAppender::LogEventBuffer& events = cba.getLogIterator();
	for (CircularBufferAppender::LogEventBuffer::const_iterator i = events.begin(); i != events.end(); ++i) {
		append(*i);
	}
}


void XMLLogServiceAppender::append(const log4cplus::spi::InternalLoggingEvent& event)
{
	std::ostringstream key;
	key << event.getLogLevel() << '.' << std::setfill('0') << std::setw(10) << ++m_event_count;
	std::string val;
	val = "<LogLevel>" + ConfigManager::xml_encode(m_log_level_manager.toString(event.getLogLevel()))
		+ "</LogLevel><Timestamp>" + ConfigManager::xml_encode(boost::lexical_cast<std::string>(event.getTimestamp().sec()))
		+ "</Timestamp><LoggerName>" + ConfigManager::xml_encode(event.getLoggerName())
		+ "</LoggerName><Message>" + ConfigManager::xml_encode(event.getMessage().substr(0, m_truncate))
		+ "</Message>";
	std::lock_guard<std::mutex> log_lock(m_log_mutex);
	m_log_event_queue[key.str()] = val;						// Add the new entry, sort as appropriate
	if (m_log_event_queue.size() > m_max_events)			// Queue size exceeded?
		m_log_event_queue.erase(m_log_event_queue.begin());	// Whack the least important
}

void XMLLogServiceAppender::writeLogEvents(pion::net::HTTPResponseWriterPtr& writer)
{
	std::ostringstream xml;
	xml << "<Events>";
	{
		std::lock_guard<std::mutex> log_lock(m_log_mutex);	// Protect the iterator for insertion
		for (LOG_QUEUE::reverse_iterator ir = m_log_event_queue.rbegin(); ir != m_log_event_queue.rend(); ++ir)
			xml << "<Event id=\"" << ir->first << "\">" << ir->second << "</Event>";
	}
	xml << "</Events>";
	writer << xml.str();
}

void XMLLogServiceAppender::acknowledgeEvent(std::string id)
{
	LOG_QUEUE::iterator it = m_log_event_queue.find(id);
	if (it != m_log_event_queue.end())
		m_log_event_queue.erase(it);
}

void XMLLogServiceAppender::acknowledgeAllEvents(void)
{
	m_log_event_queue.clear();
}


// static members of XMLLogService

const std::string	XMLLogService::XML_LOG_SERVICE_PERMISSION_TYPE = "XMLLogService";

// XMLLogService member functions

XMLLogService::XMLLogService(void)
	: PlatformService("pion.XMLLogService"), m_log_appender_ptr(new XMLLogServiceAppender())
{
	m_log_appender_ptr->setName("XMLLogServiceAppender");
	log4cplus::Logger::getRoot().addAppender(m_log_appender_ptr);
}

XMLLogService::~XMLLogService()
{
	// removeAppender() also deletes the object
	log4cplus::Logger::getRoot().removeAppender("XMLLogServiceAppender");
}

/// handles requests for XMLLogService
void XMLLogService::operator()(HTTPRequestPtr& request, TCPConnectionPtr& tcp_conn)
{
	bool allowed = getConfig().getUserManagerPtr()->accessAllowed(request->getUser(), *this);
	if (! allowed) {
		// Log an error and send a 403 (Forbidden) response.
		handleForbiddenRequest(request, tcp_conn, "User doesn't have permission for XMLLogService.");
		return;
	}

	// Set Content-type to "text/xml"
	HTTPResponseWriterPtr writer(HTTPResponseWriter::create(tcp_conn, *request,
															std::bind(&TCPConnection::finish, tcp_conn)));
	writer->getResponse().setContentType(HTTPTypes::CONTENT_TYPE_XML);

	const HTTPTypes::QueryParams qp = request->getQueryParams();
	HTTPTypes::QueryParams::const_iterator qpi;
	if (request->getMethod() == HTTPTypes::REQUEST_METHOD_GET) {
		qpi = qp.find("truncate");
		if (qpi != qp.end())
			getLogAppender().setTruncationLength(boost::lexical_cast<boost::uint32_t>(qpi->second));

		getLogAppender().writeLogEvents(writer);
		writer->send();
	} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_DELETE) {
		if (qp.empty()) {
			// The required query parameter "ack" was not found, so send a 400 (Bad Request) response.
			std::string error_msg = "Query parameter \"ack\" is required for method DELETE on " + request->getResource() + ".";
			handleBadRequest(request, tcp_conn, error_msg);
			return;
		}
		qpi = qp.find("ack");
		if (qpi != qp.end()) {
			if (qpi->second == "*") {
				// Erase all events in the queue.
				getLogAppender().acknowledgeAllEvents();
			} else {
				// Erase the event with the specified id from the queue (if found).
				getLogAppender().acknowledgeEvent(qpi->second);
			}
		} else {
			// The required query parameter "ack" was not found, so send a 400 (Bad Request) response.
			std::string error_msg = "Query parameter \"ack\" is required for method DELETE on " + request->getResource() + ".";
			handleBadRequest(request, tcp_conn, error_msg);
			return;
		}

		// send a 204 (No Content) response
		writer->getResponse().setStatusCode(HTTPTypes::RESPONSE_CODE_NO_CONTENT);
		writer->getResponse().setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_NO_CONTENT);
		writer->send();
	} else {
		// Log an error and send a 405 (Method Not Allowed) response.
		handleMethodNotAllowed(request, tcp_conn, "GET, DELETE");
	}
}


}	// end namespace plugins
}	// end namespace pion


/// creates new XMLLogService objects
extern "C" PION_SERVICE_API pion::plugins::XMLLogService *pion_create_XMLLogService(void)
{
	return new pion::plugins::XMLLogService();
}

/// destroys XMLLogService objects
extern "C" PION_SERVICE_API void pion_destroy_XMLLogService(pion::plugins::XMLLogService *service_ptr)
{
	delete service_ptr;
}

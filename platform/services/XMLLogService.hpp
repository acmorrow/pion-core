// ------------------------------------------------------------------
// pion-net: a C++ framework for building lightweight HTTP interfaces
// ------------------------------------------------------------------
// Copyright (C) 2010-2011 Atomic Labs, Inc.  (http://www.atomiclabs.com)
//
// Distributed under the Boost Software License, Version 1.0.
// See http://www.boost.org/LICENSE_1_0.txt
//

#ifndef __PION_XMLLOGSERVICE_HEADER__
#define __PION_XMLLOGSERVICE_HEADER__

#include <deque>
#include <list>
#include <memory>
#include <queue>
#include <string>
#include <mutex>
#include <boost/tuple/tuple.hpp>
#include <pion/PionLogger.hpp>
#include <pion/net/WebService.hpp>
#include <pion/net/HTTPResponseWriter.hpp>
#include "PlatformConfig.hpp"
#include "PlatformService.hpp"

namespace pion {		// begin namespace pion
namespace plugins {		// begin namespace plugins

	
///
/// XMLLogServiceAppender: caches log events in memory for use by XMLLogService
/// 
class XMLLogServiceAppender
	: public PionLogAppender
{
public:
	// default constructor and destructor
	XMLLogServiceAppender(void);
	virtual ~XMLLogServiceAppender() {}
	
	/// sets the maximum number of log events cached in memory
	inline void setMaxEvents(unsigned int n) { m_max_events = n; }

	/// sets the maximum length of Message content
	inline void setTruncationLength(unsigned int n) { m_truncate = n; }

	/// writes the events cached in memory to a response stream
	void writeLogEvents(pion::net::HTTPResponseWriterPtr& writer);

	/// deletes the event with the specified ID from the queue
	void acknowledgeEvent(std::string id);

	/// deletes all events from the queue
	void acknowledgeAllEvents(void);


private:
	/// default maximum number of events cached in memory
	static const unsigned int				DEFAULT_MAX_EVENTS;

	/// default maximum length of Message content
	static const unsigned int				DEFAULT_TRUNCATION_LENGTH;

	/// Count of events logged.
	static std::uint32_t					m_event_count;

	/// maximum number of events cached in memory
	unsigned int							m_max_events;
	
	/// number of events currently cached in memory
	unsigned int							m_num_events;

	/// memory cache of pre-formatted log events
	std::list<std::string>					m_log_events;

	/// maximum length of Message content
	unsigned								m_truncate;

	/// map queue of log events
	typedef std::map<std::string, std::string>	LOG_QUEUE;
	LOG_QUEUE								m_log_event_queue;

	/// mutex to make class thread-safe
	std::mutex							m_log_mutex;

public:
	// member functions inherited from the Appender interface class
	virtual void close() {}
protected:
	virtual void append(const log4cplus::spi::InternalLoggingEvent& event);

private:
	/// this is used to convert numeric log levels into strings
	log4cplus::LogLevelManager		m_log_level_manager;
};


///
/// XMLLogService: web service that displays log messages
/// 
class XMLLogService :
	public pion::server::PlatformService
{
public:
	// default constructor and destructor
	XMLLogService(void);
	virtual ~XMLLogService();
	
	/// handles a new HTTP request
	virtual void operator()(pion::net::HTTPRequestPtr& request,
							pion::net::TCPConnectionPtr& tcp_conn);

	/// returns the log appender used by XMLLogService
	inline XMLLogServiceAppender& getLogAppender(void) {
		return dynamic_cast<XMLLogServiceAppender&>(*m_log_appender_ptr);
	}

	/// returns the type attribute used for an XML Permission node pertaining to XMLLogService
	std::string getPermissionType(void) const { return XML_LOG_SERVICE_PERMISSION_TYPE; }

private:
	/// pointer to log appender
	PionLogAppenderPtr	m_log_appender_ptr;

	/// type identifier for XMLLogService permission type
	static const std::string			XML_LOG_SERVICE_PERMISSION_TYPE;
};

	
}	// end namespace plugins
}	// end namespace pion

#endif

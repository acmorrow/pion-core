// ------------------------------------------------------------------------
// Pion is a development platform for building Reactors that process Events
// ------------------------------------------------------------------------
// Copyright (C) 2007-2011 Atomic Labs, Inc.  (http://www.atomiclabs.com)
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

#ifdef _MSC_VER
// This could be any valid .lib file; its only purpose is to prevent the compiler  
// from trying to link to boost_zlib-*.lib (e.g. boost_zip-vc80-mt-1_37.dll).  
// HTTPProtocol only uses zlib indirectly, through boost_iostreams-*.dll.
#define BOOST_ZLIB_BINARY "zdll.lib"
#endif


#include <iostream>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <boost/regex/icu.hpp>
#include <unicode/utypes.h>
#include <unicode/ucnv.h>
#include <pion/platform/ConfigManager.hpp>
#include <pion/platform/ProtocolFactory.hpp>
#include "HTTPProtocol.hpp"

using namespace pion::net;
using namespace pion::platform;

namespace pion {	// begin namespace pion
namespace plugins {		// begin namespace plugins


// static members of HTTPProtocol

const boost::regex HTTPProtocol::EXTRACT_CHARSET_RX(";\\s*charset=([^;]+)");
const boost::regex HTTPProtocol::META_CHARSET1_RX("<meta charset=([^\\s/>]*)", boost::regex::icase);
const boost::regex HTTPProtocol::META_CHARSET2_RX("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=([^\";]+)", boost::regex::icase);
const boost::regex HTTPProtocol::UTF8_RX("utf-8", boost::regex::icase);

const std::string HTTPProtocol::MAX_REQUEST_CONTENT_LENGTH_ELEMENT_NAME = "MaxRequestContentLength";
const std::string HTTPProtocol::MAX_RESPONSE_CONTENT_LENGTH_ELEMENT_NAME = "MaxResponseContentLength";
const std::string HTTPProtocol::RAW_REQUEST_HEADERS_ELEMENT_NAME = "RawRequestHeaders";
const std::string HTTPProtocol::RAW_RESPONSE_HEADERS_ELEMENT_NAME = "RawResponseHeaders";
const std::string HTTPProtocol::ALLOW_UTF8_CONVERSION_ELEMENT_NAME = "AllowUtf8Conversion";
const std::string HTTPProtocol::ALLOW_SEARCHING_CONTENT_FOR_CHARSET_ELEMENT_NAME = "AllowSearchingContentForCharset";
const std::string HTTPProtocol::CONTENT_TYPE_ELEMENT_NAME = "ContentType";
const std::string HTTPProtocol::MAX_SIZE_ELEMENT_NAME = "MaxSize";
const std::string HTTPProtocol::MAX_EXTRACTS_ELEMENT_NAME = "MaxExtracts";
const std::string HTTPProtocol::EXTRACT_ELEMENT_NAME = "Extract";
const std::string HTTPProtocol::SOURCE_ELEMENT_NAME = "Source";
const std::string HTTPProtocol::MATCH_ELEMENT_NAME = "Match";
const std::string HTTPProtocol::FORMAT_ELEMENT_NAME = "Format";
const std::string HTTPProtocol::NAME_ELEMENT_NAME = "Name";
const std::string HTTPProtocol::TERM_ATTRIBUTE_NAME = "term";

const std::string HTTPProtocol::EXTRACT_QUERY_STRING = "query";
const std::string HTTPProtocol::EXTRACT_COOKIE_STRING = "cookie";
const std::string HTTPProtocol::EXTRACT_CS_COOKIE_STRING = "cs-cookie";
const std::string HTTPProtocol::EXTRACT_SC_COOKIE_STRING = "sc-cookie";
const std::string HTTPProtocol::EXTRACT_CS_HEADER_STRING = "cs-header";
const std::string HTTPProtocol::EXTRACT_SC_HEADER_STRING = "sc-header";
const std::string HTTPProtocol::EXTRACT_CS_CONTENT_STRING = "cs-content";
const std::string HTTPProtocol::EXTRACT_SC_CONTENT_STRING = "sc-content";
const std::string HTTPProtocol::EXTRACT_CS_RAW_CONTENT_STRING = "cs-raw-content";
const std::string HTTPProtocol::EXTRACT_SC_RAW_CONTENT_STRING = "sc-raw-content";
	
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_CS_DATA_PACKETS="urn:vocab:clickstream#cs-data-packets";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_SC_DATA_PACKETS="urn:vocab:clickstream#sc-data-packets";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_CS_MISSING_PACKETS="urn:vocab:clickstream#cs-missing-packets";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_SC_MISSING_PACKETS="urn:vocab:clickstream#sc-missing-packets";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_CS_HEADERS="urn:vocab:clickstream#cs-headers";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_SC_HEADERS="urn:vocab:clickstream#sc-headers";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_CS_BYTES="urn:vocab:clickstream#cs-bytes";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_SC_BYTES="urn:vocab:clickstream#sc-bytes";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_BYTES="urn:vocab:clickstream#bytes";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_STATUS="urn:vocab:clickstream#status";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_COMMENT="urn:vocab:clickstream#comment";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_METHOD="urn:vocab:clickstream#method";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_URI="urn:vocab:clickstream#uri";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_URI_STEM="urn:vocab:clickstream#uri-stem";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_URI_QUERY="urn:vocab:clickstream#uri-query";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_REQUEST="urn:vocab:clickstream#request";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_CACHED="urn:vocab:clickstream#cached";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_DATE="urn:vocab:clickstream#date";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_TIME="urn:vocab:clickstream#time";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_DATE_TIME="urn:vocab:clickstream#date-time";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_EPOCH_TIME="urn:vocab:clickstream#epoch-time";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_CLF_DATE="urn:vocab:clickstream#clf-date";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_REQUEST_START_TIME="urn:vocab:clickstream#request-start-time";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_REQUEST_END_TIME="urn:vocab:clickstream#request-end-time";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_RESPONSE_START_TIME="urn:vocab:clickstream#response-start-time";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_RESPONSE_END_TIME="urn:vocab:clickstream#response-end-time";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_CS_SEND_TIME="urn:vocab:clickstream#cs-send-time";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_CS_ACK_TIME="urn:vocab:clickstream#cs-ack-time";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_SC_REPLY_TIME="urn:vocab:clickstream#sc-reply-time";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_SC_SEND_TIME="urn:vocab:clickstream#sc-send-time";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_SC_ACK_TIME="urn:vocab:clickstream#sc-ack-time";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_DATA_CENTER_TIME="urn:vocab:clickstream#data-center-time";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_AUTHUSER="urn:vocab:clickstream#authuser";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_C_IP="urn:vocab:clickstream#c-ip";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_REQUEST_STATUS="urn:vocab:clickstream#request-status";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_RESPONSE_STATUS="urn:vocab:clickstream#response-status";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_REFUSED="urn:vocab:clickstream#refused";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_CANCELED="urn:vocab:clickstream#canceled";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_CS_VERSION="urn:vocab:clickstream#cs-version";


// HTTPProtocol member functions

void HTTPProtocol::reset(void)
{
	m_request_parser.reset();
	m_response_parser.reset();
	m_request.clear();
	m_response.clear();
	m_response.setStatusCode(0U);
	m_response.setStatusMessage("");
	m_request_start_time = m_request_end_time = m_request_ack_time
		= m_response_start_time = m_response_end_time = m_response_ack_time
		= boost::date_time::not_a_date_time;
	m_cs_data_packets = m_sc_data_packets = m_cs_missing_packets
		= m_sc_missing_packets = 0;
}

bool HTTPProtocol::close(EventPtr& event_ptr_ref, bool client_reset, bool server_reset)
{
	if (! m_request.isValid())
		m_request_parser.finish(m_request);

	if (! m_response.isValid())
		m_response_parser.finish(m_response);

	bool result = (m_request.isValid() && m_response.isValid());
	
	if (m_request_parser.getTotalBytesRead() > 0) {
		generateEvent(event_ptr_ref);
		if (! result) {
			if (client_reset) {
				(*event_ptr_ref).setUInt(m_canceled_term_ref, 1U);
				result = true;
			} else if (server_reset) {
				(*event_ptr_ref).setUInt(m_refused_term_ref, 1U);
				result = true;
			}
		}
	}

	return result;
}

boost::tribool HTTPProtocol::readNext(bool request, const char *ptr, size_t len, 
									  boost::posix_time::ptime data_timestamp,
									  boost::posix_time::ptime ack_timestamp,
									  EventContainer& events,
									  boost::system::error_code& ec)
{
	boost::tribool rc;

	if (ptr == NULL) {
	
		// missing data -> try to recover from lost packet
		if (request) {
			++m_cs_missing_packets;
			rc = m_request_parser.parseMissingData(m_request, len, ec);
		} else {
			++m_sc_missing_packets;
			rc = m_response_parser.parseMissingData(m_response, len, ec);
		}
	
	} else {

		// has valid data available for parsing
		
		if (request) {

			// update data packet counter
			++m_cs_data_packets;
	
			// update timestamps if necessary
			if (m_request_start_time.is_not_a_date_time()) {
				m_request_start_time = m_request_end_time = data_timestamp;
				m_request_ack_time = ack_timestamp;
			} else if (data_timestamp > m_request_end_time) {
				m_request_end_time = data_timestamp;
				m_request_ack_time = ack_timestamp;
			}
	
			// parse the data
			m_request_parser.setReadBuffer(ptr, len);
			rc = m_request_parser.parse(m_request, ec);
	
		} else {
	
			// update data packet counter
			++m_sc_data_packets;

			// update timestamps if necessary
			if (m_response_start_time.is_not_a_date_time()) {
				m_response_start_time = m_response_end_time = data_timestamp;
				m_response_ack_time = ack_timestamp;
			} else if (data_timestamp > m_response_end_time) {
				m_response_end_time = data_timestamp;
				m_response_ack_time = ack_timestamp;
			}
	
			// parse the data
			m_response_parser.setReadBuffer(ptr, len);
			rc = m_response_parser.parse(m_response, ec);
		}
	}

	if (rc == true) {
		// message has been fully parsed, generate an event
		if (request) {
			// update response to "know" about the request (this influences parsing)
			m_response.updateRequestInfo(m_request);
			if (m_request.getVersionMajor() == 0U) {
				// no version specified in request -> assume entire response is payload content
				m_response.setStatusCode(0U);
				m_response.setStatusMessage("");
				m_response_parser.skipHeaderParsing(m_response);
			}
			// wait until the response is parsed before generating an event
			rc = boost::indeterminate;
		} else {
			// finished parsing response
			if (m_response.getStatusCode() == HTTPTypes::RESPONSE_CODE_CONTINUE) {
				// if response is 100 (Continue), then just skip it
				m_response_parser.reset();
				m_response.clear();
				m_response_start_time = m_response_end_time = m_response_ack_time
					= boost::date_time::not_a_date_time;
				m_sc_data_packets = m_sc_missing_packets = 0;
				rc = boost::indeterminate;
			} else {
				// response is finished, check request
				if (m_request.isValid()) {
					// generate a new event
					EventPtr event_ptr;
					generateEvent(event_ptr);
					events.push_back(event_ptr);
				} else {
					rc = false;	// request not finished -> return false to try to recover
				}
			}
		}
	}

	return rc;
}

bool HTTPProtocol::checkRecoveryPacket(bool request, const char* ptr, size_t len)
{
	if (request && ptr != NULL && len > 7) {
		// look for valid HTTP request method at the beginning of request packet
		if (memcmp(ptr, "GET ", 4)==0 || memcmp(ptr, "PUT ", 4)==0 || memcmp(ptr, "POST ", 5)==0
			|| memcmp(ptr, "HEAD ", 5)==0 || memcmp(ptr, "TRACE ", 6)==0
			|| memcmp(ptr, "DELETE ", 7)==0 || memcmp(ptr, "CONNECT ", 8)==0
			|| memcmp(ptr, "OPTIONS ", 8)==0   )
		{
			return true;
		}
	}

	return false;
}

std::shared_ptr<Protocol> HTTPProtocol::clone(void) const
{
	HTTPProtocol* retval = new HTTPProtocol;
	retval->copyProtocol(*this);

	retval->m_cs_data_packets_term_ref = m_cs_data_packets_term_ref;
	retval->m_sc_data_packets_term_ref = m_sc_data_packets_term_ref;
	retval->m_cs_missing_packets_term_ref = m_cs_missing_packets_term_ref;
	retval->m_sc_missing_packets_term_ref = m_sc_missing_packets_term_ref;
	retval->m_cs_headers_term_ref = m_cs_headers_term_ref;
	retval->m_sc_headers_term_ref = m_sc_headers_term_ref;
	retval->m_cs_bytes_term_ref = m_cs_bytes_term_ref;
	retval->m_sc_bytes_term_ref = m_sc_bytes_term_ref;
	retval->m_bytes_term_ref = m_bytes_term_ref;
	retval->m_status_term_ref = m_status_term_ref;
	retval->m_comment_term_ref = m_comment_term_ref;
	retval->m_method_term_ref = m_method_term_ref;
	retval->m_uri_term_ref = m_uri_term_ref;
	retval->m_uri_stem_term_ref = m_uri_stem_term_ref;
	retval->m_uri_query_term_ref = m_uri_query_term_ref;
	retval->m_request_term_ref = m_request_term_ref;
	retval->m_cached_term_ref = m_cached_term_ref;
	retval->m_date_term_ref = m_date_term_ref;
	retval->m_time_term_ref = m_time_term_ref;
	retval->m_date_time_term_ref = m_date_time_term_ref;
	retval->m_epoch_time_term_ref = m_epoch_time_term_ref;
	retval->m_clf_date_term_ref = m_clf_date_term_ref;
	retval->m_request_start_time_term_ref = m_request_start_time_term_ref;
	retval->m_request_end_time_term_ref = m_request_end_time_term_ref;
	retval->m_response_start_time_term_ref = m_response_start_time_term_ref;
	retval->m_response_end_time_term_ref = m_response_end_time_term_ref;
	retval->m_cs_send_time_term_ref = m_cs_send_time_term_ref;
	retval->m_cs_ack_time_term_ref = m_cs_ack_time_term_ref;
	retval->m_sc_reply_time_term_ref = m_sc_reply_time_term_ref;
	retval->m_sc_send_time_term_ref = m_sc_send_time_term_ref;
	retval->m_sc_ack_time_term_ref = m_sc_ack_time_term_ref;
	retval->m_data_center_time_term_ref = m_data_center_time_term_ref;
	retval->m_authuser_term_ref = m_authuser_term_ref;
	retval->m_c_ip_term_ref = m_c_ip_term_ref;
	retval->m_request_status_term_ref = m_request_status_term_ref;
	retval->m_response_status_term_ref = m_response_status_term_ref;
	retval->m_refused_term_ref = m_refused_term_ref;
	retval->m_canceled_term_ref = m_canceled_term_ref;
	retval->m_cs_version_term_ref = m_cs_version_term_ref;

	retval->m_request_parser.setMaxContentLength(m_request_parser.getMaxContentLength());
	retval->m_response_parser.setMaxContentLength(m_response_parser.getMaxContentLength());
	retval->m_request_parser.setSaveRawHeaders(m_request_parser.getSaveRawHeaders());
	retval->m_response_parser.setSaveRawHeaders(m_response_parser.getSaveRawHeaders());

	retval->m_extraction_rules = m_extraction_rules;

	retval->m_vocab_ptr = m_vocab_ptr;

	return ProtocolPtr(retval);
}

void HTTPProtocol::generateEvent(EventPtr& event_ptr_ref)
{
	try {
		const Event::EventType event_type(getEventType());

		// create a new event via EventFactory
		m_event_factory.create(event_ptr_ref, event_type);

		// populate some basic fields
		(*event_ptr_ref).setUBigInt(m_cs_bytes_term_ref, m_request_parser.getTotalBytesRead());
		(*event_ptr_ref).setUBigInt(m_sc_bytes_term_ref, m_response_parser.getTotalBytesRead());
		(*event_ptr_ref).setUBigInt(m_bytes_term_ref, m_request_parser.getTotalBytesRead()
								 + m_response_parser.getTotalBytesRead());
		(*event_ptr_ref).setUInt(m_status_term_ref, m_response.getStatusCode());
		(*event_ptr_ref).setString(m_comment_term_ref, m_response.getStatusMessage());
		(*event_ptr_ref).setString(m_method_term_ref, m_request.getMethod());
		if (! m_request_parser.getRawHeaders().empty())
			(*event_ptr_ref).setString(m_cs_headers_term_ref, m_request_parser.getRawHeaders());
		if (! m_response_parser.getRawHeaders().empty())
			(*event_ptr_ref).setString(m_sc_headers_term_ref, m_response_parser.getRawHeaders());
		(*event_ptr_ref).setString(m_cs_version_term_ref, m_request.getVersionString());

		// construct uri string
		std::string uri_str(m_request.getResource());
		if (! m_request.getQueryString().empty()) {
			uri_str += '?';
			uri_str += m_request.getQueryString();
		}
		(*event_ptr_ref).setString(m_uri_term_ref, uri_str);

		// check for Authorization header
		const std::string& authorization_header = m_request.getHeader(HTTPTypes::HEADER_AUTHORIZATION);
		if (!authorization_header.empty()) {
			if (boost::algorithm::starts_with(authorization_header, "Basic ")) {
				// found -> extract Basic authenticated username
				std::string username;
				const std::string base64_encoded = authorization_header.substr(6);
				if (algo::base64_decode(base64_encoded, username)) {
					std::size_t pos = username.find(':');
					if (pos != std::string::npos) {
						username.resize(pos);
						(*event_ptr_ref).setString(m_authuser_term_ref, username);
					}
				}
			} else if (boost::algorithm::starts_with(authorization_header, "Digest ")) {
				// found -> extract Digest authenticated username
				std::size_t start_pos = authorization_header.find("username=\"");
				if (start_pos != std::string::npos) {
					start_pos += 10;	// step past first quote
					std::size_t end_pos = authorization_header.find('\"', start_pos);
					if (end_pos != std::string::npos) {
						std::string username = authorization_header.substr(start_pos, end_pos-start_pos);
						(*event_ptr_ref).setString(m_authuser_term_ref, username);
					}
				}
			}
		}

		// check for X-Forwarded-For header
		const std::string& xff_header = m_request.getHeader(HTTPTypes::HEADER_X_FORWARDED_FOR);
		if (!xff_header.empty()) {
			std::string public_ip;
			if (HTTPParser::parseForwardedFor(xff_header, public_ip)) {
				// set c-ip field if a good match was found
				(*event_ptr_ref).setString(m_c_ip_term_ref, public_ip);
			}
		}

		// populate some more fields...
		(*event_ptr_ref).setString(m_uri_stem_term_ref, m_request.getResource());
		(*event_ptr_ref).setString(m_uri_query_term_ref, m_request.getQueryString());
		(*event_ptr_ref).setString(m_request_term_ref, m_request.getFirstLine());
		(*event_ptr_ref).setUInt(m_cached_term_ref,
								 m_response.getStatusCode() == HTTPTypes::RESPONSE_CODE_NOT_MODIFIED
								 ? 1 : 0);

		// sanity checks for timestamps
		// (may have only request packets or only response packets)
		if (m_request_start_time.is_not_a_date_time())
			m_request_start_time = m_request_end_time = m_request_ack_time = m_response_start_time;
		else if (m_response_start_time.is_not_a_date_time())
			m_response_start_time = m_response_end_time = m_response_ack_time = m_request_end_time;
		if (m_response_ack_time < m_response_end_time)
			m_response_ack_time = m_response_end_time;

		// set timestamp fields
		(*event_ptr_ref).setDateTime(m_date_term_ref, m_request_start_time); 
		(*event_ptr_ref).setDateTime(m_time_term_ref, m_request_start_time); 
		(*event_ptr_ref).setDateTime(m_date_time_term_ref, m_request_start_time); 
		(*event_ptr_ref).setDateTime(m_clf_date_term_ref, m_request_start_time); 
		(*event_ptr_ref).setDateTime(m_request_start_time_term_ref, m_request_start_time); 
		(*event_ptr_ref).setDateTime(m_request_end_time_term_ref, m_request_end_time); 
		(*event_ptr_ref).setDateTime(m_response_start_time_term_ref, m_response_start_time); 
		(*event_ptr_ref).setDateTime(m_response_end_time_term_ref, m_response_end_time); 
		(*event_ptr_ref).setUInt(m_epoch_time_term_ref,
			PionTimeFacet::to_time_t(m_request_start_time) );

		// set time duration fields
		(*event_ptr_ref).setUInt(m_cs_send_time_term_ref,
			( m_request_end_time > m_request_start_time ?
			  (m_request_end_time - m_request_start_time).total_microseconds()
			  : 0 ) );
		(*event_ptr_ref).setUInt(m_cs_ack_time_term_ref,
			( m_request_ack_time > m_request_end_time ?
			  (m_request_ack_time - m_request_end_time).total_microseconds()
			  : 0 ) );
		(*event_ptr_ref).setUInt(m_sc_reply_time_term_ref,
			( m_response_start_time > m_request_end_time ?
			  (m_response_start_time - m_request_end_time).total_microseconds()
			  : 0 ) );
		(*event_ptr_ref).setUInt(m_sc_send_time_term_ref,
			( m_response_end_time > m_response_start_time ?
			  (m_response_end_time - m_response_start_time).total_microseconds()
			  : 0 ) );
		(*event_ptr_ref).setUInt(m_data_center_time_term_ref,
			( m_response_end_time > m_request_end_time ?
			  (m_response_end_time - m_request_end_time).total_microseconds()
			  : 0 ) );

		// save sc_ack_time for next calculation
		const std::uint32_t sc_ack_time = ( m_response_ack_time > m_response_end_time ?
			  (m_response_ack_time - m_response_end_time).total_microseconds() : 0 );
		(*event_ptr_ref).setUInt(m_sc_ack_time_term_ref, sc_ack_time);

		// set packet counter fields
		(*event_ptr_ref).setUInt(m_cs_data_packets_term_ref, m_cs_data_packets);
		(*event_ptr_ref).setUInt(m_sc_data_packets_term_ref, m_sc_data_packets);
		(*event_ptr_ref).setUInt(m_cs_missing_packets_term_ref, m_cs_missing_packets);
		(*event_ptr_ref).setUInt(m_sc_missing_packets_term_ref, m_sc_missing_packets);
		
		// set availability metrics
		(*event_ptr_ref).setUInt(m_request_status_term_ref, m_request.getStatus());
		(*event_ptr_ref).setUInt(m_response_status_term_ref, m_response.getStatus());

		// used to cache "final" payload content, i.e. after decoding (if needed) and conversion to UTF-8 (if needed) 
		size_t final_request_length;
		size_t final_response_length;
		boost::shared_array<char> final_request_content;
		boost::shared_array<char> final_response_content;
		boost::logic::tribool decoded_and_converted_request_flag(boost::indeterminate);
		boost::logic::tribool decoded_and_converted_response_flag(boost::indeterminate);

		// process content extraction rules
		for (ExtractionRuleVector::const_iterator i = m_extraction_rules.begin();
			i != m_extraction_rules.end(); ++i)
		{
			const ExtractionRule& rule = **i;
			if (! rule.m_running)
				continue;
			try {
				switch (rule.m_source) {
					case EXTRACT_QUERY:
						// extract query parameter from request
						rule.process(event_ptr_ref, m_request.getQueryParams().equal_range(rule.m_name), true);
						break;
					case EXTRACT_COOKIE:
						// extract cookie parameter from request
						rule.process(event_ptr_ref, m_request.getCookieParams().equal_range(rule.m_name), false);
						// extract set-cookie parameters from response
						rule.process(event_ptr_ref, m_response.getCookieParams().equal_range(rule.m_name), false);
						break;
					case EXTRACT_CS_COOKIE:
						// extract cookie parameter from request
						rule.process(event_ptr_ref, m_request.getCookieParams().equal_range(rule.m_name), false);
						break;
					case EXTRACT_SC_COOKIE:
						// extract set-cookie parameters from response
						rule.process(event_ptr_ref, m_response.getCookieParams().equal_range(rule.m_name), false);
						break;
					case EXTRACT_CS_HEADER:
						// extract HTTP header from request
						rule.process(event_ptr_ref, m_request.getHeaders().equal_range(rule.m_name), false);
						break;
					case EXTRACT_SC_HEADER:
						// extract HTTP header from response
						rule.process(event_ptr_ref, m_response.getHeaders().equal_range(rule.m_name), false);
						break;
					case EXTRACT_CS_CONTENT:
						// extract decoded and converted HTTP payload content from request
						rule.processContent(event_ptr_ref, m_request, decoded_and_converted_request_flag,
							final_request_content, final_request_length, m_logger);
						break;
					case EXTRACT_SC_CONTENT:
						// extract decoded and converted HTTP payload content from response
						rule.processContent(event_ptr_ref, m_response, decoded_and_converted_response_flag,
							final_response_content, final_response_length, m_logger);
						break;
					case EXTRACT_CS_RAW_CONTENT:
						// extract raw HTTP payload content from request
						rule.processRawContent(event_ptr_ref, m_request);
						break;
					case EXTRACT_SC_RAW_CONTENT:
						// extract raw HTTP payload content from response
						rule.processRawContent(event_ptr_ref, m_response);
						break;
				}
			} catch (RegexFailure& e) {
				PION_LOG_ERROR(m_logger, "uri_str: " << uri_str << " - " << e.what());
				if (! getConfigManager().getDebugMode()) {
					// Prevent this rule from running again.
					(*i)->m_running = false;
					PION_LOG_WARN(m_logger, "Extraction rule has been disabled: regex = " << rule.m_match_str);
				}
			} catch (Event::PionExceptionWithTermRef& e) {
				PION_LOG_ERROR(m_logger, "uri_str: " << uri_str << " - rethrowing");
				throw; // Will be caught at the end of this method, where details of e will be logged.
			} catch (std::exception& e) {
				PION_LOG_ERROR(m_logger, "uri_str: " << uri_str << "; term_id: " << rule.m_term.term_id << " - " << e.what() << " - rethrowing");
				throw;
			}
		}
	} catch (Event::PionExceptionWithTermRef& e) {
		std::string term_id = "unknown";
		if (e.m_term_ref != Vocabulary::UNDEFINED_TERM_REF && m_vocab_ptr && e.m_term_ref <= m_vocab_ptr->size())
			term_id = (*m_vocab_ptr)[e.m_term_ref].term_id;
		PION_LOG_ERROR(m_logger, "term_id: " << term_id << " - " << e.what() << " - rethrowing");
		throw;
	}
}

void HTTPProtocol::setConfig(const Vocabulary& v, const xmlNodePtr config_ptr)
{
	Protocol::setConfig(v, config_ptr);
	
	// parse maximum request content length
	std::uint64_t max_content_length = 0;
	if (ConfigManager::getConfigOption(MAX_REQUEST_CONTENT_LENGTH_ELEMENT_NAME,
		max_content_length, config_ptr))
	{
		m_request_parser.setMaxContentLength(max_content_length);
	} else {
		m_request_parser.resetMaxContentLength();
	}

	// parse maximum response content length
	if (ConfigManager::getConfigOption(MAX_RESPONSE_CONTENT_LENGTH_ELEMENT_NAME,
		max_content_length, config_ptr))
	{
		m_response_parser.setMaxContentLength(max_content_length);
	} else {
		m_response_parser.resetMaxContentLength();
	}
	
	// parse RawRequestHeaders config parameter
	std::string raw_headers_str;
	if (ConfigManager::getConfigOption(RAW_REQUEST_HEADERS_ELEMENT_NAME,
		raw_headers_str, config_ptr))
	{
		m_request_parser.setSaveRawHeaders(raw_headers_str == "true");
	} else {
		m_request_parser.setSaveRawHeaders(false);
	}

	// parse RawResponseHeaders config parameter
	if (ConfigManager::getConfigOption(RAW_RESPONSE_HEADERS_ELEMENT_NAME,
		raw_headers_str, config_ptr))
	{
		m_response_parser.setSaveRawHeaders(raw_headers_str == "true");
	} else {
		m_response_parser.setSaveRawHeaders(false);
	}

	// parse AllowUtf8Conversion config parameter
	m_allow_utf8_conversion = true;
	std::string bool_str;
	if (ConfigManager::getConfigOption(ALLOW_UTF8_CONVERSION_ELEMENT_NAME, bool_str, config_ptr)) {
		m_allow_utf8_conversion = (bool_str == "true");
	}

	// parse AllowSearchingContentForCharset config parameter
	m_allow_searching_content_for_charset = m_allow_utf8_conversion;
	if (ConfigManager::getConfigOption(ALLOW_SEARCHING_CONTENT_FOR_CHARSET_ELEMENT_NAME, bool_str, config_ptr)) {
		m_allow_searching_content_for_charset = (bool_str == "true");
	}

	// parse content extraction rules
	xmlNodePtr extract_node = config_ptr;
	m_extraction_rules.clear();
	while ((extract_node = ConfigManager::findConfigNodeByName(EXTRACT_ELEMENT_NAME, extract_node)) != NULL) {
		// get the Term we want to use
		xmlChar *xml_char_ptr = xmlGetProp(extract_node, reinterpret_cast<const xmlChar*>(TERM_ATTRIBUTE_NAME.c_str()));
		if (xml_char_ptr == NULL || xml_char_ptr[0] == '\0') {
			if (xml_char_ptr != NULL)
				xmlFree(xml_char_ptr);
			throw EmptyTermException(getId());
		}
		const std::string term_id(reinterpret_cast<char*>(xml_char_ptr));
		xmlFree(xml_char_ptr);

		// create a new rule configuration object
		ExtractionRulePtr rule_ptr(new ExtractionRule(term_id, *this));

		// make sure that the Term is valid, copy info to rule
		const Vocabulary::TermRef term_ref = v.findTerm(term_id);
		if (term_ref == Vocabulary::UNDEFINED_TERM_REF)
			throw UnknownTermException(term_id);
		rule_ptr->m_term = v[term_ref];
		
		// only string types are currently supported
		switch (rule_ptr->m_term.term_type) {
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
			throw TermNotStringException(term_id);
			break;
		case Vocabulary::TYPE_SHORT_STRING:
		case Vocabulary::TYPE_STRING:
		case Vocabulary::TYPE_LONG_STRING:
		case Vocabulary::TYPE_CHAR:
		case Vocabulary::TYPE_BLOB:
		case Vocabulary::TYPE_ZBLOB:
			break;	// these are all OK
		}

		// determine the source to use for content extraction
		xmlNodePtr source_node = ConfigManager::findConfigNodeByName(SOURCE_ELEMENT_NAME, extract_node->children);
		if (source_node == NULL)
			throw EmptySourceException(getId());
		xml_char_ptr = xmlNodeGetContent(source_node);
		if (xml_char_ptr == NULL || xml_char_ptr[0] == '\0') {
			if (xml_char_ptr != NULL)
				xmlFree(xml_char_ptr);
			throw EmptySourceException(getId());
		}
		const std::string source_str(reinterpret_cast<char*>(xml_char_ptr));
		xmlFree(xml_char_ptr);
		if (source_str == EXTRACT_QUERY_STRING) {
			rule_ptr->m_source = EXTRACT_QUERY;
		} else if (source_str == EXTRACT_COOKIE_STRING) {
			rule_ptr->m_source = EXTRACT_COOKIE;
		} else if (source_str == EXTRACT_CS_COOKIE_STRING) {
			rule_ptr->m_source = EXTRACT_CS_COOKIE;
		} else if (source_str == EXTRACT_SC_COOKIE_STRING) {
			rule_ptr->m_source = EXTRACT_SC_COOKIE;
		} else if (source_str == EXTRACT_CS_HEADER_STRING) {
			rule_ptr->m_source = EXTRACT_CS_HEADER;
		} else if (source_str == EXTRACT_SC_HEADER_STRING) {
			rule_ptr->m_source = EXTRACT_SC_HEADER;
		} else if (source_str == EXTRACT_CS_CONTENT_STRING) {
			rule_ptr->m_source = EXTRACT_CS_CONTENT;
		} else if (source_str == EXTRACT_SC_CONTENT_STRING) {
			rule_ptr->m_source = EXTRACT_SC_CONTENT;
		} else if (source_str == EXTRACT_CS_RAW_CONTENT_STRING) {
			rule_ptr->m_source = EXTRACT_CS_RAW_CONTENT;
		} else if (source_str == EXTRACT_SC_RAW_CONTENT_STRING) {
			rule_ptr->m_source = EXTRACT_SC_RAW_CONTENT;
		} else {
			throw UnknownSourceException(source_str);
		}
		
		// parse parameter name attribute
		switch (rule_ptr->m_source) {
		case EXTRACT_QUERY:
		case EXTRACT_COOKIE:
		case EXTRACT_CS_COOKIE:
		case EXTRACT_SC_COOKIE:
		case EXTRACT_CS_HEADER:
		case EXTRACT_SC_HEADER:
		{
			if (! ConfigManager::getConfigOption(NAME_ELEMENT_NAME, rule_ptr->m_name,
				extract_node->children))
			{
				throw EmptyNameException(getId());
			}
			break;
		}
		case EXTRACT_CS_CONTENT:
		case EXTRACT_SC_CONTENT:
		case EXTRACT_CS_RAW_CONTENT:
		case EXTRACT_SC_RAW_CONTENT:
			break;	// ignore parameter name attribute
		}
		
		// get Format parameter
		ConfigManager::getConfigOption(FORMAT_ELEMENT_NAME, rule_ptr->m_format,
			extract_node->children);

		// get Match regex
		std::string temp_str;
		if (ConfigManager::getConfigOption(MATCH_ELEMENT_NAME, temp_str,
			extract_node->children))
		{
			try {
				rule_ptr->m_match.assign(temp_str);
				rule_ptr->m_u32_match = boost::make_u32regex(temp_str);
				rule_ptr->m_match_str = temp_str;
			} catch (...) {
				throw BadMatchRegexException(temp_str);
			}
		}
		
		// get ContentType regex
		if (ConfigManager::getConfigOption(CONTENT_TYPE_ELEMENT_NAME, temp_str,
			extract_node->children))
		{
			try {
				rule_ptr->m_type_regex.assign(temp_str);
			} catch (...) {
				throw BadContentRegexException(temp_str);
			}
		}
		
		// get MaxSize parameter
		rule_ptr->m_max_size = std::uint32_t(-1);	// default is undefined/none
		if (ConfigManager::getConfigOption(MAX_SIZE_ELEMENT_NAME, temp_str,
										   extract_node->children))
		{
			try {
				rule_ptr->m_max_size = boost::lexical_cast<std::uint32_t>(temp_str);
			} catch (...) {}	// ignore failed casts (keep default)
		}

		// get MaxExtracts parameter
		rule_ptr->m_max_extracts = 1U;	// default to 1 for backwards-compat.
		if (ConfigManager::getConfigOption(MAX_EXTRACTS_ELEMENT_NAME, temp_str,
										   extract_node->children))
		{
			try {
				rule_ptr->m_max_extracts = boost::lexical_cast<std::uint32_t>(temp_str);
			} catch (...) {}	// ignore failed casts (keep default)
		}

		// add the content extraction rule
		m_extraction_rules.push_back(rule_ptr);

		// step to the next rule
		extract_node = extract_node->next;
	}

	// get copy of vocabulary to use for error reporting
	m_vocab_ptr.reset(new Vocabulary(v));

	// initialize references to known Terms:

	m_cs_data_packets_term_ref = v.findTerm(VOCAB_CLICKSTREAM_CS_DATA_PACKETS);
	if (m_cs_data_packets_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_CS_DATA_PACKETS);

	m_sc_data_packets_term_ref = v.findTerm(VOCAB_CLICKSTREAM_SC_DATA_PACKETS);
	if (m_sc_data_packets_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_SC_DATA_PACKETS);

	m_cs_missing_packets_term_ref = v.findTerm(VOCAB_CLICKSTREAM_CS_MISSING_PACKETS);
	if (m_cs_missing_packets_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_CS_MISSING_PACKETS);

	m_sc_missing_packets_term_ref = v.findTerm(VOCAB_CLICKSTREAM_SC_MISSING_PACKETS);
	if (m_sc_missing_packets_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_SC_MISSING_PACKETS);

	m_cs_headers_term_ref = v.findTerm(VOCAB_CLICKSTREAM_CS_HEADERS);
	if (m_cs_headers_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_CS_HEADERS);

	m_sc_headers_term_ref = v.findTerm(VOCAB_CLICKSTREAM_SC_HEADERS);
	if (m_sc_headers_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_SC_HEADERS);

	m_cs_bytes_term_ref = v.findTerm(VOCAB_CLICKSTREAM_CS_BYTES);
	if (m_cs_bytes_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_CS_BYTES);

	m_sc_bytes_term_ref = v.findTerm(VOCAB_CLICKSTREAM_SC_BYTES);
	if (m_sc_bytes_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_SC_BYTES);

	m_bytes_term_ref = v.findTerm(VOCAB_CLICKSTREAM_BYTES);
	if (m_bytes_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_BYTES);

	m_status_term_ref = v.findTerm(VOCAB_CLICKSTREAM_STATUS);
	if (m_status_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_STATUS);

	m_comment_term_ref = v.findTerm(VOCAB_CLICKSTREAM_COMMENT);
	if (m_comment_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_COMMENT);

	m_method_term_ref = v.findTerm(VOCAB_CLICKSTREAM_METHOD);
	if (m_method_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_METHOD);

	m_uri_term_ref = v.findTerm(VOCAB_CLICKSTREAM_URI);
	if (m_uri_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_URI);

	m_uri_stem_term_ref = v.findTerm(VOCAB_CLICKSTREAM_URI_STEM);
	if (m_uri_stem_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_URI_STEM);

	m_uri_query_term_ref = v.findTerm(VOCAB_CLICKSTREAM_URI_QUERY);
	if (m_uri_query_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_URI_QUERY);

	m_request_term_ref = v.findTerm(VOCAB_CLICKSTREAM_REQUEST);
	if (m_request_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_REQUEST);

	m_cached_term_ref = v.findTerm(VOCAB_CLICKSTREAM_CACHED);
	if (m_cached_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_CACHED);

	m_date_term_ref = v.findTerm(VOCAB_CLICKSTREAM_DATE);
	if (m_date_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_DATE);

	m_time_term_ref = v.findTerm(VOCAB_CLICKSTREAM_TIME);
	if (m_time_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_TIME);

	m_date_time_term_ref = v.findTerm(VOCAB_CLICKSTREAM_DATE_TIME);
	if (m_date_time_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_DATE_TIME);

	m_epoch_time_term_ref = v.findTerm(VOCAB_CLICKSTREAM_EPOCH_TIME);
	if (m_epoch_time_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_EPOCH_TIME);

	m_clf_date_term_ref = v.findTerm(VOCAB_CLICKSTREAM_CLF_DATE);
	if (m_clf_date_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_CLF_DATE);

	m_request_start_time_term_ref = v.findTerm(VOCAB_CLICKSTREAM_REQUEST_START_TIME);
	if (m_request_start_time_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_REQUEST_START_TIME);

	m_request_end_time_term_ref = v.findTerm(VOCAB_CLICKSTREAM_REQUEST_END_TIME);
	if (m_request_end_time_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_REQUEST_END_TIME);

	m_response_start_time_term_ref = v.findTerm(VOCAB_CLICKSTREAM_RESPONSE_START_TIME);
	if (m_response_start_time_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_RESPONSE_START_TIME);

	m_response_end_time_term_ref = v.findTerm(VOCAB_CLICKSTREAM_RESPONSE_END_TIME);
	if (m_response_end_time_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_RESPONSE_END_TIME);

	m_cs_send_time_term_ref = v.findTerm(VOCAB_CLICKSTREAM_CS_SEND_TIME);
	if (m_cs_send_time_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_CS_SEND_TIME);

	m_cs_ack_time_term_ref = v.findTerm(VOCAB_CLICKSTREAM_CS_ACK_TIME);
	if (m_cs_ack_time_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_CS_ACK_TIME);

	m_sc_reply_time_term_ref = v.findTerm(VOCAB_CLICKSTREAM_SC_REPLY_TIME);
	if (m_sc_reply_time_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_SC_REPLY_TIME);

	m_sc_send_time_term_ref = v.findTerm(VOCAB_CLICKSTREAM_SC_SEND_TIME);
	if (m_sc_send_time_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_SC_SEND_TIME);

	m_sc_ack_time_term_ref = v.findTerm(VOCAB_CLICKSTREAM_SC_ACK_TIME);
	if (m_sc_ack_time_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_SC_ACK_TIME);

	m_data_center_time_term_ref = v.findTerm(VOCAB_CLICKSTREAM_DATA_CENTER_TIME);
	if (m_data_center_time_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_DATA_CENTER_TIME);

	m_authuser_term_ref = v.findTerm(VOCAB_CLICKSTREAM_AUTHUSER);
	if (m_authuser_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_AUTHUSER);

	m_c_ip_term_ref = v.findTerm(VOCAB_CLICKSTREAM_C_IP);
	if (m_c_ip_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_C_IP);

	m_request_status_term_ref = v.findTerm(VOCAB_CLICKSTREAM_REQUEST_STATUS);
	if (m_request_status_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_REQUEST_STATUS);

	m_response_status_term_ref = v.findTerm(VOCAB_CLICKSTREAM_RESPONSE_STATUS);
	if (m_response_status_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_RESPONSE_STATUS);

	m_refused_term_ref = v.findTerm(VOCAB_CLICKSTREAM_REFUSED);
	if (m_refused_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_REFUSED);

	m_canceled_term_ref = v.findTerm(VOCAB_CLICKSTREAM_CANCELED);
	if (m_canceled_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_CANCELED);

	m_cs_version_term_ref = v.findTerm(VOCAB_CLICKSTREAM_CS_VERSION);
	if (m_cs_version_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_CS_VERSION);
}

void HTTPProtocol::handleRegexFailure(const std::string& regex, const std::string& str) const
{
	std::string error_msg("regex = ");
	error_msg += regex;
	if (getConfigManager().getDebugMode()) {
		// only include source string in debug mode since this data
		// has not yet been processed to remove credit card numbers
		error_msg += ", str = ";
		error_msg += str;
	}
	throw RegexFailure(error_msg);
}


// class HTTPProtocol::ExtractionRule

bool HTTPProtocol::ExtractionRule::writeToSink(FilteringStreambuf& decoder,
	DecoderSink& decoder_sink) const
{
	// NOTE: iostreams::copy() does not work as of 1.37.0 b/c it creates a copy
	// of decoder_sink instead of using a reference to decoder_sink (UGH!)
	//size_t len = boost::iostreams::copy(decoder, decoder_sink);
	char buf[4096];
	std::streamsize num_read;
	try {
		while ((num_read=decoder.sgetn(buf, 4096)) == 4096)
			decoder_sink.write(buf, 4096);
		if (num_read > 0)
			decoder_sink.write(buf, num_read);
	} catch (std::exception& e) {
		// NOTE: content decoding errors throw exceptions!
		// these should not cause the event to be lost!
		return false;
	}
	return decoder_sink.getBytes() > 0;
}

bool HTTPProtocol::ExtractionRule::tryDecoding(const pion::net::HTTPMessage& http_msg, 
	boost::shared_array<char>& decoded_content, size_t& decoded_content_length,
	std::string& content_encoding, PionLogger& logger) const
{
	// check if content is encoded
	content_encoding = boost::algorithm::to_lower_copy(http_msg.getHeader(pion::net::HTTPTypes::HEADER_CONTENT_ENCODING));
	if (content_encoding.empty()) {
		return false;
	} else if (content_encoding == "identity") {
		content_encoding.clear();
		return false;
	}

	if (http_msg.getContentLength() <= 0) {
		PION_LOG_ERROR(logger, "http_msg.getContentLength() = " << http_msg.getContentLength() << " in tryDecoding()");
		return false;
	}

	// attempt to decode content
	boost::iostreams::filtering_streambuf<boost::iostreams::input> decoder;
	DecoderSink decoder_sink;
	
	if (content_encoding == "gzip" || content_encoding == "x-gzip") {

		// payload has gzip (LZ77) encoding
		decoder.push(boost::iostreams::gzip_decompressor());
		decoder.push( boost::iostreams::basic_array_source<char>(http_msg.getContent(),
			http_msg.getContentLength()) );
		if (!writeToSink(decoder, decoder_sink)) {
			PION_LOG_WARN(logger, "Content decoding failed after " << decoder_sink.getBytes() << " bytes for " << content_encoding << " content (" << (http_msg.isChunked() ? "with" : "without") << " chunking)");
		}

	} else if (content_encoding == "deflate" || content_encoding == "x-zlib") {

		// payload has deflate ("zlib") encoding
		boost::iostreams::zlib_params deflate_param;
		// need to set "noheader" because the filter defaults to using a header
		deflate_param.noheader = true;
		decoder.push(boost::iostreams::zlib_decompressor(deflate_param));
		decoder.push( boost::iostreams::basic_array_source<char>(http_msg.getContent(),
			http_msg.getContentLength()) );
		if (!writeToSink(decoder, decoder_sink)) {
			// if deflate fails with no header, try it again with header enabled
			decoder.reset();
			decoder_sink.clear();
			deflate_param.noheader = false;
			decoder.push(boost::iostreams::zlib_decompressor(deflate_param));
			decoder.push( boost::iostreams::basic_array_source<char>(http_msg.getContent(),
				http_msg.getContentLength()) );
			if (!writeToSink(decoder, decoder_sink)) {
				PION_LOG_WARN(logger, "Content decoding failed after " << decoder_sink.getBytes() << " bytes for " << content_encoding << " content (" << (http_msg.isChunked() ? "with" : "without") << " chunking)");
			}
		}

	} else if (content_encoding == "compress" || content_encoding == "x-compress") {

		// payload has compress (LZW) encoding
		// CAN'T HANDLE
		PION_LOG_WARN(logger, "Unsupported Content-Encoding: " << content_encoding);
		return false;
	
	} else {

		// unrecognized content encoding
		// CAN'T HANDLE
		PION_LOG_WARN(logger, "Unrecognized Content-Encoding: " << content_encoding);
		return false;
	}

	decoded_content_length = decoder_sink.getBytes();

	if (decoded_content_length == 0) {
		// Since http_msg.getContentLength() > 0, this constitutes a decoding failure.
		return false;
	} else {
		// initialize decoded content cache to hold results
		try {
			decoded_content.reset(new char[decoded_content_length+1]);	// add 1 in case length == 0 + null termination
		} catch (std::bad_alloc& e) {
			PION_LOG_ERROR(logger, "decoded_content_length: " << decoded_content_length << " - " << e.what() << " - rethrowing");
			throw BadAllocException("decoded_content_length = " + boost::lexical_cast<std::string>(decoded_content_length));
		}
		decoded_content.get()[decoded_content_length] = '\0';		// null terminate buffer since it may be re-used

		// copy results into decoded content cache (a contiguous array)
		char *decoded_ptr = decoded_content.get();
		const DecoderContainer& container = decoder_sink.getContainer();
		for (DecoderContainer::const_iterator it = container.begin();
			it != container.end(); ++it)
		{
			if (it->second > 0) {
				memcpy(decoded_ptr, it->first.get(), it->second);
				decoded_ptr += it->second;
			}
		}

		// content was decoded
		return true;
	}
}

bool HTTPProtocol::ExtractionRule::tryConvertingToUtf8(
	const std::string& charset,
	const char* source,
	size_t source_length,
	boost::shared_array<char>& final_content,
	size_t& final_content_length,
	PionLogger& logger) const
{
	if (source == NULL)
		throw NullSourcePointerException();

	UErrorCode u_error_code = U_ZERO_ERROR;
	UConverter* conv = ucnv_open(charset.c_str(), &u_error_code);
	if (U_FAILURE(u_error_code)) {
		PION_LOG_ERROR(logger, "Unable to find converter for charset: " << charset);
		return false;
	} 

	// Here we set the target length to 0 so we can find out how many bytes we need to allocate.
	int32_t target_capacity = ucnv_toAlgorithmic(UCNV_UTF8, conv, NULL, 0, source, source_length, &u_error_code);

	// U_BUFFER_OVERFLOW_ERROR is expected since targetCapacity = 0
	if (u_error_code != U_BUFFER_OVERFLOW_ERROR) {
		ucnv_close(conv);
		if (u_error_code == U_INVALID_CHAR_FOUND)
			return false;
		else
			throw UnexpectedICUErrorCodeException("ucnv_toAlgorithmic", u_errorName(u_error_code));
	}

	// Now allocate the memory and do the conversion for real.
	final_content_length = target_capacity + 1;
	try {
		final_content.reset(new char[target_capacity + 1]);	// add 1 in case length == 0 + null termination
	} catch (std::bad_alloc& e) {
		ucnv_close(conv);
		PION_LOG_ERROR(logger, "target_capacity: " << target_capacity << " - " << e.what() << " - rethrowing");
		throw BadAllocException("target_capacity = " + boost::lexical_cast<std::string>(target_capacity));
	}
	final_content.get()[target_capacity] = '\0';		// null terminate buffer since it may be re-used
	u_error_code = U_ZERO_ERROR;
	ucnv_toAlgorithmic(UCNV_UTF8, conv, final_content.get(), final_content_length, source, source_length, &u_error_code);

	ucnv_close(conv);
	if (u_error_code != U_ZERO_ERROR)
		throw UnexpectedICUErrorCodeException("ucnv_toAlgorithmic", u_errorName(u_error_code));
	return true;
}


}	// end namespace plugins
}	// end namespace pion



/// creates new HTTPProtocol objects
extern "C" PION_PLUGIN_API pion::platform::Protocol *pion_create_HTTPProtocol(void) {
	return new pion::plugins::HTTPProtocol();
}

/// destroys HTTPProtocol objects
extern "C" PION_PLUGIN_API void pion_destroy_HTTPProtocol(pion::plugins::HTTPProtocol *protocol_ptr) {
	delete protocol_ptr;
}

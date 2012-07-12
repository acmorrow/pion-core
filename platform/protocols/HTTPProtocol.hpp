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

#ifndef __HTTP_PROTOCOL_HEADER__
#define __HTTP_PROTOCOL_HEADER__

#include <memory>
#include <string>
#include <vector>
#include <boost/shared_array.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/regex.hpp>
#include <boost/regex/icu.hpp>
#include <unicode/ucnv.h>
#include <pion/PionException.hpp>
#include <pion/PionAlgorithms.hpp>
#include <pion/platform/Protocol.hpp>
#include <pion/net/HTTPParser.hpp>
#include <pion/net/HTTPRequest.hpp>
#include <pion/net/HTTPResponse.hpp>

namespace pion {	// begin namespace pion
namespace plugins {		// begin namespace plugins


class HTTPProtocol
	: public pion::platform::Protocol
{
public:

	/// exception thrown if there is no source defined for a content extraction rule
	class EmptySourceException : public PionException {
	public:
		EmptySourceException(const std::string& plugin_id)
			: PionException("HTTPProtocol is missing source for content extraction rule: ", plugin_id) {}
	};

	/// exception thrown if the content extraction configuration uses an unknown source
	class UnknownSourceException : public PionException {
	public:
		UnknownSourceException(const std::string& source_str)
			: PionException("HTTPProtocol content extraction specifies an unknown source: ", source_str) {}
	};

	/// exception thrown if the content extraction configuration does not define a term
	class EmptyTermException : public PionException {
	public:
		EmptyTermException(const std::string& plugin_id)
			: PionException("HTTPProtocol content extraction rule is missing a term identifier: ", plugin_id) {}
	};

	/// exception thrown if the content extraction configuration uses an unknown term
	class UnknownTermException : public PionException {
	public:
		UnknownTermException(const std::string& term_id)
			: PionException("HTTPProtocol content extraction maps field to an unknown term: ", term_id) {}
	};

	/// exception thrown if the content extraction configuration does not define a name
	class EmptyNameException : public PionException {
	public:
		EmptyNameException(const std::string& plugin_id)
			: PionException("HTTPProtocol content extraction rule is missing a parameter name: ", plugin_id) {}
	};

	/// exception thrown if the content extraction configuration has a bad Match parameter
	class BadMatchRegexException : public PionException {
	public:
		BadMatchRegexException(const std::string& regex_str)
			: PionException("HTTPProtocol content extraction rule contains bad Match parameter: ", regex_str) {}
	};

	/// exception thrown if the content extraction configuration has a bad ContentType parameter
	class BadContentRegexException : public PionException {
	public:
		BadContentRegexException(const std::string& regex_str)
			: PionException("HTTPProtocol content extraction rule contains bad ContentType parameter: ", regex_str) {}
	};

	/// exception thrown if the term specified for content extraction is not string type
	class TermNotStringException : public PionException {
	public:
		TermNotStringException(const std::string& term_id)
			: PionException("HTTPProtocol content extraction non-string term specified: ", term_id) {}
	};

	/// exception thrown if regex_search fails and throws
	class RegexFailure : public PionException {
	public:
		RegexFailure(const std::string& error_msg)
			: PionException("Regex search failed: ", error_msg) {}
	};

	/// exception thrown if an ICU function returns an unexpected error code
	class UnexpectedICUErrorCodeException : public PionException {
	public:
		UnexpectedICUErrorCodeException(const std::string& icu_func, const std::string& u_error_code_str)
			: PionException("Unexpected ICU error code in HTTPProtocol: ", icu_func + " returned " + u_error_code_str) {}
	};

	/// exception thrown when tryConvertingToUtf8() is called with source == NULL
	class NullSourcePointerException : public PionException {
	public:
		NullSourcePointerException(void)
			: PionException("In HTTPProtocol, a NULL pointer was passed to convert to UTF-8") {}
	};

	/// exception thrown when an std::bad_alloc exception is caught in HTTPProtocol::ExtractionRule::tryConvertingToUtf8()
	class BadAllocException : public PionException {
	public:
		BadAllocException(const std::string& error_msg)
			: PionException("std::bad_alloc in HTTPProtocol: ", error_msg) {}
	};


	/// constructs HTTPProtocol object
	HTTPProtocol() : m_logger(PION_GET_LOGGER("pion.HTTPProtocol")),
		m_request_parser(true), m_response_parser(false), 
		m_request_start_time(boost::date_time::not_a_date_time),
		m_request_end_time(boost::date_time::not_a_date_time),
		m_request_ack_time(boost::date_time::not_a_date_time),
		m_response_start_time(boost::date_time::not_a_date_time),
		m_response_end_time(boost::date_time::not_a_date_time),
		m_response_ack_time(boost::date_time::not_a_date_time),
		m_cs_data_packets(0), m_sc_data_packets(0),
		m_cs_missing_packets(0), m_sc_missing_packets(0)
	{
		m_response.setStatusCode(0U);
		m_response.setStatusMessage("");
	}

	/// virtual destructor
	virtual ~HTTPProtocol() {}

	/// resets the Protocol to its initial state
	virtual void reset(void);

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
		bool client_reset, bool server_reset);

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
			pion::platform::EventContainer& events, boost::system::error_code& ec);

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
	 * clones the Protocol, returning a pointer to the cloned copy
	 *
	 * @return ProtocolPtr pointer to the cloned copy of the codec
	 */
	virtual std::shared_ptr<Protocol> clone(void) const;

	/**
	 * sets configuration parameters for this Protocol
	 *
	 * @param v the Vocabulary that this Protocol will use to describe Terms
	 * @param config_ptr pointer to a list of XML nodes containing Protocol
	 *                   configuration parameters
	 */
	virtual void setConfig(const pion::platform::Vocabulary& v, const xmlNodePtr config_ptr);


private:

	/// container type for decoded HTTP payload content
	typedef std::vector< std::pair<boost::shared_array<char>,size_t> >	DecoderContainer;

	/// data type for a filtering stream buffer
	typedef boost::iostreams::filtering_streambuf<boost::iostreams::input>	FilteringStreambuf;

	/// Boost.iostreams sink used to receive decoded HTTP payload content
	class DecoderSink
		: public boost::iostreams::sink
	{
	public:

		/// default constructor
		DecoderSink(void) : m_bytes(0) {}

		/// stores data in the sink
		std::streamsize write(const char* s, std::streamsize n) {
			if (n > 0) {
				boost::shared_array<char> ptr(new char[n]);
				memcpy(ptr.get(), s, n);
				m_data.push_back( std::make_pair(ptr, n) );
				m_bytes += n;
			}
			return n;
		}

		/// returns the container populated with decoded HTTP payload content
		inline const DecoderContainer& getContainer(void) const { return m_data; }

		/// returns total number of bytes in decoded HTTP payload content
		inline std::streamsize getBytes(void) const { return m_bytes; }
		
		/// resets the sink to it's original empty state
		inline void clear(void) { m_data.clear(); m_bytes = 0; }

	private:

		/// container that is populated with decoded HTTP payload content
		DecoderContainer	m_data;

		/// total number of bytes in decoded HTTP payload content
		std::streamsize		m_bytes;
	};

	/// data source supported for field content extraction
	enum ExtractionSource {
		EXTRACT_QUERY,			//< use map of query pairs from request URI & content (requires name=)
		EXTRACT_COOKIE,			//< use map of cookies from request+response (requires name=)
		EXTRACT_CS_COOKIE,		//< use map of cookies from request (requires name=)
		EXTRACT_SC_COOKIE,		//< use map of cookies from response (requires name=)
		EXTRACT_CS_HEADER,		//< use HTTP request header (requires name=)
		EXTRACT_SC_HEADER,		//< use HTTP response header (requires name=)
		EXTRACT_CS_CONTENT,		//< use HTTP request payload content (decoded)
		EXTRACT_SC_CONTENT,		//< use HTTP response payload content (decoded)
		EXTRACT_CS_RAW_CONTENT,	//< use HTTP request payload content (raw)
		EXTRACT_SC_RAW_CONTENT	//< use HTTP response payload content (raw)
	};

	/// data type used to determine whether or not payload content should be saved
	struct ExtractionRule {

		/**
		 * constructs a new content extraction rule
		 *
		 * @param term_id unique identifier for the event term
		 */
		ExtractionRule(const std::string& term_id, const HTTPProtocol& parent_protocol) :
			m_parent_protocol(parent_protocol), m_term(term_id), m_running(true)
		{}

		/**
		 * processes content extraction for given content
		 *
		 * @param event_ptr_ref pointer to the Event being generated
		 * @param range pair of map iterators representing content to extract
		 * @param url_decode if true, the value will first be url_decoded
		 */
		template <typename RangePair>
		inline void process(pion::platform::EventPtr& event_ptr_ref,
			RangePair range, bool url_decode) const;

		/**
		 * extract the Term value from a buffer which contains the final result of 
		 * applying any needed processing to the HTTPMessage payload content
		 *
		 * @param event_ptr_ref pointer to the Event being generated
		 * @param content_ptr pointer to a blob of payload content data
		 * @param content_length length of the payload content data blob
		 * @param final_content_is_utf8 whether the content is UTF-8 encoded
		 */
		inline void setTermValueFromFinalContent(
			pion::platform::EventPtr& event_ptr_ref,
			const char *content_ptr, 
			const size_t content_length,
			boost::logic::tribool final_content_is_utf8) const;

		/**
		 * processes content extraction for HTTPMessage payload content
		 *
		 * @param event_ptr_ref pointer to the Event being generated
		 * @param http_msg the message object to extract payload content from
		 */
		inline void processRawContent(pion::platform::EventPtr& event_ptr_ref,
			const pion::net::HTTPMessage& http_msg) const;

		/**
		 * processes content extraction for HTTPMessage payload content
		 * (decodes content if it is encoded, converts to UTF-8 if appropriate)
		 *
		 * @param event_ptr_ref pointer to the Event being generated
		 * @param http_msg the message object to extract payload content from
		 * @param decoded_and_converted_flag has the payload content been decoded (if needed) and converted (if needed)?
		 * @param final_content cached value of decoded and converted payload content
		 * @param final_content_length length of the cached decoded and converted payload content
		 * @param logger PionLogger instance to use
		 */
		inline void processContent(pion::platform::EventPtr& event_ptr_ref,
			const pion::net::HTTPMessage& http_msg,
			boost::logic::tribool& decoded_and_converted_flag,
			boost::shared_array<char>& final_content,
			size_t& final_content_length,
			PionLogger& logger) const;

		/**
		 * Truncates to the lesser of m_max_size and content_length, except that if
		 * that specifies a cutoff in the middle of a UTF-8 code point, it truncates to the
		 * beginning of the code point.
		 *
		 * @param event_ptr_ref pointer to the Event being generated
		 * @param content_ptr pointer to a blob of payload content data
		 * @param content_length length of the payload content data blob
		 * @param content_is_valid_utf8 content is valid UTF-8
		 */
		inline void setTruncatedString(pion::platform::EventPtr& event_ptr_ref,
			const char *content_ptr, const size_t content_length, 
			boost::logic::tribool content_is_valid_utf8) const;

		/**
		 * copies decoded content from a decoder into a decoder sink
		 *
		 * @param decoder used to decode the input stream
		 * @param decoder_sink used to store the output stream
		 *
		 * @return bool true if decoded content was copied, false if not
		 */
		bool writeToSink(FilteringStreambuf& decoder, DecoderSink& decoder_sink) const;

		/**
		 * attempts to decode payload content for HTTPMessage
		 *
		 * @param http_msg the message object to extract payload content from
		 * @param decoded_content cached value of decoded payload content
		 * @param decoded_content_length length of the cached decoded payload content
		 * @param content_encoding value of Content-Encoding header (if any)
		 *
		 * @return bool true if content was decoded, false if not
		 */
		bool tryDecoding(const pion::net::HTTPMessage& http_msg,
			boost::shared_array<char>& decoded_content,
			size_t& decoded_content_length,
			std::string& content_encoding,
			PionLogger& logger) const;

		/**
		 * attempts to convert payload content for HTTPMessage to UTF-8, if needed
		 *
		 * @param charset the charset that the content is currently encoded with
		 * @param http_msg the message object to extract payload content from
		 * @param decoded_content pointer to payload content, with any needed decoding already done
		 * @param decoded_content_length length of decoded_content
		 * @param final_content cached value of payload content encoded with UTF-8
		 * @param final_content_length length of final_content
		 * @param logger PionLogger instance to use
		 *
		 * @return bool true if content was decoded and converted, false if not
		 */
		bool tryConvertingToUtf8(
			const std::string& charset,
			const char* source,
			size_t source_length,
			boost::shared_array<char>& final_content,
			size_t& final_content_length,
			PionLogger& logger) const;
		

		/// the protocol this rule belongs to
		const HTTPProtocol&					m_parent_protocol;

		/// vocabulary term for the event field where the content is stored
		pion::platform::Vocabulary::Term	m_term;

		/// data source used for the content extraction
		ExtractionSource					m_source;

		/// name parameter used for extraction from dictionary/maps
		std::string							m_name;

		/// format used for regex extraction (if empty, entire value is used)
		std::string							m_format;

		/// regex that must match the source value; may contain ()'s for m_format
		boost::regex						m_match;

		/// regex that must match the source value; may contain ()'s for m_format
		boost::u32regex						m_u32_match;

		/// the original string that m_match was constructed from
		std::string							m_match_str;

		/// regex that must match the content-type (for cs-content and sc-content)
		boost::regex						m_type_regex;

		/// maximum size (in bytes) of content to save (0 = do not save)
		boost::uint32_t						m_max_size;

		/// maximum matches to extract from source using format (0 = unlimited)
		boost::uint32_t						m_max_extracts;

		/// Is this rule running?
		bool								m_running;
	};

	/// data type for a smart pointer to an extraction rule
	typedef std::shared_ptr<ExtractionRule>	ExtractionRulePtr;

	/// data type for a collection of extraction rules
	typedef std::vector<ExtractionRulePtr>		ExtractionRuleVector;


	/**
	 * generates a new Event using the existing HTTP request and 
	 * response objects 
	 * 
	 * @param event_ptr_ref pointer assigned to the new Event
	 */
	void generateEvent(pion::platform::EventPtr& event_ptr_ref);

	/**
	 * handles the failure of a regex-based extraction rule
	 *
	 * @param regex string representation of the regex that failed
	 * @param str source string extraction was performed upon
	 */
	void handleRegexFailure(const std::string& regex, const std::string& str) const;


	/// primary logging interface used by this class
	PionLogger					m_logger;

	/// parser used for HTTP request
	pion::net::HTTPParser		m_request_parser;

	/// parser used for HTTP response
	pion::net::HTTPParser		m_response_parser;

	/// HTTP request being parsed
	pion::net::HTTPRequest		m_request;

	/// HTTP response being parsed
	pion::net::HTTPResponse		m_response;

	/// timestamp for the beginning of the HTTP request (first packet)
	pion::PionDateTime			m_request_start_time;

	/// timestamp for the end of the HTTP request (last packet)
	pion::PionDateTime			m_request_end_time;

	/// timestamp acknowledging receipt of the last HTTP request packet
	pion::PionDateTime			m_request_ack_time;

	/// timestamp for the beginning of the HTTP response (first packet)
	pion::PionDateTime			m_response_start_time;

	/// timestamp for the end of the HTTP response (last packet)
	pion::PionDateTime			m_response_end_time;

	/// timestamp acknowledging receipt of the last HTTP response packet
	pion::PionDateTime			m_response_ack_time;

	/// total number of request data packets
	boost::uint32_t				m_cs_data_packets;

	/// total number of response data packets
	boost::uint32_t				m_sc_data_packets;

	/// total number of missing request data packets
	boost::uint32_t				m_cs_missing_packets;

	/// total number of missing response data packets
	boost::uint32_t				m_sc_missing_packets;

	/// collection of rules used to extract content
	ExtractionRuleVector		m_extraction_rules;

	/// whether to enable converting content to UTF-8
	bool						m_allow_utf8_conversion;

	/// whether to enable searching the content for meta tags containing charset declarations
	bool						m_allow_searching_content_for_charset;

	/// copy of universal vocabulary used for error reporting
	pion::platform::VocabularyPtr	m_vocab_ptr;
	

	/// regular expression used to extract charset from Content-Encoding HTTP header
	static const boost::regex	EXTRACT_CHARSET_RX;

	/// regular expression used to extract charset from HTML meta tag
	static const boost::regex	META_CHARSET1_RX;

	/// another regular expression used to extract charset from HTML meta tag
	static const boost::regex	META_CHARSET2_RX;

	/// regular expression used to match UTF-8 in charset string
	static const boost::regex	UTF8_RX;
	
	/// name of the MaxRequestContentLength element for Pion XML config files
	static const std::string	MAX_REQUEST_CONTENT_LENGTH_ELEMENT_NAME;

	/// name of the MaxResponseContentLength element for Pion XML config files
	static const std::string	MAX_RESPONSE_CONTENT_LENGTH_ELEMENT_NAME;

	/// name of the RawRequestHeaders element for Pion XML config files
	static const std::string	RAW_REQUEST_HEADERS_ELEMENT_NAME;

	/// name of the RawResponseHeaders element for Pion XML config files
	static const std::string	RAW_RESPONSE_HEADERS_ELEMENT_NAME;

	/// name of the AllowUtf8Conversion element for Pion XML config files
	static const std::string	ALLOW_UTF8_CONVERSION_ELEMENT_NAME;

	/// name of the AllowSearchingContentForCharset element for Pion XML config files
	static const std::string	ALLOW_SEARCHING_CONTENT_FOR_CHARSET_ELEMENT_NAME;

	/// name of the ContentType element for Pion XML config files
	static const std::string	CONTENT_TYPE_ELEMENT_NAME;

	/// name of the MaxSize element for Pion XML config files
	static const std::string	MAX_SIZE_ELEMENT_NAME;

	/// name of the MaxExtracts element for Pion XML config files
	static const std::string	MAX_EXTRACTS_ELEMENT_NAME;

	/// name of the extract element for Pion XML config files
	static const std::string	EXTRACT_ELEMENT_NAME;

	/// name of the source element for Pion XML config files
	static const std::string	SOURCE_ELEMENT_NAME;

	/// name of the match element for Pion XML config files
	static const std::string	MATCH_ELEMENT_NAME;

	/// name of the format element for Pion XML config files
	static const std::string	FORMAT_ELEMENT_NAME;

	/// name of the descriptive name element for Pion XML config files
	static const std::string	NAME_ELEMENT_NAME;

	/// name of the Term ID attribute for Pion XML config files
	static const std::string	TERM_ATTRIBUTE_NAME;


	/// string used for query string extraction source type
	static const std::string	EXTRACT_QUERY_STRING;

	/// string used for cookie extraction source type
	static const std::string	EXTRACT_COOKIE_STRING;

	/// string used for request cookie extraction source type
	static const std::string	EXTRACT_CS_COOKIE_STRING;

	/// string used for response cookie extraction source type
	static const std::string	EXTRACT_SC_COOKIE_STRING;

	/// string used for request header extraction source type
	static const std::string	EXTRACT_CS_HEADER_STRING;

	/// string used for response header extraction source type
	static const std::string	EXTRACT_SC_HEADER_STRING;

	/// string used for decoded request content extraction source type
	static const std::string	EXTRACT_CS_CONTENT_STRING;

	/// string used for decoded response content extraction source type
	static const std::string	EXTRACT_SC_CONTENT_STRING;

	/// string used for raw request content extraction source type
	static const std::string	EXTRACT_CS_RAW_CONTENT_STRING;

	/// string used for raw response content extraction source type
	static const std::string	EXTRACT_SC_RAW_CONTENT_STRING;

	/// urn:vocab:clickstream#cs-data-packets
	static const std::string	VOCAB_CLICKSTREAM_CS_DATA_PACKETS;
	pion::platform::Vocabulary::TermRef	m_cs_data_packets_term_ref; 

	/// urn:vocab:clickstream#sc-data-packets
	static const std::string	VOCAB_CLICKSTREAM_SC_DATA_PACKETS;
	pion::platform::Vocabulary::TermRef	m_sc_data_packets_term_ref; 

	/// urn:vocab:clickstream#cs-missing-packets
	static const std::string	VOCAB_CLICKSTREAM_CS_MISSING_PACKETS;
	pion::platform::Vocabulary::TermRef	m_cs_missing_packets_term_ref; 

	/// urn:vocab:clickstream#sc-missing-packets
	static const std::string	VOCAB_CLICKSTREAM_SC_MISSING_PACKETS;
	pion::platform::Vocabulary::TermRef	m_sc_missing_packets_term_ref; 

	/// urn:vocab:clickstream#cs-headers
	static const std::string	VOCAB_CLICKSTREAM_CS_HEADERS;
	pion::platform::Vocabulary::TermRef	m_cs_headers_term_ref; 

	/// urn:vocab:clickstream#sc-headers
	static const std::string	VOCAB_CLICKSTREAM_SC_HEADERS;
	pion::platform::Vocabulary::TermRef	m_sc_headers_term_ref; 

	/// urn:vocab:clickstream#cs-bytes
	static const std::string	VOCAB_CLICKSTREAM_CS_BYTES;
	pion::platform::Vocabulary::TermRef	m_cs_bytes_term_ref; 

	/// urn:vocab:clickstream#sc-bytes
	static const std::string	VOCAB_CLICKSTREAM_SC_BYTES;
	pion::platform::Vocabulary::TermRef	m_sc_bytes_term_ref; 

	/// urn:vocab:clickstream#bytes
	static const std::string	VOCAB_CLICKSTREAM_BYTES;
	pion::platform::Vocabulary::TermRef	m_bytes_term_ref; 

	/// urn:vocab:clickstream#status
	static const std::string	VOCAB_CLICKSTREAM_STATUS;
	pion::platform::Vocabulary::TermRef	m_status_term_ref; 

	/// urn:vocab:clickstream#comment
	static const std::string	VOCAB_CLICKSTREAM_COMMENT;
	pion::platform::Vocabulary::TermRef	m_comment_term_ref; 

	/// urn:vocab:clickstream#method
	static const std::string	VOCAB_CLICKSTREAM_METHOD;
	pion::platform::Vocabulary::TermRef	m_method_term_ref; 

	/// urn:vocab:clickstream#uri
	static const std::string	VOCAB_CLICKSTREAM_URI;
	pion::platform::Vocabulary::TermRef	m_uri_term_ref; 

	/// urn:vocab:clickstream#uri-stem
	static const std::string	VOCAB_CLICKSTREAM_URI_STEM;
	pion::platform::Vocabulary::TermRef	m_uri_stem_term_ref; 

	/// urn:vocab:clickstream#uri-query
	static const std::string	VOCAB_CLICKSTREAM_URI_QUERY;
	pion::platform::Vocabulary::TermRef	m_uri_query_term_ref; 

	/// urn:vocab:clickstream#request
	static const std::string	VOCAB_CLICKSTREAM_REQUEST;
	pion::platform::Vocabulary::TermRef	m_request_term_ref; 

	/// urn:vocab:clickstream#cached
	static const std::string	VOCAB_CLICKSTREAM_CACHED;
	pion::platform::Vocabulary::TermRef	m_cached_term_ref;

	/// urn:vocab:clickstream#date
	static const std::string	VOCAB_CLICKSTREAM_DATE;
	pion::platform::Vocabulary::TermRef	m_date_term_ref;

	/// urn:vocab:clickstream#time
	static const std::string	VOCAB_CLICKSTREAM_TIME;
	pion::platform::Vocabulary::TermRef	m_time_term_ref;

	/// urn:vocab:clickstream#date-time
	static const std::string	VOCAB_CLICKSTREAM_DATE_TIME;
	pion::platform::Vocabulary::TermRef	m_date_time_term_ref;

	/// urn:vocab:clickstream#epoch-time
	static const std::string	VOCAB_CLICKSTREAM_EPOCH_TIME;
	pion::platform::Vocabulary::TermRef	m_epoch_time_term_ref;

	/// urn:vocab:clickstream#clf-date
	static const std::string	VOCAB_CLICKSTREAM_CLF_DATE;
	pion::platform::Vocabulary::TermRef	m_clf_date_term_ref;

	/// urn:vocab:clickstream#request-start-time
	static const std::string	VOCAB_CLICKSTREAM_REQUEST_START_TIME;
	pion::platform::Vocabulary::TermRef	m_request_start_time_term_ref;

	/// urn:vocab:clickstream#request-end-time
	static const std::string	VOCAB_CLICKSTREAM_REQUEST_END_TIME;
	pion::platform::Vocabulary::TermRef	m_request_end_time_term_ref;

	/// urn:vocab:clickstream#response-start-time
	static const std::string	VOCAB_CLICKSTREAM_RESPONSE_START_TIME;
	pion::platform::Vocabulary::TermRef	m_response_start_time_term_ref;

	/// urn:vocab:clickstream#response-end-time
	static const std::string	VOCAB_CLICKSTREAM_RESPONSE_END_TIME;
	pion::platform::Vocabulary::TermRef	m_response_end_time_term_ref;

	/// urn:vocab:clickstream#cs-send-time
	static const std::string	VOCAB_CLICKSTREAM_CS_SEND_TIME;
	pion::platform::Vocabulary::TermRef	m_cs_send_time_term_ref;

	/// urn:vocab:clickstream#cs-ack-time
	static const std::string	VOCAB_CLICKSTREAM_CS_ACK_TIME;
	pion::platform::Vocabulary::TermRef	m_cs_ack_time_term_ref;

	/// urn:vocab:clickstream#sc-reply-time
	static const std::string	VOCAB_CLICKSTREAM_SC_REPLY_TIME;
	pion::platform::Vocabulary::TermRef	m_sc_reply_time_term_ref;

	/// urn:vocab:clickstream#sc-send-time
	static const std::string	VOCAB_CLICKSTREAM_SC_SEND_TIME;
	pion::platform::Vocabulary::TermRef	m_sc_send_time_term_ref;

	/// urn:vocab:clickstream#sc-ack-time
	static const std::string	VOCAB_CLICKSTREAM_SC_ACK_TIME;
	pion::platform::Vocabulary::TermRef	m_sc_ack_time_term_ref;

	/// urn:vocab:clickstream#data-center-time
	static const std::string	VOCAB_CLICKSTREAM_DATA_CENTER_TIME;
	pion::platform::Vocabulary::TermRef	m_data_center_time_term_ref;

	/// urn:vocab:clickstream#authuser
	static const std::string	VOCAB_CLICKSTREAM_AUTHUSER;
	pion::platform::Vocabulary::TermRef	m_authuser_term_ref;

	/// urn:vocab:clickstream#c-ip
	static const std::string	VOCAB_CLICKSTREAM_C_IP;
	pion::platform::Vocabulary::TermRef	m_c_ip_term_ref;

	/// urn:vocab:clickstream#request-status
	static const std::string	VOCAB_CLICKSTREAM_REQUEST_STATUS;
	pion::platform::Vocabulary::TermRef	m_request_status_term_ref;

	/// urn:vocab:clickstream#response-status
	static const std::string	VOCAB_CLICKSTREAM_RESPONSE_STATUS;
	pion::platform::Vocabulary::TermRef	m_response_status_term_ref;

	/// urn:vocab:clickstream#refused
	static const std::string	VOCAB_CLICKSTREAM_REFUSED;
	pion::platform::Vocabulary::TermRef	m_refused_term_ref;

	/// urn:vocab:clickstream#canceled
	static const std::string	VOCAB_CLICKSTREAM_CANCELED;
	pion::platform::Vocabulary::TermRef	m_canceled_term_ref;

	/// urn:vocab:clickstream#cs-version
	static const std::string	VOCAB_CLICKSTREAM_CS_VERSION;
	pion::platform::Vocabulary::TermRef	m_cs_version_term_ref;

	/// NOTE: in addition to the above Terms, the SnifferReactor
	/// automatically sets the following:
	/// 
	/// * urn:vocab:clickstream#c-ip
	/// * urn:vocab:clickstream#s-ip
	/// * urn:vocab:clickstream#c-port
	/// * urn:vocab:clickstream#s-port
	/// * urn:vocab:clickstream#c-mac
	/// * urn:vocab:clickstream#s-mac
	/// * urn:vocab:clickstream#ssl-time
	/// * urn:vocab:clickstream#cs-packets
	/// * urn:vocab:clickstream#sc-packets
	/// * urn:vocab:clickstream#cs-ack-packets
	/// * urn:vocab:clickstream#sc-ack-packets
	/// * urn:vocab:clickstream#cs-duplicate-packets
	/// * urn:vocab:clickstream#sc-duplicate-packets
	///
	/// SnifferReactor will also set clickstream#proxy-ip if c-ip is defined by Protocol
};


// inline member functions for HTTPProtocol::ExtractionRule

template <typename RangePair>
inline void HTTPProtocol::ExtractionRule::process(pion::platform::EventPtr& event_ptr_ref,
	RangePair range, bool url_decode) const
{
	boost::uint32_t num_extracts;
	std::string::const_iterator first, last;
	boost::match_results<std::string::const_iterator> mr;
	while (range.first != range.second) {
		const std::string content_ref = (url_decode
			? algo::url_decode(range.first->second)
			: range.first->second);
		if ( m_max_size > 0 && ! content_ref.empty() ) {
			if ( m_match.empty() ) {
				setTruncatedString(event_ptr_ref, content_ref.c_str(), content_ref.size(), boost::indeterminate);
			} else {
				num_extracts = 0U;
				first = content_ref.begin();
				last = content_ref.end();
				while (1) {
					try {
						if (! boost::regex_search(first, last, mr, m_match))
							break;
					} catch (...) {
						m_parent_protocol.handleRegexFailure(m_match.str(), content_ref);
					}
					if (m_format.empty() || mr.empty()) {
						// no format -> extract entire string
						setTruncatedString(event_ptr_ref, content_ref.c_str(), content_ref.size(), boost::indeterminate);
						break;	// stop looking for matches
					} else {
						std::string content_str(mr.format(m_format, boost::format_all));
						setTruncatedString(event_ptr_ref, content_str.c_str(), content_str.size(), boost::indeterminate);
					}
					// check max_extracts
					if (m_max_extracts && ++num_extracts >= m_max_extracts)
						break;
					// look for more matches after end of current one
					first = mr[0].second;
				}
			}
		}
		++range.first;
	}
}

inline void HTTPProtocol::ExtractionRule::setTermValueFromFinalContent(
		pion::platform::EventPtr& event_ptr_ref,
		const char *content_ptr, 
		const size_t content_length,
		boost::logic::tribool final_content_is_utf8) const
{
	boost::match_results<const char*> mr;
	if ( m_match.empty() ) {
		setTruncatedString(event_ptr_ref, content_ptr, content_length, final_content_is_utf8);
	} else {
		boost::uint32_t num_extracts = 0U;
		const char *end_ptr = content_ptr + content_length;
		while (1) {
			try {
				if (final_content_is_utf8) {
					if (! boost::u32regex_search(content_ptr, end_ptr, mr, m_u32_match))
						break;
				} else {
					if (! boost::regex_search(content_ptr, end_ptr, mr, m_match))
						break;
				}
			} catch (...) {
				m_parent_protocol.handleRegexFailure(m_match_str, content_ptr);
			}
			if (m_format.empty() || mr.empty()) {
				// no format -> extract entire string
				setTruncatedString(event_ptr_ref, content_ptr, content_length, final_content_is_utf8);
				break;	// stop looking for matches
			} else {
				std::string content_str(mr.format(m_format, boost::format_all));
				setTruncatedString(event_ptr_ref, content_str.c_str(), content_str.size(), boost::indeterminate);
			}
			// check max_extracts
			if (m_max_extracts && ++num_extracts >= m_max_extracts)
				break;
			// look for more matches after end of current one
			content_ptr = mr[0].second;
		}
	}
}

inline void HTTPProtocol::ExtractionRule::processRawContent(pion::platform::EventPtr& event_ptr_ref,
	const pion::net::HTTPMessage& http_msg) const
{
	if (m_max_size > 0 && http_msg.getContentLength() > 0) {
		bool content_type_matches = true;
		if (! m_type_regex.empty()) {
			const std::string& content_type = http_msg.getHeader(pion::net::HTTPTypes::HEADER_CONTENT_TYPE);
			try {
				content_type_matches = boost::regex_search(content_type, m_type_regex);
			} catch (...) {
				m_parent_protocol.handleRegexFailure(m_type_regex.str(), content_type);
			}
		}
		if (content_type_matches) 
			setTermValueFromFinalContent(event_ptr_ref, http_msg.getContent(), http_msg.getContentLength(), boost::indeterminate);
	}
}

inline void HTTPProtocol::ExtractionRule::processContent(pion::platform::EventPtr& event_ptr_ref,
	const pion::net::HTTPMessage& http_msg, boost::logic::tribool& decoded_and_converted_flag,
	boost::shared_array<char>& final_content, size_t& final_content_length, 
	PionLogger& logger) const
{
	if (m_max_size > 0 && http_msg.getContentLength() > 0) {
		const std::string& content_type = http_msg.getHeader(pion::net::HTTPTypes::HEADER_CONTENT_TYPE);
		bool content_type_matches = false;
		try {
			content_type_matches = m_type_regex.empty() || boost::regex_search(content_type, m_type_regex);
		} catch (...) {
			m_parent_protocol.handleRegexFailure(m_type_regex.str(), content_type);
		}
		if (content_type_matches) {
			// Try decoding and converting the content if we haven't already done so.
			if (boost::indeterminate(decoded_and_converted_flag)) {
				// See http://www.w3.org/International/tutorials/tutorial-char-enc/#Slide0400 for
				// documentation of the precedence rules for charset declarations.

				boost::shared_array<char> decoded_content;
				std::string content_encoding;
				size_t decoded_content_length;
				bool decoded_flag = tryDecoding(http_msg, decoded_content, decoded_content_length, content_encoding, logger);
				if (decoded_flag || content_encoding.empty()) {
					bool do_conversion = false;
					std::string charset;
					if (m_parent_protocol.m_allow_utf8_conversion) {
						// Get the charset, if present, from the Content-Type header.
						boost::match_results<std::string::const_iterator> mr;
						if (boost::regex_search(content_type, mr, EXTRACT_CHARSET_RX)) {
							charset = mr[1];
						}

						if (m_parent_protocol.m_allow_searching_content_for_charset) {
							if (charset.empty()) {
								if (content_type.compare(0, 9, "text/html") == 0) {
									// No charset in the Content-Type header and this is HTML, so need to look for meta tags in the content.

									boost::match_results<const char*> mr2;
									const char* p = decoded_flag? decoded_content.get() : http_msg.getContent();
									size_t length2 = decoded_flag? decoded_content_length : http_msg.getContentLength();
									size_t length1 = length2 > 512? 512 : length2; // <meta charset> tags are required to be in the first 512 bytes.
									if (boost::regex_search(p, p + length1, mr2, META_CHARSET1_RX)) {
										charset = mr2[1];
									} else if (boost::regex_search(p, p + length2, mr2, META_CHARSET2_RX)) {
										charset = mr2[1];
									}
								}
							} else {
								// A charset was found in the Content-Type header, so we don't need to look for meta tags,
								// since the former takes precedence over the latter.
							}
						}
						do_conversion = (!charset.empty() && !boost::regex_search(charset, UTF8_RX));
					}

					if (content_encoding.empty()) {
						// No decoding needed, so do conversion, if required, directly on http_msg.getContent().

						if (do_conversion) {
							decoded_and_converted_flag = tryConvertingToUtf8(charset, http_msg.getContent(), http_msg.getContentLength(), 
																			 final_content, final_content_length, logger);
						}
						if (! do_conversion || ! decoded_and_converted_flag) {
							if (pion::platform::EventValidator::isValidUTF8(http_msg.getContent(), http_msg.getContentLength(), &final_content_length)) {
								// No decoding needed, conversion failed or not needed, and no cleansing needed.
								decoded_and_converted_flag = false;
							} else {
								final_content_length = pion::platform::EventValidator::getCleansedUTF8Length(http_msg.getContent(), http_msg.getContentLength());

								try {
									final_content.reset(new char[final_content_length]);
								} catch (std::bad_alloc& e) {
									PION_LOG_ERROR(logger, "final_content_length: " << final_content_length << " - " << e.what() << " - rethrowing");
									throw BadAllocException("final_content_length = " + boost::lexical_cast<std::string>(final_content_length));
								}

								pion::platform::EventValidator::cleanseUTF8_TEMP(http_msg.getContent(), http_msg.getContentLength(), final_content.get(), &final_content_length);
								decoded_and_converted_flag = true;
							}
						}
					} else {
						// Decoded content is in decoded_content.

						if (do_conversion) {
							decoded_and_converted_flag = tryConvertingToUtf8(charset, decoded_content.get(), decoded_content_length,
																			 final_content, final_content_length, logger);
						}
						if (! do_conversion || ! decoded_and_converted_flag) {
							if (pion::platform::EventValidator::isValidUTF8(decoded_content.get(), decoded_content_length, &final_content_length)) {
								final_content.swap(decoded_content);
							} else {
								final_content_length = pion::platform::EventValidator::getCleansedUTF8Length(decoded_content.get(), decoded_content_length);

								try {
									final_content.reset(new char[final_content_length]);
								} catch (std::bad_alloc& e) {
									PION_LOG_ERROR(logger, "final_content_length: " << final_content_length << " - " << e.what() << " - rethrowing");
									throw BadAllocException("final_content_length = " + boost::lexical_cast<std::string>(final_content_length));
								}

								pion::platform::EventValidator::cleanseUTF8_TEMP(decoded_content.get(), decoded_content_length, final_content.get(), &final_content_length);
							}
							decoded_and_converted_flag = true;
						}
					}
				} else {
					// Decoding failed - still need to cleanse content if it's not valid UTF-8.
					// NOTE: tryDecoding() logs already when decoding fails
					//PION_LOG_WARN(logger, "Decoding failed for Content-Encoding: " << content_encoding);

					if (pion::platform::EventValidator::isValidUTF8(http_msg.getContent(), http_msg.getContentLength(), &final_content_length)) {
						decoded_and_converted_flag = false;
					} else {
						final_content_length = pion::platform::EventValidator::getCleansedUTF8Length(http_msg.getContent(), http_msg.getContentLength());

						try {
							final_content.reset(new char[final_content_length]);
						} catch (std::bad_alloc& e) {
							PION_LOG_ERROR(logger, "final_content_length: " << final_content_length << " - " << e.what() << " - rethrowing");
							throw BadAllocException("final_content_length = " + boost::lexical_cast<std::string>(final_content_length));
						}

						pion::platform::EventValidator::cleanseUTF8_TEMP(http_msg.getContent(), http_msg.getContentLength(), final_content.get(), &final_content_length);
						decoded_and_converted_flag = true;
					}
				}
			}

			if (decoded_and_converted_flag) {
				// final_content contains the result of decoding the content (if needed and successful), followed by
				// UTF-8 conversion or UTF-8 validation/cleansing.
				setTermValueFromFinalContent(event_ptr_ref, final_content.get(), final_content_length, true);
			} else {
				// Either no Content-Encoding header was found and no charset was specified, or decoding or conversion failed.
				// Also, http_msg.getContent() has been confirmed as valid UTF-8 (up to a length of final_content_length).
				setTermValueFromFinalContent(event_ptr_ref, http_msg.getContent(), final_content_length, true);
			}
		}
	}
}

inline void HTTPProtocol::ExtractionRule::setTruncatedString(
	pion::platform::EventPtr& event_ptr_ref,
	const char* content_ptr,
	const size_t content_length,
	boost::logic::tribool content_is_valid_utf8) const
{
	if (content_is_valid_utf8) {
		size_t offset = content_length > m_max_size? m_max_size : content_length;
		if (offset < content_length) {
			// 'offset' might be in the middle of a UTF-8 code point, 
			// in which case this will decrease 'offset' to point to the beginning of that code point.
			U8_SET_CP_START_UNSAFE(content_ptr, offset)
		}

		// Avoid re-checking UTF-8 validity at the Event level.
		(*event_ptr_ref).setBlob(m_term.term_ref, content_ptr, offset);
	} else
		(*event_ptr_ref).setString(m_term.term_ref, content_ptr, content_length);
}

}	// end namespace plugins
}	// end namespace pion

#endif

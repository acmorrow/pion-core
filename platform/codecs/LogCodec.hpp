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

#ifndef __PION_LOGCODEC_HEADER__
#define __PION_LOGCODEC_HEADER__

#include <vector>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_array.hpp>
#include <boost/lexical_cast.hpp>
#include <pion/PionConfig.hpp>
#include <pion/PionException.hpp>
#include <pion/PionHashMap.hpp>
#include <pion/PionDateTime.hpp>
#include <pion/PionAlgorithms.hpp>
#include <pion/platform/Codec.hpp>
#include <pion/platform/Vocabulary.hpp>

#ifdef PION_WIN32
#define OSEOL "\r\n"
#else
#define OSEOL "\n"
#endif

namespace pion {		// begin namespace pion
namespace plugins {		// begin namespace plugins


///
/// LogCodec: used to encode and decode Events using standard log file formats
///
class LogCodec :
	public pion::platform::Codec
{
public:

	/// exception thrown if the Codec configuration includes an empty field name
	class EmptyFieldException : public PionException {
	public:
		EmptyFieldException(const std::string& codec_id)
			: PionException("LogCodec configuration includes an empty field name: ", codec_id) {}
	};

	/// exception thrown if the Codec configuration does not define a term in a field mapping
	class EmptyTermException : public PionException {
	public:
		EmptyTermException(const std::string& codec_id)
			: PionException("LogCodec configuration is missing a term identifier: ", codec_id) {}
	};

	/// exception thrown if the Codec configuration uses an unknown term in a field mapping
	class UnknownTermException : public PionException {
	public:
		UnknownTermException(const std::string& term_id)
			: PionException("LogCodec configuration maps field to an unknown term: ", term_id) {}
	};

	/// exception thrown if the Codec is unable to parse a format
	class BadFormatException : public PionException {
	public:
		BadFormatException(const std::string& term_id)
			: PionException("LogCodec format contains an unknown term: ", term_id) {}
	};


	/// constructs a new LogCodec object
	LogCodec(void)
		: pion::platform::Codec(), m_read_buf(new char[READ_BUFFER_SIZE+1]),
		m_read_end(m_read_buf.get() + READ_BUFFER_SIZE),
		m_flush_after_write(false), m_handle_elf_headers(false), m_wrote_elf_headers(false),
		m_time_offset(0),
		m_event_split(EVENT_SPLIT_SET), m_event_join(EVENT_JOIN_STRING), m_comment_chars(COMMENT_CHAR_SET),
		m_field_split(FIELD_SPLIT_SET), m_field_join(FIELD_JOIN_STRING), m_consume_delims(true)
	{}

	/// virtual destructor: this class is meant to be extended
	virtual ~LogCodec() {}

	/// returns an HTTP content type that is used by this Codec
	virtual const std::string& getContentType(void) const { return CONTENT_TYPE; }

	/**
	 * clones the codec, returning a pointer to the cloned copy
	 *
	 * @return CodecPtr pointer to the cloned copy of the codec
	 */
	virtual pion::platform::CodecPtr clone(void) const;

	/**
	 * writes an Event to an output stream
	 *
	 * @param out the output stream to which the Event will be written
	 * @param e the Event to write to the output stream
	 */
	virtual void write(std::ostream& out, const pion::platform::Event& e);

	/**
	 * writes any needed footers (currently none for LogCodec) to an output stream
	 *
	 * @param out the output stream to which the footers will be written
	 */
	virtual void finish(std::ostream& out) {};

	/**
	 * reads an Event from an input stream
	 *
	 * @param in the input stream to read the Event from
	 * @param e the Event read, if any; null if error
	 * @return true if successful, false otherwise
	 */
	virtual bool read(std::istream& in, pion::platform::Event& e);

	/**
	 * sets configuration parameters for this Codec
	 *
	 * @param v the Vocabulary that this Codec will use to describe Terms
	 * @param config_ptr pointer to a list of XML nodes containing Codec
	 *                   configuration parameters
	 */
	virtual void setConfig(const pion::platform::Vocabulary& v, const xmlNodePtr config_ptr);

	/**
	 * this updates the Vocabulary information used by this Codec; it should be
	 * called whenever the global Vocabulary is updated
	 *
	 * @param v the Vocabulary that this Codec will use to describe Terms
	 */
	virtual void updateVocabulary(const pion::platform::Vocabulary& v);

	/// resets the configuration for this Codec
	inline void reset(void) {
		m_field_map.clear();
		m_format.clear();
		m_wrote_elf_headers = false;
	}


private:

	/// data type used to configure how the log format describes Vocabulary Terms
	struct LogField {
		/// constructs a new LogField structure
		LogField(const std::string& field, const pion::platform::Vocabulary::Term& term, char delim_start,
				 char delim_end, bool opt_delims, bool urlencode, char escape_char, const std::string& empty_val,
				 bool do_time_offset, const PionDateTime::time_duration_type& time_offset)
			: log_field(field), log_term(term), log_delim_start(delim_start), log_delim_end(delim_end),
			  log_opt_delims(opt_delims), log_urlencode(urlencode), log_escape_char(escape_char),
			  log_empty_val(empty_val), log_do_time_offset(do_time_offset), log_time_offset(time_offset)
		{}

		/// copy constructor
		LogField(const LogField& f)
			: log_field(f.log_field), log_term(f.log_term), log_delim_start(f.log_delim_start),
			  log_delim_end(f.log_delim_end), log_opt_delims(f.log_opt_delims),
			  log_urlencode(f.log_urlencode), log_escape_char(f.log_escape_char),
			  log_empty_val(f.log_empty_val), log_do_time_offset(f.log_do_time_offset), log_time_offset(f.log_time_offset)
		{}

		/// assignment operator
		inline LogField& operator=(const LogField& f) {
			log_field = f.log_field;
			log_term = f.log_term;
			log_delim_start = f.log_delim_start;
			log_delim_end = f.log_delim_end;
			log_opt_delims = f.log_opt_delims;
			log_urlencode = f.log_urlencode;
			log_escape_char = f.log_escape_char;
			log_empty_val = f.log_empty_val;
			log_do_time_offset = f.log_do_time_offset;
			log_time_offset = f.log_time_offset;
			return *this;
		}

		/// writes an empty value to an output stream
		inline void writeEmptyValue(std::ostream& out) const {
			if (log_delim_start != '\0')
				out << log_delim_start;
			out << log_empty_val;
			if (log_delim_end != '\0')
				out << log_delim_end;
		}

		/**
		 * writes the value for a single field
		 *
		 * @param out the output stream to which the value will be written
		 * @param value the value to write
		 */
		inline void write(std::ostream& out, const pion::platform::Event::ParameterValue& value);

		/**
		 * reads the value for a single field
		 *
		 * @param buf array of bytes to read the value from
		 * @param e the Event to read the field for
		 */
		inline void read(const char *buf, pion::platform::Event& e);

		/// the name of the field
		std::string							log_field;
		/// the Vocabulary Term that the data field represents
		pion::platform::Vocabulary::Term	log_term;
		/// used to encode and decode date_time fields
		PionTimeFacet						log_time_facet;
		/// a character that delimits the beginning of the field value, or '\0' if none
		char								log_delim_start;
		/// a character that delimits the end of the field value, or '\0' if none
		char								log_delim_end;
		/// whether start/end delimiters are optional (default: false)
		bool								log_opt_delims;
		/// if true that the log field is url-encoded
		bool								log_urlencode;
		/// a character that escapes a delimiter within a field value (default: "\")
		char								log_escape_char;
		/// a string that represents an empty field value (default: "-" if no delimiters)
		std::string							log_empty_val;
		/// whether to apply a time offset to dates and times
		bool								log_do_time_offset;
		/// time offset to apply
		PionDateTime::time_duration_type	log_time_offset;
	};

	/// data type for a pointer to a LogField object
	typedef boost::shared_ptr<LogField>		LogFieldPtr;

	/// data type that maps field names to LogFields
	typedef std::unordered_map<std::string,
		LogFieldPtr>		FieldMap;

	/// data type that keeps track of the log file's current field format
	typedef std::vector<LogFieldPtr>		CurrentFormat;

	/// traits_type used for the standard char-istream
	typedef std::istream::traits_type		traits_type;

	/// data type used to represent a standard char-istream streambuf
	typedef std::basic_streambuf<std::istream::char_type,
		std::istream::traits_type>			streambuf_type;

	/// data type used to represent an integer value resulting from an istream read
	typedef std::istream::int_type			int_type;


	/**
	 * maps a data field to a Vocabulary Term
	 *
	 * @param field the name of the data field
	 * @param term the Vocabulary Term to map the data field to
	 * @param delim_start character used to delimit the start of the data field value
	 * @param delim_end character used to delimit the end of the data field value
	 * @param opt_delims flag to specify if start/end delimiters are optional
	 * @param urlencode flag to specify if the field content is urlencoded
	 * @param escape_char character used to escape a delimiter within a field value
	 * @param empty_val string used to represent an empty data field value
	 */
	inline void mapFieldToTerm(const std::string& field, const pion::platform::Vocabulary::Term& term,
							   char delim_start, char delim_end, bool opt_delims,
							   bool urlencode, char escape_char, const std::string& empty_val,
							   bool do_time_offset, const PionDateTime::time_duration_type& time_offset);

	/**
	 * translate C-style escape sequences in-place
	 *
	 * @param cstring the C-string containing C-style escape sequences to translate
	 */
	inline char * cstyle(char *cstring);

	/**
	 * changes the current format used by the Codec (for ELF only)
	 *
	 * @param fmt the new format to use (a sequence of field names separated by spaces)
	 */
	inline void changeELFFormat(char *fmt);

	/**
	 * writes the version number and field format headers (for ELF only)
	 *
	 * @param out the output stream to which the headers will be written
	 */
	inline void writeELFHeaders(std::ostream& out) const;

	/**
	 * skips occurances of "empty" events (i.e. records with no data) and comments,
	 * and detects and handles any field format changes (for ELF only)
	 *
	 * @param buf_ptr pointer to an istream streambuf used for reading
	 *
	 * @param return int_type value of the next byte available to be consumed
	 */
	inline int_type consumeVoidsAndComments(streambuf_type *buf_ptr);


	/// content type used by this Codec
	static const std::string		CONTENT_TYPE;

	/// name of the flush element for Pion XML config files
	static const std::string		FLUSH_ELEMENT_NAME;

	/// name of the headers element for Pion XML config files
	static const std::string		HEADERS_ELEMENT_NAME;

	/// name of the time offset element for Pion XML config files
	static const std::string		TIME_OFFSET_ELEMENT_NAME;

	/// name of the field mapping element for Pion XML config files
	static const std::string		FIELD_ELEMENT_NAME;

	/// name of the Term ID attribute for Pion XML config files
	static const std::string		TERM_ATTRIBUTE_NAME;

	/// name of the start delimiter attribute for Pion XML config files
	static const std::string		START_ATTRIBUTE_NAME;

	/// name of the end delimiter attribute for Pion XML config files
	static const std::string		END_ATTRIBUTE_NAME;

	/// name of the delimiters-optional attribute for Pion XML config files
	static const std::string		OPTIONAL_ATTRIBUTE_NAME;

	/// name of the urlencode attribute for Pion XML config files
	static const std::string		URLENCODE_ATTRIBUTE_NAME;

	/// name of the escape character attribute for Pion XML config files
	static const std::string		ESCAPE_ATTRIBUTE_NAME;

	/// name of the empty value attribute for Pion XML config files
	static const std::string		EMPTY_ATTRIBUTE_NAME;

	/// name of the event specifications element for Pion XML config files
	static const std::string		EVENTS_ELEMENT_NAME;

	/// name of the field specifications element for Pion XML config files
	static const std::string		FIELDS_ELEMENT_NAME;

	/// name of the split-string attribute for Pion XML config files
	static const std::string		SPLIT_ATTRIBUTE_NAME;

	/// name of the join-string attribute for Pion XML config files
	static const std::string		JOIN_ATTRIBUTE_NAME;

	/// name of the comment-chars attribute for Pion XML config files
	static const std::string		COMMENT_ATTRIBUTE_NAME;

	/// name of the consume-consecutive-delimiters attribute for Pion XML config files
	static const std::string		CONSUME_ATTRIBUTE_NAME;

	/// maximum size of the read buffer
	static const unsigned int		READ_BUFFER_SIZE;


	/// default values for various settings
	static const std::string		EVENT_SPLIT_SET;
	static const std::string		EVENT_JOIN_STRING;
	static const std::string		COMMENT_CHAR_SET;
	static const std::string		FIELD_SPLIT_SET;
	static const std::string		FIELD_JOIN_STRING;
	static const bool				CONSUME_DELIMS_FLAG;


	/// special support for ELF
	static const std::string		VERSION_ELF_HEADER;
	static const std::string		DATE_ELF_HEADER;
	static const std::string		SOFTWARE_ELF_HEADER;
	static const std::string		FIELDS_ELF_HEADER;


	/// memory buffer used to read events
	boost::scoped_array<char>		m_read_buf;

	/// pointer to the end of the read buffer
	const char * const				m_read_end;

	/// used to configure which fields map to Vocabulary Terms (for reading)
	FieldMap						m_field_map;

	/// represents the current sequence of data fields in the log format
	CurrentFormat					m_format;

	/// true if the codec should flush the output stream after each write
	bool							m_flush_after_write;

	/// true if the codec should handle ELF headers (and other ELF behaviors)
	bool							m_handle_elf_headers;

	/// did we write the ELF headers already?
	bool							m_wrote_elf_headers;

	/// time offset in minutes
	boost::int32_t					m_time_offset;

	/// the event split set for the log file
	std::string						m_event_split;

	/// the event join string for the log file
	std::string						m_event_join;

	/// the comment character set for the log file
	std::string						m_comment_chars;

	/// the field split set for the log file
	std::string						m_field_split;

	/// the field join string for the log file
	std::string						m_field_join;

	/// true if the codec should consume consecutive field delimiters
	bool							m_consume_delims;
};


// inline member functions for LogCodec

inline void LogCodec::mapFieldToTerm(const std::string& field, const pion::platform::Vocabulary::Term& term,
									 char delim_start, char delim_end, bool opt_delims,
									 bool urlencode, char escape_char, const std::string& empty_val,
									 bool do_time_offset, const PionDateTime::time_duration_type& time_offset)
{
	for (FieldMap::const_iterator i = m_field_map.begin(); i != m_field_map.end(); ++i) {
		if (i->second->log_term.term_ref == term.term_ref)
			throw PionException("Duplicate Field Term");
	}

	if (m_field_map.find(field) != m_field_map.end())
		throw PionException("Duplicate Field Name");

	// prepare a new Logfield object
	LogFieldPtr field_ptr(new LogField(field, term, delim_start, delim_end, opt_delims, urlencode, escape_char, empty_val, do_time_offset, time_offset));
	switch (term.term_type) {
		case pion::platform::Vocabulary::TYPE_DATE_TIME:
		case pion::platform::Vocabulary::TYPE_DATE:
		case pion::platform::Vocabulary::TYPE_TIME:
			field_ptr->log_time_facet.setFormat(term.term_format);
			break;
		default:
			break; // do nothing
	}
	// add it to the mapping of field names
	m_field_map[field] = field_ptr;
	// append the new field to the current (default) format
	m_format.push_back(field_ptr);
}

inline char * LogCodec::cstyle(char *cstring)
{
	char *ptr = cstring;
	size_t len = strlen(cstring);
	size_t num, nlen;
	unsigned int value;

	while ( (ptr = strchr(ptr, '\\')) ) {
		nlen = 1;
		switch (ptr[1]) {
		case 'a': *ptr = '\a'; break;
		case 'b': *ptr = '\b'; break;
		case 'f': *ptr = '\f'; break;
		case 'n': *ptr = '\n'; break;
		case 'r': *ptr = '\r'; break;
		case 't': *ptr = '\t'; break;
		case 'v': *ptr = '\v'; break;
		case '_': *ptr = ' '; break;
		case '0': case '1': case '2': case '3':
		case '4': case '5': case '6': case '7':
			nlen = sscanf(ptr + 1, "%o", &value);
			*ptr = (char)value;
			break;
		case 'x':
			nlen = sscanf(ptr + 1, "%x", &value);
			*ptr = (char)value;
			break;
		}
		num = ptr - cstring + nlen;
		ptr++;
		memmove(ptr, ptr + nlen, len - num);
	}

	return cstring;
}

inline void LogCodec::changeELFFormat(char *fmt)
{
	m_format.clear();
	char *ptr;
	bool last_field = false;
	while (!last_field && *fmt != '\0' && m_event_split.find(*fmt) == std::string::npos) {
		// skip leading spaces
		while (*fmt == ' ') ++fmt;
		// find the end of the field name
		ptr = fmt;
		while (*ptr != '\0' && m_event_split.find(*ptr) == std::string::npos && *ptr != ' ') ++ptr;
		// set last_field if we're at the end
		if (*ptr == '\0' || m_event_split.find(*ptr) != std::string::npos) last_field = true;
		*ptr = '\0';
		FieldMap::const_iterator i = m_field_map.find(fmt);
		if (i == m_field_map.end())
			throw BadFormatException(fmt);
		m_format.push_back(i->second);
		fmt = ptr + 1;
	}
}

inline void LogCodec::writeELFHeaders(std::ostream& out) const
{
	PionDateTime time_now(boost::posix_time::second_clock::universal_time());
	out << VERSION_ELF_HEADER << " 1.0" << m_event_join;
	out << DATE_ELF_HEADER << ' ' << time_now << m_event_join;
	out << SOFTWARE_ELF_HEADER << " Pion v" << PION_VERSION << m_event_join;
	out << FIELDS_ELF_HEADER;
	CurrentFormat::const_iterator i = m_format.begin();
	while (i != m_format.end())
		out << ' ' << (*i++)->log_field;
	out << m_event_join;
}

inline LogCodec::int_type LogCodec::consumeVoidsAndComments(streambuf_type *buf_ptr)
{
	int_type c = buf_ptr->sgetc();
	char * const read_buf = m_read_buf.get();
	char * read_ptr;

	while (!traits_type::eq_int_type(c, traits_type::eof())) {
		if (m_field_split.find(c) != std::string::npos || m_event_split.find(c) != std::string::npos) {
			c = buf_ptr->snextc();
		} else if (m_comment_chars.find(c) != std::string::npos) {
			// ignore comment line (sorta...)
			read_ptr = read_buf;
			do {
				// check for end of line
				if (m_event_split.find(c) != std::string::npos)
					break;
				// read in the comment in case it matters...
				if (read_ptr < m_read_end)
					*(read_ptr++) = c;
				// get the next character
				c = buf_ptr->snextc();
			} while (!traits_type::eq_int_type(c, traits_type::eof()));
			*read_ptr = '\0';
			if (m_handle_elf_headers) {
				// check if it is an ELF format change
				read_buf[FIELDS_ELF_HEADER.size()] = '\0';
				if (FIELDS_ELF_HEADER == read_buf)
					changeELFFormat(read_buf + FIELDS_ELF_HEADER.size() + 1);
			}
		} else {
			break;
		}
	}
	return c;
}


// inline member functions for LogCodec::LogField

inline void LogCodec::LogField::write(std::ostream& out, const pion::platform::Event::ParameterValue& value)
{
	std::ostringstream oss;

	switch(log_term.term_type) {
		case pion::platform::Vocabulary::TYPE_NULL:
		case pion::platform::Vocabulary::TYPE_OBJECT:
			break;	// ignore unsupported field
		case pion::platform::Vocabulary::TYPE_INT8:
		case pion::platform::Vocabulary::TYPE_INT16:
		case pion::platform::Vocabulary::TYPE_INT32:
			oss << boost::get<boost::int32_t>(value);
			break;
		case pion::platform::Vocabulary::TYPE_INT64:
			oss << boost::get<boost::int64_t>(value);
			break;
		case pion::platform::Vocabulary::TYPE_UINT8:
		case pion::platform::Vocabulary::TYPE_UINT16:
		case pion::platform::Vocabulary::TYPE_UINT32:
			oss << boost::get<boost::uint32_t>(value);
			break;
		case pion::platform::Vocabulary::TYPE_UINT64:
			oss << boost::get<boost::uint64_t>(value);
			break;
		case pion::platform::Vocabulary::TYPE_FLOAT:
			oss << boost::get<float>(value);
			break;
		case pion::platform::Vocabulary::TYPE_DOUBLE:
			// using boost::lexical_cast<std::string> ensures precision appropriate to type double
			oss << boost::lexical_cast<std::string>(boost::get<double>(value));
			break;
		case pion::platform::Vocabulary::TYPE_LONG_DOUBLE:
			// using boost::lexical_cast<std::string> ensures precision appropriate to type long double
			oss << boost::lexical_cast<std::string>(boost::get<long double>(value));
			break;
		case pion::platform::Vocabulary::TYPE_SHORT_STRING:
		case pion::platform::Vocabulary::TYPE_STRING:
		case pion::platform::Vocabulary::TYPE_LONG_STRING:
		case pion::platform::Vocabulary::TYPE_BLOB:
		case pion::platform::Vocabulary::TYPE_ZBLOB:
		{
			const pion::platform::Event::BlobType& ss = boost::get<const pion::platform::Event::BlobType&>(value);
			if (ss.size() > 0) {
				if (log_urlencode) {
					std::string temp_str(ss.get());
					oss << algo::url_encode(temp_str);
				} else {
					oss.write(ss.get(), ss.size());
				}
			}
			break;
		}
		case pion::platform::Vocabulary::TYPE_CHAR:
		{
			const pion::platform::Event::BlobType& ss = boost::get<const pion::platform::Event::BlobType&>(value);
			if (ss.size() > 0) {
				if (log_urlencode) {
					std::string temp_str(ss.get());
					temp_str = algo::url_encode(temp_str);
					if (temp_str.size() > log_term.term_size)
						temp_str.resize(log_term.term_size);
					oss << temp_str;
				} else {
					oss.write(ss.get(), ss.size() < log_term.term_size ? ss.size() : log_term.term_size);
				}
			}
			break;
		}
		case pion::platform::Vocabulary::TYPE_DATE_TIME:
		case pion::platform::Vocabulary::TYPE_DATE:
		case pion::platform::Vocabulary::TYPE_TIME:
		{
			PionDateTime dt = boost::get<const PionDateTime&>(value);
			if (log_do_time_offset) {
				dt -= log_time_offset;
			}
			if (log_urlencode) {
				std::string temp_str;
				log_time_facet.toString(temp_str, dt);
				oss << algo::url_encode(temp_str);
			} else {
				log_time_facet.write(oss, dt);
			}
			break;
		}
	}

	if (log_delim_start != '\0')
		out << log_delim_start;
	if (oss.str().empty())
		out << log_empty_val;
	else {
		std::string src = oss.str();
		std::string dst;
		for (std::string::iterator i = src.begin(); i != src.end(); i++) {
			if (*i == log_delim_end)
				dst.push_back(log_escape_char);
			dst.push_back(*i);
		}
		out << dst;
	}
	if (log_delim_end != '\0')
		out << log_delim_end;
}

inline void LogCodec::LogField::read(const char *buf, pion::platform::Event& e)
{
	switch(log_term.term_type) {
		case pion::platform::Vocabulary::TYPE_NULL:
		case pion::platform::Vocabulary::TYPE_OBJECT:
			break;	// ignore unsupported field
		case pion::platform::Vocabulary::TYPE_INT8:
		case pion::platform::Vocabulary::TYPE_INT16:
		case pion::platform::Vocabulary::TYPE_INT32:
			e.setInt(log_term.term_ref, boost::lexical_cast<boost::int32_t>(buf));
			break;
		case pion::platform::Vocabulary::TYPE_INT64:
			e.setBigInt(log_term.term_ref, boost::lexical_cast<boost::int64_t>(buf));
			break;
		case pion::platform::Vocabulary::TYPE_UINT8:
		case pion::platform::Vocabulary::TYPE_UINT16:
		case pion::platform::Vocabulary::TYPE_UINT32:
			e.setUInt(log_term.term_ref, boost::lexical_cast<boost::uint32_t>(buf));
			break;
		case pion::platform::Vocabulary::TYPE_UINT64:
			e.setUBigInt(log_term.term_ref, boost::lexical_cast<boost::uint64_t>(buf));
			break;
		case pion::platform::Vocabulary::TYPE_FLOAT:
			e.setFloat(log_term.term_ref, boost::lexical_cast<float>(buf));
			break;
		case pion::platform::Vocabulary::TYPE_DOUBLE:
			e.setDouble(log_term.term_ref, boost::lexical_cast<double>(buf));
			break;
		case pion::platform::Vocabulary::TYPE_LONG_DOUBLE:
			e.setLongDouble(log_term.term_ref, boost::lexical_cast<long double>(buf));
			break;
		case pion::platform::Vocabulary::TYPE_SHORT_STRING:
		case pion::platform::Vocabulary::TYPE_STRING:
		case pion::platform::Vocabulary::TYPE_LONG_STRING:
			if (log_urlencode) {
				std::string temp_str(algo::url_decode(buf));
				e.setString(log_term.term_ref, temp_str);
			} else {
				e.setString(log_term.term_ref, buf);
			}
			break;
		case pion::platform::Vocabulary::TYPE_BLOB:
		case pion::platform::Vocabulary::TYPE_ZBLOB:
			if (log_urlencode) {
				std::string temp_str(algo::url_decode(buf));
				e.setBlob(log_term.term_ref, temp_str);
			} else {
				e.setBlob(log_term.term_ref, buf);
			}
			break;
		case pion::platform::Vocabulary::TYPE_CHAR:
			if (log_urlencode) {
				std::string temp_str(algo::url_decode(buf));
				if (temp_str.size() > log_term.term_size)
					temp_str.resize(log_term.term_size);
				e.setString(log_term.term_ref, temp_str);
			} else if (strlen(buf) > log_term.term_size) {
				e.setString(log_term.term_ref, std::string(buf, log_term.term_size));
			} else {
				e.setString(log_term.term_ref, buf);
			}
			break;
		case pion::platform::Vocabulary::TYPE_DATE_TIME:
		case pion::platform::Vocabulary::TYPE_DATE:
		case pion::platform::Vocabulary::TYPE_TIME:
		{
			PionDateTime dt;
			if (log_urlencode) {
				std::string temp_str(algo::url_decode(buf));
				log_time_facet.fromString(temp_str, dt);
			} else {
				log_time_facet.fromString(buf, dt);
			}
			if (log_do_time_offset) {
				dt += log_time_offset;
			}
			e.setDateTime(log_term.term_ref, dt);
			break;
		}
	}
}


}	// end namespace plugins
}	// end namespace pion

#endif

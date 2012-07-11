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

#ifndef __PION_JSONCODEC_HEADER__
#define __PION_JSONCODEC_HEADER__

#include <vector>
#include <map>
#include <queue>
#include <pion/PionConfig.hpp>
#include <pion/platform/Codec.hpp>
#include <yajl/yajl_gen.h>


namespace pion {		// begin namespace pion
namespace plugins {		// begin namespace plugins


struct Context;

///
/// JSONCodec: used to encode and decode Events using JSON
/// (Work in progress...)
///
class JSONCodec :
	public pion::platform::Codec
{
public:

	// TODO: EmptyFieldException and EmptyTermException should probably be moved
	// to class Codec, and removed from LogCodec.

	/// exception thrown if the Codec configuration includes an empty field name
	class EmptyFieldException : public PionException {
	public:
		EmptyFieldException(const std::string& codec_id)
			: PionException("JSONCodec configuration includes an empty field name: ", codec_id) {}
	};

	/// exception thrown if the Codec configuration does not define a term in a field mapping
	class EmptyTermException : public PionException {
	public:
		EmptyTermException(const std::string& codec_id)
			: PionException("JSONCodec configuration is missing a term identifier: ", codec_id) {}
	};


	/// constructs a new JSONCodec object
	JSONCodec(void)
		: pion::platform::Codec(),
		m_flush_after_write(false), m_yajl_generator(NULL), m_yajl_handle(NULL),
		m_no_events_written(true), m_first_read_attempt(true)
	{}
	
	/// virtual destructor: this class is meant to be extended
	virtual ~JSONCodec() {
		if (m_yajl_generator)
			yajl_gen_free(m_yajl_generator);
		if (m_yajl_handle)
			yajl_free(m_yajl_handle);
	}
	
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
	 * writes the JSON array end token ']' to an output stream and frees the JSON generator
	 *
	 * @param out the output stream to which the token will be written
	 */
	virtual void finish(std::ostream& out);

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
	}

	/// data type used to configure the formatting of Vocabulary Terms
	struct JSONField {
		JSONField(const std::string& f, const pion::platform::Vocabulary::Term& t)
			: field_name(f), term(t)
		{}

		/// the name of the field
		std::string							field_name;

		/// the Vocabulary Term that the data field represents
		pion::platform::Vocabulary::Term	term;

		/// used to encode and decode date_time fields
		PionTimeFacet						time_facet;
	};

	/// data type for a shared pointer to a JSONField object
	typedef boost::shared_ptr<JSONField>	JSONFieldPtr;
	
	/// data type that maps field names to Terms
	typedef std::unordered_map<std::string,
		JSONFieldPtr>					FieldMap;

	/// an ordered list of the fields in the current configuration
	typedef std::vector<JSONFieldPtr>		CurrentFormat;

	/// representation of a JSON object corresponding to an Event
	typedef std::unordered_multimap<pion::platform::Vocabulary::TermRef, std::string>
											JSONObject;

	/// data type for a shared pointer to a JSONObject object
	typedef boost::shared_ptr<JSONObject>	JSONObjectPtr;

	/// queue of parsed JSONObjects 
	typedef std::queue<JSONObjectPtr>		JSONObjectQueue;

private:

	/// traits_type used for the standard char-istream 
	typedef std::istream::traits_type		traits_type;

	/// data type used to represent a standard char-istream streambuf
	typedef std::basic_streambuf<
		std::istream::char_type,
		std::istream::traits_type>	streambuf_type;

	/// data type used to represent an integer value resulting from an istream read
	typedef std::istream::int_type			int_type;

	/**
	 * maps a data field to a Vocabulary Term
	 *
	 * @param field the name of the data field
	 * @param term the Vocabulary Term to map the data field to
	 */
	inline void mapFieldToTerm(const std::string& field,
							   const pion::platform::Vocabulary::Term& term);


	/// content type used by this Codec
	static const std::string		CONTENT_TYPE;

	/// name of the field mapping element for Pion XML config files
	static const std::string		FIELD_ELEMENT_NAME;

	/// name of the Term ID attribute for Pion XML config files
	static const std::string		TERM_ATTRIBUTE_NAME;

	/// maximum size of the read buffer
	static const unsigned int		READ_BUFFER_SIZE;

	/// used by YAJL for indenting JSON output
	static std::string				INDENT_STRING;


	/// used to configure which fields map to Vocabulary Terms (for reading)
	FieldMap						m_field_map;

	/// used to map TermRefs to JSONFieldPtrs
	std::map<pion::platform::Vocabulary::TermRef, JSONFieldPtr>
									m_JSON_field_ptr_map;

	/// represents the sequence of data fields in the current configuration
	CurrentFormat					m_format;

	/// true if the Codec should flush the output stream after each write
	bool							m_flush_after_write;

	/// pointer to JSON generator
	yajl_gen						m_yajl_generator;

	/// pointer to JSON parser
	yajl_handle						m_yajl_handle;

	/// queue of Events parsed by the JSON parser
	JSONObjectQueue					m_json_object_queue;

	/// context passed to the YAJL parser
	boost::shared_ptr<Context>		m_context;

	/// keeps track of whether a first Event has been written yet
	bool							m_no_events_written;

	/// keeps track of whether a first Event has been read yet
	bool							m_first_read_attempt;
};


// inline member functions for JSONCodec

inline void JSONCodec::mapFieldToTerm(const std::string& field,
									  const pion::platform::Vocabulary::Term& term)
{
	if (m_field_map[field])
		throw PionException("Duplicate Field Name");

	// prepare a new JSON field object
	JSONFieldPtr field_ptr(new JSONField(field, term));
	switch (term.term_type) {
		case pion::platform::Vocabulary::TYPE_DATE_TIME:
		case pion::platform::Vocabulary::TYPE_DATE:
		case pion::platform::Vocabulary::TYPE_TIME:
			field_ptr->time_facet.setFormat(term.term_format);
			break;
		default:
			break; // do nothing
	}

	// add it to the mapping of field names
	m_field_map[field] = field_ptr;

	// append the new field to the current format
	m_format.push_back(field_ptr);
}

// TODO: put this somewhere
struct Context {
	Context(const JSONCodec::FieldMap& field_map, JSONCodec::JSONObjectQueue& json_object_queue)
		: field_map(field_map), json_object_queue(json_object_queue)
		, m_array_started(false), m_array_ended(false)
	{}

	const JSONCodec::FieldMap& field_map;
	JSONCodec::JSONObjectQueue& json_object_queue;
	JSONCodec::JSONObjectPtr json_object_ptr;
	pion::platform::Vocabulary::TermRef term_ref;
	bool m_array_started;
	bool m_array_ended;
};

}	// end namespace plugins
}	// end namespace pion

#endif


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

#ifndef __PION_VOCABULARY_HEADER__
#define __PION_VOCABULARY_HEADER__

#include <memory>
#include <string>
#include <vector>
#include <pion/PionConfig.hpp>
#include <pion/PionException.hpp>
#include <pion/PionHashMap.hpp>


namespace pion {		// begin namespace pion
namespace platform {	// begin namespace platform (Pion Platform Library)


///
/// Vocabulary: maps URI identifiers and numeric references to Term information
///
class PION_PLATFORM_API Vocabulary
{
public:

	/// data type for numeric term references
	typedef unsigned long		TermRef;
	
	/// null term reference constant (0)
	static const TermRef		UNDEFINED_TERM_REF;
	
	/// data type for the type of data that a term represents
	enum DataType {
		TYPE_NULL = 0,			///< NULL or undefined type
		TYPE_INT8,				///< 8-bit, signed integer (uses std::int32_t)
		TYPE_UINT8,				///< 8-bit, unsigned integer (uses std::uint32_t)
		TYPE_INT16,				///< 16-bit, signed integer (uses std::int32_t)
		TYPE_UINT16,			///< 16-bit, unsigned integer (uses std::uint32_t)
		TYPE_INT32,				///< 32-bit, signed integer (uses std::int32_t)
		TYPE_UINT32,			///< 32-bit, unsigned integer (uses std::uint32_t)
		TYPE_INT64,				///< 64-bit, signed integer (uses std::int64_t)
		TYPE_UINT64,			///< 64-bit, unsigned integer (uses std::uint64_t)
		TYPE_FLOAT,				///< floating point number (uses float)
		TYPE_DOUBLE,			///< large floating point number (uses double)
		TYPE_LONG_DOUBLE,		///< very large floating point number (uses long double)
		TYPE_SHORT_STRING,		///< variable-length string up to 255 bytes
		TYPE_STRING,			///< variable-length string up to 65535 bytes
		TYPE_LONG_STRING,		///< variable-length string up to 2^32-1 bytes
		TYPE_DATE_TIME,			///< represents a specific date and/or time using term_format
		TYPE_DATE,				///< represents a specific date using term_format
		TYPE_TIME,				///< represents a specific time of day using term_format
		TYPE_CHAR,				///< fixed-length string of size term_size
		TYPE_BLOB,				///< BLOB; like longstring, but may contain NULLs
		TYPE_ZBLOB,				///< ZBLOB; like BLOB except, that it gets compressed in/out a DB
		TYPE_OBJECT				///< object is currently reserved for Event Types
	};
	
	/// data type for vocabulary terms
	struct Term {
		/// default constructor
		Term(void)
			: term_ref(UNDEFINED_TERM_REF),
			term_type(TYPE_NULL), term_size(0)
			{}
		/// construct term with identifier
		Term(const std::string& id)
			: term_id(id), term_ref(UNDEFINED_TERM_REF),
			term_type(TYPE_NULL), term_size(0)
			{}
		/// copy constructor
		Term(const Term& t)
			: term_id(t.term_id), term_ref(t.term_ref),
			term_comment(t.term_comment), term_type(t.term_type),
			term_size(t.term_size), term_format(t.term_format)
			{}
		/// assignment operator
		inline Term& operator=(const Term& t) {
			term_id = t.term_id;
			term_ref = t.term_ref;
			term_comment = t.term_comment;
			term_type = t.term_type;
			term_size = t.term_size;
			term_format = t.term_format;
			return *this;
		}
		/// URI used to uniquely identify the term
		std::string				term_id;
		/// used to reference the term within this Vocabulary object
		TermRef					term_ref;
		/// descriptive comment for the term
		std::string				term_comment;
		/// type of data that the term represents
		DataType				term_type;
		/// maximum length if data type == TYPE_CHAR
		size_t					term_size;
		/// format used for date/time Term types
		std::string				term_format;
	};
	
	/// data type for a pointer to a Term object
	typedef std::shared_ptr<Term>		TermPtr;
	
	/// exception thrown if you try to add a term with an empty identifier
	class EmptyTermIdException : public std::exception {
	public:
		virtual const char* what() const throw() {
			return "Tried adding a vocabulary term with an empty identifier";
		}
	};
	
	/// exception thrown if the DataType is not recognized
	class UnknownDataTypeException : public PionException {
	public:
		UnknownDataTypeException(const std::string& data_type)
			: PionException("Could not parse unknown data type: ", data_type) {}
	};

	/// exception thrown if you try to add a duplicate Term
	class DuplicateTermException : public PionException {
	public:
		DuplicateTermException(const std::string& term_id)
			: PionException("Tried adding a duplicate term to the Vocabulary: ", term_id) {}
	};
	
	/// exception thrown if there is a problem finding a Term
	class TermNotFoundException : public PionException {
	public:
		TermNotFoundException(const std::string& term_id)
			: PionException("Unable to find Term identifier: ", term_id) {}
	};

	/// exception thrown by refreshTerm() if a Term in use is no longer defined
	class TermNoLongerDefinedException : public PionException {
	public:
		TermNoLongerDefinedException(const std::string& term_id)
			: PionException("Term was removed from Vocabulary while still in-use: ", term_id) {}
	};

	
	/// public destructor: not virtual, should not be extended
	~Vocabulary() {}
		
	/// constructs a new Vocabulary instance
	Vocabulary(void);
	
	/// (copy) constructs a new Vocabulary instance
	Vocabulary(const Vocabulary& v);
	
	/// returns the number of Terms that are defined within the Vocabulary
	inline size_t size(void) const { return m_num_terms; }	
	
	/**
	 * returns a reference to the definition for a particular Term
	 * 
	 * @param term_ref reference number assigned to the Term
	 * @return const Term& reference to the Term's definition
	 */
	inline const Term& operator[](const TermRef& term_ref) const {
		PION_ASSERT(term_ref <= m_num_terms);
		return *(m_ref_map[term_ref]);
	}
	
	/**
	 * returns the numeric Term identifier for a Term
	 * 
	 * @param term_id unique identifier for the Term
	 * @return const TermRef& the numeric reference assigned to the Term
	 */
	inline TermRef findTerm(const std::string& term_id) const {
		TermRef term_ref = UNDEFINED_TERM_REF;
		TermStringMap::const_iterator i = m_uri_map.find(term_id);
		if (i != m_uri_map.end())
			term_ref = i->second->term_ref;
		return term_ref;
	}

	/**
	 * returns the Term, identified with the TermRef
	 *
	 * @param term_ref term_ref of the Term to be found
	 * @return const Term& the full term
	 */
	inline const Term& findTerm(const TermRef& term_ref) const {
		return *m_ref_map[(term_ref > m_num_terms ? UNDEFINED_TERM_REF : term_ref)];
	}

	/**
	 * adds a new Term if it has not yet been defined
	 *
	 * @param t the Term to identify or define
	 * @return const TermRef& the reference number assigned to the new Term
	 */
	TermRef addTerm(const Term& t);
	
	/**
	 * removes a Term from the Vocabulary (use with caution!!!)
	 *
	 * @param term_id unique identifier for the Term to remove
	 */
	void removeTerm(const std::string& term_id);
	
	/**
	 * update the settings for a Term
	 *
	 * @param t the Term to update (t.term_id is used to find the Term to change)
	 */
	void updateTerm(const Term& t);

	/**
	 * refresh a Term copy after Vocabulary was updated
	 *
	 * @param t the Term to refresh (t.term_id is used to find the updated Term)
	 */
	void refreshTerm(Term& t) const;
	
	/**
	 * Incorporates all the data from another Vocabulary into this one
	 *
	 * @param v the Vocabulary object copy data from
	 *
	 * @return const Vocabulary& reference to this Vocabulary object
	 */
	const Vocabulary& operator+=(const Vocabulary& v);
	
	/**
	 * parses data type from a string
	 *
	 * @param str the string to parse
	 * @return DataType the type matching the parsed string
	 */
	static DataType parseDataType(std::string str);
	
	/**
	 * returns a string that represents a particular data type
	 *
	 * @param type the data type to get a string for
	 * @return std::string a temporary string object that represents the data type
	 */
	static std::string getDataTypeAsString(const DataType type);
	
	
private:

	/// Vocabulary assignment is not allowed: use operator+=() instead
	Vocabulary& operator=(const Vocabulary& v);


	/// data type that maps strings to Term definition objects
	typedef std::unordered_map<std::string, TermPtr>	TermStringMap;
	
	/// data type that maps Term reference numbers to Term definition objects
	typedef std::vector<TermPtr>					TermRefMap;
	
	
	/// used to map Term reference numbers to Term definition objects
	TermRefMap						m_ref_map;

	/// used to map URI Term identifiers to Term definition objects
	TermStringMap					m_uri_map;
	
	/// number of Terms that are defined within the Vocabulary
	TermRef							m_num_terms;
};


/// data type for a pointer to a Vocabulary object
typedef std::shared_ptr<Vocabulary>	VocabularyPtr;


}	// end namespace platform
}	// end namespace pion

#endif

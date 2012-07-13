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

#ifndef __PION_COMPARISON_HEADER__
#define __PION_COMPARISON_HEADER__

#include <memory>
#include <boost/regex.hpp>
#include <boost/regex/icu.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/algorithm/string/compare.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/tuple/tuple.hpp>
#include <pion/PionConfig.hpp>
#include <pion/PionException.hpp>
#include <pion/PionLogger.hpp>
#include <pion/platform/Vocabulary.hpp>
#include <pion/platform/Event.hpp>
#include <unicode/uiter.h>
#include <unicode/ustring.h>
#include <unicode/stsearch.h>

namespace pion {		// begin namespace pion
namespace platform {	// begin namespace platform (Pion Platform Library)

///
/// Comparison: used to perform a comparison on the value of a Term
///
class PION_PLATFORM_API Comparison
{
public:

	/// data type representing the type of comparison operation to perform
	enum ComparisonType {
		TYPE_FALSE = 0,						// always false
		TYPE_TRUE,							// always true
		TYPE_IS_DEFINED,					// true if at least one value is defined
		TYPE_IS_NOT_DEFINED,				// true if no values are defined
		// numeric operations
		TYPE_EQUALS,
		TYPE_NOT_EQUALS,
		TYPE_GREATER_THAN,
		TYPE_LESS_THAN,
		TYPE_GREATER_OR_EQUAL,
		TYPE_LESS_OR_EQUAL,
		// string operations
		TYPE_EXACT_MATCH,
		TYPE_NOT_EXACT_MATCH,
		TYPE_CONTAINS,
		TYPE_NOT_CONTAINS,
		TYPE_STARTS_WITH,
		TYPE_NOT_STARTS_WITH,
		TYPE_ENDS_WITH,
		TYPE_NOT_ENDS_WITH,
		TYPE_ORDERED_BEFORE,
		TYPE_NOT_ORDERED_BEFORE,
		TYPE_ORDERED_AFTER,
		TYPE_NOT_ORDERED_AFTER,
		TYPE_REGEX,
		TYPE_NOT_REGEX,
		// string operations using UCOL_PRIMARY
		TYPE_EXACT_MATCH_PRIMARY,
		TYPE_NOT_EXACT_MATCH_PRIMARY,
		TYPE_CONTAINS_PRIMARY,
		TYPE_NOT_CONTAINS_PRIMARY,
		TYPE_STARTS_WITH_PRIMARY,
		TYPE_NOT_STARTS_WITH_PRIMARY,
		TYPE_ENDS_WITH_PRIMARY,
		TYPE_NOT_ENDS_WITH_PRIMARY,
		TYPE_ORDERED_BEFORE_PRIMARY,
		TYPE_NOT_ORDERED_BEFORE_PRIMARY,
		TYPE_ORDERED_AFTER_PRIMARY,
		TYPE_NOT_ORDERED_AFTER_PRIMARY,
		// date_time operations
		TYPE_SAME_DATE_TIME,
		TYPE_NOT_SAME_DATE_TIME,
		TYPE_EARLIER_DATE_TIME,
		TYPE_LATER_DATE_TIME,
		TYPE_SAME_OR_EARLIER_DATE_TIME,
		TYPE_SAME_OR_LATER_DATE_TIME,
		// date operations
		TYPE_SAME_DATE,
		TYPE_NOT_SAME_DATE,
		TYPE_EARLIER_DATE,
		TYPE_LATER_DATE,
		TYPE_SAME_OR_EARLIER_DATE,
		TYPE_SAME_OR_LATER_DATE,
		// time of day operations
		TYPE_SAME_TIME,
		TYPE_NOT_SAME_TIME,
		TYPE_EARLIER_TIME,
		TYPE_LATER_TIME,
		TYPE_SAME_OR_EARLIER_TIME,
		TYPE_SAME_OR_LATER_TIME,
		// Update LAST_COMPARISON_TYPE if adding types after TYPE_SAME_OR_LATER_TIME.
		LAST_COMPARISON_TYPE = TYPE_SAME_OR_LATER_TIME
	};

	/// exception thrown if the Comparison type is not recognized
	class UnknownComparisonTypeException : public PionException {
	public:
		UnknownComparisonTypeException(const std::string& comparison_type)
			: PionException("Could not parse unknown comparison type: ", comparison_type) {}
	};

	/// exception thrown if an invalid comparison was evaluated
	class InvalidComparisonException : public std::exception {
	public:
		virtual const char* what() const throw() {
			return "An invalid comparison was attempted";
		}
	};

	/// exception thrown if an invalid comparison type is given for a Vocabulary Term
	class InvalidTypeForTermException : public std::exception {
	public:
		virtual const char* what() const throw() {
			return "Invalid comparison type given for Vocabulary Term";
		}
	};

	/// exception thrown if an invalid value is given for the type of comparison
	class InvalidValueForTypeException : public std::exception {
	public:
		virtual const char* what() const throw() {
			return "Invalid value given for comparison type";
		}
	};

	/// exception thrown if boost::u32regex_search fails and throws
	class RegexFailure : public PionException {
	public:
		RegexFailure(const std::string& search_str)
			: PionException("str = " + search_str) {}
	};

	/// exception thrown if an ICU function returns an unexpected error code
	class UnexpectedICUErrorCodeException : public PionException {
	public:
		UnexpectedICUErrorCodeException(const std::string& icu_func, const std::string& u_error_code_str)
			: PionException("Unexpected ICU error code in Comparison: ", icu_func + " returned " + u_error_code_str) {}
	};

	/// exception thrown when an std::bad_alloc exception is caught
	class BadAllocException : public PionException {
	public:
		BadAllocException(const std::string& error_msg)
			: PionException("std::bad_alloc in Comparison: ", error_msg) {}
	};


	/// virtual destructor: you may extend this class
	virtual ~Comparison() {}

	/**
	 * constructs a new Comparison
	 *
	 * @param term the term that will be examined
	 */
	explicit Comparison(const Vocabulary::Term& term, bool debug_mode = false)
		: m_term(term), m_type(TYPE_FALSE), m_match_all_values(false),
		  m_running(true), m_debug_mode(debug_mode),
		  m_logger(PION_GET_LOGGER("pion.platform.Comparison"))
	{}

	/// standard copy constructor
	Comparison(const Comparison& c)
		: m_term(c.m_term), m_type(c.m_type), m_value(c.m_value),
		m_str_value(c.m_str_value), m_comparison_func(c.m_comparison_func), 
		m_regex(c.m_regex), m_regex_str(c.m_regex_str),
		m_match_all_values(c.m_match_all_values), 
		m_running(c.m_running), m_debug_mode(c.m_debug_mode),
		m_logger(c.m_logger)
	{}

	/**
	 * evaluates the result of the Comparison
	 *
	 * @param e the Event to evaluate
	 *
	 * @return 	true if the Comparison succeeded; false if it did not
	 */
	inline bool evaluate(const Event& e);

	/**
	 * evaluates the result of the Comparison
	 *
	 * @param values_range range of values (within an event) to evaluate
	 *
	 * @return 	true if the Comparison succeeded; false if it did not
	 */
	inline bool evaluateRange(const Event::ValuesRange& values_range);

	/**
	 * configures the Comparison information
	 *
	 * @param the type of Comparison to perform
	 * @param value the value that the Vocabulary Term is compared to
	 * @param match_all_values if true, all values of the Vocabulary Term must match
	 */
	template <typename T>
	inline void configure(const ComparisonType type,
						  const T& value,
						  const bool match_all_values = false);

	/**
	 * configures the Comparison information (alternate form for string comparisons)
	 *
	 * @param the type of Comparison to perform
	 * @param value the value that the Vocabulary Term is compared to
	 * @param match_all_values if true, all values of the Vocabulary Term must match
	 */
	inline void configure(const ComparisonType type, const char *value,
						  const bool match_all_values = false)
	{
		std::string value_str(value);
		configure(type, value_str, match_all_values);
	}

	/**
	 * configures the Comparison information (alternate form for string comparisons)
	 *
	 * @param the type of Comparison to perform
	 * @param value the value that the Vocabulary Term is compared to
	 * @param match_all_values if true, all values of the Vocabulary Term must match
	 */
	void configure(const ComparisonType type, const std::string &value,
				   const bool match_all_values = false);

	/**
	 * configures the Comparison information (alternate form comparisons that have not value)
	 *
	 * @param the type of Comparison to perform
	 */
	void configure(const ComparisonType type);

	/**
	 * this updates the Vocabulary information used by this Comparison;
	 * it should be called whenever the global Vocabulary is updated
	 *
	 * @param v the Vocabulary that this Comparison will use to describe Terms
	 */
	void updateVocabulary(const Vocabulary& v);


	/// returns the Vocabulary Term to examine
	inline const Vocabulary::Term& getTerm(void) const { return m_term; }

	/// returns the type of Comparison that this is
	inline ComparisonType getType(void) const { return m_type; }

	/// returns the value that the Vocabulary Term is compared to
	inline const Event::ParameterValue& getValue(void) const { return m_value; }

	/// returns true if all Vocabulary Term values must match
	inline bool getMatchAllValues(void) const { return m_match_all_values; }

	/// returns the compiled (or empty) regular expression
	inline const boost::u32regex& getRegex(void) const { return m_regex; }

	/// returns the original string that the regular expression was constructed from
	inline const std::string& getRegexStr(void) const { return m_regex_str; }

	/**
	 * parses Comparison type from a string
	 *
	 * @param str the string to parse
	 * @return ComparisonType the type matching the parsed string
	 */
	static ComparisonType parseComparisonType(std::string str);

	/**
	 * returns a string that represents a particular Comparison type
	 *
	 * @param comparison_type the Comparison type to get a string for
	 * @return std::string a temporary string object that represents the Comparison type
	 */
	static std::string getComparisonTypeAsString(const ComparisonType comparison_type);

	/// returns true for comparison types which require a value, i.e. which have arity > 1
	static bool requiresValue(ComparisonType t);

	static void writeComparisonsXML(std::ostream& out);

	inline bool isRunning(void) const { return m_running; }

private:

	/// helper class used to determine if two numbers are equal
	template <typename T>
	class CompareEquals {
	public:
		CompareEquals(const Event::ParameterValue& value) : m_value(boost::get<const T&>(value)) {}
		inline bool operator()(const Event::ParameterValue& event_value) const {
			return boost::get<const T&>(event_value) == m_value;
		}
	private:
		const T&	m_value;
	};

	/// helper class used to determine if one number is greater than another
	template <typename T>
	class CompareGreaterThan {
	public:
		CompareGreaterThan(const Event::ParameterValue& value) : m_value(boost::get<const T&>(value)) {}
		inline bool operator()(const Event::ParameterValue& event_value) const {
			return boost::get<const T&>(event_value) > m_value;
		}
	private:
		const T&	m_value;
	};

	/// helper class used to determine if one number is less than another
	template <typename T>
	class CompareLessThan {
	public:
		CompareLessThan(const Event::ParameterValue& value) : m_value(boost::get<const T&>(value)) {}
		inline bool operator()(const Event::ParameterValue& event_value) const {
			return boost::get<const T&>(event_value) < m_value;
		}
	private:
		const T&	m_value;
	};

	/// helper class used to determine if one number is greater than or equal to another
	template <typename T>
	class CompareGreaterOrEqual {
	public:
		CompareGreaterOrEqual(const Event::ParameterValue& value) : m_value(boost::get<const T&>(value)) {}
		inline bool operator()(const Event::ParameterValue& event_value) const {
			return boost::get<const T&>(event_value) >= m_value;
		}
	private:
		const T&	m_value;
	};

	/// helper class used to determine if one number is less than or equal to another
	template <typename T>
	class CompareLessOrEqual {
	public:
		CompareLessOrEqual(const Event::ParameterValue& value) : m_value(boost::get<const T&>(value)) {}
		inline bool operator()(const Event::ParameterValue& event_value) const {
			return boost::get<const T&>(event_value) <= m_value;
		}
	private:
		const T&	m_value;
	};

	class ComparisonFunctor {
	public:
		ComparisonFunctor(PionLogger& logger, const std::string& value, UColAttributeValue attr);
		virtual ~ComparisonFunctor();

		virtual bool operator()(const Event::ParameterValue& event_value) const = 0;

	protected:
		int32_t				m_pattern_buf_len;
		UChar*				m_pattern_buf;
		UCollator*			m_collator;
		mutable PionLogger	m_logger;
	};

	/// helper class used to determine if one string matches another
	class CompareStringExactMatch : public ComparisonFunctor {
	public:
		CompareStringExactMatch(PionLogger& logger, const std::string& value, UColAttributeValue attr = UCOL_DEFAULT);
		~CompareStringExactMatch() {}

		virtual bool operator()(const Event::ParameterValue& event_value) const {
			const Event::BlobType& blob = boost::get<const Event::BlobType&>(event_value);

			// Handle cases where at least one of the two strings is empty.
			if (m_pattern_buf_len == 0)
				return blob.size() == 0; // A string is an exact match for an empty string iff it is empty.
			else if (blob.size() == 0)
				return false; // A non-empty string can't exactly match an empty string.

			UCharIterator text_iter, pattern_iter;
			uiter_setUTF8(&text_iter, blob.get(), blob.size());
			uiter_setString(&pattern_iter, m_pattern_buf, m_pattern_buf_len);
			UErrorCode u_error_code = U_ZERO_ERROR;
			UCollationResult result = ucol_strcollIter(m_collator, &text_iter, &pattern_iter, &u_error_code);
			if (U_FAILURE(u_error_code)) {
				PION_LOG_ERROR(m_logger, "ucol_strcollIter() returned error in CompareStringExactMatch: " << u_errorName(u_error_code));
				return false;
			}
			return (result == UCOL_EQUAL);
		}
	};

	/// helper class used to determine if one string contains another
	class CompareStringContains : public ComparisonFunctor {
	public:
		CompareStringContains(PionLogger& logger, const std::string& value, UColAttributeValue attr = UCOL_DEFAULT);
		~CompareStringContains() {}

		virtual bool operator()(const Event::ParameterValue& event_value) const {
			//// TODO: It would be nice to create a code unit iterator here instead of a UnicodeString, as in 
			//// CompareStringStartsWith and CompareStringEndsWith, to avoid having to convert the entire blob upfront.
			//// Unfortunately, the only type of iterator a StringSearch will take is a CharacterIterator,
			//// whereas the only type of code unit iterator ICU provides that can wrap UTF-8 data is UCharIterator.
			//// The ICU documentation states that it is possible to create a CharacterIterator subclass for UTF-8 strings,
			//// but suggests that it's not trivial.  See http://userguide.icu-project.org/strings/utf-8.
			//UnicodeString text = UnicodeString::fromUTF8(boost::get<const Event::BlobType&>(event_value).get());

			const Event::BlobType& blob = boost::get<const Event::BlobType&>(event_value);

			// Handle cases where at least one of the two strings is empty.
			if (m_pattern_buf_len == 0)
				return true; // This is a reasonable response for this degenerate case.
			else if (blob.size() == 0)
				return false; // An empty string can't contain a non-empty string.

			int32_t text_buf_len;
			UErrorCode u_error_code = U_ZERO_ERROR;
			u_strFromUTF8(NULL, 0, &text_buf_len, blob.get(), blob.size(), &u_error_code);
			if (U_FAILURE(u_error_code) && u_error_code != U_BUFFER_OVERFLOW_ERROR) {
				PION_LOG_ERROR(m_logger, "u_strFromUTF8() returned error in CompareStringContains: " << u_errorName(u_error_code));
				return false;
			}

			std::unique_ptr<UChar[]> text_buf;
			try {
				text_buf.reset(new UChar[text_buf_len]);
			} catch (std::bad_alloc& e) {
				PION_LOG_ERROR(m_logger, "text_buf_len: " << text_buf_len << " - " << e.what() << " - rethrowing");
				throw BadAllocException("text_buf_len = " + boost::lexical_cast<std::string>(text_buf_len));
			}

			u_error_code = U_ZERO_ERROR; // Need to reset, because u_strFromUTF8 returns U_BUFFER_OVERFLOW_ERROR when destCapacity = 0.
			u_strFromUTF8(text_buf.get(), text_buf_len, NULL, blob.get(), blob.size(), &u_error_code);
			// Use u_strFromUTF8Lenient instead?
			if (U_FAILURE(u_error_code)) {
				PION_LOG_ERROR(m_logger, "u_strFromUTF8() returned error in CompareStringContains: " << u_errorName(u_error_code));
				return false;
			}

			UStringSearch* ss = usearch_openFromCollator(m_pattern_buf, m_pattern_buf_len, text_buf.get(), text_buf_len, m_collator, NULL, &u_error_code);
			if (U_FAILURE(u_error_code)) {
				PION_LOG_ERROR(m_logger, "usearch_openFromCollator() returned error in CompareStringContains: " << u_errorName(u_error_code));
				return false;
			}

			int pos = usearch_first(ss, &u_error_code);
			if (U_FAILURE(u_error_code)) {
				PION_LOG_ERROR(m_logger, "usearch_first() returned error in CompareStringContains: " << u_errorName(u_error_code));
				usearch_close(ss);
				return false;
			}

			usearch_close(ss);
			return (pos != USEARCH_DONE);
		}
	};

	/// helper class used to determine if one string starts with another
	class CompareStringStartsWith : public ComparisonFunctor {
	public:
		CompareStringStartsWith(PionLogger& logger, const std::string& value, UColAttributeValue attr = UCOL_DEFAULT);
		~CompareStringStartsWith();

		virtual bool operator()(const Event::ParameterValue& event_value) const {
			const Event::BlobType& blob = boost::get<const Event::BlobType&>(event_value);

			// Handle cases where at least one of the two strings is empty.
			if (m_pattern_buf_len == 0)
				return true; // This is a reasonable response for this degenerate case.
			else if (blob.size() == 0)
				return false; // An empty string can't start with a non-empty string.

			// Create a code unit iterator from the Event value.
			UCharIterator text_iter;
			uiter_setUTF8(&text_iter, blob.get(), blob.size());

			// Populate a buffer by parsing UTF-8 bytes from the blob into code units, until the number of code units is the same as in the pattern.
			std::unique_ptr<UChar[]> text_prefix_buf;
			try {
				text_prefix_buf.reset(new UChar[m_pattern_buf_len]);
			} catch (std::bad_alloc& e) {
				PION_LOG_ERROR(m_logger, "m_pattern_buf_len: " << m_pattern_buf_len << " - " << e.what() << " - rethrowing");
				throw BadAllocException("m_pattern_buf_len = " + boost::lexical_cast<std::string>(m_pattern_buf_len));
			}
			for (int i = 0; i < m_pattern_buf_len; ++i) {
				UChar32 c = text_iter.next(&text_iter);
				if (c == U_SENTINEL) {
					// If the iteration failed, the text is too short to start with the pattern, so return false.
					return false;
				}
				text_prefix_buf[i] = c;
			}

			UCollationResult result = ucol_strcoll(m_collator, text_prefix_buf.get(), m_pattern_buf_len, m_pattern_buf, m_pattern_buf_len);
			return (result == UCOL_EQUAL);
		}
	};

	/// helper class used to determine if one string ends with another
	class CompareStringEndsWith : public ComparisonFunctor {
	public:
		CompareStringEndsWith(PionLogger& logger, const std::string& value, UColAttributeValue attr = UCOL_DEFAULT);
		~CompareStringEndsWith() {}

		virtual bool operator()(const Event::ParameterValue& event_value) const {
			const Event::BlobType& blob = boost::get<const Event::BlobType&>(event_value);

			// Handle cases where at least one of the two strings is empty.
			if (m_pattern_buf_len == 0)
				return true; // This is a reasonable response for this degenerate case.
			else if (blob.size() == 0)
				return false; // An empty string can't end with a non-empty string.

			// Create a code unit iterator from the Event value.
			UCharIterator text_iter;
			uiter_setUTF8(&text_iter, blob.get(), blob.size());

			// Try to iterate back m_pattern_buf_len code units from the end (where m_pattern_buf_len is the number of code units in the pattern).
			int32_t p = text_iter.move(&text_iter, -m_pattern_buf_len, UITER_LIMIT);

			// If the iteration failed, the text is too short to end with the pattern, so return false.
			if (p == U_SENTINEL)
				return false;

			// Compare the last m_pattern_buf_len code units of the text with all m_pattern_buf_len code units of the pattern.
			UCharIterator pattern_iter;
			uiter_setString(&pattern_iter, m_pattern_buf, m_pattern_buf_len);
			UErrorCode u_error_code = U_ZERO_ERROR;
			UCollationResult result = ucol_strcollIter(m_collator, &text_iter, &pattern_iter, &u_error_code);
			if (U_FAILURE(u_error_code)) {
				PION_LOG_ERROR(m_logger, "ucol_strcollIter() returned error in CompareStringEndsWith: " << u_errorName(u_error_code));
				return false;
			}
			return (result == UCOL_EQUAL);
		}
	};

	/// helper class used to determine if one string is ordered before another
	class CompareStringOrderedBefore : public ComparisonFunctor {
	public:
		CompareStringOrderedBefore(PionLogger& logger, const std::string& value, UColAttributeValue attr = UCOL_DEFAULT);
		~CompareStringOrderedBefore() {}

		virtual bool operator()(const Event::ParameterValue& event_value) const {
			const Event::BlobType& blob = boost::get<const Event::BlobType&>(event_value);

			// Handle cases where at least one of the two strings is empty.
			if (m_pattern_buf_len == 0)
				return false; // Nothing is ordered before an empty string.
			else if (blob.size() == 0)
				return true; // An empty string is ordered before any non-empty string.

			UCharIterator text_iter, pattern_iter;
			uiter_setUTF8(&text_iter, blob.get(), blob.size());
			uiter_setString(&pattern_iter, m_pattern_buf, m_pattern_buf_len);
			UErrorCode u_error_code = U_ZERO_ERROR;
			UCollationResult result = ucol_strcollIter(m_collator, &text_iter, &pattern_iter, &u_error_code);
			if (U_FAILURE(u_error_code)) {
				PION_LOG_ERROR(m_logger, "ucol_strcollIter() returned error in CompareStringOrderedBefore: " << u_errorName(u_error_code));
				return false;
			}
			return (result == UCOL_LESS);
		}
	};

	/// helper class used to determine if one string is ordered after another
	class CompareStringOrderedAfter : public ComparisonFunctor {
	public:
		CompareStringOrderedAfter(PionLogger& logger, const std::string& value, UColAttributeValue attr = UCOL_DEFAULT);
		~CompareStringOrderedAfter() {}

		virtual bool operator()(const Event::ParameterValue& event_value) const {
			const Event::BlobType& blob = boost::get<const Event::BlobType&>(event_value);

			// Handle cases where at least one of the two strings is empty.
			if (blob.size() == 0)
				return false; // An empty string is not ordered after anything.
			else if (m_pattern_buf_len == 0)
				return true; // A non-empty string comes after an empty string.

			UCharIterator text_iter, pattern_iter;
			uiter_setUTF8(&text_iter, blob.get(), blob.size());
			uiter_setString(&pattern_iter, m_pattern_buf, m_pattern_buf_len);
			UErrorCode u_error_code = U_ZERO_ERROR;
			UCollationResult result = ucol_strcollIter(m_collator, &text_iter, &pattern_iter, &u_error_code);
			if (U_FAILURE(u_error_code)) {
				PION_LOG_ERROR(m_logger, "ucol_strcollIter() returned error in CompareStringOrderedAfter: " << u_errorName(u_error_code));
				return false;
			}
			return (result == UCOL_GREATER);
		}
	};

	/// helper class used to determine if a string matches a regular expression
	class CompareStringRegex {
	public:
		CompareStringRegex(const boost::u32regex& value) : m_regex(value) {}
		inline bool operator()(const Event::ParameterValue& event_value) const {
			try {
				// note: regex_match must match the ENTIRE string; use regex_search
				// instead to match any part of the string
				return boost::u32regex_search(boost::get<const Event::BlobType&>(event_value).get(), m_regex);
			} catch (...) {
				throw RegexFailure(boost::get<const Event::BlobType&>(event_value).get());
			}
		}
	private:
		const boost::u32regex&	m_regex;
	};

	/// helper class used to determine if two date_time values are equivalent
	class CompareSameDateTime {
	public:
		CompareSameDateTime(const Event::ParameterValue& value) : m_value(boost::get<const PionDateTime&>(value)) {}
		inline bool operator()(const Event::ParameterValue& event_value) const {
			return boost::get<const PionDateTime&>(event_value) == m_value;
		}
	private:
		const PionDateTime&	m_value;
	};

	/// helper class used to determine if one date_time value is earlier than another
	class CompareEarlierDateTime {
	public:
		CompareEarlierDateTime(const Event::ParameterValue& value) : m_value(boost::get<const PionDateTime&>(value)) {}
		inline bool operator()(const Event::ParameterValue& event_value) const {
			return boost::get<const PionDateTime&>(event_value) < m_value;
		}
	private:
		const PionDateTime&	m_value;
	};

	/// helper class used to determine if one date_time value is later than another
	class CompareLaterDateTime {
	public:
		CompareLaterDateTime(const Event::ParameterValue& value) : m_value(boost::get<const PionDateTime&>(value)) {}
		inline bool operator()(const Event::ParameterValue& event_value) const {
			return boost::get<const PionDateTime&>(event_value) > m_value;
		}
	private:
		const PionDateTime&	m_value;
	};

	/// helper class used to determine if one date_time value is the same as or earlier than another
	class CompareSameOrEarlierDateTime {
	public:
		CompareSameOrEarlierDateTime(const Event::ParameterValue& value) : m_value(boost::get<const PionDateTime&>(value)) {}
		inline bool operator()(const Event::ParameterValue& event_value) const {
			return boost::get<const PionDateTime&>(event_value) <= m_value;
		}
	private:
		const PionDateTime&	m_value;
	};

	/// helper class used to determine if one date_time value is the same as or later than another
	class CompareSameOrLaterDateTime {
	public:
		CompareSameOrLaterDateTime(const Event::ParameterValue& value) : m_value(boost::get<const PionDateTime&>(value)) {}
		inline bool operator()(const Event::ParameterValue& event_value) const {
			return boost::get<const PionDateTime&>(event_value) >= m_value;
		}
	private:
		const PionDateTime&	m_value;
	};

	/// helper class used to determine if two dates are equivalent
	class CompareSameDate {
	public:
		CompareSameDate(const Event::ParameterValue& value) : m_value(boost::get<const PionDateTime&>(value)) {}
		inline bool operator()(const Event::ParameterValue& event_value) const {
			return boost::get<const PionDateTime&>(event_value).date() == m_value.date();
		}
	private:
		const PionDateTime&	m_value;
	};

	/// helper class used to determine if one date is earlier than another
	class CompareEarlierDate {
	public:
		CompareEarlierDate(const Event::ParameterValue& value) : m_value(boost::get<const PionDateTime&>(value)) {}
		inline bool operator()(const Event::ParameterValue& event_value) const {
			return boost::get<const PionDateTime&>(event_value).date() < m_value.date();
		}
	private:
		const PionDateTime&	m_value;
	};

	/// helper class used to determine if one date is later than another
	class CompareLaterDate {
	public:
		CompareLaterDate(const Event::ParameterValue& value) : m_value(boost::get<const PionDateTime&>(value)) {}
		inline bool operator()(const Event::ParameterValue& event_value) const {
			return boost::get<const PionDateTime&>(event_value).date() > m_value.date();
		}
	private:
		const PionDateTime&	m_value;
	};

	/// helper class used to determine if one date is the same as or earlier than another
	class CompareSameOrEarlierDate {
	public:
		CompareSameOrEarlierDate(const Event::ParameterValue& value) : m_value(boost::get<const PionDateTime&>(value)) {}
		inline bool operator()(const Event::ParameterValue& event_value) const {
			return boost::get<const PionDateTime&>(event_value).date() <= m_value.date();
		}
	private:
		const PionDateTime&	m_value;
	};

	/// helper class used to determine if one date is the same as or later than another
	class CompareSameOrLaterDate {
	public:
		CompareSameOrLaterDate(const Event::ParameterValue& value) : m_value(boost::get<const PionDateTime&>(value)) {}
		inline bool operator()(const Event::ParameterValue& event_value) const {
			return boost::get<const PionDateTime&>(event_value).date() >= m_value.date();
		}
	private:
		const PionDateTime&	m_value;
	};

	/// helper class used to determine if two times of day are equivalent
	class CompareSameTime {
	public:
		CompareSameTime(const Event::ParameterValue& value) : m_value(boost::get<const PionDateTime&>(value)) {}
		inline bool operator()(const Event::ParameterValue& event_value) const {
			return boost::get<const PionDateTime&>(event_value).time_of_day() == m_value.time_of_day();
		}
	private:
		const PionDateTime&	m_value;
	};

	/// helper class used to determine if one time of day is earlier than another
	class CompareEarlierTime {
	public:
		CompareEarlierTime(const Event::ParameterValue& value) : m_value(boost::get<const PionDateTime&>(value)) {}
		inline bool operator()(const Event::ParameterValue& event_value) const {
			return boost::get<const PionDateTime&>(event_value).time_of_day() < m_value.time_of_day();
		}
	private:
		const PionDateTime&	m_value;
	};

	/// helper class used to determine if one time of day is later than another
	class CompareLaterTime {
	public:
		CompareLaterTime(const Event::ParameterValue& value) : m_value(boost::get<const PionDateTime&>(value)) {}
		inline bool operator()(const Event::ParameterValue& event_value) const {
			return boost::get<const PionDateTime&>(event_value).time_of_day() > m_value.time_of_day();
		}
	private:
		const PionDateTime&	m_value;
	};

	/// helper class used to determine if one time of day is the same as or earlier than another
	class CompareSameOrEarlierTime {
	public:
		CompareSameOrEarlierTime(const Event::ParameterValue& value) : m_value(boost::get<const PionDateTime&>(value)) {}
		inline bool operator()(const Event::ParameterValue& event_value) const {
			return boost::get<const PionDateTime&>(event_value).time_of_day() <= m_value.time_of_day();
		}
	private:
		const PionDateTime&	m_value;
	};

	/// helper class used to determine if one time of day is the same as or later than another
	class CompareSameOrLaterTime {
	public:
		CompareSameOrLaterTime(const Event::ParameterValue& value) : m_value(boost::get<const PionDateTime&>(value)) {}
		inline bool operator()(const Event::ParameterValue& event_value) const {
			return boost::get<const PionDateTime&>(event_value).time_of_day() >= m_value.time_of_day();
		}
	private:
		const PionDateTime&	m_value;
	};


	/**
	 * checks if a given comparison type is valid for the Vocabulary Term
	 *
	 * @param type the comparison type to check
	 *
	 * @return true if the type is valid for the Vocabulary Term
	 */
	bool checkForValidType(const ComparisonType type) const;

	/**
	 * helper function that compares a range of values using the provided function
	 *
	 * @param comparison_func a function used to compare values
	 * @param values_range the range of values to compare
	 *
	 * @return tuple, with bool for success, and Event:Iterator
	 *			true if the comparisons are successful; false if they are not
	 *			Iterator points to the last compared value/node
	 */
	template <typename ComparisonFunction>
	inline bool checkComparison(const ComparisonFunction& comparison_func,
								const Event::ValuesRange& values_range) const;

	/// identifies the Vocabulary Term to examine
	Vocabulary::Term			m_term;

	/// identifies what type of Comparison this is
	ComparisonType				m_type;

	/// the value that the Vocabulary Term is compared to (if not string or regex type)
	Event::ParameterValue		m_value;

	/// the string that the Vocabulary Term is compared to (if string comparison type)
	std::string					m_str_value;

	std::shared_ptr<ComparisonFunctor>
								m_comparison_func;

	/// the regex that the Vocabulary Term is compared to (if regex comparison type)
	boost::u32regex				m_regex;

	/// the original string that the regex was constructed from
	std::string					m_regex_str;

	/// true if all values for the Vocabulary Term must match
	bool						m_match_all_values;

	/// Is this rule running?
	bool						m_running;

	/// true if running in "debug mode"
	bool						m_debug_mode;

	/// primary logging interface used by this class
	mutable PionLogger			m_logger;
};



template <typename T>
inline void Comparison::configure(const ComparisonType type,
								  const T& value,
								  const bool match_all_values)
{
	if (! checkForValidType(type))
		throw InvalidTypeForTermException();

	m_type = type;
	m_value = value;
	m_match_all_values = match_all_values;

	// make sure the value matches the comparison type
	switch (m_term.term_type) {
		case Vocabulary::TYPE_NULL:
		case Vocabulary::TYPE_OBJECT:
			throw InvalidValueForTypeException();
			break;
		case Vocabulary::TYPE_INT8:
		case Vocabulary::TYPE_INT16:
		case Vocabulary::TYPE_INT32:
			if (boost::get<std::int32_t>(&m_value) == NULL)
				throw InvalidValueForTypeException();
			break;
		case Vocabulary::TYPE_UINT8:
		case Vocabulary::TYPE_UINT16:
		case Vocabulary::TYPE_UINT32:
			if (boost::get<std::uint32_t>(&m_value) == NULL)
				throw InvalidValueForTypeException();
			break;
		case Vocabulary::TYPE_INT64:
			if (boost::get<std::int64_t>(&m_value) == NULL)
				throw InvalidValueForTypeException();
			break;
		case Vocabulary::TYPE_UINT64:
			if (boost::get<std::uint64_t>(&m_value) == NULL)
				throw InvalidValueForTypeException();
			break;
		case Vocabulary::TYPE_FLOAT:
			if (boost::get<float>(&m_value) == NULL)
				throw InvalidValueForTypeException();
			break;
		case Vocabulary::TYPE_DOUBLE:
			if (boost::get<double>(&m_value) == NULL)
				throw InvalidValueForTypeException();
			break;
		case Vocabulary::TYPE_LONG_DOUBLE:
			if (boost::get<long double>(&m_value) == NULL)
				throw InvalidValueForTypeException();
			break;
		case Vocabulary::TYPE_SHORT_STRING:
		case Vocabulary::TYPE_STRING:
		case Vocabulary::TYPE_LONG_STRING:
		case Vocabulary::TYPE_CHAR:
		case Vocabulary::TYPE_BLOB:
		case Vocabulary::TYPE_ZBLOB:
			throw InvalidValueForTypeException();
			break;
		case Vocabulary::TYPE_DATE_TIME:
		case Vocabulary::TYPE_DATE:
		case Vocabulary::TYPE_TIME:
			if (boost::get<PionDateTime>(&m_value) == NULL)
				throw InvalidValueForTypeException();
			break;
	}
}

//************************************************************************************************************************************************

// inline member functions for Comparison

template <typename ComparisonFunction>
inline bool Comparison::checkComparison(const ComparisonFunction& comparison_func,
										const Event::ValuesRange& values_range) const
{
	boost::tribool result = boost::indeterminate;
//	Event::ConstIterator value = values_range.second;

	try {
		for (Event::ConstIterator i = values_range.first;
			 i != values_range.second; ++i)
		{
			if (comparison_func(i->value)) {
				if (! m_match_all_values) {
					result = true;
//					value = i;
					break;
				}
			} else {
				if (m_match_all_values) {
					result = false;
					break;
				}
			}
		}
	} catch (PionException& e) {
		PION_LOG_ERROR(m_logger, "term_id: " << m_term.term_id << " - " << e.what() << " - rethrowing");
		throw;
	}

	if (boost::indeterminate(result))
		result = m_match_all_values;

	return result;
}


// This method now evaluates only a values_range, in order to support TransformReactor2
// See Comparison::evaluate() on how values_range is defined, and the special case of *_DEFINED
inline bool Comparison::evaluateRange(const Event::ValuesRange& values_range)
{
	bool result = false;
	bool negate_result = false;

	switch (m_type) {
		case TYPE_FALSE:
			result = false;
			break;
		case TYPE_TRUE:
			result = true;
			break;
		case TYPE_IS_DEFINED:
			result = (values_range.first != values_range.second);
			break;
		case TYPE_IS_NOT_DEFINED:
			result = (values_range.first == values_range.second);
			break;

		case TYPE_EQUALS:
		case TYPE_NOT_EQUALS:
			switch (m_term.term_type) {
				case Vocabulary::TYPE_INT8:
				case Vocabulary::TYPE_INT16:
				case Vocabulary::TYPE_INT32:
				{
					CompareEquals<std::int32_t> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_INT64:
				{
					CompareEquals<std::int64_t> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_UINT8:
				case Vocabulary::TYPE_UINT16:
				case Vocabulary::TYPE_UINT32:
				{
					CompareEquals<std::uint32_t> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_UINT64:
				{
					CompareEquals<std::uint64_t> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_FLOAT:
				{
					CompareEquals<float> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_DOUBLE:
				{
					CompareEquals<double> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_LONG_DOUBLE:
				{
					CompareEquals<long double> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				default:
					throw InvalidComparisonException();
			}
			if (m_type == TYPE_NOT_EQUALS)
				result = !result;
			break;

		case TYPE_GREATER_THAN:
			switch (m_term.term_type) {
				case Vocabulary::TYPE_INT8:
				case Vocabulary::TYPE_INT16:
				case Vocabulary::TYPE_INT32:
				{
					CompareGreaterThan<std::int32_t> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_INT64:
				{
					CompareGreaterThan<std::int64_t> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_UINT8:
				case Vocabulary::TYPE_UINT16:
				case Vocabulary::TYPE_UINT32:
				{
					CompareGreaterThan<std::uint32_t> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_UINT64:
				{
					CompareGreaterThan<std::uint64_t> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_FLOAT:
				{
					CompareGreaterThan<float> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_DOUBLE:
				{
					CompareGreaterThan<double> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_LONG_DOUBLE:
				{
					CompareGreaterThan<long double> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				default:
					throw InvalidComparisonException();
			}
			break;

		case TYPE_LESS_THAN:
			switch (m_term.term_type) {
				case Vocabulary::TYPE_INT8:
				case Vocabulary::TYPE_INT16:
				case Vocabulary::TYPE_INT32:
				{
					CompareLessThan<std::int32_t> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_INT64:
				{
					CompareLessThan<std::int64_t> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_UINT8:
				case Vocabulary::TYPE_UINT16:
				case Vocabulary::TYPE_UINT32:
				{
					CompareLessThan<std::uint32_t> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_UINT64:
				{
					CompareLessThan<std::uint64_t> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_FLOAT:
				{
					CompareLessThan<float> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_DOUBLE:
				{
					CompareLessThan<double> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_LONG_DOUBLE:
				{
					CompareLessThan<long double> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				default:
					throw InvalidComparisonException();
			}
			break;

		case TYPE_GREATER_OR_EQUAL:
			switch (m_term.term_type) {
				case Vocabulary::TYPE_INT8:
				case Vocabulary::TYPE_INT16:
				case Vocabulary::TYPE_INT32:
				{
					CompareGreaterOrEqual<std::int32_t> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_INT64:
				{
					CompareGreaterOrEqual<std::int64_t> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_UINT8:
				case Vocabulary::TYPE_UINT16:
				case Vocabulary::TYPE_UINT32:
				{
					CompareGreaterOrEqual<std::uint32_t> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_UINT64:
				{
					CompareGreaterOrEqual<std::uint64_t> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_FLOAT:
				{
					CompareGreaterOrEqual<float> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_DOUBLE:
				{
					CompareGreaterOrEqual<double> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_LONG_DOUBLE:
				{
					CompareGreaterOrEqual<long double> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				default:
					throw InvalidComparisonException();
			}
			break;

		case TYPE_LESS_OR_EQUAL:
			switch (m_term.term_type) {
				case Vocabulary::TYPE_INT8:
				case Vocabulary::TYPE_INT16:
				case Vocabulary::TYPE_INT32:
				{
					CompareLessOrEqual<std::int32_t> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_INT64:
				{
					CompareLessOrEqual<std::int64_t> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_UINT8:
				case Vocabulary::TYPE_UINT16:
				case Vocabulary::TYPE_UINT32:
				{
					CompareLessOrEqual<std::uint32_t> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_UINT64:
				{
					CompareLessOrEqual<std::uint64_t> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_FLOAT:
				{
					CompareLessOrEqual<float> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_DOUBLE:
				{
					CompareLessOrEqual<double> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				case Vocabulary::TYPE_LONG_DOUBLE:
				{
					CompareLessOrEqual<long double> comparison_func(m_value);
					result = checkComparison(comparison_func, values_range);
					break;
				}
				default:
					throw InvalidComparisonException();
			}
			break;

		case TYPE_NOT_EXACT_MATCH:
		case TYPE_NOT_EXACT_MATCH_PRIMARY:
		case TYPE_NOT_CONTAINS:
		case TYPE_NOT_CONTAINS_PRIMARY:
		case TYPE_NOT_STARTS_WITH:
		case TYPE_NOT_STARTS_WITH_PRIMARY:
		case TYPE_NOT_ENDS_WITH:
		case TYPE_NOT_ENDS_WITH_PRIMARY:
		case TYPE_NOT_ORDERED_BEFORE:
		case TYPE_NOT_ORDERED_BEFORE_PRIMARY:
		case TYPE_NOT_ORDERED_AFTER:
		case TYPE_NOT_ORDERED_AFTER_PRIMARY:
			negate_result = true;
		case TYPE_EXACT_MATCH:
		case TYPE_EXACT_MATCH_PRIMARY:
		case TYPE_CONTAINS:
		case TYPE_CONTAINS_PRIMARY:
		case TYPE_STARTS_WITH:
		case TYPE_STARTS_WITH_PRIMARY:
		case TYPE_ENDS_WITH:
		case TYPE_ENDS_WITH_PRIMARY:
		case TYPE_ORDERED_BEFORE:
		case TYPE_ORDERED_BEFORE_PRIMARY:
		case TYPE_ORDERED_AFTER:
		case TYPE_ORDERED_AFTER_PRIMARY:
			switch (m_term.term_type) {
				case Vocabulary::TYPE_SHORT_STRING:
				case Vocabulary::TYPE_STRING:
				case Vocabulary::TYPE_LONG_STRING:
				case Vocabulary::TYPE_CHAR:
				case Vocabulary::TYPE_BLOB:
				case Vocabulary::TYPE_ZBLOB:
					result = checkComparison(*m_comparison_func, values_range);
					break;
				default:
					throw InvalidComparisonException();
			}
			if (negate_result)
				result = !result;
			break;

		case TYPE_REGEX:
		case TYPE_NOT_REGEX:
			switch (m_term.term_type) {
				case Vocabulary::TYPE_SHORT_STRING:
				case Vocabulary::TYPE_STRING:
				case Vocabulary::TYPE_LONG_STRING:
				case Vocabulary::TYPE_CHAR:
				case Vocabulary::TYPE_BLOB:
				case Vocabulary::TYPE_ZBLOB:
				{
					CompareStringRegex comparison_func(m_regex);
					try {
						result = checkComparison(comparison_func, values_range);
					} catch (pion::platform::Comparison::RegexFailure& e) {
						PION_LOG_ERROR(m_logger, "Regex search failed: regex = " << m_regex_str << ", " << e.what());

						if (! m_debug_mode) {
							// Prevent this rule from running again.
							PION_LOG_WARN(m_logger, "Comparison rule has been disabled: regex = " << m_regex_str);
							m_running = false;
						}

						// Since neither true nor false is an appropriate return value, rethrow for RuleChain to handle.
						throw;
					}
					break;
				}
				default:
					throw InvalidComparisonException();
			}
			if (m_type == TYPE_NOT_REGEX)
				result = !result;
			break;

		case TYPE_SAME_DATE_TIME:
		case TYPE_NOT_SAME_DATE_TIME:
		{
			PION_ASSERT(m_term.term_type == Vocabulary::TYPE_DATE_TIME)
			CompareSameDateTime comparison_func(m_value);
			result = checkComparison(comparison_func, values_range);
			if (m_type == TYPE_NOT_SAME_DATE_TIME)
				result = !result;
			break;
		}

		case TYPE_EARLIER_DATE_TIME:
		{
			PION_ASSERT(m_term.term_type == Vocabulary::TYPE_DATE_TIME)
			CompareEarlierDateTime comparison_func(m_value);
			result = checkComparison(comparison_func, values_range);
			break;
		}

		case TYPE_LATER_DATE_TIME:
		{
			PION_ASSERT(m_term.term_type == Vocabulary::TYPE_DATE_TIME)
			CompareLaterDateTime comparison_func(m_value);
			result = checkComparison(comparison_func, values_range);
			break;
		}

		case TYPE_SAME_OR_EARLIER_DATE_TIME:
		{
			PION_ASSERT(m_term.term_type == Vocabulary::TYPE_DATE_TIME)
			CompareSameOrEarlierDateTime comparison_func(m_value);
			result = checkComparison(comparison_func, values_range);
			break;
		}

		case TYPE_SAME_OR_LATER_DATE_TIME:
		{
			PION_ASSERT(m_term.term_type == Vocabulary::TYPE_DATE_TIME)
			CompareSameOrLaterDateTime comparison_func(m_value);
			result = checkComparison(comparison_func, values_range);
			break;
		}

		case TYPE_SAME_DATE:
		case TYPE_NOT_SAME_DATE:
		{
			PION_ASSERT(m_term.term_type == Vocabulary::TYPE_DATE_TIME || m_term.term_type == Vocabulary::TYPE_DATE)
			CompareSameDate comparison_func(m_value);
			result = checkComparison(comparison_func, values_range);
			if (m_type == TYPE_NOT_SAME_DATE)
				result = !result;
			break;
		}

		case TYPE_EARLIER_DATE:
		{
			PION_ASSERT(m_term.term_type == Vocabulary::TYPE_DATE_TIME || m_term.term_type == Vocabulary::TYPE_DATE)
			CompareEarlierDate comparison_func(m_value);
			result = checkComparison(comparison_func, values_range);
			break;
		}

		case TYPE_LATER_DATE:
		{
			PION_ASSERT(m_term.term_type == Vocabulary::TYPE_DATE_TIME || m_term.term_type == Vocabulary::TYPE_DATE)
			CompareLaterDate comparison_func(m_value);
			result = checkComparison(comparison_func, values_range);
			break;
		}

		case TYPE_SAME_OR_EARLIER_DATE:
		{
			PION_ASSERT(m_term.term_type == Vocabulary::TYPE_DATE_TIME || m_term.term_type == Vocabulary::TYPE_DATE)
			CompareSameOrEarlierDate comparison_func(m_value);
			result = checkComparison(comparison_func, values_range);
			break;
		}

		case TYPE_SAME_OR_LATER_DATE:
		{
			PION_ASSERT(m_term.term_type == Vocabulary::TYPE_DATE_TIME || m_term.term_type == Vocabulary::TYPE_DATE)
			CompareSameOrLaterDate comparison_func(m_value);
			result = checkComparison(comparison_func, values_range);
			break;
		}

		case TYPE_SAME_TIME:
		case TYPE_NOT_SAME_TIME:
		{
			PION_ASSERT(m_term.term_type == Vocabulary::TYPE_DATE_TIME || m_term.term_type == Vocabulary::TYPE_TIME)
			CompareSameTime comparison_func(m_value);
			result = checkComparison(comparison_func, values_range);
			if (m_type == TYPE_NOT_SAME_TIME)
				result = !result;
			break;
		}

		case TYPE_EARLIER_TIME:
		{
			PION_ASSERT(m_term.term_type == Vocabulary::TYPE_DATE_TIME || m_term.term_type == Vocabulary::TYPE_TIME)
			CompareEarlierTime comparison_func(m_value);
			result = checkComparison(comparison_func, values_range);
			break;
		}

		case TYPE_LATER_TIME:
		{
			PION_ASSERT(m_term.term_type == Vocabulary::TYPE_DATE_TIME || m_term.term_type == Vocabulary::TYPE_TIME)
			CompareLaterTime comparison_func(m_value);
			result = checkComparison(comparison_func, values_range);
			break;
		}

		case TYPE_SAME_OR_EARLIER_TIME:
		{
			PION_ASSERT(m_term.term_type == Vocabulary::TYPE_DATE_TIME || m_term.term_type == Vocabulary::TYPE_TIME)
			CompareSameOrEarlierTime comparison_func(m_value);
			result = checkComparison(comparison_func, values_range);
			break;
		}

		case TYPE_SAME_OR_LATER_TIME:
		{
			PION_ASSERT(m_term.term_type == Vocabulary::TYPE_DATE_TIME || m_term.term_type == Vocabulary::TYPE_TIME)
			CompareSameOrLaterTime comparison_func(m_value);
			result = checkComparison(comparison_func, values_range);
			break;
		}
	}

	return result;
}

// This is the refactored version of evaluate
// Since IS_DEFINED and IS_NOT_DEFINED allow for looking at Event, in addition to range_value,
// it was necessary to put some convoluted special-case logic here...
inline bool Comparison::evaluate(const Event& e)
{
	if (m_type == TYPE_IS_DEFINED || m_type == TYPE_IS_NOT_DEFINED) {
		// If object type matches looked for type, return true -- refactored out of evaluateRange
		if (e.getType() == m_term.term_ref) return m_type == TYPE_IS_DEFINED;
	}
	/// get a range of iterators representing all the values for the Term
	Event::ValuesRange values_range = e.equal_range(m_term.term_ref);
	return Comparison::evaluateRange(values_range);
}


}	// end namespace platform
}	// end namespace pion

#endif

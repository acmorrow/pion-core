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

#ifndef __PION_TRANSFORM_HEADER__
#define __PION_TRANSFORM_HEADER__

#include <cctype>
#include <cstring>
#include <set>
#include <boost/regex.hpp>
#include <boost/regex/icu.hpp>
#include <boost/algorithm/string/compare.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/tokenizer.hpp>
#include <pion/PionAlgorithms.hpp>
#include <pion/PionConfig.hpp>
#include <pion/PionException.hpp>
#include <pion/platform/Vocabulary.hpp>
#include <pion/platform/Event.hpp>
#include <pion/platform/Comparison.hpp>
#include <pion/PionLogger.hpp>
#include <pion/platform/ConfigManager.hpp>

namespace pion {		// begin namespace pion
namespace platform {	// begin namespace platform (Pion Platform Library)

class PION_PLATFORM_API Transform
{
public:
	/// identifies the Vocabulary Term that is being tested (for regex)
	Vocabulary::Term			m_term;
	
	/// true if the transform should run in "debug mode"
	const bool					m_debug_mode;

	/// invalid/missing type of transformation
	class MissingTransformField : public PionException {
	public:
		MissingTransformField(const std::string& str)
			: PionException("Invalid type of transformation: ", str) {}
	};

	/// exception thrown if regex_replace fails and throws
	class RegexFailure : public PionException {
	public:
		RegexFailure(const std::string& what)
			: PionException("Regex replace failed: ", what) {}
	};

	/// exception thrown if AssignValue catches an exception
	class ValueAssignmentException : public PionException {
	public:
		ValueAssignmentException(const std::string& value)
			: PionException("AssignValue failed for value: ", value) {}
	};

	/// virtual destructor: you may extend this class
	virtual ~Transform() {}

	/**
	 * constructs a new Transform rule
	 *
	 * @param term the term that will be examined
	 * @param set_term the term that will be set (if not InPlace)
	 * @param debug_mode true if the transform should run in "debug mode"
	 */
	Transform(const Vocabulary& v, const Vocabulary::Term& term, bool debug_mode)
		: m_term(term), m_debug_mode(debug_mode)
	{
	}

   /**
	* updateVocabulary Updates all Transform classes to use new vocabulary value
	*
	* @param v New vocabulary object, updates only m_term for each class
	*/
	void updateVocabulary(const Vocabulary& v)
	{
		// assume that Term references never change
		m_term = v[m_term.term_ref];
	}

   /**
	* removeTerm removes m_term.term_ref value(s) from event
	*
	* @param e EventPtr pointing to event, where value will be removed from
	*/
	inline void removeTerm(EventPtr& e)
	{
		e->clear(m_term.term_ref);
	}

	/**
	 * transforms event terms
	 *
	 * @param e the Event to transform
	 *
	 * @return true if the Transformation occured; false if it did not
	 */
	virtual bool transform(EventPtr& d, const EventPtr& s) = 0;

	/// Definitions for static ELEMENT NAMEs
	static const std::string			LOOKUP_TERM_NAME;
	static const std::string			TERM_ELEMENT_NAME;
	static const std::string			LOOKUP_MATCH_ELEMENT_NAME;
	static const std::string			LOOKUP_FORMAT_ELEMENT_NAME;
	static const std::string			LOOKUP_DEFAULT_ELEMENT_NAME;
	static const std::string			VALUE_ELEMENT_NAME;
	static const std::string			RULES_STOP_ON_FIRST_ELEMENT_NAME;
	static const std::string			RULE_ELEMENT_NAME;
	static const std::string			TYPE_ELEMENT_NAME;
	static const std::string			TRANSFORMATION_SET_VALUE_NAME;
	static const std::string			LOOKUP_DEFAULTACTION_ELEMENT_NAME;
	static const std::string			LOOKUP_LOOKUP_ELEMENT_NAME;
	static const std::string			LOOKUP_KEY_ATTRIBUTE_NAME;
	static const std::string			SOURCE_TERM_ELEMENT_NAME;
	static const std::string			REGEXP_ELEMENT_NAME;
	static const std::string			REGEXP_ATTRIBUTE_NAME;
	static const std::string			SEP_ATTRIBUTE_NAME;
	static const std::string			UNIQ_ATTRIBUTE_NAME;
};

	/**
	 * AssignValue Assigns "value" into "term" of "e" -- does the appropriate casts to make it fit
	 *
	 * @param e the Event to assign the term/value into
	 * @param term the term (type) to use for assignment
	 * @param value the value (as a string) to assign, using appropriate cast
	 *
	 * @return true if the assignment was possible/done
	 */
inline bool AssignValue(EventPtr& e, const Vocabulary::Term& term, const std::string& value)
{
	if (value.empty())		// New shortcut -- if empty value, don't assign
	  return true;

	try {
		e->set(term, value);
	} catch (...) {
		throw Transform::ValueAssignmentException(value);
	}
	return true;
}

/// TransformAssignValue -- Transformation based on assigning a value to a Term
/// Current implementation takes the std::string value of the "SetValue", and does
/// a lexical cast upon assignment -- theoretically an implementation that did a lexical
/// cast to start with, and then used an assignment might be faster... (once vs. many times)
/// Also, this implementation will throw a every assignment (if not possible to lexically cast)
/// while, a pre-cast version would throw once -- at configuration time...
class PION_PLATFORM_API TransformAssignValue
	: public Transform
{
	/// The value to set the target event/term to
	std::string					m_tr_set_value;

public:

	/**
	 * TransformAssignValue constructs a transformation assignment based on Value
	 *
	 * @param v Vocabulary to use
	 * @param term The source term type to use
	 * @param config_ptr Pointer to XML configuration of the AssignValue entity
	 */
	TransformAssignValue(const Vocabulary& v, const Vocabulary::Term& term, const xmlNodePtr config_ptr, bool debug_mode = false)
		: Transform(v, term, debug_mode)
	{
		// <Value>escape(value)</Value>
		std::string val;
		if (! ConfigManager::getConfigOptionEmptyOk(VALUE_ELEMENT_NAME, val, config_ptr))
			throw MissingTransformField("Missing Value in TransformationAssignValue");
		m_tr_set_value = val;
	}

	/**
	 * transform Assigns a fixed value to event/term
	 *
	 * @param d Destination event (pointer) to modify term
	 * @param s Source event (dummy), passed in only for conformity
	 *
	 * @return true if the Transformation occured; false if it did not
	 */
	virtual bool transform(EventPtr& d, const EventPtr& s)
	{
		return AssignValue(d, m_term, m_tr_set_value);
	}
};


/// TransformAssignTerm -- Transformation based on copying Terms from the source event
/// Should the type be re-cast, if non-matching?
class PION_PLATFORM_API TransformAssignTerm
	: public Transform
{
	/// identifies the Vocabulary Term that is being copied from
	Vocabulary::Term			m_src_term;

public:

	/**
	 * TransformAssignTerm constructs a transformation assignment based on a source term
	 *
	 * @param v Vocabulary to use
	 * @param term The source term type to use
	 * @param config_ptr Pointer to XML configuration of the AssignTerm entity
	 */
	TransformAssignTerm(const Vocabulary& v, const Vocabulary::Term& term, const xmlNodePtr config_ptr, bool debug_mode = false)
		: Transform(v, term, debug_mode)
	{
		// <Term>src-term</Term>
		std::string term_id;
		if (! ConfigManager::getConfigOption(VALUE_ELEMENT_NAME, term_id, config_ptr))
			throw MissingTransformField("Missing Source-Term in TransformationAssignTerm");
		Vocabulary::TermRef term_ref = v.findTerm(term_id);
		if (term_ref == Vocabulary::UNDEFINED_TERM_REF)
			throw MissingTransformField("Invalid Source-Term in TransformationAssignTerm");
		m_src_term = v[term_ref];
	}

	/**
	 * transform Assigns a copy of the source term into the destination event/term
	 *
	 * @param d Destination event (pointer) to modify term
	 * @param s Source event to copy the termS/valueS from
	 *
	 * @return true if the Transformation occured; false if it did not
	 */
	virtual bool transform(EventPtr& d, const EventPtr& s)
	{
		bool AnyCopied = false;
		Event::ValuesRange values_range = s->equal_range(m_src_term.term_ref);
		for (Event::ConstIterator ec = values_range.first; ec != values_range.second; ec++) {
			std::string str;
			AnyCopied |= AssignValue(d, m_term, s->write(str, ec->value, m_src_term));
		}
		return AnyCopied;	// true, if any were copied...
	}
};

/// TransformURLEncode -- Transformation of URLEncoding source term into destination
class PION_PLATFORM_API TransformURLEncode
	: public Transform
{
	/// identifies the Vocabulary Term that is being copied from
	Vocabulary::Term			m_src_term;

public:

	/**
	 * TransformURLEncode constructs a transformation of URLEncoding
	 *
	 * @param v Vocabulary to use
	 * @param term The source term type to use
	 * @param config_ptr Pointer to XML configuration of the AssignTerm entity
	 */
	TransformURLEncode(const Vocabulary& v, const Vocabulary::Term& term, const xmlNodePtr config_ptr, bool debug_mode = false)
		: Transform(v, term, debug_mode)
	{
		// <Term>src-term</Term>
		std::string term_id;
		if (! ConfigManager::getConfigOption(VALUE_ELEMENT_NAME, term_id, config_ptr))
			throw MissingTransformField("Missing Source-Term in TransformationURLEncode");
		Vocabulary::TermRef term_ref = v.findTerm(term_id);
		if (term_ref == Vocabulary::UNDEFINED_TERM_REF)
			throw MissingTransformField("Invalid Source-Term in TransformationURLEncode");
		m_src_term = v[term_ref];
	}

	/**
	 * transform Assigns a URLEncoded copy of source term
	 *
	 * @param d Destination event (pointer) to modify term
	 * @param s Source event to copy the termS/valueS from
	 *
	 * @return true if the Transformation occured; false if it did not
	 */
	virtual bool transform(EventPtr& d, const EventPtr& s)
	{
		bool AnyCopied = false;
		Event::ValuesRange values_range = s->equal_range(m_src_term.term_ref);
		for (Event::ConstIterator ec = values_range.first; ec != values_range.second; ec++) {
			std::string str;
			AnyCopied |= AssignValue(d, m_term, algo::url_encode(s->write(str, ec->value, m_src_term)));
		}
		return AnyCopied;	// true, if any were copied...
	}
};

/// TransformURLDecode  -- Transformation of URLDecoding source term into destination
class PION_PLATFORM_API TransformURLDecode
	: public Transform
{
	/// identifies the Vocabulary Term that is being copied from
	Vocabulary::Term			m_src_term;

public:

	/**
	 * TransformURLDecode constructs a transformation of URLDecoding
	 *
	 * @param v Vocabulary to use
	 * @param term The source term type to use
	 * @param config_ptr Pointer to XML configuration of the AssignTerm entity
	 */
	TransformURLDecode(const Vocabulary& v, const Vocabulary::Term& term, const xmlNodePtr config_ptr, bool debug_mode = false)
		: Transform(v, term, debug_mode)
	{
		// <Term>src-term</Term>
		std::string term_id;
		if (! ConfigManager::getConfigOption(VALUE_ELEMENT_NAME, term_id, config_ptr))
			throw MissingTransformField("Missing Source-Term in TransformationURLDecode");
		Vocabulary::TermRef term_ref = v.findTerm(term_id);
		if (term_ref == Vocabulary::UNDEFINED_TERM_REF)
			throw MissingTransformField("Invalid Source-Term in TransformationURLDecode");
		m_src_term = v[term_ref];
	}

	/**
	 * transform Assigns a URLDecoded copy of source term
	 *
	 * @param d Destination event (pointer) to modify term
	 * @param s Source event to copy the termS/valueS from
	 *
	 * @return true if the Transformation occured; false if it did not
	 */
	virtual bool transform(EventPtr& d, const EventPtr& s)
	{
		bool AnyCopied = false;
		Event::ValuesRange values_range = s->equal_range(m_src_term.term_ref);
		for (Event::ConstIterator ec = values_range.first; ec != values_range.second; ec++) {
			std::string str;
			AnyCopied |= AssignValue(d, m_term, algo::url_decode(s->write(str, ec->value, m_src_term)));
		}
		return AnyCopied;	// true, if any were copied...
	}
};


/// TransformSplitTerm -- Transformation of splitting a term to multiple values based on separator
class PION_PLATFORM_API TransformSplitTerm
	: public Transform
{
	/// identifies the Vocabulary Term that is being copied from
	Vocabulary::Term			m_src_term;

	/// separator characters
	boost::char_separator<char> m_sep;

public:

	/**
	 * TransformSplitTerm constructs a transformation assignment based on a source term
	 *
	 * @param v Vocabulary to use
	 * @param term The source term type to use
	 * @param config_ptr Pointer to XML configuration of the AssignTerm entity
	 */
	TransformSplitTerm(const Vocabulary& v, const Vocabulary::Term& term, const xmlNodePtr config_ptr, bool debug_mode = false)
		: Transform(v, term, debug_mode)
	{
		// <Term>src-term</Term>
		std::string term_id;
		if (! ConfigManager::getConfigOption(VALUE_ELEMENT_NAME, term_id, config_ptr))
			throw MissingTransformField("Missing Source-Term in TransformationSplitTerm");
		Vocabulary::TermRef term_ref = v.findTerm(term_id);
		if (term_ref == Vocabulary::UNDEFINED_TERM_REF)
			throw MissingTransformField("Invalid Source-Term in TransformationSplitTerm");
		m_src_term = v[term_ref];
		std::string separator = ConfigManager::getAttribute(SEP_ATTRIBUTE_NAME, ConfigManager::findConfigNodeByName(VALUE_ELEMENT_NAME, config_ptr));
		if (separator.empty())
			throw MissingTransformField("Missing separator value in TransformationSplitTerm");
		m_sep = boost::char_separator<char>(separator.c_str());
	}

	/**
	 * transform iterates through all values, and splits each into separate value
	 *
	 * @param d Destination event (pointer) to modify term
	 * @param s Source event to copy the termS/valueS from
	 *
	 * @return true if the Transformation occured; false if it did not
	 */
	virtual bool transform(EventPtr& d, const EventPtr& s)
	{
		bool AnyCopied = false;
		typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
		Event::ValuesRange values_range = s->equal_range(m_src_term.term_ref);
		std::string str;
		for (Event::ConstIterator ec = values_range.first; ec != values_range.second; ec++) {
			tokenizer str_tok(s->write(str, ec->value, m_src_term), m_sep);
			for (tokenizer::iterator tok_iter = str_tok.begin(); tok_iter != str_tok.end(); ++tok_iter)
				AnyCopied |= AssignValue(d, m_term, *tok_iter);
		}
		return AnyCopied;	// true, if any were copied...
	}
};

/// TransformJoinTerm -- Transformation of joining multiple values of a term with a separator
class PION_PLATFORM_API TransformJoinTerm
	: public Transform
{
	/// identifies the Vocabulary Term that is being copied from
	Vocabulary::Term			m_src_term;

	/// separator characters
	std::string					m_separator;

	/// Unique values only?
	bool						m_unique;

public:

	/**
	 * TransformJoinTerm constructs a transformation based on joining multivalues from source term
	 *
	 * @param v Vocabulary to use
	 * @param term The source term type to use
	 * @param config_ptr Pointer to XML configuration of the AssignTerm entity
	 */
	TransformJoinTerm(const Vocabulary& v, const Vocabulary::Term& term, const xmlNodePtr config_ptr, bool debug_mode = false)
		: Transform(v, term, debug_mode), m_unique(false)
	{
		// <Term>src-term</Term>
		std::string term_id;
		if (! ConfigManager::getConfigOption(VALUE_ELEMENT_NAME, term_id, config_ptr))
			throw MissingTransformField("Missing Source-Term in TransformationJoinTerm");
		Vocabulary::TermRef term_ref = v.findTerm(term_id);
		if (term_ref == Vocabulary::UNDEFINED_TERM_REF)
			throw MissingTransformField("Invalid Source-Term in TransformationJoinTerm");
		m_src_term = v[term_ref];
		m_separator = ConfigManager::getAttribute(SEP_ATTRIBUTE_NAME, ConfigManager::findConfigNodeByName(VALUE_ELEMENT_NAME, config_ptr));
		if (m_separator.empty())
			throw MissingTransformField("Missing separator value in TransformationSplitTerm");
		std::string uniq = ConfigManager::getAttribute(UNIQ_ATTRIBUTE_NAME, ConfigManager::findConfigNodeByName(VALUE_ELEMENT_NAME, config_ptr));
		if (uniq == "true")
			m_unique = true;
	}

	/**
	 * transform combines multiple values of source term into destination
	 *
	 * @param d Destination event (pointer) to modify term
	 * @param s Source event to copy the termS/valueS from
	 *
	 * @return true if the Transformation occured; false if it did not
	 */
	virtual bool transform(EventPtr& d, const EventPtr& s)
	{
		std::set<std::string> seen;
		Event::ValuesRange values_range = s->equal_range(m_src_term.term_ref);
		std::string result, str;
		for (Event::ConstIterator ec = values_range.first; ec != values_range.second; ec++) {
			s->write(str, ec->value, m_src_term);
			if (!str.empty() && seen.find(str) == seen.end()) {
				if (!result.empty())
					result += m_separator;
				result += str;
				if (m_unique)
					seen.insert(str);
			}
		}
		return AssignValue(d, m_term, result);
	}
};



/// TransformLookup -- Transformation based on doing lookups
class PION_PLATFORM_API TransformLookup
	: public Transform
{
	/// Define HashMapped, key-value pair array KVP
	typedef std::unordered_map<std::string, std::string>	KVP;

	/// Term to pull out of the source event
	Vocabulary::Term			m_lookup_term;

	/// [optional] regular expression to apply to Lookup Term
	boost::u32regex				m_match;

	/// the original string that m_match was constructed from
	std::string					m_match_str;

	/// [optional] format to apply to regular expression, default: $&
	std::string					m_format;

	/// Treatment for default value, if lookup fails
	enum { DEF_UNDEF, DEF_SRCTERM, DEF_OUTPUT, DEF_FIXED }
								m_default;

	/// [optional] fixed string to use as default value with DEF_FIXED
	std::string					m_fixed;

	/// Keys & Values, hashmap for keys
	KVP							m_lookup;

	/// Is this rule running?
	bool						m_running;

public:

	/**
	 * TransformLookup constructs a transformation assignment based on a lookup set of values
	 *
	 * @param v Vocabulary to use
	 * @param term The source term type to use
	 * @param config_ptr Pointer to XML configuration of the Lookup entity
	 */
	TransformLookup(const Vocabulary& v, const Vocabulary::Term& term, const xmlNodePtr config_ptr, bool debug_mode = false)
		: Transform(v, term, debug_mode)
	{
		//	<LookupTerm>src-term</LookupTerm>
		std::string term_id;
		if (! ConfigManager::getConfigOption(LOOKUP_TERM_NAME, term_id, config_ptr))
			throw MissingTransformField("Missing LookupTerm in TransformationAssignLookup");
		Vocabulary::TermRef term_ref = v.findTerm(term_id);
		if (term_ref == Vocabulary::UNDEFINED_TERM_REF)
			throw MissingTransformField("Invalid LookupTerm in TransformationAssignLookup");
		m_lookup_term = v[term_ref];
		//[opt]		<Match>escape(regexp)</Match>
		std::string val;
		if (ConfigManager::getConfigOptionEmptyOk(LOOKUP_MATCH_ELEMENT_NAME, val, config_ptr)) {
			try {
				m_match = boost::make_u32regex(val);
				m_match_str = val;
			} catch (...) {
				throw MissingTransformField("Invalid regular expression in TransformationLookup: " + val);
			}
		}
		//	[opt]		<Format>escape(format)</Format>
		m_format.clear();
		if (ConfigManager::getConfigOptionEmptyOk(LOOKUP_FORMAT_ELEMENT_NAME, val, config_ptr))
			m_format = val;

		//	[opt]		<DefaultAction>leave-undefined|src-term|output|fixedvalue</DefaultAction>
		m_default = DEF_UNDEF;
		if (ConfigManager::getConfigOption(LOOKUP_DEFAULTACTION_ELEMENT_NAME, val, config_ptr)) {
			if (val == "src-term")
				m_default = DEF_SRCTERM;
			else if (val == "output")
				m_default = DEF_OUTPUT;
			else if (val == "fixedvalue")
				m_default = DEF_FIXED;
		}
		//	[opt]		<DefaultValue>escape(text)</DefaultValue>
		m_fixed.clear();
		if (m_default == DEF_FIXED && ConfigManager::getConfigOptionEmptyOk(LOOKUP_DEFAULT_ELEMENT_NAME, val, config_ptr))
			m_fixed = val;
		//	[rpt/]		<Lookup key="escape(key)">escape(value)</Lookup>
		xmlNodePtr LookupNode = config_ptr;
		while ( (LookupNode = ConfigManager::findConfigNodeByName(LOOKUP_LOOKUP_ELEMENT_NAME, LookupNode)) != NULL) {
			// get the value (element content)
			xmlChar *xml_char_ptr = xmlNodeGetContent(LookupNode);
			if (xml_char_ptr == NULL || xml_char_ptr[0]=='\0') {
				if (xml_char_ptr != NULL)
					xmlFree(xml_char_ptr);
				throw MissingTransformField("Missing Value in TransformationLookup");
			}
			const std::string val_str(reinterpret_cast<char*>(xml_char_ptr));
			xmlFree(xml_char_ptr);
			// next get the Term we want to map to
			xml_char_ptr = xmlGetProp(LookupNode, reinterpret_cast<const xmlChar*>(LOOKUP_KEY_ATTRIBUTE_NAME.c_str()));
			if (xml_char_ptr == NULL || xml_char_ptr[0]=='\0') {
				if (xml_char_ptr != NULL)
					xmlFree(xml_char_ptr);
				throw MissingTransformField("Missing Key in TransformationLookup");
			}
			const std::string key_str(reinterpret_cast<char*>(xml_char_ptr));
			xmlFree(xml_char_ptr);
			if (m_lookup.find(key_str) != m_lookup.end())
				throw MissingTransformField("Duplicate Key in TransformationLookup");
			m_lookup[key_str] = val_str;
			LookupNode = LookupNode->next;
		}
		if (m_lookup.empty())
			throw MissingTransformField("No Key-Values in TransformationLookup");

		m_running = true;
	}

	/// Destructor, to dispose of the lookup table
	virtual ~TransformLookup() {
		m_lookup.clear();
	}

	/**
	 * transform Assigns a value based on regex/lookup/default
	 *
	 * @param d Destination event (pointer) to modify term
	 * @param s Source event to use as basis for lookup
	 *
	 * @return true if the Transformation occured; false if it did not
	 */
	virtual bool transform(EventPtr& d, const EventPtr& s)
	{
		if (!m_running)
			return false;
		Event::ValuesRange values_range = s->equal_range(m_lookup_term.term_ref);
		Event::ConstIterator ec = values_range.first;
		// if ec == values_range.second ... source term was not found...
		bool AnyCopied = false;
		while (ec != values_range.second) {
			// Get the source term
			std::string str;
			s->write(str, ec->value, m_lookup_term);
			// If regex defined, do the regular expression, replacing the key value
			if (! m_match.empty()) {
				try {
					str = boost::u32regex_replace(str, m_match, m_format, boost::format_all | boost::format_no_copy);
				} catch (...) {
					// Get the source string again
					str.clear();
					s->write(str, ec->value, m_lookup_term);
					if (! m_debug_mode) {
						// Not running anymore
						m_running = false;
					}
					// Throw on this, to get an error message logged
					throw RegexFailure("str=" + str + ", regex=" + m_match_str);
				}
			}
			// Find the value, using the key
			KVP::const_iterator i = m_lookup.find(str);
			if (i != m_lookup.end())	// Found: assign the lookup value
				AnyCopied |= AssignValue(d, m_term, i->second);
			else						// Not found: perform default action
				switch (m_default) {
					case DEF_UNDEF:		// Leave undefined, i.e. do nothing
						break;
					case DEF_SRCTERM:	// Re-get the original value, assign it
						{
							std::string str;
							AnyCopied |= AssignValue(d, m_term, s->write(str, ec->value, m_lookup_term));
						}
						break;
					case DEF_OUTPUT:	// Assign the regex output value
						AnyCopied |= AssignValue(d, m_term, str);
						break;
					case DEF_FIXED:		// Assign the fixed value
						AnyCopied |= AssignValue(d, m_term, m_fixed);
						break;
				}
			ec++;			// repeat for all matching source terms
		}
		return AnyCopied;	// true, if any were copied...
	}
};

/// TransformRules -- Transformation based on a full set of rules
class PION_PLATFORM_API TransformRules
	: public Transform
{
	/// Should TransformRules stop at first successfull transformation for this dest-term
	bool										m_short_circuit;

	/// set value, for assignment, or for use as format for TYPE_REGEX
	std::vector<std::string>					m_set_value;

	/// pointer to instantiated & configured Comparison
	std::vector<Comparison *>					m_comparison;

	/// are we running?
	std::vector<bool>							m_running;

public:

	/**
	 * TransformRules constructs a transformation assignment based on a set of Rules
	 *
	 * @param v Vocabulary to use
	 * @param term The source term type to use
	 * @param config_ptr Pointer to XML configuration of the Rules entity
	 */
	TransformRules(const Vocabulary& v, const Vocabulary::Term& term, const xmlNodePtr config_ptr, bool debug_mode = false)
		: Transform(v, term, debug_mode)
	{
		// <StopOnFirstMatch>true|false</StopOnFirstMatch>			-> DEFAULT: true
		m_short_circuit = false;
		std::string short_circuit_str;
		if (! ConfigManager::getConfigOption(RULES_STOP_ON_FIRST_ELEMENT_NAME, short_circuit_str, config_ptr))
			throw MissingTransformField("Missing StopOnFirstMatch in TransformationAssignRules");
		if (short_circuit_str == "true")
			m_short_circuit = true;

		//	[rpt]		<Rule>
		xmlNodePtr RuleNode = config_ptr;
		while ( (RuleNode = ConfigManager::findConfigNodeByName(RULE_ELEMENT_NAME, RuleNode)) != NULL)
		{
			//	<Term>src-term</Term>
			std::string term_id;
			if (! ConfigManager::getConfigOption(TERM_ELEMENT_NAME, term_id, RuleNode->children))
				throw MissingTransformField("Missing Source-Term in TransformationAssignRules");
			Vocabulary::TermRef term_ref = v.findTerm(term_id);
			if (term_ref == Vocabulary::UNDEFINED_TERM_REF)
				throw MissingTransformField("Invalid Term in TransformationAssignRules");

			//	<Type>test-type</Type>
			std::string val;
			if (! ConfigManager::getConfigOption(TYPE_ELEMENT_NAME, val, RuleNode->children))
				throw MissingTransformField("Missing Value in TransformationAssignRules");
			Comparison::ComparisonType ctype = Comparison::parseComparisonType(val);

			//	<Value>escape(test-value)</Value>
			std::string value_str;
			if (Comparison::requiresValue(ctype))
				if (! ConfigManager::getConfigOptionEmptyOk(VALUE_ELEMENT_NAME, value_str, RuleNode->children))
					throw MissingTransformField("Missing Value in TransformationAssignRules");

			Comparison *comp = new Comparison(v[term_ref], debug_mode);
			comp->configure(ctype, value_str);
			m_comparison.push_back(comp);

			//	<SetValue>escape(set-value)</SetValue>
			val.clear();
			if (! ConfigManager::getConfigOptionEmptyOk(TRANSFORMATION_SET_VALUE_NAME, val, RuleNode->children))
				throw MissingTransformField("Missing SetValue in TransformationAssignRules");
			m_set_value.push_back(val);

			// Set running state
			m_running.push_back(true);

			RuleNode = RuleNode->next;
		}
	}

	/// Destructor for TransformRules -- needed for clearing comparison objects
	virtual ~TransformRules() {
		for (unsigned int i = 0; i < m_comparison.size(); i++)
			delete m_comparison[i];
	}

	/**
	 * transform modifies the destination event, based on rules
	 *
	 * @param d Destination event (pointer) to modify term
	 * @param s Source event to copy the termS/valueS from
	 *
	 * @return true if the Transformation occured; false if it did not
	 */
	virtual bool transform(EventPtr& d, const EventPtr& s)
	{
		bool AnyAssigned = false;
		// Loop through all TESTs, break out if any term successful on any test and short_circuit
		for (unsigned int i = 0; i < m_comparison.size(); i++)
			if (m_running[i]) {
				switch (m_comparison[i]->getType()) {
					// We'll take out the cases where there might not be values to iterate through, and handle them individually.
					case Comparison::TYPE_IS_DEFINED:
						if (s->getType() == m_comparison[i]->getTerm().term_ref || s->isDefined(m_comparison[i]->getTerm().term_ref))
							AnyAssigned |= AssignValue(d, m_term, m_set_value[i]);
						break;
					case Comparison::TYPE_TRUE:
						AnyAssigned |= AssignValue(d, m_term, m_set_value[i]);
						break;
					case Comparison::TYPE_FALSE:
						break;
					case Comparison::TYPE_IS_NOT_DEFINED:
						if (! (s->getType() == m_comparison[i]->getTerm().term_ref || s->isDefined(m_comparison[i]->getTerm().term_ref)))
							AnyAssigned |= AssignValue(d, m_term, m_set_value[i]);
						break;
					default:
						{
							Event::ValuesRange values_range = s->equal_range(m_comparison[i]->getTerm().term_ref);
							for (Event::ConstIterator ec = values_range.first; ec != values_range.second; ec++)
								try {
									Event::ConstIterator ec_past = ec;
									if (m_comparison[i]->evaluateRange(std::make_pair(ec, ++ec_past))) {
										if (m_comparison[i]->getType() == Comparison::TYPE_REGEX) {		// Only for POSITIVE regex...
											// Get the original value
											// For Regex... get the precompiled from Comparison
											// For Format... use the set_value
											std::string str;
											str = boost::u32regex_replace(s->write(str, ec->value, m_comparison[i]->getTerm()), m_comparison[i]->getRegex(),
																		m_set_value[i], boost::format_all | boost::format_no_copy);
											// Assign the result
											AnyAssigned |= AssignValue(d, m_term, str);
										} else
											AnyAssigned |= AssignValue(d, m_term, m_set_value[i]);
									}
								} catch (...) {
									// Get the original value again...
									std::string str;
									if (! m_debug_mode) {
										// This rule won't be running again...
										m_running[i] = false;
									}
									// Throw on this, to get an error message logged
									throw RegexFailure("str=" + s->write(str, ec->value, m_comparison[i]->getTerm()) + ", regex=" + m_comparison[i]->getRegexStr());
								}
						}
						break;
				}
				// If short_circuit AND any values were assigned -> don't go further in the chain
				if (m_short_circuit && AnyAssigned)
					break;
			}
		return AnyAssigned;
	}
};

/// TransformRegex -- Transformation based on a set of regexp's
class PION_PLATFORM_API TransformRegex
	: public Transform
{
	/// identifies the Vocabulary Term that is being copied from
	Vocabulary::Term							m_src_term;

	/// set format for regex's
	std::vector<std::string>					m_format;

	/// regex's
	std::vector<boost::u32regex>				m_regex;

	/// the strings the regex's were created from (as read from the configuration) 
	std::vector<std::string>					m_regex_str;

	/// Is still regex still alive?
	std::vector<bool>							m_running;

public:

	/**
	 * TransformRegex constructs a transformation assignment based on a series of consecutive regular expressions
	 *
	 * @param v Vocabulary to use
	 * @param term The source term type to use
	 * @param config_ptr Pointer to XML configuration of the Regex entity
	 */
	TransformRegex(const Vocabulary& v, const Vocabulary::Term& term, const xmlNodePtr config_ptr, bool debug_mode = false)
		: Transform(v, term, debug_mode)
	{
		//	<SourceTerm>src-term</SourceTerm>
		std::string term_id;
		if (! ConfigManager::getConfigOption(SOURCE_TERM_ELEMENT_NAME, term_id, config_ptr))
			throw MissingTransformField("Missing SourceTerm in TransformationRegex");
		Vocabulary::TermRef term_ref = v.findTerm(term_id);
		if (term_ref == Vocabulary::UNDEFINED_TERM_REF)
			throw MissingTransformField("Invalid SourceTerm in TransformationRegex");
		m_src_term = v[term_ref];
		xmlNodePtr RegexNode = config_ptr;
		while ( (RegexNode = ConfigManager::findConfigNodeByName(REGEXP_ELEMENT_NAME, RegexNode)) != NULL) {
			// get the FORMAT (element content)
			xmlChar *xml_char_ptr = xmlNodeGetContent(RegexNode);
			std::string val;
			if (xml_char_ptr != NULL && xml_char_ptr[0] != '\0')
				val = reinterpret_cast<char*>(xml_char_ptr);
			if (xml_char_ptr != NULL) xmlFree(xml_char_ptr);
			m_format.push_back(val);
			// next get the Term we want to map to
			xml_char_ptr = xmlGetProp(RegexNode, reinterpret_cast<const xmlChar*>(REGEXP_ATTRIBUTE_NAME.c_str()));
			if (xml_char_ptr == NULL || xml_char_ptr[0]=='\0') {
				if (xml_char_ptr != NULL)
					xmlFree(xml_char_ptr);
				throw MissingTransformField("Missing Regexp in TransformationRegex");
			}
			val = reinterpret_cast<char*>(xml_char_ptr);
			xmlFree(xml_char_ptr);
			boost::u32regex reg;
			try {
				reg = boost::make_u32regex(val);
			} catch (...) {
				throw MissingTransformField("Invalid regular expression in TransformationRegex");
			}
			m_regex.push_back(reg);
			m_regex_str.push_back(val);
			m_running.push_back(true);
			RegexNode = RegexNode->next;
		}
		if (m_regex.empty())
			throw MissingTransformField("No Regexp's in TransformationRegex");
	}

	/// Destructor -- not needed
	virtual ~TransformRegex() {	}

	/**
	 * transform Adds a term to destination based on a series of successive regular expressions
	 *
	 * @param d Destination event (pointer) to modify term
	 * @param s Source event to copy the termS/valueS from
	 *
	 * @return true if the Transformation occured; false if it did not
	 */
	virtual bool transform(EventPtr& d, const EventPtr& s)
	{
		bool AnyAssigned = false;
		// Iterate through all values from source term
		Event::ValuesRange values_range = s->equal_range(m_src_term.term_ref);
		for (Event::ConstIterator ec = values_range.first; ec != values_range.second; ec++) {
			// Take the original value from source term set
			std::string str;
			s->write(str, ec->value, m_src_term);
			// Run through all regexp's
			for (unsigned int i = 0; i < m_regex.size(); i++)
				if (m_running[i]) {
					std::string res;
					try {
						res = boost::u32regex_replace(str, m_regex[i], m_format[i], boost::format_all | boost::format_no_copy);
					} catch (...) {
						if (! m_debug_mode) {
							// This rule won't be running again...
							m_running[i] = false;
						}
						// Throw on this, to get an error message logged
						throw RegexFailure("str=" + str + ", regex=" + m_regex_str[i]);
					}
					if (!res.empty())
						str = res;
				}
			AnyAssigned |= AssignValue(d, m_term, str);
		}
		return AnyAssigned;
	}
};


/**
 * Finds credit card numbers in a sequence of characters and replaces them with X's
 *
 * Note: this algorithm should work for any ASCII or UTF-8 sequence but will not
 * work for other types of character encodings.  Using Boost's ICU regex
 * algorithms is not required because doing so does not affect the results for
 * ASCII and UTF-8 encodings.
 *
 * @param first iterator pointing to the beginning of the character sequence
 * @param last iterator pointing to the end of the character sequence
 *
 * @return true if at least one match was found
 */
template <class IteratorType>
inline bool HideCreditCardNumbers(IteratorType first, IteratorType last)
{
	// static regular expressions used to find and verify credit card numbers
	//
	// Visa: starts with "4", has 16 digits in blocks of 4
	// 4(?:[\s+-]?\d){15}
	//
	// MasterCard: starts with 51-55, has 16 digits in blocks of 4
	// 5[1-5](?:[\s+-]?\d){14}
	//
	// Amex: starts with 34 or 37, has 15 digits as 4-6-5
	// (?:34|37)(?:[\s+-]?\d){13}
	//
	// Discover: starts with 6011 or 65, has 16 digits in blocks of 4
	// (?:6011|65\d\d)(?:[\s+-]?\d){12}
	//
	// Diners: starts with 36, 38 or 300-305, all have 14 digits (blocks?)
	// (?:30[0-5]|36\d|38\d)(?:[\s+-]?\d){11}
	//
	// JCB: starts with 35, has 16 digits (blocks?)
	// 35(?:[\s+-]?\d){14}
	//
	// combined:
	// 4(?:[\s+-]?\d){15}|5[1-5](?:[\s+-]?\d){14}|(?:34|37)(?:[\s+-]?\d){13}|(?:6011|65\d\d)(?:[\s+-]?\d){12}|(?:30[0-5]|36\d|38\d)(?:[\s+-]?\d){11}|35(?:[\s+-]?\d){14}
	//
	// resources:
	// http://en.wikipedia.org/wiki/Credit_card_number
	// http://en.wikipedia.org/wiki/List_of_Bank_Identification_Numbers
	// http://www.regular-expressions.info/creditcard.html
	// 
	static const boost::regex FIND_CC_NUMBER_RX("\\b(?:4(?:[\\s+-]?\\d){15}|5[1-5](?:[\\s+-]?\\d){14}|(?:34|37)(?:[\\s+-]?\\d){13}|(?:6011|65\\d\\d)(?:[\\s+-]?\\d){12}|(?:30[0-5]|36\\d|38\\d)(?:[\\s+-]?\\d){11}|35(?:[\\s+-]?\\d){14})\\b");

	// variable used to store regex match information
	boost::match_results<IteratorType> results;
	
	// keeps track of whether or not we found a match
	bool found_match = false;

	// loop through the string looking for each possible match
	while (boost::regex_search(first, last, results, FIND_CC_NUMBER_RX)) {
		// basic verification succeeded
		found_match = true;
		
		// replace match string with X's
		for (IteratorType tmp_it = results[0].first; tmp_it != results[0].second; ++tmp_it) {
			*tmp_it = 'X';
		}

		// start searching for more cc numbers after end of current match
		// (no need to re-check earlier bytes)
		first = results[0].second;
	}
	
	return found_match;
}


/**
 * Finds credit card numbers within Event fields and replaces them with X's
 *
 * Warning: will throw exception if Term is not BlobType
 *
 * @param e reference to the Event to process
 * @param term_ref reference to the Term to process
 *
 * @return true if at least one match was found
 */
inline bool HideCreditCardNumbers(Event& e, const Vocabulary::TermRef& term_ref)
{
	// keeps track of whether or not we found a match
	bool found_match = false;

	// get range of matching parameters within the event & iterate
	Event::ValuesRange values_range = e.equal_range(term_ref);
	for (Event::ConstIterator it = values_range.first; it != values_range.second; it++) {
		const Event::BlobType& b(boost::get<const Event::BlobType&>(it->value));
		char *first = const_cast<char*>(b.get());
		char *last = first + b.size();
		if (HideCreditCardNumbers(first, last)) {
			found_match = true;
		}
	}

	return found_match;
}


/**
 * Finds credit card numbers within all BlobType Event fields and replaces them with X's
 *
 * @param e reference to the Event to process
 *
 * @return true if at least one match was found
 */
inline bool HideCreditCardNumbers(Event& e)
{
	// keeps track of whether or not we found a match
	bool found_match = false;

	// get range of matching parameters within the event & iterate
	for (Event::ConstIterator it = e.begin(); it != e.end(); it++) {
		if (boost::get<const Event::BlobType&>(& it->value)) {		// make sure it's BlobType
			const Event::BlobType& b(boost::get<const Event::BlobType&>(it->value));
			char *first = const_cast<char*>(b.get());
			char *last = first + b.size();
			if (HideCreditCardNumbers(first, last)) {
				found_match = true;
			}
		}
	}

	return found_match;
}


}	// end namespace platform
}	// end namespace pion


#endif

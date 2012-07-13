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

#include <memory>
#include <vector>
#include <thread>
#include <boost/detail/atomic_count.hpp>
#include <pion/PionConfig.hpp>
#include <pion/platform/Vocabulary.hpp>
#include <pion/platform/Event.hpp>
#include <pion/platform/Comparison.hpp>
#include <pion/platform/RuleChain.hpp>
#include <pion/PionUnitTestDefs.hpp>
#include <pion/platform/PionPlatformUnitTest.hpp>
#include <boost/test/unit_test.hpp>

using namespace pion;
using namespace pion::platform;


class ComparisonTests_F {
public:
	ComparisonTests_F()
		: m_plain_int_term("urn:vocab:test#plain-int"),
		m_string_term("urn:vocab:test#simple-string"),
		m_date_time_term("urn:vocab:test#date-time"),
		m_date_term("urn:vocab:test#date"),
		m_time_term("urn:vocab:test#time"),
		m_object_term("urn:vocab:test#simple-object")
	{
		// initialize our initial term set
		m_plain_int_term.term_type = Vocabulary::TYPE_INT16;
		m_string_term.term_type = Vocabulary::TYPE_STRING;
		m_date_time_term.term_type = Vocabulary::TYPE_DATE_TIME;
		m_date_term.term_type = Vocabulary::TYPE_DATE;
		m_time_term.term_type = Vocabulary::TYPE_TIME;
		m_object_term.term_type = Vocabulary::TYPE_OBJECT;
		m_date_time_term.term_format = "%Y-%m-%d %H:%M:%S";
		m_date_term.term_format = "%Y-%m-%d";
		m_time_term.term_format = "%H:%M:%S";

		m_vocabulary.addTerm(m_plain_int_term);
		m_vocabulary.addTerm(m_string_term);
		m_vocabulary.addTerm(m_date_time_term);
		m_vocabulary.addTerm(m_date_term);
		m_vocabulary.addTerm(m_time_term);
		m_vocabulary.addTerm(m_object_term);

		m_plain_int_term.term_ref = m_vocabulary.findTerm(m_plain_int_term.term_id);
		m_string_term.term_ref = m_vocabulary.findTerm(m_string_term.term_id);
		m_date_time_term.term_ref = m_vocabulary.findTerm(m_date_time_term.term_id);
		m_date_term.term_ref = m_vocabulary.findTerm(m_date_term.term_id);
		m_time_term.term_ref = m_vocabulary.findTerm(m_time_term.term_id);
		m_object_term.term_ref = m_vocabulary.findTerm(m_object_term.term_id);
	}
	~ComparisonTests_F() {
	}
	void checkStringComparisonTrue(EventPtr e, Comparison::ComparisonType type, const std::string& configured_string) {
		Comparison c(m_string_term);
		c.configure(type, configured_string);
		BOOST_CHECK(c.evaluate(*e));
	}
	void checkStringComparisonTrue(const std::string& value_for_string_term, Comparison::ComparisonType type, const std::string& configured_string) {
		EventPtr e(m_event_factory.create(m_object_term.term_ref));
		e->setString(m_string_term.term_ref, value_for_string_term);
		Comparison c(m_string_term);
		c.configure(type, configured_string);
		BOOST_CHECK(c.evaluate(*e));
	}
	void checkStringComparisonFalse(EventPtr e, Comparison::ComparisonType type, const std::string& configured_string) {
		Comparison c(m_string_term);
		c.configure(type, configured_string);
		BOOST_CHECK(! c.evaluate(*e));
	}
	void checkStringComparisonFalse(const std::string& value_for_string_term, Comparison::ComparisonType type, const std::string& configured_string) {
		EventPtr e(m_event_factory.create(m_object_term.term_ref));
		e->setString(m_string_term.term_ref, value_for_string_term);
		Comparison c(m_string_term);
		c.configure(type, configured_string);
		BOOST_CHECK(! c.evaluate(*e));
	}

	EventFactory		m_event_factory;
	Vocabulary			m_vocabulary;
	Vocabulary::Term	m_plain_int_term;
	Vocabulary::Term	m_string_term;
	Vocabulary::Term	m_date_time_term;
	Vocabulary::Term	m_date_term;
	Vocabulary::Term	m_time_term;
	Vocabulary::Term	m_object_term;
};

BOOST_FIXTURE_TEST_SUITE(ComparisonTests_S, ComparisonTests_F)

BOOST_AUTO_TEST_CASE(checkThrowIfInvalidType) {
	Comparison c(m_plain_int_term);
	PionDateTime t;
	BOOST_CHECK_THROW(c.configure(Comparison::TYPE_SAME_DATE_TIME, t), Comparison::InvalidTypeForTermException);
	BOOST_CHECK_THROW(c.configure(Comparison::TYPE_CONTAINS, "error"), Comparison::InvalidTypeForTermException);
}

BOOST_AUTO_TEST_CASE(checkThrowIfInvalidValue) {
	Comparison c(m_plain_int_term);
	BOOST_CHECK_THROW(c.configure(Comparison::TYPE_EQUALS), Comparison::InvalidValueForTypeException);
	BOOST_CHECK_THROW(c.configure(Comparison::TYPE_LESS_THAN, "error"), Comparison::InvalidValueForTypeException);
	BOOST_CHECK_THROW(c.configure(Comparison::TYPE_GREATER_THAN, 28.3), Comparison::InvalidValueForTypeException);
}

BOOST_AUTO_TEST_CASE(checkEventTypeComparisons) {
	EventPtr event_ptr(m_event_factory.create(m_object_term.term_ref));

	Comparison c(m_object_term);
	c.configure(Comparison::TYPE_IS_DEFINED);
	BOOST_CHECK(c.evaluate(*event_ptr));
	c.configure(Comparison::TYPE_IS_NOT_DEFINED);
	BOOST_CHECK(! c.evaluate(*event_ptr));

	Comparison c2(m_plain_int_term);
	c2.configure(Comparison::TYPE_IS_DEFINED);
	BOOST_CHECK(! c2.evaluate(*event_ptr));
	c2.configure(Comparison::TYPE_IS_NOT_DEFINED);
	BOOST_CHECK(c2.evaluate(*event_ptr));
}

BOOST_AUTO_TEST_CASE(checkGenericComparisons) {
	EventPtr event_ptr(m_event_factory.create(m_object_term.term_ref));
	event_ptr->setInt(m_plain_int_term.term_ref, 100);
	Comparison c(m_plain_int_term);
	
	c.configure(Comparison::TYPE_FALSE);
	BOOST_CHECK(! c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_TRUE);
	BOOST_CHECK(c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_IS_DEFINED);
	BOOST_CHECK(c.evaluate(*event_ptr));

	Comparison c2(m_date_term);
	c2.configure(Comparison::TYPE_IS_NOT_DEFINED);
	BOOST_CHECK(c2.evaluate(*event_ptr));
}

BOOST_AUTO_TEST_CASE(checkInt16Comparisons) {
	EventPtr event_ptr(m_event_factory.create(m_object_term.term_ref));
	event_ptr->setInt(m_plain_int_term.term_ref, 100);
	Comparison c(m_plain_int_term);
	
	c.configure(Comparison::TYPE_EQUALS, static_cast<std::int32_t>(100));
	BOOST_CHECK(c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_NOT_EQUALS, static_cast<std::int32_t>(101));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_GREATER_THAN, static_cast<std::int32_t>(99));
	BOOST_CHECK(c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_LESS_THAN, static_cast<std::int32_t>(101));
	BOOST_CHECK(c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_GREATER_OR_EQUAL, static_cast<std::int32_t>(100));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_GREATER_OR_EQUAL, static_cast<std::int32_t>(10));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_LESS_OR_EQUAL, static_cast<std::int32_t>(100));
	BOOST_CHECK(c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_LESS_OR_EQUAL, static_cast<std::int32_t>(1000));
	BOOST_CHECK(c.evaluate(*event_ptr));
}

BOOST_AUTO_TEST_CASE(checkStringComparisons) {
	EventPtr event_ptr(m_event_factory.create(m_object_term.term_ref));
	event_ptr->setString(m_string_term.term_ref, "Atomic");
	Comparison c(m_string_term);
	
	c.configure(Comparison::TYPE_EXACT_MATCH, "Atomic");
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_EXACT_MATCH, "Atom");
	BOOST_CHECK(! c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_NOT_EXACT_MATCH, "Atom");
	BOOST_CHECK(c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_NOT_EXACT_MATCH, "Pion");
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_NOT_EXACT_MATCH, "Atomic");
	BOOST_CHECK(! c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_CONTAINS, "tom");
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_CONTAINS, "tack");
	BOOST_CHECK(! c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_CONTAINS, "Atomic");
	BOOST_CHECK(c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_CONTAINS, "-Atomic-");
	BOOST_CHECK(! c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_NOT_CONTAINS, "tack");
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_NOT_CONTAINS, "tom");
	BOOST_CHECK(! c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_STARTS_WITH, "At");
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_STARTS_WITH, "Add");
	BOOST_CHECK(! c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_STARTS_WITH, "Atomic");
	BOOST_CHECK(c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_STARTS_WITH, "Atomic7");
	BOOST_CHECK(! c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_NOT_STARTS_WITH, "Add");
	BOOST_CHECK(c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_NOT_STARTS_WITH, "mic");
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_NOT_STARTS_WITH, "At");
	BOOST_CHECK(! c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_ENDS_WITH, "mic");
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_ENDS_WITH, "mack");
	BOOST_CHECK(! c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_ENDS_WITH, "Atomic");
	BOOST_CHECK(c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_ENDS_WITH, "pre-Atomic");
	BOOST_CHECK(! c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_NOT_ENDS_WITH, "Bic");
	BOOST_CHECK(c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_NOT_ENDS_WITH, "tom");
	BOOST_CHECK(c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_NOT_ENDS_WITH, "mic");
	BOOST_CHECK(! c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_ORDERED_BEFORE, "Bat");
	BOOST_CHECK(c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_ORDERED_BEFORE, "Ata");
	BOOST_CHECK(! c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_NOT_ORDERED_BEFORE, "Ata");
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_NOT_ORDERED_BEFORE, "Bat");
	BOOST_CHECK(! c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_ORDERED_AFTER, "Ata");
	BOOST_CHECK(c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_ORDERED_AFTER, "Bat");
	BOOST_CHECK(! c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_NOT_ORDERED_AFTER, "Bat");
	BOOST_CHECK(c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_NOT_ORDERED_AFTER, "Ata");
	BOOST_CHECK(! c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_REGEX, "^A.o.*c$");
	BOOST_CHECK(c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_REGEX, "Bat");
	BOOST_CHECK(! c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_NOT_REGEX, "AA+");
	BOOST_CHECK(c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_NOT_REGEX, "A.*c");
	BOOST_CHECK(! c.evaluate(*event_ptr));

	event_ptr->clear(m_string_term.term_ref);
	event_ptr->setString(m_string_term.term_ref, "GET /favicon.ico HTTP/1.1");
	c.configure(Comparison::TYPE_REGEX, "\\.(png|gif|jpg|jpeg|ico)");
	BOOST_CHECK(c.evaluate(*event_ptr));
}

BOOST_AUTO_TEST_CASE(checkUtf8StringComparisons) {
	EventPtr e(m_event_factory.create(m_object_term.term_ref));
	e->setString(m_string_term.term_ref, "Äiti was here");

	checkStringComparisonTrue(e, Comparison::TYPE_EXACT_MATCH, "Äiti was here");
	checkStringComparisonFalse(e, Comparison::TYPE_EXACT_MATCH, "Äiti");

	checkStringComparisonTrue(e, Comparison::TYPE_NOT_EXACT_MATCH, "Äiti");
	checkStringComparisonFalse(e, Comparison::TYPE_NOT_EXACT_MATCH, "Äiti was here");

	checkStringComparisonTrue(e, Comparison::TYPE_EXACT_MATCH_PRIMARY, "Aiti was here");
	checkStringComparisonFalse(e, Comparison::TYPE_EXACT_MATCH_PRIMARY, "Äiti");

	checkStringComparisonTrue(e, Comparison::TYPE_NOT_EXACT_MATCH_PRIMARY, "Äiti");
	checkStringComparisonFalse(e, Comparison::TYPE_NOT_EXACT_MATCH_PRIMARY, "Aiti was here");

	checkStringComparisonTrue(e, Comparison::TYPE_CONTAINS, "here");
	checkStringComparisonFalse(e, Comparison::TYPE_CONTAINS, "hére");

	checkStringComparisonTrue(e, Comparison::TYPE_NOT_CONTAINS, "hére");
	checkStringComparisonFalse(e, Comparison::TYPE_NOT_CONTAINS, "here");

	checkStringComparisonTrue(e, Comparison::TYPE_CONTAINS_PRIMARY, "hére");
	checkStringComparisonFalse(e, Comparison::TYPE_CONTAINS_PRIMARY, "wasp");

	checkStringComparisonTrue(e, Comparison::TYPE_NOT_CONTAINS_PRIMARY, "wasp");
	checkStringComparisonFalse(e, Comparison::TYPE_NOT_CONTAINS_PRIMARY, "hére");

	checkStringComparisonTrue(e, Comparison::TYPE_STARTS_WITH, "Ä");
	checkStringComparisonFalse(e, Comparison::TYPE_STARTS_WITH, "A");

	checkStringComparisonTrue(e, Comparison::TYPE_NOT_STARTS_WITH, "Aiti");
	checkStringComparisonFalse(e, Comparison::TYPE_NOT_STARTS_WITH, "Äiti");

	checkStringComparisonTrue(e, Comparison::TYPE_STARTS_WITH_PRIMARY, "a");
	checkStringComparisonFalse(e, Comparison::TYPE_STARTS_WITH_PRIMARY, "iti");

	checkStringComparisonTrue(e, Comparison::TYPE_NOT_STARTS_WITH_PRIMARY, "AA");
	checkStringComparisonFalse(e, Comparison::TYPE_NOT_STARTS_WITH_PRIMARY, "Aiti");

	checkStringComparisonTrue(e, Comparison::TYPE_ENDS_WITH, "e");
	checkStringComparisonFalse(e, Comparison::TYPE_ENDS_WITH, "é");

	checkStringComparisonTrue(e, Comparison::TYPE_NOT_ENDS_WITH, "hére");
	checkStringComparisonFalse(e, Comparison::TYPE_NOT_ENDS_WITH, "here");

	checkStringComparisonTrue(e, Comparison::TYPE_REGEX, "^Ä.t.*e$");
	checkStringComparisonFalse(e, Comparison::TYPE_REGEX, "Bat");

	checkStringComparisonTrue(e, Comparison::TYPE_NOT_REGEX, "AA+");
	checkStringComparisonFalse(e, Comparison::TYPE_NOT_REGEX, "Ä.*e");
}

BOOST_AUTO_TEST_CASE(checkCompareStringStartsWith) {
	Comparison c1(m_string_term);
	c1.configure(Comparison::TYPE_STARTS_WITH, "äb");
	Comparison c2(m_string_term);
	c2.configure(Comparison::TYPE_STARTS_WITH_PRIMARY, "AB");
	Comparison c3(m_string_term);
	c3.configure(Comparison::TYPE_NOT_STARTS_WITH, "ab");
	Comparison c4(m_string_term);
	c4.configure(Comparison::TYPE_NOT_STARTS_WITH_PRIMARY, "ab");

	EventPtr e1(m_event_factory.create(m_object_term.term_ref));
	e1->setString(m_string_term.term_ref, "");
	EventPtr e2(m_event_factory.create(m_object_term.term_ref));
	e2->setString(m_string_term.term_ref, "ä");
	EventPtr e3(m_event_factory.create(m_object_term.term_ref));
	e3->setString(m_string_term.term_ref, "äb");
	EventPtr e4(m_event_factory.create(m_object_term.term_ref));
	e4->setString(m_string_term.term_ref, "äbc");
	EventPtr e5(m_event_factory.create(m_object_term.term_ref));         // It's important to have one long string here, because CompareStringStartsWith 
	e5->setString(m_string_term.term_ref, "äbcdefghijklmnopqrstuvwxyz"); // may only convert part of the input from UTF-8 to UTF-16, for efficiency reasons.
	EventPtr e6(m_event_factory.create(m_object_term.term_ref));
	e6->setString(m_string_term.term_ref, "ab");
	EventPtr e7(m_event_factory.create(m_object_term.term_ref));
	e7->setString(m_string_term.term_ref, "äz");

	BOOST_CHECK(! c1.evaluate(*e1));
	BOOST_CHECK(! c1.evaluate(*e2));
	BOOST_CHECK(c1.evaluate(*e3));
	BOOST_CHECK(c1.evaluate(*e4));
	BOOST_CHECK(c1.evaluate(*e5));
	BOOST_CHECK(! c1.evaluate(*e6));
	BOOST_CHECK(! c1.evaluate(*e7));

	BOOST_CHECK(! c2.evaluate(*e1));
	BOOST_CHECK(! c2.evaluate(*e2));
	BOOST_CHECK(c2.evaluate(*e3));
	BOOST_CHECK(c2.evaluate(*e4));
	BOOST_CHECK(c2.evaluate(*e5));
	BOOST_CHECK(c2.evaluate(*e6));
	BOOST_CHECK(! c2.evaluate(*e7));

	BOOST_CHECK(c3.evaluate(*e1));
	BOOST_CHECK(c3.evaluate(*e2));
	BOOST_CHECK(c3.evaluate(*e3));
	BOOST_CHECK(c3.evaluate(*e4));
	BOOST_CHECK(c3.evaluate(*e5));
	BOOST_CHECK(! c3.evaluate(*e6));
	BOOST_CHECK(c3.evaluate(*e7));

	BOOST_CHECK(c4.evaluate(*e1));
	BOOST_CHECK(c4.evaluate(*e2));
	BOOST_CHECK(! c4.evaluate(*e3));
	BOOST_CHECK(! c4.evaluate(*e4));
	BOOST_CHECK(! c4.evaluate(*e5));
	BOOST_CHECK(! c4.evaluate(*e6));
	BOOST_CHECK(c4.evaluate(*e7));
}

BOOST_AUTO_TEST_CASE(checkCompareStringEndsWith) {
	Comparison c1(m_string_term);
	c1.configure(Comparison::TYPE_ENDS_WITH, "ÿz");
	Comparison c2(m_string_term);
	c2.configure(Comparison::TYPE_ENDS_WITH_PRIMARY, "YZ");
	Comparison c3(m_string_term);
	c3.configure(Comparison::TYPE_NOT_ENDS_WITH, "yz");
	Comparison c4(m_string_term);
	c4.configure(Comparison::TYPE_NOT_ENDS_WITH_PRIMARY, "yz");

	EventPtr e1(m_event_factory.create(m_object_term.term_ref));
	e1->setString(m_string_term.term_ref, "");
	EventPtr e2(m_event_factory.create(m_object_term.term_ref));
	e2->setString(m_string_term.term_ref, "z");
	EventPtr e3(m_event_factory.create(m_object_term.term_ref));
	e3->setString(m_string_term.term_ref, "ÿz");
	EventPtr e4(m_event_factory.create(m_object_term.term_ref));
	e4->setString(m_string_term.term_ref, "xÿz");
	EventPtr e5(m_event_factory.create(m_object_term.term_ref));         // It's important to have one long string here, because CompareStringStartsWith 
	e5->setString(m_string_term.term_ref, "äbcdefghijklmnopqrstuvwxÿz"); // may only convert part of the input from UTF-8 to UTF-16, for efficiency reasons.
	EventPtr e6(m_event_factory.create(m_object_term.term_ref));
	e6->setString(m_string_term.term_ref, "yz");
	EventPtr e7(m_event_factory.create(m_object_term.term_ref));
	e7->setString(m_string_term.term_ref, "ÿq");

	BOOST_CHECK(! c1.evaluate(*e1));
	BOOST_CHECK(! c1.evaluate(*e2));
	BOOST_CHECK(c1.evaluate(*e3));
	BOOST_CHECK(c1.evaluate(*e4));
	BOOST_CHECK(c1.evaluate(*e5));
	BOOST_CHECK(! c1.evaluate(*e6));
	BOOST_CHECK(! c1.evaluate(*e7));

	BOOST_CHECK(! c2.evaluate(*e1));
	BOOST_CHECK(! c2.evaluate(*e2));
	BOOST_CHECK(c2.evaluate(*e3));
	BOOST_CHECK(c2.evaluate(*e4));
	BOOST_CHECK(c2.evaluate(*e5));
	BOOST_CHECK(c2.evaluate(*e6));
	BOOST_CHECK(! c2.evaluate(*e7));

	BOOST_CHECK(c3.evaluate(*e1));
	BOOST_CHECK(c3.evaluate(*e2));
	BOOST_CHECK(c3.evaluate(*e3));
	BOOST_CHECK(c3.evaluate(*e4));
	BOOST_CHECK(c3.evaluate(*e5));
	BOOST_CHECK(! c3.evaluate(*e6));
	BOOST_CHECK(c3.evaluate(*e7));

	BOOST_CHECK(c4.evaluate(*e1));
	BOOST_CHECK(c4.evaluate(*e2));
	BOOST_CHECK(! c4.evaluate(*e3));
	BOOST_CHECK(! c4.evaluate(*e4));
	BOOST_CHECK(! c4.evaluate(*e5));
	BOOST_CHECK(! c4.evaluate(*e6));
	BOOST_CHECK(c4.evaluate(*e7));
}

/*
ICU implements the Unicode Collation Algorithm, which is a multi-level sort.

   1. If there are any differences in base letters, that determines the result
   2. Otherwise, if there are any differences in accents, that determines the results
   3. Otherwise, if there are any differences in case, that determines the results
   4. Otherwise, if there are any differences in punctuation, that determines the results

See http://www.unicode.org/reports/tr10/ for more information.
*/
BOOST_AUTO_TEST_CASE(checkDefaultOrderComparisonsWithBaseLetterDifferences) {
	checkStringComparisonTrue("ä1", Comparison::TYPE_ORDERED_BEFORE, "a2");
	checkStringComparisonTrue("a1", Comparison::TYPE_ORDERED_BEFORE, "ä2");
	checkStringComparisonTrue("A1", Comparison::TYPE_ORDERED_BEFORE, "a2");
	checkStringComparisonTrue("a1", Comparison::TYPE_ORDERED_BEFORE, "A2");

	checkStringComparisonFalse("ä1", Comparison::TYPE_ORDERED_AFTER, "a2");
	checkStringComparisonFalse("a1", Comparison::TYPE_ORDERED_AFTER, "ä2");
	checkStringComparisonFalse("A1", Comparison::TYPE_ORDERED_AFTER, "a2");
	checkStringComparisonFalse("a1", Comparison::TYPE_ORDERED_AFTER, "A2");

	checkStringComparisonFalse("a2", Comparison::TYPE_ORDERED_BEFORE, "ä1");
	checkStringComparisonFalse("ä2", Comparison::TYPE_ORDERED_BEFORE, "a1");
	checkStringComparisonFalse("a2", Comparison::TYPE_ORDERED_BEFORE, "A1");
	checkStringComparisonFalse("A2", Comparison::TYPE_ORDERED_BEFORE, "a1");

	checkStringComparisonTrue("a2", Comparison::TYPE_ORDERED_AFTER, "ä1");
	checkStringComparisonTrue("ä2", Comparison::TYPE_ORDERED_AFTER, "a1");
	checkStringComparisonTrue("a2", Comparison::TYPE_ORDERED_AFTER, "A1");
	checkStringComparisonTrue("A2", Comparison::TYPE_ORDERED_AFTER, "a1");
}

BOOST_AUTO_TEST_CASE(checkDefaultOrderComparisonsWithAccentDifferences) {
	checkStringComparisonTrue("a", Comparison::TYPE_ORDERED_BEFORE, "Ä");
	checkStringComparisonTrue("A", Comparison::TYPE_ORDERED_BEFORE, "ä");

	checkStringComparisonFalse("a", Comparison::TYPE_ORDERED_AFTER, "Ä");
	checkStringComparisonFalse("A", Comparison::TYPE_ORDERED_AFTER, "ä");

	checkStringComparisonFalse("Ä", Comparison::TYPE_ORDERED_BEFORE, "a");
	checkStringComparisonFalse("ä", Comparison::TYPE_ORDERED_BEFORE, "A");

	checkStringComparisonTrue("Ä", Comparison::TYPE_ORDERED_AFTER, "a");
	checkStringComparisonTrue("ä", Comparison::TYPE_ORDERED_AFTER, "A");
}

BOOST_AUTO_TEST_CASE(checkDefaultOrderComparisonsWithCaseDifferences) {
	checkStringComparisonTrue("a", Comparison::TYPE_ORDERED_BEFORE, "A");
	checkStringComparisonTrue("ä", Comparison::TYPE_ORDERED_BEFORE, "Ä");

	checkStringComparisonFalse("a", Comparison::TYPE_ORDERED_AFTER, "A");
	checkStringComparisonFalse("ä", Comparison::TYPE_ORDERED_AFTER, "Ä");

	checkStringComparisonFalse("A", Comparison::TYPE_ORDERED_BEFORE, "a");
	checkStringComparisonFalse("Ä", Comparison::TYPE_ORDERED_BEFORE, "ä");

	checkStringComparisonTrue("A", Comparison::TYPE_ORDERED_AFTER, "a");
	checkStringComparisonTrue("Ä", Comparison::TYPE_ORDERED_AFTER, "ä");
}

/*
Note that the Unicode Collation Algorithm has the behavior that increasing the collation strength might 
cause two strings that were equal to become ordered, but it can't cause the order of two strings to reverse.
See http://www.unicode.org/reports/tr10/ for more information.
*/
BOOST_AUTO_TEST_CASE(checkPrimaryVsDefaultOrderComparisons) {
	checkStringComparisonTrue("a", Comparison::TYPE_ORDERED_BEFORE, "Ä");
	checkStringComparisonFalse("a", Comparison::TYPE_ORDERED_BEFORE_PRIMARY, "Ä");
	checkStringComparisonFalse("a", Comparison::TYPE_ORDERED_AFTER, "Ä");
	checkStringComparisonFalse("a", Comparison::TYPE_ORDERED_AFTER_PRIMARY, "Ä");

	checkStringComparisonTrue("A", Comparison::TYPE_ORDERED_BEFORE, "ä");
	checkStringComparisonFalse("A", Comparison::TYPE_ORDERED_BEFORE_PRIMARY, "ä");
	checkStringComparisonFalse("A", Comparison::TYPE_ORDERED_AFTER, "ä");
	checkStringComparisonFalse("A", Comparison::TYPE_ORDERED_AFTER_PRIMARY, "ä");

	checkStringComparisonTrue("a", Comparison::TYPE_ORDERED_BEFORE, "A");
	checkStringComparisonFalse("a", Comparison::TYPE_ORDERED_BEFORE_PRIMARY, "A");
	checkStringComparisonFalse("a", Comparison::TYPE_ORDERED_AFTER, "A");
	checkStringComparisonFalse("a", Comparison::TYPE_ORDERED_AFTER_PRIMARY, "A");
}

BOOST_AUTO_TEST_CASE(checkOrderComparisonsBetweenStringsDifferingOnlyInAccents) {
	EventPtr event_ptr(m_event_factory.create(m_object_term.term_ref));
	event_ptr->setString(m_string_term.term_ref, "Atômic");
	Comparison c(m_string_term);

	// test default ordering (collator strength = UCOL_TERTIARY)
	c.configure(Comparison::TYPE_ORDERED_BEFORE, "Atomic");
	BOOST_CHECK(! c.evaluate(*event_ptr));
	c.configure(Comparison::TYPE_EXACT_MATCH, "Atomic");
	BOOST_CHECK(! c.evaluate(*event_ptr));
	c.configure(Comparison::TYPE_ORDERED_AFTER, "Atomic");
	BOOST_CHECK(c.evaluate(*event_ptr));

	// same as previous block, but with collator strength = UCOL_PRIMARY
	c.configure(Comparison::TYPE_ORDERED_BEFORE_PRIMARY, "Atomic");
	BOOST_CHECK(! c.evaluate(*event_ptr));
	c.configure(Comparison::TYPE_EXACT_MATCH_PRIMARY, "Atomic");
	BOOST_CHECK(c.evaluate(*event_ptr));
	c.configure(Comparison::TYPE_ORDERED_AFTER_PRIMARY, "Atomic");
	BOOST_CHECK(! c.evaluate(*event_ptr));

	// the negations of the Comparisons in the previous two blocks
	c.configure(Comparison::TYPE_NOT_ORDERED_BEFORE, "Atomic");
	BOOST_CHECK(c.evaluate(*event_ptr));
	c.configure(Comparison::TYPE_NOT_EXACT_MATCH, "Atomic");
	BOOST_CHECK(c.evaluate(*event_ptr));
	c.configure(Comparison::TYPE_NOT_ORDERED_AFTER, "Atomic");
	BOOST_CHECK(! c.evaluate(*event_ptr));
	c.configure(Comparison::TYPE_NOT_ORDERED_BEFORE_PRIMARY, "Atomic");
	BOOST_CHECK(c.evaluate(*event_ptr));
	c.configure(Comparison::TYPE_NOT_EXACT_MATCH_PRIMARY, "Atomic");
	BOOST_CHECK(! c.evaluate(*event_ptr));
	c.configure(Comparison::TYPE_NOT_ORDERED_AFTER_PRIMARY, "Atomic");
	BOOST_CHECK(c.evaluate(*event_ptr));
}

BOOST_AUTO_TEST_CASE(checkComparisonsWithValidUtf8FourByteSequences) {
	const char UTF8_ENCODED_TEST_CHAR_ARRAY[] = {
		(char)0xF0, (char)0x90, (char)0x82, (char)0x88,		// UTF-8 encoding of U+10088 (LINEAR B IDEOGRAM B107F SHE-GOAT)
		(char)0xE2, (char)0x82, (char)0xA8,					// UTF-8 encoding of U+2260 (NOT EQUAL TO)
		(char)0xF0, (char)0x90, (char)0x82, (char)0x89};	// UTF-8 encoding of U+10089 (LINEAR B IDEOGRAM B107M HE-GOAT)
	const std::string UTF8_ENCODED_TEST_STRING(UTF8_ENCODED_TEST_CHAR_ARRAY, sizeof(UTF8_ENCODED_TEST_CHAR_ARRAY));
	const char UTF8_ENCODED_TEST_CHAR_ARRAY_2[] = {
		(char)0xF0, (char)0x90, (char)0x82, (char)0x89,		// UTF-8 encoding of U+10089 (LINEAR B IDEOGRAM B107M HE-GOAT)
		(char)0xE2, (char)0x89, (char)0x88,					// UTF-8 encoding of U+2248 (ALMOST EQUAL TO)
		(char)0xF0, (char)0x90, (char)0x82, (char)0x88};	// UTF-8 encoding of U+10088 (LINEAR B IDEOGRAM B107F SHE-GOAT)
	const std::string UTF8_ENCODED_TEST_STRING_2(UTF8_ENCODED_TEST_CHAR_ARRAY_2, sizeof(UTF8_ENCODED_TEST_CHAR_ARRAY_2));

	Comparison c1(m_string_term);
	c1.configure(Comparison::TYPE_CONTAINS, "\xE2\x82\xA8"); // NOT EQUAL TO
	Comparison c1p(m_string_term);
	c1p.configure(Comparison::TYPE_CONTAINS_PRIMARY, "\xE2\x82\xA8"); // NOT EQUAL TO
	Comparison c2(m_string_term);
	c2.configure(Comparison::TYPE_STARTS_WITH, "\xF0\x90\x82\x88"); // SHE-GOAT
	Comparison c2p(m_string_term);
	c2p.configure(Comparison::TYPE_STARTS_WITH_PRIMARY, "\xF0\x90\x82\x88"); // SHE-GOAT
	Comparison c3(m_string_term);
	c3.configure(Comparison::TYPE_ENDS_WITH, "\xF0\x90\x82\x89"); // HE-GOAT
	Comparison c3p(m_string_term);
	c3p.configure(Comparison::TYPE_ENDS_WITH_PRIMARY, "\xF0\x90\x82\x89"); // HE-GOAT

	EventPtr e1(m_event_factory.create(m_object_term.term_ref));
	e1->setString(m_string_term.term_ref, UTF8_ENCODED_TEST_STRING);
	EventPtr e2(m_event_factory.create(m_object_term.term_ref));
	e2->setString(m_string_term.term_ref, UTF8_ENCODED_TEST_STRING_2);

	BOOST_CHECK(c1.evaluate(*e1));
	BOOST_CHECK(! c1.evaluate(*e2));
	BOOST_CHECK(c1p.evaluate(*e1));
	BOOST_CHECK(! c1p.evaluate(*e2));
	BOOST_CHECK(c2.evaluate(*e1));
	BOOST_CHECK(! c2.evaluate(*e2));
	BOOST_CHECK(c2p.evaluate(*e1));
	BOOST_CHECK(! c2p.evaluate(*e2));
	BOOST_CHECK(c3.evaluate(*e1));
	BOOST_CHECK(! c3.evaluate(*e2));
	BOOST_CHECK(c3p.evaluate(*e1));
	BOOST_CHECK(! c3p.evaluate(*e2));
}

BOOST_AUTO_TEST_CASE(checkConjoinedLetter) {
	EventPtr event_ptr(m_event_factory.create(m_object_term.term_ref));
	event_ptr->setString(m_string_term.term_ref, "Caesar");
	Comparison c(m_string_term);

	c.configure(Comparison::TYPE_EXACT_MATCH, "Cæsar");
	BOOST_CHECK(! c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_EXACT_MATCH_PRIMARY, "Cæsar");
	BOOST_CHECK(c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_CONTAINS, "æ");
	BOOST_CHECK(! c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_CONTAINS_PRIMARY, "æ");
	BOOST_CHECK(c.evaluate(*event_ptr));
}

BOOST_AUTO_TEST_CASE(checkCombiningDiacriticalMarks) {
	std::string primary_string = "bad";

	// test string where a character has a combining diacritical mark
	char UTF8_ENCODED_TEST_CHAR_ARRAY[] = {
		0x62,								// 'b'
		0x61,								// 'a'
		(char)0xCC, (char)0x80,				// UTF-8 encoding of U+0300 (COMBINING GRAVE ACCENT)
		0x64};								// 'd'
	const std::string UTF8_ENCODED_TEST_STRING(UTF8_ENCODED_TEST_CHAR_ARRAY, sizeof(UTF8_ENCODED_TEST_CHAR_ARRAY));

	// test string where a character has two combining diacritical marks
	char UTF8_ENCODED_TEST_CHAR_ARRAY_2[] = {
		0x62,								// 'b'
		0x61,								// 'a'
		(char)0xCC, (char)0x80,				// UTF-8 encoding of U+0300 (COMBINING GRAVE ACCENT)
		(char)0xCC, (char)0xA5,				// UTF-8 encoding of U+0325 (COMBINING RING BELOW)
		0x64};								// 'd'
	const std::string UTF8_ENCODED_TEST_STRING_2(UTF8_ENCODED_TEST_CHAR_ARRAY_2, sizeof(UTF8_ENCODED_TEST_CHAR_ARRAY_2));

	checkStringComparisonTrue(primary_string, Comparison::TYPE_ORDERED_BEFORE, UTF8_ENCODED_TEST_STRING);
	checkStringComparisonTrue(primary_string, Comparison::TYPE_ORDERED_BEFORE, UTF8_ENCODED_TEST_STRING_2);
	checkStringComparisonFalse(primary_string, Comparison::TYPE_ORDERED_BEFORE_PRIMARY, UTF8_ENCODED_TEST_STRING);
	checkStringComparisonFalse(primary_string, Comparison::TYPE_ORDERED_BEFORE_PRIMARY, UTF8_ENCODED_TEST_STRING_2);

	checkStringComparisonFalse(primary_string, Comparison::TYPE_EXACT_MATCH, UTF8_ENCODED_TEST_STRING);
	checkStringComparisonFalse(primary_string, Comparison::TYPE_EXACT_MATCH, UTF8_ENCODED_TEST_STRING_2);
	checkStringComparisonTrue(primary_string, Comparison::TYPE_EXACT_MATCH_PRIMARY, UTF8_ENCODED_TEST_STRING);
	checkStringComparisonTrue(primary_string, Comparison::TYPE_EXACT_MATCH_PRIMARY, UTF8_ENCODED_TEST_STRING_2);

	checkStringComparisonFalse(primary_string, Comparison::TYPE_ORDERED_AFTER, UTF8_ENCODED_TEST_STRING);
	checkStringComparisonFalse(primary_string, Comparison::TYPE_ORDERED_AFTER, UTF8_ENCODED_TEST_STRING_2);
	checkStringComparisonFalse(primary_string, Comparison::TYPE_ORDERED_AFTER_PRIMARY, UTF8_ENCODED_TEST_STRING);
	checkStringComparisonFalse(primary_string, Comparison::TYPE_ORDERED_AFTER_PRIMARY, UTF8_ENCODED_TEST_STRING_2);
}

BOOST_AUTO_TEST_CASE(checkConfigureComparisonWithInvalidUtf8String) {
	Comparison c(m_string_term);

	// C3 is the first byte of Ä in UTF-8 encoding
	BOOST_CHECK_THROW(c.configure(Comparison::TYPE_STARTS_WITH, "\xC3"), Comparison::UnexpectedICUErrorCodeException);
}

BOOST_AUTO_TEST_CASE(checkConfigureComparisonWithEmptyString) {
	EventPtr e1(m_event_factory.create(m_object_term.term_ref));
	e1->setString(m_string_term.term_ref, "abc");
	EventPtr e2(m_event_factory.create(m_object_term.term_ref));
	e2->setString(m_string_term.term_ref, "");

	Comparison c(m_string_term);

	c.configure(Comparison::TYPE_EXACT_MATCH, "");
	BOOST_CHECK(! c.evaluate(*e1));
	BOOST_CHECK(c.evaluate(*e2));

	c.configure(Comparison::TYPE_STARTS_WITH, "");
	BOOST_CHECK(c.evaluate(*e1));
	BOOST_CHECK(c.evaluate(*e2));

	c.configure(Comparison::TYPE_ENDS_WITH, "");
	BOOST_CHECK(c.evaluate(*e1));
	BOOST_CHECK(c.evaluate(*e2));

	c.configure(Comparison::TYPE_ORDERED_BEFORE, "");
	BOOST_CHECK(! c.evaluate(*e1));
	BOOST_CHECK(! c.evaluate(*e2));

	c.configure(Comparison::TYPE_ORDERED_AFTER, "");
	BOOST_CHECK(c.evaluate(*e1));
	BOOST_CHECK(! c.evaluate(*e2));

	c.configure(Comparison::TYPE_CONTAINS, "");
	BOOST_CHECK(c.evaluate(*e1));
	BOOST_CHECK(c.evaluate(*e2));
}

BOOST_AUTO_TEST_CASE(checkEvaluateComparisonWithEmptyValue) {
	EventPtr e(m_event_factory.create(m_object_term.term_ref));
	e->setString(m_string_term.term_ref, "");

	Comparison c(m_string_term);

	c.configure(Comparison::TYPE_EXACT_MATCH, "abc");
	BOOST_CHECK(! c.evaluate(*e));
	c.configure(Comparison::TYPE_EXACT_MATCH, "");
	BOOST_CHECK(c.evaluate(*e));

	c.configure(Comparison::TYPE_STARTS_WITH, "abc");
	BOOST_CHECK(! c.evaluate(*e));
	c.configure(Comparison::TYPE_STARTS_WITH, "");
	BOOST_CHECK(c.evaluate(*e));

	c.configure(Comparison::TYPE_ENDS_WITH, "abc");
	BOOST_CHECK(! c.evaluate(*e));
	c.configure(Comparison::TYPE_ENDS_WITH, "");
	BOOST_CHECK(c.evaluate(*e));

	c.configure(Comparison::TYPE_ORDERED_BEFORE, "abc");
	BOOST_CHECK(c.evaluate(*e));
	c.configure(Comparison::TYPE_ORDERED_BEFORE, "");
	BOOST_CHECK(! c.evaluate(*e));

	c.configure(Comparison::TYPE_ORDERED_AFTER, "abc");
	BOOST_CHECK(! c.evaluate(*e));
	c.configure(Comparison::TYPE_ORDERED_AFTER, "");
	BOOST_CHECK(! c.evaluate(*e));

	c.configure(Comparison::TYPE_CONTAINS, "abc");
	BOOST_CHECK(! c.evaluate(*e));
	c.configure(Comparison::TYPE_CONTAINS, "");
	BOOST_CHECK(c.evaluate(*e));
}

BOOST_AUTO_TEST_CASE(checkComparisonCopyWorksForRegex) {
	// prepare event
	EventPtr event_ptr(m_event_factory.create(m_object_term.term_ref));
	event_ptr->setString(m_string_term.term_ref, "Atomic");

	// prepare original comparison
	Comparison c_orig(m_string_term);
	c_orig.configure(Comparison::TYPE_REGEX, "^A.o.*c$");

	// create a copy
	Comparison c_copy(c_orig);

	// make sure it works correctly
	BOOST_CHECK(c_copy.evaluate(*event_ptr));
}

BOOST_AUTO_TEST_CASE(checkDateTimeComparisons) {
	EventPtr event_ptr(m_event_factory.create(m_object_term.term_ref));
	PionTimeFacet f(m_date_time_term.term_format);	
	event_ptr->setDateTime(m_date_time_term.term_ref, f.fromString("2007-12-22 12:22:22"));
	Comparison c(m_date_time_term);
	
	// DateTime comparisons
	c.configure(Comparison::TYPE_SAME_DATE_TIME, f.fromString("2007-12-22 12:22:22"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_NOT_SAME_DATE_TIME, f.fromString("2007-12-22 12:12:22"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_EARLIER_DATE_TIME, f.fromString("2007-12-22 14:22:22"));
	BOOST_CHECK(c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_LATER_DATE_TIME, f.fromString("2007-12-21 12:22:22"));
	BOOST_CHECK(c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_SAME_OR_EARLIER_DATE_TIME, f.fromString("2007-12-22 12:22:22"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_SAME_OR_EARLIER_DATE_TIME, f.fromString("2007-12-22 12:22:23"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_SAME_OR_LATER_DATE_TIME, f.fromString("2007-12-22 12:22:22"));
	BOOST_CHECK(c.evaluate(*event_ptr));

	c.configure(Comparison::TYPE_SAME_OR_LATER_DATE_TIME, f.fromString("2007-12-21 12:22:22"));
	BOOST_CHECK(c.evaluate(*event_ptr));

	f.setFormat("%Y-%m-%d");
	
	// Date comparisons
	c.configure(Comparison::TYPE_SAME_DATE, f.fromString("2007-12-22"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_NOT_SAME_DATE, f.fromString("2006-12-22"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_EARLIER_DATE, f.fromString("2008-01-20"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_LATER_DATE, f.fromString("2006-12-21"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_SAME_OR_EARLIER_DATE, f.fromString("2007-12-22"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_SAME_OR_EARLIER_DATE, f.fromString("2007-12-23"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_SAME_OR_LATER_DATE, f.fromString("2007-12-22"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_SAME_OR_LATER_DATE, f.fromString("2007-09-20"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	f.setFormat("%H:%M:%S");
	
	// Time comparisons
	c.configure(Comparison::TYPE_SAME_TIME, f.fromString("12:22:22"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_NOT_SAME_TIME, f.fromString("12:12:22"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_EARLIER_TIME, f.fromString("14:22:22"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_LATER_TIME, f.fromString("10:22:22"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_SAME_OR_EARLIER_TIME, f.fromString("12:22:22"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_SAME_OR_EARLIER_TIME, f.fromString("12:22:23"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_SAME_OR_LATER_TIME, f.fromString("12:22:22"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_SAME_OR_LATER_TIME, f.fromString("09:22:22"));
	BOOST_CHECK(c.evaluate(*event_ptr));
}

BOOST_AUTO_TEST_CASE(checkDateComparisons) {
	EventPtr event_ptr(m_event_factory.create(m_object_term.term_ref));
	PionTimeFacet f("%Y-%m-%d");
	event_ptr->setDateTime(m_date_term.term_ref, f.fromString("2007-12-22"));
	Comparison c(m_date_term);
	
	// Date comparisons
	c.configure(Comparison::TYPE_SAME_DATE, f.fromString("2007-12-22"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_NOT_SAME_DATE, f.fromString("2006-12-22"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_EARLIER_DATE, f.fromString("2008-01-20"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_LATER_DATE, f.fromString("2006-12-21"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_SAME_OR_EARLIER_DATE, f.fromString("2007-12-22"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_SAME_OR_EARLIER_DATE, f.fromString("2007-12-23"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_SAME_OR_LATER_DATE, f.fromString("2007-12-22"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_SAME_OR_LATER_DATE, f.fromString("2007-09-20"));
	BOOST_CHECK(c.evaluate(*event_ptr));
}

BOOST_AUTO_TEST_CASE(checkTimeComparisons) {
	EventPtr event_ptr(m_event_factory.create(m_object_term.term_ref));
	PionTimeFacet f("%H:%M:%S");
	event_ptr->setDateTime(m_time_term.term_ref, f.fromString("12:22:22"));
	Comparison c(m_time_term);
	
	// Time comparisons
	c.configure(Comparison::TYPE_SAME_TIME, f.fromString("12:22:22"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_NOT_SAME_TIME, f.fromString("12:12:22"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_EARLIER_TIME, f.fromString("14:22:22"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_LATER_TIME, f.fromString("10:22:22"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_SAME_OR_EARLIER_TIME, f.fromString("12:22:22"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_SAME_OR_EARLIER_TIME, f.fromString("12:22:23"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_SAME_OR_LATER_TIME, f.fromString("12:22:22"));
	BOOST_CHECK(c.evaluate(*event_ptr));
	
	c.configure(Comparison::TYPE_SAME_OR_LATER_TIME, f.fromString("09:22:22"));
	BOOST_CHECK(c.evaluate(*event_ptr));
}

BOOST_AUTO_TEST_CASE(checkConfigureDateTimeComparisonsUsingStrings) {
	// Set up an Event to compare against.
	EventPtr event_ptr(m_event_factory.create(m_object_term.term_ref));
	PionTimeFacet fdt(m_date_time_term.term_format);	
	event_ptr->setDateTime(m_date_time_term.term_ref, fdt.fromString("2007-12-22 12:22:22"));
	PionTimeFacet fd(m_date_term.term_format);	
	event_ptr->setDateTime(m_date_term.term_ref, fd.fromString("2007-12-22"));
	PionTimeFacet ft(m_time_term.term_format);	
	event_ptr->setDateTime(m_time_term.term_ref, ft.fromString("12:22:22"));

	// Try some date-time strings.
	Comparison cdt(m_date_time_term);
	cdt.configure(Comparison::TYPE_SAME_DATE_TIME, "2007-12-22 12:22:22");
	BOOST_CHECK(cdt.evaluate(*event_ptr));
	cdt.configure(Comparison::TYPE_SAME_DATE_TIME, "2007-12-22 12:22:34");
	BOOST_CHECK(! cdt.evaluate(*event_ptr));

	// Try some date strings.
	Comparison cd(m_date_term);
	cd.configure(Comparison::TYPE_SAME_DATE, "2007-12-22");
	BOOST_CHECK(cd.evaluate(*event_ptr));
	cd.configure(Comparison::TYPE_SAME_DATE, "2008-12-22");
	BOOST_CHECK(! cd.evaluate(*event_ptr));

	// Try some time strings.
	Comparison ct(m_time_term);
	ct.configure(Comparison::TYPE_SAME_TIME, "12:22:22");
	BOOST_CHECK(ct.evaluate(*event_ptr));
	ct.configure(Comparison::TYPE_SAME_TIME, "12:22:01");
	BOOST_CHECK(! ct.evaluate(*event_ptr));
}

BOOST_AUTO_TEST_CASE(checkRuleChainWithMatchAllComparisonsFalse) {
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<MatchAllComparisons>false</MatchAllComparisons>"
		"<Comparison>"
			"<Term>urn:vocab:test#plain-int</Term>"
			"<Type>less-than</Type>"
			"<Value>10</Value>"
		"</Comparison>"
		"<Comparison>"
			"<Term>urn:vocab:test#simple-string</Term>"
			"<Type>contains</Type>"
			"<Value>Atom</Value>"
		"</Comparison>");

	RuleChain r;
	r.setConfig(m_vocabulary, config_ptr);

	EventPtr e1(m_event_factory.create(m_object_term.term_ref));
	e1->setInt(m_plain_int_term.term_ref, 5);
	e1->setString(m_string_term.term_ref, "Atomic");
	EventPtr e2(m_event_factory.create(m_object_term.term_ref));
	e2->setInt(m_plain_int_term.term_ref, 15);
	e2->setString(m_string_term.term_ref, "Atomic");
	EventPtr e3(m_event_factory.create(m_object_term.term_ref));
	e3->setInt(m_plain_int_term.term_ref, 5);
	e3->setString(m_string_term.term_ref, "Pion");
	EventPtr e4(m_event_factory.create(m_object_term.term_ref));
	e4->setInt(m_plain_int_term.term_ref, 15);
	e4->setString(m_string_term.term_ref, "Pion");

	BOOST_CHECK(r(e1));
	BOOST_CHECK(r(e2));
	BOOST_CHECK(r(e3));
	BOOST_CHECK(! r(e4));

	// Now test some Events where at least one of the Terms being tested is undefined.
	EventPtr e5(m_event_factory.create(m_object_term.term_ref));
	e5->setInt(m_plain_int_term.term_ref, 5);
	EventPtr e6(m_event_factory.create(m_object_term.term_ref));
	e6->setString(m_string_term.term_ref, "Atomic");
	EventPtr e7(m_event_factory.create(m_object_term.term_ref));

	BOOST_CHECK(r(e5));
	BOOST_CHECK(r(e6));
	BOOST_CHECK(! r(e7));
}

BOOST_AUTO_TEST_CASE(checkRuleChainWithMatchAllComparisonsTrue) {
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<MatchAllComparisons>true</MatchAllComparisons>"
		"<Comparison>"
			"<Term>urn:vocab:test#plain-int</Term>"
			"<Type>less-than</Type>"
			"<Value>10</Value>"
		"</Comparison>"
		"<Comparison>"
			"<Term>urn:vocab:test#simple-string</Term>"
			"<Type>contains</Type>"
			"<Value>Atom</Value>"
		"</Comparison>");

	RuleChain r;
	r.setConfig(m_vocabulary, config_ptr);

	EventPtr e1(m_event_factory.create(m_object_term.term_ref));
	e1->setInt(m_plain_int_term.term_ref, 5);
	e1->setString(m_string_term.term_ref, "Atomic");
	EventPtr e2(m_event_factory.create(m_object_term.term_ref));
	e2->setInt(m_plain_int_term.term_ref, 15);
	e2->setString(m_string_term.term_ref, "Atomic");
	EventPtr e3(m_event_factory.create(m_object_term.term_ref));
	e3->setInt(m_plain_int_term.term_ref, 5);
	e3->setString(m_string_term.term_ref, "Pion");
	EventPtr e4(m_event_factory.create(m_object_term.term_ref));
	e4->setInt(m_plain_int_term.term_ref, 15);
	e4->setString(m_string_term.term_ref, "Pion");

	BOOST_CHECK(r(e1));
	BOOST_CHECK(! r(e2));
	BOOST_CHECK(! r(e3));
	BOOST_CHECK(! r(e4));

	// Now test some Events where at least one of the Terms being tested is undefined.
	EventPtr e5(m_event_factory.create(m_object_term.term_ref));
	e5->setInt(m_plain_int_term.term_ref, 5);
	EventPtr e6(m_event_factory.create(m_object_term.term_ref));
	e6->setString(m_string_term.term_ref, "Atomic");
	EventPtr e7(m_event_factory.create(m_object_term.term_ref));

	BOOST_CHECK(! r(e5));
	BOOST_CHECK(! r(e6));
	BOOST_CHECK(! r(e7));
}

BOOST_AUTO_TEST_CASE(checkRuleChainWithIsNotDefinedComparison) {
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<MatchAllComparisons>true</MatchAllComparisons>"
		"<Comparison>"
			"<Term>urn:vocab:test#simple-string</Term>"
			"<Type>is-not-defined</Type>"
		"</Comparison>"
		"<Comparison>"
			"<Term>urn:vocab:test#plain-int</Term>"
			"<Type>less-than</Type>"
			"<Value>10</Value>"
		"</Comparison>");

	RuleChain r;
	r.setConfig(m_vocabulary, config_ptr);

	EventPtr e1(m_event_factory.create(m_object_term.term_ref));
	e1->setString(m_string_term.term_ref, "Atomic");
	e1->setInt(m_plain_int_term.term_ref, 5);
	EventPtr e2(m_event_factory.create(m_object_term.term_ref));
	e2->setString(m_string_term.term_ref, "Atomic");
	e2->setInt(m_plain_int_term.term_ref, 15);
	EventPtr e3(m_event_factory.create(m_object_term.term_ref));
	e3->setInt(m_plain_int_term.term_ref, 5);
	EventPtr e4(m_event_factory.create(m_object_term.term_ref));
	e4->setInt(m_plain_int_term.term_ref, 15);
	EventPtr e5(m_event_factory.create(m_object_term.term_ref));

	BOOST_CHECK(! r(e1));
	BOOST_CHECK(! r(e2));
	BOOST_CHECK(r(e3));
	BOOST_CHECK(! r(e4));
	BOOST_CHECK(! r(e5));
}

BOOST_AUTO_TEST_CASE(checkRuleChainWithEventTypeComparison) {
	xmlNodePtr config_ptr_1 = PionPlatformUnitTest::makeReactorConfigFromString(
		"<MatchAllComparisons>true</MatchAllComparisons>"
		"<Comparison>"
			"<Term>urn:vocab:test#simple-object</Term>"
			"<Type>is-not-defined</Type>"
		"</Comparison>"
		"<Comparison>"
			"<Term>urn:vocab:test#plain-int</Term>"
			"<Type>less-than</Type>"
			"<Value>10</Value>"
		"</Comparison>");
	xmlNodePtr config_ptr_2 = PionPlatformUnitTest::makeReactorConfigFromString(
		"<MatchAllComparisons>false</MatchAllComparisons>"
		"<Comparison>"
			"<Term>urn:vocab:test#simple-object</Term>"
			"<Type>is-not-defined</Type>"
		"</Comparison>"
		"<Comparison>"
			"<Term>urn:vocab:test#plain-int</Term>"
			"<Type>less-than</Type>"
			"<Value>10</Value>"
		"</Comparison>");
	xmlNodePtr config_ptr_3 = PionPlatformUnitTest::makeReactorConfigFromString(
		"<MatchAllComparisons>true</MatchAllComparisons>"
		"<Comparison>"
			"<Term>urn:vocab:test#simple-object</Term>"
			"<Type>is-defined</Type>"
		"</Comparison>"
		"<Comparison>"
			"<Term>urn:vocab:test#plain-int</Term>"
			"<Type>less-than</Type>"
			"<Value>10</Value>"
		"</Comparison>");
	xmlNodePtr config_ptr_4 = PionPlatformUnitTest::makeReactorConfigFromString(
		"<MatchAllComparisons>false</MatchAllComparisons>"
		"<Comparison>"
			"<Term>urn:vocab:test#simple-object</Term>"
			"<Type>is-defined</Type>"
		"</Comparison>"
		"<Comparison>"
			"<Term>urn:vocab:test#plain-int</Term>"
			"<Type>less-than</Type>"
			"<Value>10</Value>"
		"</Comparison>");

	RuleChain r;
	EventPtr e1(m_event_factory.create(m_object_term.term_ref));
	e1->setInt(m_plain_int_term.term_ref, 5);
	EventPtr e2(m_event_factory.create(m_object_term.term_ref));
	e2->setInt(m_plain_int_term.term_ref, 15);

	r.setConfig(m_vocabulary, config_ptr_1);
	BOOST_CHECK(! r(e1));
	BOOST_CHECK(! r(e2));

	r.setConfig(m_vocabulary, config_ptr_2);
	BOOST_CHECK(r(e1));
	BOOST_CHECK(! r(e2));

	r.setConfig(m_vocabulary, config_ptr_3);
	BOOST_CHECK(r(e1));
	BOOST_CHECK(! r(e2));

	r.setConfig(m_vocabulary, config_ptr_4);
	BOOST_CHECK(r(e1));
	BOOST_CHECK(r(e2));
}

BOOST_AUTO_TEST_CASE(checkComparisonDisabledAfterRegexException) {
#if defined(PION_USE_LOG4CPLUS)
	// Set up XmlLogService for checking errors and warnings, and clear existing Events.
	pion::server::PlatformConfig platform_cfg;
	PionPlatformUnitTest::setupXmlLogService(platform_cfg);
	PionPlatformUnitTest::sendRequestAndGetResponse(platform_cfg, pion::net::HTTPRequest("/xmllog?ack=*"), "DELETE");
#endif

	// Make a RuleChain with two Comparisons, one of which has a problematic regex.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<MatchAllComparisons>false</MatchAllComparisons>"
		"<Comparison>"
			"<Term>urn:vocab:test#simple-string</Term>"
			"<Type>regex</Type>"
			"<Value>(a*ba*)*c</Value>"
		"</Comparison>"
		"<Comparison>"
			"<Term>urn:vocab:test#plain-int</Term>"
			"<Type>less-than</Type>"
			"<Value>10</Value>"
		"</Comparison>");
	RuleChain r;
	r.setConfig(m_vocabulary, config_ptr);

	// Create an event that should cause the first comparison to match and the second to not match.
	EventPtr e1(m_event_factory.create(m_object_term.term_ref));
	e1->setString(m_string_term.term_ref, "ac");
	e1->setInt(m_plain_int_term.term_ref, 15);

	// Confirm that the rule succeeded.
	BOOST_CHECK(r(e1));

	// Create an event that should cause the first comparison to throw an exception and be disabled and the second to match.
	// See https://svn.boost.org/trac/boost/ticket/620 (but note that the string "abababababababababababab" mentioned there
	// is not sufficient to make boost::u32regex_search() run out of memory, so a longer one is used here.)
	EventPtr e2(m_event_factory.create(m_object_term.term_ref));
	e2->setString(m_string_term.term_ref, "abababababababababababababababababababababababab");
	e2->setInt(m_plain_int_term.term_ref, 5);

	// Confirm that the rule succeeded.
	BOOST_CHECK(r(e2));

#if defined(PION_USE_LOG4CPLUS)
	// Use XmlLogService to confirm that the expected messages were logged.
	std::vector<std::string> messages;
	PionPlatformUnitTest::getXmlLogMessages(platform_cfg, messages);
	BOOST_REQUIRE_EQUAL(messages.size(), 3U);
	sort(messages.begin(), messages.end());
	BOOST_CHECK_EQUAL(messages[0], "Comparison rule has been disabled: regex = (a*ba*)*c");
	BOOST_CHECK_EQUAL(messages[1], "Regex search failed: regex = (a*ba*)*c, str = abababababababababababababababababababababababab");
	BOOST_CHECK_EQUAL(messages[2], "term_id: urn:vocab:test#simple-string - str = abababababababababababababababababababababababab - ret"); // truncated
#endif

	// Check that the first comparison is disabled by running the rule on e1 again and checking that this time it fails.
	BOOST_CHECK(! r(e1));

	// Confirm that the second comparison is still working by running the rule on e2 again and checking that it succeeds.
	BOOST_CHECK(r(e2));

#if defined(PION_USE_LOG4CPLUS)
	// Confirm that no additional error was logged.
	PionPlatformUnitTest::getXmlLogMessages(platform_cfg, messages);
	BOOST_CHECK_EQUAL(messages.size(), 3U);
#endif
}

BOOST_AUTO_TEST_SUITE_END()


class Comparison_F : public Comparison {
public:
	Comparison_F() : Comparison(Vocabulary::Term()) {};
};

BOOST_FIXTURE_TEST_SUITE(Comparison_S, Comparison_F)

BOOST_AUTO_TEST_CASE(checkConsistencyOfComparisonTable) {
	for (ComparisonType t = TYPE_FALSE; t <= LAST_COMPARISON_TYPE; t = ComparisonType(t+1))
		BOOST_CHECK_EQUAL(t, parseComparisonType(getComparisonTypeAsString(t)));
}

BOOST_AUTO_TEST_CASE(checkRequiresValue) {
	BOOST_CHECK(! requiresValue(TYPE_FALSE));
	BOOST_CHECK(! requiresValue(TYPE_IS_NOT_DEFINED));
	BOOST_CHECK(requiresValue(TYPE_EQUALS));
	BOOST_CHECK(requiresValue(TYPE_SAME_OR_LATER_TIME));
}

BOOST_AUTO_TEST_SUITE_END()


// tests to verify thread safety of Comparison class
class ComparisonThreadSafetyTests_F
	: public ComparisonTests_F
{
public:
	ComparisonThreadSafetyTests_F()
		: ComparisonTests_F(), m_num_matches(0),
		m_comparison(ComparisonTests_F::m_string_term)
	{}

	virtual ~ComparisonThreadSafetyTests_F() {}

	void checkValue(std::string str) {
		EventFactory event_factory;
		EventPtr event_ptr(event_factory.create(m_object_term.term_ref));
		event_ptr->setString(m_string_term.term_ref, str);
		for (int n = 0 ; n < 100; ++n) {
			if (m_comparison.evaluate(*event_ptr))
				++m_num_matches;
		}
	}
	
	void addValueToCheck(const std::string& str) {
		ThreadPtr thread_ptr(new std::thread(std::bind(
			&ComparisonThreadSafetyTests_F::checkValue, this, str)));
		m_threads.push_back(thread_ptr);
	}

	void waitForThreads(void) {
		for (ThreadPool::iterator it=m_threads.begin(); it != m_threads.end(); ++it) {
			(*it)->join();
		}
	}

	typedef std::shared_ptr<std::thread>	ThreadPtr;
	typedef std::vector<ThreadPtr>				ThreadPool;
	
	boost::detail::atomic_count		m_num_matches;
	Comparison						m_comparison;
	ThreadPool						m_threads;
};

BOOST_FIXTURE_TEST_SUITE(ComparisonThreadSafetyTests_S, ComparisonThreadSafetyTests_F)

BOOST_AUTO_TEST_CASE(checkStartsWithThreadSafety) {
	m_comparison.configure(Comparison::TYPE_STARTS_WITH, "66.249");

	addValueToCheck("66.249.71.144");
	addValueToCheck("210.146.46.3");
	addValueToCheck("74.6.22.189");
	addValueToCheck("208.80.193.32");
	addValueToCheck("65.98.89.43");
	addValueToCheck("65.98.89.43");
	addValueToCheck("66.249.71.146");
	addValueToCheck("66.249.71.146");
	addValueToCheck("66.249.71.146");
	addValueToCheck("66.249.71.146");
	addValueToCheck("66.249.71.145");
	addValueToCheck("66.249.71.146");
	addValueToCheck("74.6.22.153");
	addValueToCheck("74.6.22.153");
	addValueToCheck("213.136.52.113");
	addValueToCheck("208.111.154.95");

	waitForThreads();

	BOOST_CHECK_EQUAL(m_num_matches, 700);
}

BOOST_AUTO_TEST_CASE(checkEndsWithThreadSafety) {
	m_comparison.configure(Comparison::TYPE_ENDS_WITH, "71.146");

	addValueToCheck("66.249.71.146");
	addValueToCheck("210.146.46.3");
	addValueToCheck("74.6.22.189");
	addValueToCheck("208.80.193.32");
	addValueToCheck("65.98.89.43");
	addValueToCheck("65.98.89.43");
	addValueToCheck("66.249.71.146");
	addValueToCheck("66.249.71.146");
	addValueToCheck("66.249.71.146");
	addValueToCheck("66.249.71.146");
	addValueToCheck("66.249.71.145");
	addValueToCheck("66.249.71.146");
	addValueToCheck("74.6.22.153");
	addValueToCheck("74.6.22.153");
	addValueToCheck("213.136.52.113");
	addValueToCheck("208.111.154.95");

	waitForThreads();

	BOOST_CHECK_EQUAL(m_num_matches, 600);
}

BOOST_AUTO_TEST_CASE(checkContainsThreadSafety) {
	m_comparison.configure(Comparison::TYPE_CONTAINS, "249.71");

	addValueToCheck("66.249.71.146");
	addValueToCheck("210.146.46.3");
	addValueToCheck("74.6.22.189");
	addValueToCheck("208.80.193.32");
	addValueToCheck("65.98.89.43");
	addValueToCheck("65.98.89.43");
	addValueToCheck("66.249.71.146");
	addValueToCheck("66.249.71.146");
	addValueToCheck("66.249.71.146");
	addValueToCheck("66.249.71.146");
	addValueToCheck("66.249.71.145");
	addValueToCheck("66.249.71.146");
	addValueToCheck("74.6.22.153");
	addValueToCheck("74.6.22.153");
	addValueToCheck("213.136.52.113");
	addValueToCheck("208.111.154.95");

	waitForThreads();

	BOOST_CHECK_EQUAL(m_num_matches, 700);
}

BOOST_AUTO_TEST_SUITE_END()

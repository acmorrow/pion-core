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

#include <pion/PionConfig.hpp>
#include <pion/platform/Event.hpp>
#include <pion/platform/Vocabulary.hpp>
#include <boost/test/unit_test.hpp>

using namespace pion;
using namespace pion::platform;


static const std::string ASCII_STRING_1 = "ABC";

static const char INVALID_UTF8_CHAR_ARRAY_1[] = {
		0x41,									// 'A'
		(char)0xFE,								// can never occur in a valid UTF-8 sequence
		0x42};									// 'B'

static const std::string INVALID_UTF8_STRING_1(INVALID_UTF8_CHAR_ARRAY_1, sizeof(INVALID_UTF8_CHAR_ARRAY_1));

static const char CLEANSED_UTF8_CHAR_ARRAY_1[] = {
		0x41,									// 'A'
		(char)0xEF, (char)0xBF,	(char)0xBD,		// UTF-8 encoding of U+FFFD (REPLACEMENT CHARACTER)
		0x42};									// 'B'

static const std::string CLEANSED_UTF8_STRING_1(CLEANSED_UTF8_CHAR_ARRAY_1, sizeof(CLEANSED_UTF8_CHAR_ARRAY_1));

static const char INVALID_UTF8_CHAR_ARRAY_2[] = {
		0x41,									// 'A'
		(char)0xC8,								// valid lead byte, requires one trail byte...
		0x58,								    // ...not a trail byte.
		0x42,									// 'B'
		(char)0xC8,								// valid lead byte, requires one trail byte...
		(char)0xC8,								// ...invalid second byte.
		0x63,									// 'C'
		(char)0xE2,								// valid lead byte, requires two trail bytes...
		(char)0x82,								// ...valid second byte...
		0x58,								    // ...not a trail byte.
		0x64};									// 'D'

static const std::string INVALID_UTF8_STRING_2(INVALID_UTF8_CHAR_ARRAY_2, sizeof(INVALID_UTF8_CHAR_ARRAY_2));

static const char CLEANSED_UTF8_CHAR_ARRAY_2[] = {
		0x41,									// 'A'
		(char)0xEF, (char)0xBF,	(char)0xBD,		// UTF-8 encoding of U+FFFD (REPLACEMENT CHARACTER)
		0x58,									// 'X'
		0x42,									// 'B'
		(char)0xEF, (char)0xBF,	(char)0xBD,		// UTF-8 encoding of U+FFFD (REPLACEMENT CHARACTER)
		(char)0xEF, (char)0xBF,	(char)0xBD,		// UTF-8 encoding of U+FFFD (REPLACEMENT CHARACTER)
		0x63,									// 'C'
		(char)0xEF, (char)0xBF,	(char)0xBD,		// UTF-8 encoding of U+FFFD (REPLACEMENT CHARACTER)
		0x58,									// 'X'
		0x64};									// 'D'

static const std::string CLEANSED_UTF8_STRING_2(CLEANSED_UTF8_CHAR_ARRAY_2, sizeof(CLEANSED_UTF8_CHAR_ARRAY_2));

static const char INVALID_UTF8_CHAR_ARRAY_3[] = {
		(char)0x82,								// trail byte
		(char)0x82,								// trail byte
		(char)0x82,								// trail byte
		(char)0x82,								// trail byte
		(char)0x82};							// trail byte

static const std::string INVALID_UTF8_STRING_3(INVALID_UTF8_CHAR_ARRAY_3, sizeof(INVALID_UTF8_CHAR_ARRAY_3));

static const char CLEANSED_UTF8_CHAR_ARRAY_3[] = {
		(char)0xEF, (char)0xBF,	(char)0xBD,		// UTF-8 encoding of U+FFFD (REPLACEMENT CHARACTER)
		(char)0xEF, (char)0xBF,	(char)0xBD,		// UTF-8 encoding of U+FFFD (REPLACEMENT CHARACTER)
		(char)0xEF, (char)0xBF,	(char)0xBD,		// UTF-8 encoding of U+FFFD (REPLACEMENT CHARACTER)
		(char)0xEF, (char)0xBF,	(char)0xBD,		// UTF-8 encoding of U+FFFD (REPLACEMENT CHARACTER)
		(char)0xEF, (char)0xBF,	(char)0xBD};	// UTF-8 encoding of U+FFFD (REPLACEMENT CHARACTER)

static const std::string CLEANSED_UTF8_STRING_3(CLEANSED_UTF8_CHAR_ARRAY_3, sizeof(CLEANSED_UTF8_CHAR_ARRAY_3));

static const char INVALID_UTF8_CHAR_ARRAY_4[] = {
		0x41,									// 'A'
		(char)0xE7,								// valid lead byte, requires two trail bytes...
		(char)0x8C,								// ...valid second byte...
		(char)0xAB,								// ...valid third byte.
		(char)0xAB};							// another trail byte

static const std::string INVALID_UTF8_STRING_4(INVALID_UTF8_CHAR_ARRAY_4, sizeof(INVALID_UTF8_CHAR_ARRAY_4));

static const char CLEANSED_UTF8_CHAR_ARRAY_4[] = {
		0x41,									// 'A'
		(char)0xE7, (char)0x8C,	(char)0xAB,		// UTF-8 encoding of U+732B (simplified Chinese character for "cat")
		(char)0xEF, (char)0xBF,	(char)0xBD};	// UTF-8 encoding of U+FFFD (REPLACEMENT CHARACTER)

static const std::string CLEANSED_UTF8_STRING_4(CLEANSED_UTF8_CHAR_ARRAY_4, sizeof(CLEANSED_UTF8_CHAR_ARRAY_4));

static const char INVALID_UTF8_CHAR_ARRAY_5[] = {
		(char)0xC8};							// valid lead byte, requires one trail byte.

static const std::string INVALID_UTF8_STRING_5(INVALID_UTF8_CHAR_ARRAY_5, sizeof(INVALID_UTF8_CHAR_ARRAY_5));

static const char CLEANSED_UTF8_CHAR_ARRAY_5[] = {
		(char)0xEF, (char)0xBF,	(char)0xBD};	// UTF-8 encoding of U+FFFD (REPLACEMENT CHARACTER)

static const std::string CLEANSED_UTF8_STRING_5(CLEANSED_UTF8_CHAR_ARRAY_5, sizeof(CLEANSED_UTF8_CHAR_ARRAY_5));


class EventTests_F {
public:
	EventTests_F()
		: m_null_term("urn:vocab:test#null-term"), m_plain_int_term("urn:vocab:test#plain-old-int"),
		m_big_int_term("urn:vocab:test#big-int"), m_fixed_term("urn:vocab:test#fixed-text"),
		m_string_term("urn:vocab:test#string"),
		m_date_term("urn:vocab:test#date"), m_object_term("urn:vocab:test#simple-object")
	{
		// initialize our initial term set
		m_null_term.term_ref = static_cast<Vocabulary::TermRef>(1);
		m_plain_int_term.term_ref = static_cast<Vocabulary::TermRef>(2);
		m_big_int_term.term_ref = static_cast<Vocabulary::TermRef>(3);
		m_fixed_term.term_ref = static_cast<Vocabulary::TermRef>(4);
		m_date_term.term_ref = static_cast<Vocabulary::TermRef>(5);
		m_object_term.term_ref = static_cast<Vocabulary::TermRef>(6);
		m_null_term.term_type = Vocabulary::TYPE_NULL;
		m_plain_int_term.term_type = Vocabulary::TYPE_INT16;
		m_big_int_term.term_type = Vocabulary::TYPE_UINT64;
		m_fixed_term.term_type = Vocabulary::TYPE_CHAR;
		m_string_term.term_type = Vocabulary::TYPE_STRING;
		m_date_term.term_type = Vocabulary::TYPE_DATE;
		m_object_term.term_type = Vocabulary::TYPE_OBJECT;
		m_null_term.term_comment = "A plain, old integer number";
		m_plain_int_term.term_comment = "A plain, old integer number";
		m_big_int_term.term_comment = "A really big positive integer";
		m_fixed_term.term_comment = "Ten bytes of text";
		m_date_term.term_comment = "A date";
		m_object_term.term_comment = "An object containing other Terms";
		m_fixed_term.term_size = 10;
		m_date_term.term_format = "%Y-%m-%d";
	}
	~EventTests_F() {
	}
	void addAllTerms() {
		m_null_term.term_ref = m_vocabulary.addTerm(m_null_term);
		m_plain_int_term.term_ref = m_vocabulary.addTerm(m_plain_int_term);
		m_big_int_term.term_ref = m_vocabulary.addTerm(m_big_int_term);
		m_fixed_term.term_ref = m_vocabulary.addTerm(m_fixed_term);
		m_string_term.term_ref = m_vocabulary.addTerm(m_string_term);
		m_date_term.term_ref = m_vocabulary.addTerm(m_date_term);
		m_object_term.term_ref = m_vocabulary.addTerm(m_object_term);
	}

	EventFactory		m_event_factory;
	Vocabulary			m_vocabulary;
	Vocabulary::Term	m_null_term;
	Vocabulary::Term	m_plain_int_term;
	Vocabulary::Term	m_big_int_term;
	Vocabulary::Term	m_fixed_term;
	Vocabulary::Term	m_string_term;
	Vocabulary::Term	m_date_term;
	Vocabulary::Term	m_object_term;
};

BOOST_FIXTURE_TEST_SUITE(EventTests_S, EventTests_F)

BOOST_AUTO_TEST_CASE(checkEventAllocatorIsOk) {
	void *mem_ptr = m_event_factory.getAllocator().malloc(sizeof(Event));
	Event *event_ptr = new (mem_ptr) Event(m_object_term.term_ref, &m_event_factory.getAllocator());
	event_ptr->setInt(m_plain_int_term.term_ref, 24);
	event_ptr->setUBigInt(m_big_int_term.term_ref, 2025221224);
	event_ptr->setDateTime(m_date_term.term_ref, PionDateTime(boost::gregorian::date(2007, 4, 5)));

	event_ptr->~Event();
	m_event_factory.getAllocator().free(event_ptr, sizeof(Event));
}

BOOST_AUTO_TEST_CASE(checkEmptyEventValues) {
	EventPtr event_ptr(m_event_factory.create(m_object_term.term_ref));
	BOOST_CHECK(event_ptr->empty());
	BOOST_CHECK(event_ptr->begin() == event_ptr->end());
	BOOST_CHECK(event_ptr->find(m_null_term.term_ref) == event_ptr->end());
	BOOST_CHECK(event_ptr->getPointer(m_null_term.term_ref) == NULL);
	BOOST_CHECK(! event_ptr->isDefined(m_null_term.term_ref));

	Event::ValuesRange range = event_ptr->equal_range(m_null_term.term_ref);
	BOOST_CHECK(range.first == range.second);
	BOOST_CHECK(range.first == event_ptr->end());
	BOOST_CHECK(range.second == event_ptr->end());
}

BOOST_AUTO_TEST_CASE(checkEventAssignmentValues) {
	addAllTerms();
	std::string short_msg_str("short msg");
	EventPtr event_ptr(m_event_factory.create(m_object_term.term_ref));
	event_ptr->setInt(m_plain_int_term.term_ref, 24);
	event_ptr->setUBigInt(m_big_int_term.term_ref, 0x1FFFFFFFFFFFFFFFULL);
	event_ptr->setString(m_fixed_term.term_ref, short_msg_str);
	event_ptr->setDateTime(m_date_term.term_ref, PionDateTime(boost::gregorian::date(2007, 4, 5)));

	const Event::ParameterValue *value_ptr = event_ptr->getPointer(m_plain_int_term.term_ref);
	BOOST_REQUIRE(value_ptr != NULL);
	BOOST_CHECK_EQUAL(boost::get<std::int32_t>(*value_ptr), 24);
	BOOST_CHECK_EQUAL(event_ptr->getUBigInt(m_big_int_term.term_ref), 0x1FFFFFFFFFFFFFFFULL);
	BOOST_CHECK_EQUAL(event_ptr->getString(m_fixed_term.term_ref), short_msg_str);
	BOOST_CHECK_EQUAL(event_ptr->getBlob(m_fixed_term.term_ref).get(), short_msg_str);
	PionDateTime pdt = event_ptr->getDateTime(m_date_term.term_ref);
	BOOST_CHECK_EQUAL(pdt.date().year(), 2007);
	BOOST_CHECK_EQUAL(pdt.date().month(), 4);
	BOOST_CHECK_EQUAL(pdt.date().day(), 5);

	std::uint32_t n;
	BOOST_CHECK(event_ptr->getInt(m_plain_int_term.term_ref, n));
	BOOST_CHECK_EQUAL(n, 24UL);

	std::uint64_t n64;
	BOOST_CHECK(event_ptr->getUBigInt(m_big_int_term.term_ref, n64));
	BOOST_CHECK_EQUAL(n64, 0x1FFFFFFFFFFFFFFFULL);

	std::string str;
	BOOST_CHECK(event_ptr->getString(m_fixed_term.term_ref, str));
	BOOST_CHECK_EQUAL(str, short_msg_str);

	Event::BlobType b;
	BOOST_CHECK(event_ptr->getBlob(m_fixed_term.term_ref, b));
	BOOST_CHECK(! b.empty());
	BOOST_CHECK(! b.unique());
	BOOST_CHECK_EQUAL(b.use_count(), 2);
	BOOST_CHECK_EQUAL(short_msg_str, b.get());
}

BOOST_AUTO_TEST_CASE(checkEventAssignmentToBlobValues) {
	addAllTerms();
	std::string short_msg_str("short msg");
	EventPtr event_a(m_event_factory.create(m_object_term.term_ref));
	event_a->setString(m_fixed_term.term_ref, short_msg_str);
	BOOST_CHECK(event_a->getBlob(m_fixed_term.term_ref) == short_msg_str);
	BOOST_CHECK(event_a->getBlob(m_fixed_term.term_ref).unique());
	BOOST_CHECK_EQUAL(event_a->getBlob(m_fixed_term.term_ref).use_count(), 1);
	BOOST_CHECK_EQUAL(strcmp(event_a->getString(m_fixed_term.term_ref), short_msg_str.c_str()), 0);

	EventPtr event_b(m_event_factory.create(m_object_term.term_ref));
	event_b->setBlob(m_fixed_term.term_ref, event_a->getBlob(m_fixed_term.term_ref));
	BOOST_CHECK(event_b->getBlob(m_fixed_term.term_ref) == short_msg_str);
	BOOST_CHECK(! event_a->getBlob(m_fixed_term.term_ref).unique());
	BOOST_CHECK(! event_b->getBlob(m_fixed_term.term_ref).unique());
	BOOST_CHECK_EQUAL(event_a->getBlob(m_fixed_term.term_ref).use_count(), 2);
	BOOST_CHECK_EQUAL(event_b->getBlob(m_fixed_term.term_ref).use_count(), 2);

	EventPtr event_c(m_event_factory.create(m_object_term.term_ref));
	*event_c += *event_b;
	BOOST_CHECK(event_c->getBlob(m_fixed_term.term_ref) == short_msg_str);
	BOOST_CHECK(! event_a->getBlob(m_fixed_term.term_ref).unique());
	BOOST_CHECK(! event_b->getBlob(m_fixed_term.term_ref).unique());
	BOOST_CHECK(! event_c->getBlob(m_fixed_term.term_ref).unique());
	BOOST_CHECK_EQUAL(event_a->getBlob(m_fixed_term.term_ref).use_count(), 3);
	BOOST_CHECK_EQUAL(event_b->getBlob(m_fixed_term.term_ref).use_count(), 3);
	BOOST_CHECK_EQUAL(event_c->getBlob(m_fixed_term.term_ref).use_count(), 3);
}

BOOST_AUTO_TEST_CASE(checkMultipleTermValues) {
	EventPtr event_ptr(m_event_factory.create(m_object_term.term_ref));
	event_ptr->setInt(m_plain_int_term.term_ref, 10);
	event_ptr->setInt(m_plain_int_term.term_ref, 100);
	event_ptr->setInt(m_plain_int_term.term_ref, 1000);
	event_ptr->setInt(m_plain_int_term.term_ref, 10000);

	BOOST_CHECK(! event_ptr->empty());
	BOOST_CHECK(event_ptr->isDefined(m_plain_int_term.term_ref));
	BOOST_CHECK(event_ptr->find(m_plain_int_term.term_ref) != event_ptr->end());
	BOOST_CHECK_EQUAL(event_ptr->getInt(m_plain_int_term.term_ref) % 10, 0);

	Event::ValuesRange range = event_ptr->equal_range(m_plain_int_term.term_ref);
	BOOST_CHECK(range.first == event_ptr->begin());
	BOOST_CHECK(range.first != range.second);
	BOOST_CHECK(range.first != event_ptr->end());

	Event::ConstIterator i = range.first;
	BOOST_REQUIRE(i != event_ptr->end());
	BOOST_CHECK_EQUAL(boost::get<std::int32_t>(i->value), 10);
	++i;
	BOOST_REQUIRE(i != range.second);
	BOOST_CHECK_EQUAL(boost::get<std::int32_t>(i->value), 100);
	++i;
	BOOST_REQUIRE(i != range.second);
	BOOST_CHECK_EQUAL(boost::get<std::int32_t>(i->value), 1000);
	++i;
	BOOST_REQUIRE(i != range.second);
	BOOST_CHECK_EQUAL(boost::get<std::int32_t>(i->value), 10000);
	++i;
	BOOST_CHECK(i == range.second);
	BOOST_CHECK(i == event_ptr->end());
}

BOOST_AUTO_TEST_CASE(checkClearTermValues) {
	EventPtr event_ptr(m_event_factory.create(m_object_term.term_ref));
	event_ptr->setInt(m_plain_int_term.term_ref, 10);
	event_ptr->setInt(m_plain_int_term.term_ref, 100);
	event_ptr->clear(m_plain_int_term.term_ref);
}

BOOST_AUTO_TEST_CASE(checkEventOperatorPlusEquals) {
	EventPtr a_ptr(m_event_factory.create(m_object_term.term_ref));
	EventPtr b_ptr(m_event_factory.create(m_object_term.term_ref));
	a_ptr->setInt(m_plain_int_term.term_ref, 10);
	b_ptr->setUBigInt(m_big_int_term.term_ref, 2025221224UL);

	BOOST_CHECK(!a_ptr->isDefined(m_big_int_term.term_ref));
	*a_ptr += *b_ptr;
	BOOST_CHECK(a_ptr->isDefined(m_big_int_term.term_ref));
	BOOST_CHECK_EQUAL(a_ptr->getUBigInt(m_big_int_term.term_ref), 2025221224UL);

	BOOST_CHECK(a_ptr->isDefined(m_plain_int_term.term_ref));
	BOOST_CHECK(b_ptr->isDefined(m_big_int_term.term_ref));
	BOOST_CHECK_EQUAL(a_ptr->getInt(m_plain_int_term.term_ref), 10);
	BOOST_CHECK_EQUAL(b_ptr->getUBigInt(m_big_int_term.term_ref), 2025221224UL);
}

BOOST_AUTO_TEST_CASE(checkEventCopyValues) {
	EventPtr a_ptr(m_event_factory.create(m_object_term.term_ref));
	EventPtr b_ptr(m_event_factory.create(m_object_term.term_ref));
	b_ptr->setUBigInt(m_big_int_term.term_ref, 2025221224UL);
	b_ptr->setUBigInt(m_big_int_term.term_ref, 25UL);
	b_ptr->setInt(m_plain_int_term.term_ref, 10);
	b_ptr->setInt(m_plain_int_term.term_ref, 100);
	b_ptr->setInt(m_plain_int_term.term_ref, 1000);
	b_ptr->setInt(m_plain_int_term.term_ref, 10000);

	BOOST_CHECK(!a_ptr->isDefined(m_plain_int_term.term_ref));
	BOOST_CHECK(!a_ptr->isDefined(m_big_int_term.term_ref));

	a_ptr->copyValues(*b_ptr, m_big_int_term.term_ref);

	BOOST_CHECK(!a_ptr->isDefined(m_plain_int_term.term_ref));
	BOOST_CHECK(a_ptr->isDefined(m_big_int_term.term_ref));

	Event::ValuesRange range = a_ptr->equal_range(m_big_int_term.term_ref);
	BOOST_CHECK(range.first == a_ptr->begin());
	BOOST_CHECK(range.first != range.second);
	BOOST_CHECK(range.first != a_ptr->end());

	Event::ConstIterator i = range.first;
	BOOST_REQUIRE(i != a_ptr->end());
	BOOST_CHECK_EQUAL(boost::get<std::uint64_t>(i->value), 2025221224UL);
	++i;
	BOOST_REQUIRE(i != range.second);
	BOOST_CHECK_EQUAL(boost::get<std::uint64_t>(i->value), 25UL);
	++i;
	BOOST_CHECK(i == range.second);
	BOOST_CHECK(i == a_ptr->end());
}

BOOST_AUTO_TEST_CASE(checkEventSetWrite) {
	EventPtr b_ptr(m_event_factory.create(m_object_term.term_ref));
	std::string s("2025221224");
	b_ptr->set(m_big_int_term, s);
	BOOST_CHECK_EQUAL(b_ptr->getUBigInt(m_big_int_term.term_ref), 2025221224UL);
	std::string r;
	Event::ValuesRange range = b_ptr->equal_range(m_big_int_term.term_ref);
	Event::ConstIterator i = range.first;
	b_ptr->write(r, i->value, m_big_int_term);
	BOOST_CHECK_EQUAL(s, r);
}

BOOST_AUTO_TEST_CASE(checkEventSetWrite2) {
	EventPtr a_ptr(m_event_factory.create(m_object_term.term_ref));
	std::string s("221224");
	a_ptr->set(m_plain_int_term, s);
	BOOST_CHECK_EQUAL(a_ptr->getInt(m_plain_int_term.term_ref), 221224);
	std::string r;
	Event::ValuesRange range = a_ptr->equal_range(m_plain_int_term.term_ref);
	Event::ConstIterator i = range.first;
	a_ptr->write(r, i->value, m_plain_int_term);
	BOOST_CHECK_EQUAL(s, r);
}

BOOST_AUTO_TEST_CASE(checkParameterValueOperatorEquals) {
	Event::ParameterValue pv1 = std::int32_t(4);
	Event::ParameterValue pv2 = std::int32_t(4);
	BOOST_CHECK(pv1 == pv2);

	pv2 = std::int32_t(5);
	//BOOST_CHECK(pv1 != pv2); // can't use this because boost::variant doesn't define operator!=
	BOOST_CHECK(!(pv1 == pv2));

	pv2 = 5.0;
	BOOST_CHECK(!(pv1 == pv2));

	char buf1[] = "abc";
	pv1 = Event::BlobType(m_event_factory.getAllocator(), buf1, strlen(buf1));
	BOOST_CHECK_EQUAL(strcmp(boost::get<Event::BlobType&>(pv1).get(), "abc"), 0);
	BOOST_CHECK(!(pv1 == pv2));

	char buf2[] = "abc";
	pv2 = m_event_factory.make_blob(buf2, strlen(buf2));
	BOOST_CHECK_EQUAL(strcmp(boost::get<Event::BlobType&>(pv2).get(), "abc"), 0);
	BOOST_CHECK(pv1 == pv2);

	char buf3[] = "abd";
	pv2 = m_event_factory.make_blob(buf3);
	BOOST_CHECK_EQUAL(strcmp(boost::get<Event::BlobType&>(pv2).get(), "abd"), 0);
	BOOST_CHECK(!(pv1 == pv2));

	const std::string str("dab");
	pv2 = m_event_factory.make_blob(str);
	BOOST_CHECK_EQUAL(str, boost::get<Event::BlobType&>(pv2).get());
	BOOST_CHECK(!(pv1 == pv2));
}

BOOST_AUTO_TEST_CASE(checkParameterValueCreatedWithBlobParams) {
	Event::BlobType str(m_event_factory.getAllocator(), "abc");
	BOOST_CHECK(str.unique());
	BOOST_CHECK_EQUAL(str.use_count(), 1);

	{
		Event::ParameterValue pv1 = str;
		BOOST_CHECK(! str.unique());
		BOOST_CHECK_EQUAL(str.use_count(), 2);
		BOOST_CHECK_EQUAL(strcmp(boost::get<Event::BlobType&>(pv1).get(), "abc"), 0);
	}	// should destruct BlobType copy

	BOOST_CHECK(str.unique());
	BOOST_CHECK_EQUAL(str.use_count(), 1);

	Event::ParameterValue pv2 = str;
	BOOST_CHECK(! str.unique());
	BOOST_CHECK_EQUAL(str.use_count(), 2);

	pv2 = std::uint32_t(42);	// should destruct BlobType copy
	BOOST_CHECK_EQUAL(boost::get<std::uint32_t>(pv2), 42U);
	BOOST_CHECK(str.unique());
	BOOST_CHECK_EQUAL(str.use_count(), 1);
}

BOOST_AUTO_TEST_CASE(checkParameterValueConstructedFromValue) {
	std::uint32_t num(42);
	Event::ParameterValue pv1(num);
	BOOST_CHECK_EQUAL(num, boost::get<std::uint32_t>(pv1));

	std::string abc_str("abc");
	Event::BlobType::BlobParams bp(m_event_factory.getAllocator(), "abc", 3);
	Event::ParameterValue pv2(bp);
	BOOST_CHECK_EQUAL(abc_str, boost::get<Event::BlobType&>(pv2).get());

	Event::BlobType str(m_event_factory.make_blob("abc"));
	BOOST_CHECK(str.unique());
	BOOST_CHECK_EQUAL(str.use_count(), 1);
	Event::ParameterValue pv3(str);
	BOOST_CHECK(! str.unique());
	BOOST_CHECK_EQUAL(str.use_count(), 2);
	BOOST_CHECK_EQUAL(strcmp(boost::get<Event::BlobType&>(pv3).get(), "abc"), 0);
}

BOOST_AUTO_TEST_CASE(testAllSetStringOverloadsWithValidUTF8Data) {
	addAllTerms();

	EventPtr e1(m_event_factory.create(m_object_term.term_ref));
	e1->setString(m_string_term.term_ref, ASCII_STRING_1.c_str(), ASCII_STRING_1.size());
	BOOST_CHECK_EQUAL(e1->getString(m_string_term.term_ref), ASCII_STRING_1);

	EventPtr e2(m_event_factory.create(m_object_term.term_ref));
	e2->setString(m_string_term.term_ref, ASCII_STRING_1.c_str());
	BOOST_CHECK_EQUAL(e2->getString(m_string_term.term_ref), ASCII_STRING_1);

	EventPtr e3(m_event_factory.create(m_object_term.term_ref));
	e3->setString(m_string_term.term_ref, ASCII_STRING_1);
	BOOST_CHECK_EQUAL(e3->getString(m_string_term.term_ref), ASCII_STRING_1);

	EventPtr e4(m_event_factory.create(m_object_term.term_ref));
	e4->setString(m_string_term.term_ref, Event::BlobType(m_event_factory.getAllocator(), ASCII_STRING_1.c_str(), ASCII_STRING_1.size()));
	BOOST_CHECK_EQUAL(e4->getString(m_string_term.term_ref), ASCII_STRING_1);
}	

BOOST_AUTO_TEST_CASE(testAllSetStringOverloadsWithInvalidUTF8Data) {
	addAllTerms();

	EventPtr e1(m_event_factory.create(m_object_term.term_ref));
	e1->setString(m_string_term.term_ref, INVALID_UTF8_STRING_1.c_str(), INVALID_UTF8_STRING_1.size());
	BOOST_CHECK_EQUAL(e1->getString(m_string_term.term_ref), CLEANSED_UTF8_STRING_1);

	EventPtr e2(m_event_factory.create(m_object_term.term_ref));
	e2->setString(m_string_term.term_ref, INVALID_UTF8_STRING_2.c_str());
	BOOST_CHECK_EQUAL(e2->getString(m_string_term.term_ref), CLEANSED_UTF8_STRING_2);

	EventPtr e3(m_event_factory.create(m_object_term.term_ref));
	e3->setString(m_string_term.term_ref, INVALID_UTF8_STRING_3);
	BOOST_CHECK_EQUAL(e3->getString(m_string_term.term_ref), CLEANSED_UTF8_STRING_3);

	EventPtr e4(m_event_factory.create(m_object_term.term_ref));
	e4->setString(m_string_term.term_ref, Event::BlobType(m_event_factory.getAllocator(), INVALID_UTF8_STRING_4.c_str(), INVALID_UTF8_STRING_4.size()));
	BOOST_CHECK_EQUAL(e4->getString(m_string_term.term_ref), CLEANSED_UTF8_STRING_4);
}	

BOOST_AUTO_TEST_SUITE_END()


BOOST_AUTO_TEST_CASE(testIsValidUTF8) {
	std::size_t trimmed_len;
	BOOST_CHECK(EventValidator::isValidUTF8(ASCII_STRING_1.c_str(), ASCII_STRING_1.size(), &trimmed_len));
	BOOST_CHECK_EQUAL(trimmed_len, ASCII_STRING_1.size());
	BOOST_CHECK(EventValidator::isValidUTF8(ASCII_STRING_1.c_str(), ASCII_STRING_1.size(), NULL));

	BOOST_CHECK(! EventValidator::isValidUTF8(INVALID_UTF8_STRING_1.c_str(), INVALID_UTF8_STRING_1.size(), NULL));
	BOOST_CHECK(! EventValidator::isValidUTF8(INVALID_UTF8_STRING_2.c_str(), INVALID_UTF8_STRING_2.size(), NULL));
	BOOST_CHECK(! EventValidator::isValidUTF8(INVALID_UTF8_STRING_3.c_str(), INVALID_UTF8_STRING_3.size(), NULL));

	// Since the first byte is valid and the second is a valid lead byte, with len = 2, isValidUTF8() should return true.
	std::size_t len = 2;
	BOOST_CHECK(EventValidator::isValidUTF8(INVALID_UTF8_STRING_2.c_str(), len, &trimmed_len));
	BOOST_CHECK_EQUAL(trimmed_len, 1);

	std::size_t* trimmed_len_ptr = NULL;
	// With trimmed_len_ptr == NULL, a partial code point at the end is not allowed.
	BOOST_CHECK(! EventValidator::isValidUTF8(INVALID_UTF8_STRING_2.c_str(), len, trimmed_len_ptr));

	// Test with all trail byte sequence.
	BOOST_CHECK(! EventValidator::isValidUTF8(INVALID_UTF8_STRING_3.c_str(), 1, &trimmed_len));
	BOOST_CHECK(! EventValidator::isValidUTF8(INVALID_UTF8_STRING_3.c_str(), 2, &trimmed_len));
	BOOST_CHECK(! EventValidator::isValidUTF8(INVALID_UTF8_STRING_3.c_str(), 3, &trimmed_len));
	BOOST_CHECK(! EventValidator::isValidUTF8(INVALID_UTF8_STRING_3.c_str(), 4, &trimmed_len));
	BOOST_CHECK(! EventValidator::isValidUTF8(INVALID_UTF8_STRING_3.c_str(), 5, &trimmed_len));

	// Test with a sequence that starts OK but has too many trail bytes.
	BOOST_CHECK(EventValidator::isValidUTF8(INVALID_UTF8_STRING_4.c_str(), 2, &trimmed_len));
	BOOST_CHECK_EQUAL(trimmed_len, 1);
	BOOST_CHECK(EventValidator::isValidUTF8(INVALID_UTF8_STRING_4.c_str(), 3, &trimmed_len));
	BOOST_CHECK_EQUAL(trimmed_len, 1);
	BOOST_CHECK(EventValidator::isValidUTF8(INVALID_UTF8_STRING_4.c_str(), 4, &trimmed_len));
	BOOST_CHECK_EQUAL(trimmed_len, 4);
	BOOST_CHECK(! EventValidator::isValidUTF8(INVALID_UTF8_STRING_4.c_str(), 5, &trimmed_len));

	// Test with a sequence that has only an incomplete code point.
	BOOST_CHECK(EventValidator::isValidUTF8(INVALID_UTF8_STRING_5.c_str(), INVALID_UTF8_STRING_5.size(), &trimmed_len));
	BOOST_CHECK_EQUAL(trimmed_len, 0);
	BOOST_CHECK(! EventValidator::isValidUTF8(INVALID_UTF8_STRING_5.c_str(), INVALID_UTF8_STRING_5.size(), NULL));

	// Test empty string.
	BOOST_CHECK(EventValidator::isValidUTF8("", 0, &trimmed_len));
	BOOST_CHECK_EQUAL(trimmed_len, 0);
	BOOST_CHECK(EventValidator::isValidUTF8("", 0, NULL));

	// Test length 0 for non-empty string.
	BOOST_CHECK(EventValidator::isValidUTF8(ASCII_STRING_1.c_str(), 0, &trimmed_len));
	BOOST_CHECK_EQUAL(trimmed_len, 0);
	BOOST_CHECK(EventValidator::isValidUTF8(ASCII_STRING_1.c_str(), 0, NULL));
}

BOOST_AUTO_TEST_CASE(testGetCleansedUTF8Length) {
	size_t len = EventValidator::getCleansedUTF8Length(ASCII_STRING_1.c_str(), ASCII_STRING_1.size());
	BOOST_CHECK_EQUAL(len, ASCII_STRING_1.size());

	// Using BOOST_CHECK_GE because getCleansedUTF8Length() returns an an upper bound on the required length.
	len = EventValidator::getCleansedUTF8Length(INVALID_UTF8_STRING_1.c_str(), INVALID_UTF8_STRING_1.size());
	BOOST_CHECK_GE(len, CLEANSED_UTF8_STRING_1.size());
	len = EventValidator::getCleansedUTF8Length(INVALID_UTF8_STRING_2.c_str(), INVALID_UTF8_STRING_2.size());
	BOOST_CHECK_GE(len, CLEANSED_UTF8_STRING_2.size());
	len = EventValidator::getCleansedUTF8Length(INVALID_UTF8_STRING_3.c_str(), INVALID_UTF8_STRING_3.size());
	BOOST_CHECK_GE(len, CLEANSED_UTF8_STRING_3.size());
	len = EventValidator::getCleansedUTF8Length(INVALID_UTF8_STRING_4.c_str(), INVALID_UTF8_STRING_4.size());
	BOOST_CHECK_GE(len, CLEANSED_UTF8_STRING_4.size());
}

BOOST_AUTO_TEST_CASE(testCleanseUTF8) {
	char buf[100];
	size_t actual_len;
	EventValidator::cleanseUTF8_TEMP(ASCII_STRING_1.c_str(), ASCII_STRING_1.size(), buf, &actual_len);
	BOOST_CHECK_EQUAL(actual_len, ASCII_STRING_1.size());
	BOOST_CHECK(strncmp(buf, ASCII_STRING_1.c_str(), actual_len) == 0);

	EventValidator::cleanseUTF8_TEMP(INVALID_UTF8_STRING_1.c_str(), INVALID_UTF8_STRING_1.size(), buf, &actual_len);
	BOOST_CHECK_EQUAL(actual_len, CLEANSED_UTF8_STRING_1.size());
	BOOST_CHECK(strncmp(buf, CLEANSED_UTF8_STRING_1.c_str(), actual_len) == 0);

	EventValidator::cleanseUTF8_TEMP(INVALID_UTF8_STRING_2.c_str(), INVALID_UTF8_STRING_2.size(), buf, &actual_len);
	BOOST_CHECK_EQUAL(actual_len, CLEANSED_UTF8_STRING_2.size());
	BOOST_CHECK(strncmp(buf, CLEANSED_UTF8_STRING_2.c_str(), actual_len) == 0);

	EventValidator::cleanseUTF8_TEMP(INVALID_UTF8_STRING_3.c_str(), INVALID_UTF8_STRING_3.size(), buf, &actual_len);
	BOOST_CHECK_EQUAL(actual_len, CLEANSED_UTF8_STRING_3.size());
	BOOST_CHECK(strncmp(buf, CLEANSED_UTF8_STRING_3.c_str(), actual_len) == 0);

	EventValidator::cleanseUTF8_TEMP(INVALID_UTF8_STRING_4.c_str(), INVALID_UTF8_STRING_4.size(), buf, &actual_len);
	BOOST_CHECK_EQUAL(actual_len, CLEANSED_UTF8_STRING_4.size());
	BOOST_CHECK(strncmp(buf, CLEANSED_UTF8_STRING_4.c_str(), actual_len) == 0);

	EventValidator::cleanseUTF8_TEMP(INVALID_UTF8_STRING_5.c_str(), INVALID_UTF8_STRING_5.size(), buf, &actual_len);
	BOOST_CHECK_EQUAL(actual_len, CLEANSED_UTF8_STRING_5.size());
	BOOST_CHECK(strncmp(buf, CLEANSED_UTF8_STRING_5.c_str(), actual_len) == 0);
}

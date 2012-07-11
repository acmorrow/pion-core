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

#include <string>
#include <set>
#include <list>
#include <pion/PionConfig.hpp>
#include <boost/detail/atomic_count.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/filesystem/operations.hpp>
#include <pion/PionId.hpp>
#include <pion/platform/PionPlatformUnitTest.hpp>
#include <pion/platform/Event.hpp>
#include <pion/platform/VocabularyManager.hpp>
#include <pion/platform/ReactionEngine.hpp>
#include <pion/platform/CodecFactory.hpp>
#include <pion/platform/ProtocolFactory.hpp>
#include <pion/platform/DatabaseManager.hpp>

using namespace pion::platform;

// static strings used by these unit tests
static const std::string LOG_OUTPUT_FILE(LOG_FILE_DIR + "tr2.out");
static const std::string CLF_LOG_FILE(LOG_FILE_DIR + "tr2.log");


// The names ReactionEngineReadyToAddReactors2_F and ReactionEngineReadyToAddReactors2_S are used so that the unit test framework 
// won't confuse them with ReactionEngineReadyToAddReactors_F and ReactionEngineReadyToAddReactors_S in TransformReactorTests.
// Regrettably, it WILL compile without the 2's, but some tests will end up running with the wrong fixture.  
class ReactionEngineReadyToAddReactors2_F {
public:
	ReactionEngineReadyToAddReactors2_F() :
		m_vocab_mgr(), m_codec_factory(m_vocab_mgr), m_protocol_factory(m_vocab_mgr), m_database_mgr(m_vocab_mgr),
		m_reaction_engine(NULL)
	{
		PionPlatformUnitTest::cleanup_platform_config_files();
		
		m_vocab_mgr.setConfigFile(VOCABS_CONFIG_FILE);
		m_vocab_mgr.openConfigFile();
		m_codec_factory.setConfigFile(CODECS_CONFIG_FILE);
		m_codec_factory.openConfigFile();

		boost::filesystem::remove(LOG_OUTPUT_FILE);
		BOOST_REQUIRE(! boost::filesystem::exists(LOG_OUTPUT_FILE)); // Confirm, to avoid accidentally passing tests due to previous output.

		// Create a new (empty) reactor configuration file.
		m_reactor_config_file = CONFIG_FILE_DIR + "reactors1.xml";
		if (boost::filesystem::exists(m_reactor_config_file))
			boost::filesystem::remove(m_reactor_config_file);

		// Create and initialize a ReactionEngine.
		m_reaction_engine = new ReactionEngine(m_vocab_mgr, m_codec_factory, m_protocol_factory, m_database_mgr);
		m_reaction_engine->setConfigFile(m_reactor_config_file);
		m_reaction_engine->setNumThreads(1);
		m_reaction_engine->createConfigFile();
	}
	virtual ~ReactionEngineReadyToAddReactors2_F() {
		delete m_reaction_engine;
	}

	VocabularyManager	m_vocab_mgr;
	CodecFactory		m_codec_factory;
	ProtocolFactory		m_protocol_factory;
	DatabaseManager		m_database_mgr;
	ReactionEngine*		m_reaction_engine;
	std::string			m_reactor_config_file;
};

BOOST_FIXTURE_TEST_SUITE(ReactionEngineReadyToAddReactors2_S, ReactionEngineReadyToAddReactors2_F)

BOOST_AUTO_TEST_CASE(checkFilterCLFLogFile) {
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>FilterReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Comparison>"
			"<Term>urn:vocab:clickstream#c-ip</Term>"
			"<Type>starts-with</Type>"
			"<Value>66.249</Value>"
		"</Comparison>");
	std::string reactor_id = m_reaction_engine->addReactor(config_ptr);

	// Add a LogOutputReactor and connect it to the FilterReactor so it has something to send output to.
	config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Name>CLF Log Writer</Name>"
		"<Plugin>LogOutputReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Codec>3f49f2da-bfe3-11dc-8875-0016cb926e68</Codec>"
		"<Filename>../" + LOG_OUTPUT_FILE + "</Filename>");
	std::string log_writer_id = m_reaction_engine->addReactor(config_ptr);
	m_reaction_engine->addReactorConnection(reactor_id, log_writer_id);

	// Start the ReactionEngine.
	m_reaction_engine->start();

	// Read in events from a CLF log file.
	CodecPtr clf_codec_ptr = m_codec_factory.getCodec("3f49f2da-bfe3-11dc-8875-0016cb926e68");
	boost::uint64_t events_read = PionPlatformUnitTest::feedFileToReactor(
		*m_reaction_engine, reactor_id, *clf_codec_ptr, CLF_LOG_FILE);
	BOOST_CHECK_EQUAL(events_read, 16UL);

	// Check that exactly 7 events passed the filter.
	PionPlatformUnitTest::checkReactorEventsOut(*m_reaction_engine, log_writer_id, static_cast<boost::uint64_t>(7));
}

BOOST_AUTO_TEST_SUITE_END()


class FilterReactorEventValidator_F : public ReactionEngineReadyToAddReactors2_F {
public:
	FilterReactorEventValidator_F()
		: m_curr_event_id(0), m_num_events_validated(0)
	{
		VocabularyPtr vocab_ptr(m_vocab_mgr.getVocabulary());
		m_page_event_ref = vocab_ptr->findTerm("urn:vocab:clickstream#page-event");
		m_page_number_ref = vocab_ptr->findTerm("urn:vocab:clickstream#page-number");
		m_sc_content_term_ref = vocab_ptr->findTerm("urn:vocab:clickstream#sc-content");
	}

	typedef std::set<unsigned> ExpectedEventIds;

	void addEventWithContentTerm(const std::string& content, bool should_pass_filter) {
		EventPtr e(m_event_factory.create(m_page_event_ref));
		e->setUInt(m_page_number_ref, ++m_curr_event_id);
		e->setString(m_sc_content_term_ref, content);
		m_events.push_back(e);
		if (should_pass_filter)
			m_expected_event_ids.insert(m_curr_event_id);
	}

	void MockEventHandler(EventPtr e, const ExpectedEventIds& expected_event_ids) {
		if (! e.get())
			return;

		// Check that the ID of the Event is in the set of expected event IDs.
		BOOST_CHECK(expected_event_ids.count(e->getUInt(m_page_number_ref)));

		// must be atomic because multiple threads hit this at once
		++m_num_events_validated;
	}

	void sendEventsAndValidateOutput(std::list<EventPtr>& events, const std::string& reactor_id) {
		// Add an Event handler to check that the output of the FilterReactor is as expected.
		Reactor::EventHandler eventValidator = std::bind(&FilterReactorEventValidator_F::MockEventHandler, this, std::placeholders::_1, m_expected_event_ids);
		m_reaction_engine->addTempConnectionOut(reactor_id, pion::PionId().to_string(), "blah", eventValidator);

		// Start the ReactionEngine and send the Events to the FilterReactor.
		m_reaction_engine->start();
		for (std::list<EventPtr>::iterator i = events.begin(); i != events.end(); ++i)
			m_reaction_engine->send(reactor_id, *i);

		// Check that the expected number of Events were output by the FilterReactor and validated.
		int num_events_expected = m_expected_event_ids.size();
		PionPlatformUnitTest::checkReactorEventsOut(*m_reaction_engine, reactor_id, num_events_expected);
		BOOST_CHECK_EQUAL(m_num_events_validated, num_events_expected);
	}

	EventFactory		m_event_factory;
	Vocabulary::TermRef	m_page_event_ref;
	Vocabulary::TermRef	m_page_number_ref;
	Vocabulary::TermRef	m_sc_content_term_ref;
	int					m_curr_event_id;
	std::list<EventPtr> m_events;
	boost::detail::atomic_count		m_num_events_validated;
	ExpectedEventIds	m_expected_event_ids;
};

BOOST_FIXTURE_TEST_SUITE(FilterReactorEventValidator_S, FilterReactorEventValidator_F)

BOOST_AUTO_TEST_CASE(checkComparisonOfTypeContains) {
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>FilterReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Comparison>"
			"<Term>urn:vocab:clickstream#sc-content</Term>"
			"<Type>contains</Type>"
			"<Value>hello</Value>"
		"</Comparison>");
	std::string reactor_id = m_reaction_engine->addReactor(config_ptr);

	// Create test Events, specifying the content Term values and whether they should pass the filter. 
	addEventWithContentTerm("hello, world", true);
	addEventWithContentTerm("goodbye", false);
	addEventWithContentTerm("hello again", true);

// This test fails occasionally, probably due to not waiting long enough to get the expected events.
// If it keeps happening, try setting wait_seconds, the optional 4th parameter of checkReactorEventsOut(),
// to something > 1 in sendEventsAndValidateOutput().

	sendEventsAndValidateOutput(m_events, reactor_id);
}

BOOST_AUTO_TEST_CASE(checkComparisonOfTypeNotStartsWith) {
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>FilterReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Comparison>"
			"<Term>urn:vocab:clickstream#sc-content</Term>"
			"<Type>not-starts-with</Type>"
			"<Value>hello</Value>"
		"</Comparison>");
	std::string reactor_id = m_reaction_engine->addReactor(config_ptr);

	// Create test Events, specifying the content Term values and whether they should pass the filter. 
	addEventWithContentTerm("hello, world", false);
	addEventWithContentTerm("goodbye", true);
	addEventWithContentTerm("hello again", false);

	sendEventsAndValidateOutput(m_events, reactor_id);
}

// This test fails with the old code, i.e. without the ICU Collator.
// ICU implements the Unicode Collation Algorithm; see http://www.unicode.org/reports/tr10/ for more information.
BOOST_AUTO_TEST_CASE(checkComparisonsOfTypeOrderedBefore) {
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>FilterReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Comparison>"
			"<Term>urn:vocab:clickstream#sc-content</Term>"
			"<Type>ordered-before</Type>"
			"<Value>bäd</Value>"
		"</Comparison>");
	std::string reactor_id = m_reaction_engine->addReactor(config_ptr);

	// Create test Events, specifying the content Term values and whether they should pass the filter. 
	addEventWithContentTerm("bad", true);
	addEventWithContentTerm("bat", false);
	addEventWithContentTerm("bät", false);

	sendEventsAndValidateOutput(m_events, reactor_id);
}

BOOST_AUTO_TEST_CASE(checkComparisonOfTypeContainsPrimary) {
	// Add a FilterReactor with a Comparison with Type = contains-primary.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>FilterReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Comparison>"
			"<Term>urn:vocab:clickstream#sc-content</Term>"
			"<Type>contains-primary</Type>"
			"<Value>Gödel's résumé</Value>"
		"</Comparison>");
	std::string reactor_id = m_reaction_engine->addReactor(config_ptr);

	// Create test Events, specifying the content Term values and whether they should pass the filter. 
	addEventWithContentTerm("Kurt Gödel's résumé", true);
	addEventWithContentTerm("kurt gödel's résumé", true);
	addEventWithContentTerm("Kurt Godel's resume", true);
	addEventWithContentTerm("M. C. Escher's résumé", false);

	sendEventsAndValidateOutput(m_events, reactor_id);
}

BOOST_AUTO_TEST_CASE(checkUtf8RegexUsingHexadecimalEscapeCodes) {
	// Add a FilterReactor that does a regular expression Comparison that includes a non-US-ASCII character.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>FilterReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Comparison>"
			"<Term>urn:vocab:clickstream#sc-content</Term>"
			"<Type>regex</Type>"
			"<Value>\xCE\xB1=(\\S+)</Value>"
		"</Comparison>");
	std::string reactor_id = m_reaction_engine->addReactor(config_ptr);

	// Create two test strings, one that should pass the filter and one that shouldn't.
	char UTF8_ENCODED_TEST_CHAR_ARRAY_1[] = {
		(char)0xCE, (char)0xB1,				// UTF-8 encoding of U+03B1 (GREEK SMALL LETTER ALPHA)
		0x3D,								// '='
		0x32,								// '2'
		0x20,								// space
		(char)0xCE, (char)0xB2,				// UTF-8 encoding of U+03B2 (GREEK SMALL LETTER BETA)
		0x3D,								// '='
		0x33};								// '3'
	char UTF8_ENCODED_TEST_CHAR_ARRAY_2[] = {
		(char)0xCE, (char)0xB1,				// UTF-8 encoding of U+03B1 (GREEK SMALL LETTER ALPHA)
		(char)0xCE, (char)0xB2,				// UTF-8 encoding of U+03B2 (GREEK SMALL LETTER BETA)
		0x3D,								// '='
		0x36};								// '6'
	const std::string UTF8_ENCODED_TEST_STRING_1(UTF8_ENCODED_TEST_CHAR_ARRAY_1, sizeof(UTF8_ENCODED_TEST_CHAR_ARRAY_1));
	const std::string UTF8_ENCODED_TEST_STRING_2(UTF8_ENCODED_TEST_CHAR_ARRAY_2, sizeof(UTF8_ENCODED_TEST_CHAR_ARRAY_2));

	// Create test Events, specifying the content Term values and whether they should pass the filter. 
	addEventWithContentTerm(UTF8_ENCODED_TEST_STRING_1, true);
	addEventWithContentTerm(UTF8_ENCODED_TEST_STRING_2, false);

	sendEventsAndValidateOutput(m_events, reactor_id);
}

// This test is the same as checkUtf8RegexUsingHexadecimalEscapeCodes, except that the non-US-ASCII
// character 'alpha' appears directly in the regular expression.  (This may not be apparent if you are
// not viewing this test code with UTF-8 encoding, but note that reactors.xml is always UTF-8 encoded.)
BOOST_AUTO_TEST_CASE(checkUtf8RegexUsingEmbeddedUtf8) {
	// Add a FilterReactor that does a regular expression Comparison that includes a non-US-ASCII character.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>FilterReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Comparison>"
			"<Term>urn:vocab:clickstream#sc-content</Term>"
			"<Type>regex</Type>"
			"<Value>α=(\\S+)</Value>"
		"</Comparison>");
	std::string reactor_id = m_reaction_engine->addReactor(config_ptr);

	// Create test Events, specifying the content Term values and whether they should pass the filter. 
	addEventWithContentTerm("α=2 β=3", true);
	addEventWithContentTerm("αβ=6", false);

	sendEventsAndValidateOutput(m_events, reactor_id);
}

BOOST_AUTO_TEST_CASE(checkCaseInsensitiveUtf8Regex) {
	// Add a FilterReactor that does case insensitive regular expression Comparisons using non-US-ASCII characters.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>FilterReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Comparison>"
			"<Term>urn:vocab:clickstream#sc-content</Term>"
			"<Type>regex</Type>"
			"<Value>(?i)Äiti</Value>"
		"</Comparison>");
	std::string reactor_id = m_reaction_engine->addReactor(config_ptr);

	// Create test Events, specifying the content Term values and whether they should pass the filter. 
	addEventWithContentTerm("lowercase: äiti", true);
	addEventWithContentTerm("uppercase: Äiti", true);
	addEventWithContentTerm("plain A: Aiti", false);

	sendEventsAndValidateOutput(m_events, reactor_id);
}

// This test uses a regex that matches exactly one "word" character, and tests it against a UTF-8 string
// where the character we'd like to match consists of two bytes.
BOOST_AUTO_TEST_CASE(checkCharBasedRulesRegexAgainstNonAsciiChar) {
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>FilterReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Comparison>"
			"<Term>urn:vocab:clickstream#sc-content</Term>"
			"<Type>regex</Type>"
			"<Value>x=(\\w);</Value>"
		"</Comparison>");
	std::string reactor_id = m_reaction_engine->addReactor(config_ptr);

	// Create test Events, specifying the content Term values and whether they should pass the filter. 
	addEventWithContentTerm("x=α;", true);
	addEventWithContentTerm("x=a;", true);
	addEventWithContentTerm("x=ab;", false);

	sendEventsAndValidateOutput(m_events, reactor_id);
}

BOOST_AUTO_TEST_SUITE_END()

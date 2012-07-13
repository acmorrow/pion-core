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
#include <fstream>
#include <pion/PionConfig.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/filesystem/operations.hpp>
#include <pion/PionUnitTestDefs.hpp>
#include <pion/platform/PionPlatformUnitTest.hpp>
#include <pion/platform/Event.hpp>
#include <pion/platform/Transform.hpp>
#include "../server/PlatformConfig.hpp"

using namespace std;
using namespace pion;
using namespace pion::platform;


/// static strings used by these unit tests
static const std::string TR2_REACTORS_TEMPLATE_FILE(CONFIG_FILE_DIR + "tr2-reactors.tmpl");
static const std::string LOG_OUTPUT_FILE(LOG_FILE_DIR + "tr2.out");
static const std::string CLF_LOG_FILE(LOG_FILE_DIR + "tr2.log");
static const int NUM_CLF_RECORDS = 16;
static const std::string CLF_LOG_EXPECTED_FILE_1(LOG_FILE_DIR + "tr2-expected.out");
static const std::string CLF_LOG_EXPECTED_FILE_2(LOG_FILE_DIR + "clf-expected.out");
static const std::string STOCK_PRICE_LOG_FILE(LOG_FILE_DIR + "stocks.log");
static const int NUM_STOCK_RECORDS = 4;
static const std::string STOCK_PRICE_LOG_EXPECTED_FILE(LOG_FILE_DIR + "stocks-expected.out");


/// interface class for TransformReactor tests
class TransformReactorTests_F {
public:
	TransformReactorTests_F() :
		m_clf_codec_id("3f49f2da-bfe3-11dc-8875-0016cb926e68"),
		m_transformer_id("25bcc7f0-e109-11dd-aef9-001c25b8b54e"),
		m_output_log_id("18883550-e105-11dd-8c4d-001c25b8b54e")
	{
		PionPlatformUnitTest::cleanup_platform_config_files();

		if (boost::filesystem::exists(REACTORS_CONFIG_FILE))
			boost::filesystem::remove(REACTORS_CONFIG_FILE);

		if (boost::filesystem::exists(LOG_OUTPUT_FILE))
			boost::filesystem::remove(LOG_OUTPUT_FILE);

		boost::filesystem::copy_file(TR2_REACTORS_TEMPLATE_FILE, REACTORS_CONFIG_FILE);

		// start the ServiceManager, ReactionEngine, etc., add plugin paths
		m_platform_cfg.setConfigFile(PLATFORM_CONFIG_FILE);
		m_platform_cfg.openConfigFile();

		// get codec for CLF format
		m_codec_ptr = m_platform_cfg.getCodecFactory().getCodec(m_clf_codec_id);
	}

	virtual ~TransformReactorTests_F() {}


	const std::string				m_clf_codec_id;
	const std::string				m_transformer_id;
	const std::string				m_output_log_id;
	EventFactory					m_event_factory;
	pion::server::PlatformConfig	m_platform_cfg;
	CodecPtr						m_codec_ptr;
};


// TransformReactor unit tests

BOOST_FIXTURE_TEST_SUITE(TransformReactorTests_S, TransformReactorTests_F)

BOOST_AUTO_TEST_CASE(checkTransformLogFile) {
	// read in events from the log file
	std::uint64_t events_read = PionPlatformUnitTest::feedFileToReactor(
		m_platform_cfg.getReactionEngine(), m_transformer_id, *m_codec_ptr, CLF_LOG_FILE);

	// make sure that all events were read from the log
	BOOST_CHECK_EQUAL(events_read, static_cast<std::uint64_t>(NUM_CLF_RECORDS));

	// make sure that the reactor received all of the events read
	PionPlatformUnitTest::checkReactorEventsIn(m_platform_cfg.getReactionEngine(), m_transformer_id, events_read);

	// flush all events in the transform reactor to bypass waiting for time-outs (though there are none)
	m_platform_cfg.getReactionEngine().stopReactor(m_transformer_id);

	// check number of output events for each of the log reactors
	// (make sure they have finished writing data)
	PionPlatformUnitTest::checkReactorEventsOut(m_platform_cfg.getReactionEngine(), m_output_log_id, static_cast<std::uint64_t>(NUM_CLF_RECORDS));

	// make sure that the output files match what is expected
	BOOST_CHECK(PionUnitTest::check_files_match(LOG_OUTPUT_FILE, CLF_LOG_EXPECTED_FILE_1));
}

BOOST_AUTO_TEST_SUITE_END()


class ReactionEngineReadyToAddReactors_F {
public:
	ReactionEngineReadyToAddReactors_F() :
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
	virtual ~ReactionEngineReadyToAddReactors_F() {
		delete m_reaction_engine;
	}

	VocabularyManager	m_vocab_mgr;
	CodecFactory		m_codec_factory;
	ProtocolFactory		m_protocol_factory;
	DatabaseManager		m_database_mgr;
	ReactionEngine*		m_reaction_engine;
	std::string			m_reactor_config_file;
};

BOOST_FIXTURE_TEST_SUITE(ReactionEngineReadyToAddReactors_S, ReactionEngineReadyToAddReactors_F)

// Tests transformations with source terms of integer type.
// Also tests a transformation where the source term isn't the same as the term being modified.
BOOST_AUTO_TEST_CASE(checkTransformCLFLogFile) {
	// Add a TransformReactor that does some transformations with source terms of integer type.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Name>Float Transformer</Name>"
		"<Plugin>TransformReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Transformation>"
			"<Term>urn:vocab:clickstream#referer</Term>"
			"<Type>Lookup</Type>"
			"<LookupTerm>urn:vocab:clickstream#status</LookupTerm>"
			"<Lookup key=\"200\">OK</Lookup>"
		"</Transformation>"
		"<Transformation>"
			"<Term>urn:vocab:clickstream#bytes</Term>"
			"<Type>Regex</Type>"
			"<SourceTerm>urn:vocab:clickstream#bytes</SourceTerm>"
			"<Regex exp=\"\\d\\d\\d\\d\\d+\">10000</Regex>"
			"<Regex exp=\"^\\d\\d\\d$\">1000</Regex>"
			"<Regex exp=\"^\\d\\d$\">100</Regex>"
		"</Transformation>");
	std::string transformer_id = m_reaction_engine->addReactor(config_ptr);

	// Add a LogOutputReactor and connect it to the TransformReactor so it has something to send output to.
	config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Name>CLF Log Writer</Name>"
		"<Plugin>LogOutputReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Codec>3f49f2da-bfe3-11dc-8875-0016cb926e68</Codec>"
		"<Filename>../" + LOG_OUTPUT_FILE + "</Filename>");
	std::string log_writer_id = m_reaction_engine->addReactor(config_ptr);
	m_reaction_engine->addReactorConnection(transformer_id, log_writer_id);

	// Start the ReactionEngine.
	m_reaction_engine->start();

	// Read in events from a CLF log file.
	CodecPtr clf_codec_ptr = m_codec_factory.getCodec("3f49f2da-bfe3-11dc-8875-0016cb926e68");
	PionPlatformUnitTest::feedFileToReactor(
		*m_reaction_engine, transformer_id, *clf_codec_ptr, CLF_LOG_FILE);

	// Make sure the LogOutputReactor has finished writing the expected number of records.
	PionPlatformUnitTest::checkReactorEventsOut(*m_reaction_engine, log_writer_id, static_cast<std::uint64_t>(NUM_CLF_RECORDS));

	// Stop the LogOutputReactor and confirm that the log file it wrote has the expected contents.
	m_reaction_engine->stopReactor(log_writer_id);
	BOOST_CHECK(PionUnitTest::check_files_match(LOG_OUTPUT_FILE, CLF_LOG_EXPECTED_FILE_2));
}

// tests transformation of float terms
BOOST_AUTO_TEST_CASE(checkTransformStockPriceLogFile) {
	// Add a TransformReactor that does regular expression transformations on a term of type float.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Name>Float Transformer</Name>"
		"<Plugin>TransformReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Transformation>"
			"<Term>urn:vocab:stocks#price</Term>"
			"<SourceTerm>urn:vocab:stocks#price</SourceTerm>"
			"<Regex exp=\"\\d\\d\\d\\.\">-1</Regex>"
			"<Type>Regex</Type>"
		"</Transformation>");
	std::string transformer_id = m_reaction_engine->addReactor(config_ptr);

	// Add a LogOutputReactor and connect it to the TransformReactor so it has something to send output to.
	config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Name>Stock Price Log Writer</Name>"
		"<Plugin>LogOutputReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Codec>90eb7478-1629-11dd-81cb-0019e3f89cd2</Codec>"
		"<Filename>../" + LOG_OUTPUT_FILE + "</Filename>");
	std::string log_writer_id = m_reaction_engine->addReactor(config_ptr);
	m_reaction_engine->addReactorConnection(transformer_id, log_writer_id);

	// Start the ReactionEngine.
	m_reaction_engine->start();

	// Read in events from a log of stock prices.
	CodecPtr stock_codec_ptr = m_codec_factory.getCodec("90eb7478-1629-11dd-81cb-0019e3f89cd2");
	PionPlatformUnitTest::feedFileToReactor(
		*m_reaction_engine, transformer_id, *stock_codec_ptr, STOCK_PRICE_LOG_FILE);

	// Make sure the LogOutputReactor has finished writing the expected number of records.
	PionPlatformUnitTest::checkReactorEventsOut(*m_reaction_engine, log_writer_id, static_cast<std::uint64_t>(NUM_STOCK_RECORDS));

	// Stop the LogOutputReactor and confirm that the log file it wrote has the expected contents.
	m_reaction_engine->stopReactor(log_writer_id);
	BOOST_CHECK(PionUnitTest::check_files_match(LOG_OUTPUT_FILE, STOCK_PRICE_LOG_EXPECTED_FILE));
}

BOOST_AUTO_TEST_SUITE_END()


class TransformReactorEventValidator_F : public ReactionEngineReadyToAddReactors_F {
public:
	TransformReactorEventValidator_F() {
		m_num_events_validated = 0;
		m_page_event_ref = m_vocab_mgr.getVocabulary()->findTerm("urn:vocab:clickstream#page-event");
		m_sc_content_term_ref = m_vocab_mgr.getVocabulary()->findTerm("urn:vocab:clickstream#sc-content");
		m_sc_packets_term_ref = m_vocab_mgr.getVocabulary()->findTerm("urn:vocab:clickstream#sc-packets");
		m_uri_term_ref = m_vocab_mgr.getVocabulary()->findTerm("urn:vocab:clickstream#uri");
	}

	typedef std::map<Vocabulary::TermRef, std::string> ExpectedTerms;

	void MockEventHandler(EventPtr e, const ExpectedTerms& expected_terms) {
		if (m_num_events_validated > 0)
			return;

		BOOST_REQUIRE(! e->empty());
		for (ExpectedTerms::const_iterator i = expected_terms.begin(); i != expected_terms.end(); ++i)
			BOOST_CHECK_EQUAL(e->getString(i->first), i->second);

		++m_num_events_validated;
	}

	void sendEventAndValidateOutput(EventPtr e, const std::string& transformer_id) {
		// Add an Event handler to check that the output of the TransformReactor is as expected.
		Reactor::EventHandler eventValidator = std::bind(&TransformReactorEventValidator_F::MockEventHandler, this, std::placeholders::_1, m_expected_terms);
		m_reaction_engine->addTempConnectionOut(transformer_id, PionId().to_string(), "blah", eventValidator);

		// Start the ReactionEngine and send the input Event to the TransformReactor.
		m_reaction_engine->start();
		m_reaction_engine->send(transformer_id, e);

		// Check that the TransformReactor output an Event and that the Event was validated.
		PionPlatformUnitTest::checkReactorEventsOut(*m_reaction_engine, transformer_id, 1);
		BOOST_CHECK_EQUAL(m_num_events_validated, 1);
	}

	EventFactory		m_event_factory;
	int					m_num_events_validated;
	Vocabulary::TermRef	m_page_event_ref;
	Vocabulary::TermRef	m_sc_content_term_ref;
	Vocabulary::TermRef	m_sc_packets_term_ref;
	Vocabulary::TermRef	m_uri_term_ref;
	ExpectedTerms		m_expected_terms;
};

BOOST_FIXTURE_TEST_SUITE(TransformReactorEventValidator_S, TransformReactorEventValidator_F)

BOOST_AUTO_TEST_CASE(checkBasicRegexTransformation) {
	// Add a TransformReactor that does a regular expression transformation.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>TransformReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Transformation>"
			"<Term>urn:vocab:clickstream#sc-content</Term>"
			"<SourceTerm>urn:vocab:clickstream#sc-content</SourceTerm>"
			"<Regex exp=\"x=(\\S+)\">$1</Regex>"
			"<Type>Regex</Type>"
		"</Transformation>");
	std::string transformer_id = m_reaction_engine->addReactor(config_ptr);

	// Create an input Event and specify expected Term value(s) in the corresponding output Event.
	EventPtr e(m_event_factory.create(m_page_event_ref));
	e->setString(m_sc_content_term_ref, "some content x=hello blah blah");
	m_expected_terms[m_sc_content_term_ref] = "hello";

	sendEventAndValidateOutput(e, transformer_id);
}

BOOST_AUTO_TEST_CASE(checkRulesTransformation) {
	// Add a TransformReactor that does a Rule based Transformation.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>TransformReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Transformation>"
			"<Term>urn:vocab:clickstream#sc-content</Term>"
			"<Type>Rules</Type>"
			"<StopOnFirstMatch>true</StopOnFirstMatch>"
			"<Rule>"
				"<Term>urn:vocab:clickstream#sc-content</Term>"
				"<Type>exact-match</Type>"
				"<Value>hello</Value>"
				"<SetValue>Rule 1: content is hello</SetValue>"
			"</Rule>"
			"<Rule>"
				"<Term>urn:vocab:clickstream#sc-content</Term>"
				"<Type>starts-with</Type>"
				"<Value>hello</Value>"
				"<SetValue>Rule 2: content starts with hello</SetValue>"
			"</Rule>"
			"<Rule>"
				"<Term>urn:vocab:clickstream#sc-content</Term>"
				"<Type>contains</Type>"
				"<Value>hello</Value>"
				"<SetValue>Rule 3: content contains hello</SetValue>"
			"</Rule>"
		"</Transformation>");
	std::string transformer_id = m_reaction_engine->addReactor(config_ptr);

	// Create an input Event and specify expected Term value(s) in the corresponding output Event.
	EventPtr e(m_event_factory.create(m_page_event_ref));
	e->setString(m_sc_content_term_ref, "hello, world");
	m_expected_terms[m_sc_content_term_ref] = "Rule 2: content starts with hello";

	sendEventAndValidateOutput(e, transformer_id);
}

BOOST_AUTO_TEST_CASE(checkRulesTransformationWithUndefinedTerm) {
	// Add a TransformReactor that does a Rule based Transformation with Rules based on two different Terms.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>TransformReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Transformation>"
			"<Term>urn:vocab:clickstream#sc-content</Term>"
			"<Type>Rules</Type>"
			"<StopOnFirstMatch>true</StopOnFirstMatch>"
			"<Rule>"
				"<Term>urn:vocab:clickstream#visitor-id</Term>"
				"<Type>starts-with</Type>"
				"<Value>hello</Value>"
				"<SetValue>Rule 1: visitor-id Term starts with hello</SetValue>"
			"</Rule>"
			"<Rule>"
				"<Term>urn:vocab:clickstream#sc-content</Term>"
				"<Type>starts-with</Type>"
				"<Value>hello</Value>"
				"<SetValue>Rule 2: sc-content Term starts with hello</SetValue>"
			"</Rule>"
		"</Transformation>");
	std::string transformer_id = m_reaction_engine->addReactor(config_ptr);

	// Create an input Event with the visitor-id Term undefined and the sc-content Term set to a value 
	// such that the second Rule should match.
	EventPtr e(m_event_factory.create(m_page_event_ref));
	e->setString(m_sc_content_term_ref, "hello, world");
	m_expected_terms[m_sc_content_term_ref] = "Rule 2: sc-content Term starts with hello";

	sendEventAndValidateOutput(e, transformer_id);
}

BOOST_AUTO_TEST_CASE(checkRuleTypeIsDefined) {
	// Add a TransformReactor that does Rule based Transformations of type is-defined.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>TransformReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Transformation>"
			"<Term>urn:vocab:clickstream#sc-content</Term>"
			"<Type>Rules</Type>"
			"<StopOnFirstMatch>true</StopOnFirstMatch>"
			"<Rule>"
				"<Term>urn:vocab:clickstream#visitor-id</Term>"
				"<Type>is-defined</Type>"
				"<SetValue>Rule 1: visitor-id Term is defined</SetValue>"
			"</Rule>"
			"<Rule>"
				"<Term>urn:vocab:clickstream#sc-content</Term>"
				"<Type>is-defined</Type>"
				"<SetValue>Rule 2: sc-content Term is defined</SetValue>"
			"</Rule>"
		"</Transformation>");
	std::string transformer_id = m_reaction_engine->addReactor(config_ptr);

	// Create an input Event and specify expected Term value(s) in the corresponding output Event.
	EventPtr e(m_event_factory.create(m_page_event_ref));
	e->setString(m_sc_content_term_ref, "hello, world");

	m_expected_terms[m_sc_content_term_ref] = "Rule 2: sc-content Term is defined";

	sendEventAndValidateOutput(e, transformer_id);
}

BOOST_AUTO_TEST_CASE(checkRuleTypeIsNotDefined) {
	// Add a TransformReactor that does a Rule based Transformation of type is-not-defined.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>TransformReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Transformation>"
			"<Term>urn:vocab:clickstream#sc-content</Term>"
			"<Type>Rules</Type>"
			"<StopOnFirstMatch>true</StopOnFirstMatch>"
			"<Rule>"
				"<Term>urn:vocab:clickstream#sc-content</Term>"
				"<Type>is-not-defined</Type>"
				"<SetValue>Rule 1: sc-content Term is not defined</SetValue>"
			"</Rule>"
			"<Rule>"
				"<Term>urn:vocab:clickstream#visitor-id</Term>"
				"<Type>is-not-defined</Type>"
				"<SetValue>Rule 2: visitor-id Term is not defined</SetValue>"
			"</Rule>"
		"</Transformation>");
	std::string transformer_id = m_reaction_engine->addReactor(config_ptr);

	// Create an input Event and specify expected Term value(s) in the corresponding output Event.
	EventPtr e(m_event_factory.create(m_page_event_ref));
	e->setString(m_sc_content_term_ref, "hello, world");

	m_expected_terms[m_sc_content_term_ref] = "Rule 2: visitor-id Term is not defined";

	sendEventAndValidateOutput(e, transformer_id);
}

BOOST_AUTO_TEST_CASE(checkRuleTypeIsDefinedWithEventType) {
	// Add a TransformReactor that does a Rule based Transformation of type is-defined where the
	// Term, urn:vocab:clickstream#page-event, is the same as the type of the Event.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>TransformReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Transformation>"
			"<Term>urn:vocab:clickstream#sc-content</Term>"
			"<Type>Rules</Type>"
			"<StopOnFirstMatch>true</StopOnFirstMatch>"
			"<Rule>"
				"<Term>urn:vocab:clickstream#page-event</Term>"
				"<Type>is-defined</Type>"
				"<SetValue>Rule 1: page-event Term is defined</SetValue>"
			"</Rule>"
			"<Rule>"
				"<Term>urn:vocab:clickstream#sc-content</Term>"
				"<Type>starts-with</Type>"
				"<Value>hello</Value>"
				"<SetValue>Rule 2: content starts with hello</SetValue>"
			"</Rule>"
		"</Transformation>");
	std::string transformer_id = m_reaction_engine->addReactor(config_ptr);

	// Create an input Event and specify expected Term value(s) in the corresponding output Event.
	EventPtr e(m_event_factory.create(m_page_event_ref));
	e->setString(m_sc_content_term_ref, "hello, world");

	m_expected_terms[m_sc_content_term_ref] = "Rule 1: page-event Term is defined";

	sendEventAndValidateOutput(e, transformer_id);
}

BOOST_AUTO_TEST_CASE(checkRuleTypeIsNotDefinedWithEventType) {
	// Add a TransformReactor that does a Rule based Transformation of type is-not-defined where the
	// Term, urn:vocab:clickstream#page-event, is the same as the type of the Event.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>TransformReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Transformation>"
			"<Term>urn:vocab:clickstream#sc-content</Term>"
			"<Type>Rules</Type>"
			"<StopOnFirstMatch>true</StopOnFirstMatch>"
			"<Rule>"
				"<Term>urn:vocab:clickstream#page-event</Term>"
				"<Type>is-not-defined</Type>"
				"<SetValue>Rule 1: page-event Term is not defined</SetValue>"
			"</Rule>"
			"<Rule>"
				"<Term>urn:vocab:clickstream#sc-content</Term>"
				"<Type>starts-with</Type>"
				"<Value>hello</Value>"
				"<SetValue>Rule 2: content starts with hello</SetValue>"
			"</Rule>"
		"</Transformation>");
	std::string transformer_id = m_reaction_engine->addReactor(config_ptr);

	// Create an input Event and specify expected Term value(s) in the corresponding output Event.
	EventPtr e(m_event_factory.create(m_page_event_ref));
	e->setString(m_sc_content_term_ref, "hello, world");

	m_expected_terms[m_sc_content_term_ref] = "Rule 2: content starts with hello";

	sendEventAndValidateOutput(e, transformer_id);
}

BOOST_AUTO_TEST_CASE(checkUtf8RegexUsingHexadecimalEscapeCodes) {
	// Add a TransformReactor that does a regular expression transformation that includes a non-US-ASCII character.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>TransformReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Transformation>"
			"<Term>urn:vocab:clickstream#sc-content</Term>"
			"<SourceTerm>urn:vocab:clickstream#sc-content</SourceTerm>"
			"<Regex exp=\"\xCE\xB1=(\\S+)\">$1</Regex>"
			"<Type>Regex</Type>"
		"</Transformation>");
	std::string transformer_id = m_reaction_engine->addReactor(config_ptr);

	// Create an input Event and specify expected Term value(s) in the corresponding output Event.
	EventPtr e(m_event_factory.create(m_page_event_ref));
	char UTF8_ENCODED_TEST_CHAR_ARRAY[] = {
		(char)0xCE, (char)0xB1,				// UTF-8 encoding of U+03B1 (GREEK SMALL LETTER ALPHA)
		0x3D,								// '='
		0x31,								// '1'
		0x20,								// space
		(char)0xCE, (char)0xB2,				// UTF-8 encoding of U+03B2 (GREEK SMALL LETTER BETA)
		0x3D,								// '='
		0x32};								// '2'
	const std::string UTF8_ENCODED_TEST_STRING(UTF8_ENCODED_TEST_CHAR_ARRAY, sizeof(UTF8_ENCODED_TEST_CHAR_ARRAY));
	e->setString(m_sc_content_term_ref, UTF8_ENCODED_TEST_STRING);
	m_expected_terms[m_sc_content_term_ref] = "1";

	sendEventAndValidateOutput(e, transformer_id);
}

// This test is the same as checkUtf8RegexUsingHexadecimalEscapeCodes, except that the non-US-ASCII
// character 'alpha' appears directly in the regular expression.  (This may not be apparent if you are
// not viewing this test code with UTF-8 encoding, but note that reactors.xml is always UTF-8 encoded.)
BOOST_AUTO_TEST_CASE(checkUtf8RegexUsingEmbeddedUtf8) {
	// Add a TransformReactor that does a regular expression transformation that includes a non-US-ASCII character.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>TransformReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Transformation>"
			"<Term>urn:vocab:clickstream#sc-content</Term>"
			"<SourceTerm>urn:vocab:clickstream#sc-content</SourceTerm>"
			"<Regex exp=\"α=(\\S+)\">$1</Regex>"
			"<Type>Regex</Type>"
		"</Transformation>");
	std::string transformer_id = m_reaction_engine->addReactor(config_ptr);

	// Create an input Event and specify expected Term value(s) in the corresponding output Event.
	EventPtr e(m_event_factory.create(m_page_event_ref));
	char UTF8_ENCODED_TEST_CHAR_ARRAY[] = {
		(char)0xCE, (char)0xB1,				// UTF-8 encoding of U+03B1 (GREEK SMALL LETTER ALPHA)
		0x3D,								// '='
		0x31,								// '1'
		0x20,								// space
		(char)0xCE, (char)0xB2,				// UTF-8 encoding of U+03B2 (GREEK SMALL LETTER BETA)
		0x3D,								// '='
		0x32};								// '2'
	const std::string UTF8_ENCODED_TEST_STRING(UTF8_ENCODED_TEST_CHAR_ARRAY, sizeof(UTF8_ENCODED_TEST_CHAR_ARRAY));
	e->setString(m_sc_content_term_ref, UTF8_ENCODED_TEST_STRING);
	m_expected_terms[m_sc_content_term_ref] = "1";

	sendEventAndValidateOutput(e, transformer_id);
}

BOOST_AUTO_TEST_CASE(checkCaseInsensitiveUtf8Regex) {
	// Add a TransformReactor that does case insensitive regular expression transformations using non-US-ASCII characters.

	// A couple of arbitrary Terms of string type.
	std::string string_term_1 = "urn:vocab:atom#icon";
	std::string string_term_2 = "urn:vocab:atom#logo";
	Vocabulary::TermRef	string_term_1_ref = m_vocab_mgr.getVocabulary()->findTerm(string_term_1);
	Vocabulary::TermRef	string_term_2_ref = m_vocab_mgr.getVocabulary()->findTerm(string_term_2);

	// TODO: Put this in a config file (tr-reactor-i18n.xml?) for testing regex rules with non-US-ASCII characters.
	// Note that the first line of our config files is always: <?xml version="1.0" encoding="UTF-8"?>
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>TransformReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Transformation>"
			"<Term>" + string_term_1 + "</Term>"
			"<SourceTerm>urn:vocab:clickstream#sc-content</SourceTerm>"
			"<Regex exp=\"(?i)Äiti\">found it with uppercase</Regex>"
			"<Type>Regex</Type>"
		"</Transformation>"
		"<Transformation>"
			"<Term>" + string_term_2 + "</Term>"
			"<SourceTerm>urn:vocab:clickstream#sc-content</SourceTerm>"
			"<Regex exp=\"(?i)äiti\">found it with lowercase</Regex>"
			"<Type>Regex</Type>"
		"</Transformation>");
	std::string transformer_id = m_reaction_engine->addReactor(config_ptr);


	// Create an input Event and specify expected Term value(s) in the corresponding output Event.
	EventPtr e(m_event_factory.create(m_page_event_ref));
	//char utf8_content[] = {
	//	(char)0xC3, (char)0x84,	// UTF-8 encoding of U+00C4 (LATIN CAPITAL LETTER A WITH DIAERESIS)
	//	0x69,					// 'i'
	//	0x74,					// 't'
	//	0x69,					// 'i'
	//	0x00 };
	char utf8_content[] = "Äiti was here";
	e->setString(m_sc_content_term_ref, utf8_content);
	m_expected_terms[string_term_1_ref] = "found it with uppercase";
	m_expected_terms[string_term_2_ref] = "found it with lowercase";

	sendEventAndValidateOutput(e, transformer_id);
}

BOOST_AUTO_TEST_CASE(checkBasicRulesTransformation) {
	// Add a TransformReactor that does a Rules transformation.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>TransformReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Transformation>"
			"<Term>urn:vocab:clickstream#sc-content</Term>"
			"<Type>Rules</Type>"
			"<StopOnFirstMatch>true</StopOnFirstMatch>"
			"<Rule>"
				"<Term>urn:vocab:clickstream#sc-content</Term>"
				"<Type>regex</Type>"
				"<Value>x=(\\S+)</Value>"
				"<SetValue>$1</SetValue>"
			"</Rule>"
		"</Transformation>");
	std::string transformer_id = m_reaction_engine->addReactor(config_ptr);

	// Create an input Event and specify expected Term value(s) in the corresponding output Event.
	EventPtr e(m_event_factory.create(m_page_event_ref));
	e->setString(m_sc_content_term_ref, "some content x=hello blah blah");
	m_expected_terms[m_sc_content_term_ref] = "hello";

	sendEventAndValidateOutput(e, transformer_id);
}

// This test uses a regex that matches exactly one "word" character, and tests it against a UTF-8 string
// where the character we'd like to match consists of two bytes.
BOOST_AUTO_TEST_CASE(checkCharBasedRulesRegexAgainstNonAsciiChar) {
	// Add a TransformReactor that does a Rules transformation.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>TransformReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Transformation>"
			"<Term>urn:vocab:clickstream#sc-content</Term>"
			"<Type>Rules</Type>"
			"<StopOnFirstMatch>true</StopOnFirstMatch>"
			"<Rule>"
				"<Term>urn:vocab:clickstream#sc-content</Term>"
				"<Type>regex</Type>"
				"<Value>x=(\\w);</Value>"
				"<SetValue>$1</SetValue>"
			"</Rule>"
		"</Transformation>");
	std::string transformer_id = m_reaction_engine->addReactor(config_ptr);

	// Create an input Event and specify expected Term value(s) in the corresponding output Event.
	EventPtr e(m_event_factory.create(m_page_event_ref));
	e->setString(m_sc_content_term_ref, "x=α;");
	m_expected_terms[m_sc_content_term_ref] = "α";

	sendEventAndValidateOutput(e, transformer_id);
}

BOOST_AUTO_TEST_CASE(checkTransformLookup) {
	// Add a TransformReactor that does a Lookup transformation.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>TransformReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Transformation>"
			"<Term>urn:vocab:test#blob-1</Term>"
			"<Type>Lookup</Type>"
			"<LookupTerm>urn:vocab:clickstream#sc-content</LookupTerm>"
			"<Lookup key=\"x init\">special case x</Lookup>"
			"<Lookup key=\"α init\">special case alpha</Lookup>"
			"<Match>(\\w)\\s*=\\s*\\d+\\b;?</Match>"
			"<Format>$1 init</Format>"
			"<DefaultAction>output</DefaultAction>"
		"</Transformation>");
	std::string transformer_id = m_reaction_engine->addReactor(config_ptr);

	pion::platform::Vocabulary::TermRef	blob_1_term_ref = m_vocab_mgr.getVocabulary()->findTerm("urn:vocab:test#blob-1");

	// Create an input Event that should be modified by the regex and then match one of the lookup keys.
	EventPtr e0(m_event_factory.create(m_page_event_ref));
	e0->setString(m_sc_content_term_ref, "x=1;");
	m_expected_terms[blob_1_term_ref] = "special case x";

	sendEventAndValidateOutput(e0, transformer_id);

	// Same as the previous case, except that this time boost::u32regex is required for correct matching.
	EventPtr e1(m_event_factory.create(m_page_event_ref));
	e1->setString(m_sc_content_term_ref, "α=1;");
	m_expected_terms[blob_1_term_ref] = "special case alpha";

	sendEventAndValidateOutput(e1, transformer_id);

	// Create an input Event that should be modified by the regex but not match any lookup key.
	// Since DefaultAction = output, we expect the output to be the regex output.
	EventPtr e2(m_event_factory.create(m_page_event_ref));
	e2->setString(m_sc_content_term_ref, "a=2;");
	m_expected_terms[blob_1_term_ref] = "a init";

	sendEventAndValidateOutput(e2, transformer_id);

	// Create an input Event that should not be modified by the regex but should match one of the lookup keys.
	EventPtr e3(m_event_factory.create(m_page_event_ref));
	e3->setString(m_sc_content_term_ref, "α init");
	m_expected_terms[blob_1_term_ref] = "special case alpha";

	sendEventAndValidateOutput(e3, transformer_id);

	// Finally, create an input Event that shouldn't match anything.
	EventPtr e4(m_event_factory.create(m_page_event_ref));
	e4->setString(m_sc_content_term_ref, "blah");
	m_expected_terms[blob_1_term_ref] = "blah";

	sendEventAndValidateOutput(e4, transformer_id);
}

BOOST_AUTO_TEST_CASE(checkSplitTerm) {
	// Add a TransformReactor that does a Rule based Transformation of type is-not-defined where the
	// Term, urn:vocab:clickstream#page-event, is the same as the type of the Event.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>TransformReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Transformation>"
			"<Term>urn:vocab:clickstream#sc-content</Term>"
			"<Type>SplitTerm</Type>"
			"<Value sep=\"|\">urn:vocab:clickstream#sc-content</Value>"
		"</Transformation>");
	std::string transformer_id = m_reaction_engine->addReactor(config_ptr);

	// Create an input Event and specify expected Term value(s) in the corresponding output Event.
	EventPtr e(m_event_factory.create(m_page_event_ref));
	e->setString(m_sc_content_term_ref, "hello|world");

	m_expected_terms[m_sc_content_term_ref] = "hello";

	// See if the |world drops off
	sendEventAndValidateOutput(e, transformer_id);

	xmlNodePtr config_ptr2 = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>TransformReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Transformation>"
			"<Term>urn:vocab:clickstream#sc-content</Term>"
			"<Type>JoinTerm</Type>"
			"<Value sep=\",\">urn:vocab:clickstream#sc-content</Value>"
		"</Transformation>");
	std::string transformer_id2 = m_reaction_engine->addReactor(config_ptr2);

	m_expected_terms[m_sc_content_term_ref] = "hello,world";

	// See if we can glue them together again, with a different separator
	sendEventAndValidateOutput(e, transformer_id2);
}

BOOST_AUTO_TEST_CASE(checkJoinTerm) {
	// Add a TransformReactor that does a Rule based Transformation of type is-not-defined where the
	// Term, urn:vocab:clickstream#page-event, is the same as the type of the Event.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>TransformReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Transformation>"
			"<Term>urn:vocab:clickstream#sc-content</Term>"
			"<Type>JoinTerm</Type>"
			"<Value sep=\",\">urn:vocab:clickstream#sc-content</Value>"
		"</Transformation>");
	std::string transformer_id = m_reaction_engine->addReactor(config_ptr);

	// Create an input Event and specify expected Term value(s) in the corresponding output Event.
	EventPtr e(m_event_factory.create(m_page_event_ref));
	e->setString(m_sc_content_term_ref, "hello");
	e->setString(m_sc_content_term_ref, "world");

	// See if the two values get glued together into one value
	m_expected_terms[m_sc_content_term_ref] = "hello,world";

	sendEventAndValidateOutput(e, transformer_id);
}

BOOST_AUTO_TEST_CASE(checkJoinUniqueTerm) {
	// Add a TransformReactor that does a Rule based Transformation of type is-not-defined where the
	// Term, urn:vocab:clickstream#page-event, is the same as the type of the Event.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>TransformReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Transformation>"
			"<Term>urn:vocab:clickstream#sc-content</Term>"
			"<Type>JoinTerm</Type>"
			"<Value sep=\",\" uniq=\"true\">urn:vocab:clickstream#sc-content</Value>"
		"</Transformation>");
	std::string transformer_id = m_reaction_engine->addReactor(config_ptr);

	// Create an input Event and specify expected Term value(s) in the corresponding output Event.
	EventPtr e(m_event_factory.create(m_page_event_ref));
	e->setString(m_sc_content_term_ref, "hello");
	e->setString(m_sc_content_term_ref, "hello");
	e->setString(m_sc_content_term_ref, "world");

	// See if the three values get glued together into one value, with the dupe hello dropped
	m_expected_terms[m_sc_content_term_ref] = "hello,world";

	sendEventAndValidateOutput(e, transformer_id);
}

BOOST_AUTO_TEST_CASE(checkURLEncodeDecode) {
	// Add a TransformReactor that does a Rule based Transformation of type is-not-defined where the
	// Term, urn:vocab:clickstream#page-event, is the same as the type of the Event.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>TransformReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Transformation>"
			"<Term>urn:vocab:clickstream#sc-content</Term>"
			"<Type>URLEncode</Type>"
			"<Value>urn:vocab:clickstream#sc-content</Value>"
		"</Transformation>");
	std::string transformer_id = m_reaction_engine->addReactor(config_ptr);

	// Create an input Event and specify expected Term value(s) in the corresponding output Event.
	EventPtr e(m_event_factory.create(m_page_event_ref));
	e->setString(m_sc_content_term_ref, "hello?and what");

	// Verify that ? turns into %3F and space into %20
	m_expected_terms[m_sc_content_term_ref] = "hello%3Fand%20what";

	sendEventAndValidateOutput(e, transformer_id);


	// Verify that the changed strings turns back via URLDecode
	xmlNodePtr config_ptr2 = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>TransformReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Transformation>"
			"<Term>urn:vocab:clickstream#sc-content</Term>"
			"<Type>URLDecode</Type>"
			"<Value>urn:vocab:clickstream#sc-content</Value>"
		"</Transformation>");
	std::string transformer_id2 = m_reaction_engine->addReactor(config_ptr2);

	// See if the original string comes back, i.e. %3F turns into ? and %20 turns into space
	m_expected_terms[m_sc_content_term_ref] = "hello?and what";

	sendEventAndValidateOutput(e, transformer_id2);
}




BOOST_AUTO_TEST_CASE(checkAssignment) {
	// Add a TransformReactor that does a Rule based Transformation of type is-not-defined where the
	// Term, urn:vocab:clickstream#page-event, is the same as the type of the Event.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>TransformReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Transformation>"
			"<Term>urn:vocab:clickstream#sc-content</Term>"
			"<Type>AssignTerm</Type>"
			"<Value>urn:vocab:clickstream#uri</Value>"
		"</Transformation>");
	std::string transformer_id = m_reaction_engine->addReactor(config_ptr);

	// Create an input Event and specify expected Term value(s) in the corresponding output Event.
	EventPtr e(m_event_factory.create(m_page_event_ref));
	e->setString(m_uri_term_ref, "hello");

	// See if the three values get glued together into one value, with the dupe hello dropped
	m_expected_terms[m_sc_content_term_ref] = "hello";

	sendEventAndValidateOutput(e, transformer_id);
}

/*
 * Doesn't seem to work, probably the m_expected_terms assignment fails to work...
 *
BOOST_AUTO_TEST_CASE(checkAssignment2) {
	// Add a TransformReactor that does a Rule based Transformation of type is-not-defined where the
	// Term, urn:vocab:clickstream#page-event, is the same as the type of the Event.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>TransformReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Transformation>"
			"<Term>urn:vocab:clickstream#sc-packets</Term>"
			"<Type>AssignTerm</Type>"
			"<Value>urn:vocab:clickstream#sc-packets</Value>"
		"</Transformation>");
	std::string transformer_id = m_reaction_engine->addReactor(config_ptr);

	// Create an input Event and specify expected Term value(s) in the corresponding output Event.
	EventPtr e(m_event_factory.create(m_page_event_ref));
	e->setUInt(m_sc_packets_term_ref, 5);

	m_expected_terms[m_sc_packets_term_ref] = 5;

	sendEventAndValidateOutput(e, transformer_id);
}
*/

BOOST_AUTO_TEST_CASE(checkTypeTransformation) {
	// Add a TransformReactor that does a Rule based Transformation of type is-not-defined where the
	// Term, urn:vocab:clickstream#page-event, is the same as the type of the Event.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>TransformReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Transformation>"
			"<Term>urn:vocab:clickstream#sc-content</Term>"
			"<Type>AssignTerm</Type>"
			"<Value>urn:vocab:clickstream#sc-packets</Value>"
		"</Transformation>");
	std::string transformer_id = m_reaction_engine->addReactor(config_ptr);

	// Create an input Event and specify expected Term value(s) in the corresponding output Event.
	EventPtr e(m_event_factory.create(m_page_event_ref));
	e->setUInt(m_sc_packets_term_ref, 5);

	m_expected_terms[m_sc_content_term_ref] = "5";

	sendEventAndValidateOutput(e, transformer_id);
}


BOOST_AUTO_TEST_SUITE_END()


// HideCreditCard unit tests

BOOST_AUTO_TEST_CASE(checkHideCreditCardInStrings) {
	std::string original;
	std::string sanitized;

	original = "This does not have a 4349753849753894753049 credit card number!";
	sanitized = original;
	BOOST_CHECK(! HideCreditCardNumbers(sanitized.begin(), sanitized.end()));
	BOOST_CHECK_EQUAL(original, sanitized);
	
	original = "This does not have a 1234-5678-9012-4123 credit card number!";
	sanitized = original;
	BOOST_CHECK(! HideCreditCardNumbers(sanitized.begin(), sanitized.end()));
	BOOST_CHECK_EQUAL(original, sanitized);
	
	original = "This does have a 4444-5555-6666-7777 credit card number!";
	sanitized = original;
	BOOST_CHECK(HideCreditCardNumbers(sanitized.begin(), sanitized.end()));
	BOOST_CHECK_EQUAL(sanitized, "This does have a XXXXXXXXXXXXXXXXXXX credit card number!");

	original = "This has one 4444 5555 6666 7777 too!";
	sanitized = original;
	BOOST_CHECK(HideCreditCardNumbers(sanitized.begin(), sanitized.end()));
	BOOST_CHECK_EQUAL(sanitized, "This has one XXXXXXXXXXXXXXXXXXX too!");

	original = "This has two numbers 4444 5555 6666 7777 and another 4444-5555-6666-7777!";
	sanitized = original;
	BOOST_CHECK(HideCreditCardNumbers(sanitized.begin(), sanitized.end()));
	BOOST_CHECK_EQUAL(sanitized, "This has two numbers XXXXXXXXXXXXXXXXXXX and another XXXXXXXXXXXXXXXXXXX!");

	original = "number=4444555566667777&another=4444+5555+6666+7777&testing=querydata";
	sanitized = original;
	BOOST_CHECK(HideCreditCardNumbers(sanitized.begin(), sanitized.end()));
	BOOST_CHECK_EQUAL(sanitized, "number=XXXXXXXXXXXXXXXX&another=XXXXXXXXXXXXXXXXXXX&testing=querydata");

	original = "Testing number at end 4444-5555-6666-7777";
	sanitized = original;
	BOOST_CHECK(HideCreditCardNumbers(sanitized.begin(), sanitized.end()));
	BOOST_CHECK_EQUAL(sanitized, "Testing number at end XXXXXXXXXXXXXXXXXXX");

	original = "4444-5555-6666-7777 Testing number at beginning";
	sanitized = original;
	BOOST_CHECK(HideCreditCardNumbers(sanitized.begin(), sanitized.end()));
	BOOST_CHECK_EQUAL(sanitized, "XXXXXXXXXXXXXXXXXXX Testing number at beginning");

	original = "Test with other word boundaries.  ccnum:44 44-5555 66-66 7777!";
	sanitized = original;
	BOOST_CHECK(HideCreditCardNumbers(sanitized.begin(), sanitized.end()));
	BOOST_CHECK_EQUAL(sanitized, "Test with other word boundaries.  ccnum:XXXXXXXXXXXXXXXXXXXXX!");

	original = "Here is number 444-4-55 55-66 66-7777 with dashes and spaces!";
	sanitized = original;
	BOOST_CHECK(HideCreditCardNumbers(sanitized.begin(), sanitized.end()));
	BOOST_CHECK_EQUAL(sanitized, "Here is number XXXXXXXXXXXXXXXXXXXXXX with dashes and spaces!");

	original = "Multiple\r\n lines 4444-5555-6666-7777 credit card\r\nnumber!";
	sanitized = original;
	BOOST_CHECK(HideCreditCardNumbers(sanitized.begin(), sanitized.end()));
	BOOST_CHECK_EQUAL(sanitized, "Multiple\r\n lines XXXXXXXXXXXXXXXXXXX credit card\r\nnumber!");
}

class CreditCardInEventTests_F {
public:
	CreditCardInEventTests_F() {}
	virtual ~CreditCardInEventTests_F() {}

	EventFactory		m_event_factory;
};

BOOST_FIXTURE_TEST_SUITE(CreditCardInEventTests_S, CreditCardInEventTests_F)

BOOST_AUTO_TEST_CASE(checkHideCreditCardInEventWithOnlyNumericFields) {
	EventPtr event_ptr(m_event_factory.create(0));
	event_ptr->setUInt(1, 1UL);
	event_ptr->setUInt(2, 10UL);
	BOOST_CHECK(! HideCreditCardNumbers(*event_ptr));	// should return false but not throw
	BOOST_CHECK_THROW(HideCreditCardNumbers(*event_ptr, 1), boost::bad_get);	// should throw (not string type)
}

BOOST_AUTO_TEST_CASE(checkHideCreditCardInEventWithNumericAndStringFields) {
	EventPtr event_ptr(m_event_factory.create(1));
	event_ptr->setUInt(1, 1UL);
	event_ptr->setString(2, "Multiple\r\n lines 4444-5555-6666-7777 credit card\r\nnumber!");
	BOOST_CHECK(HideCreditCardNumbers(*event_ptr));
	BOOST_CHECK(! HideCreditCardNumbers(*event_ptr));	// should be no more to hide
	std::string expected_result("Multiple\r\n lines XXXXXXXXXXXXXXXXXXX credit card\r\nnumber!");
	BOOST_CHECK_EQUAL(expected_result, event_ptr->getString(2));
	BOOST_CHECK_EQUAL(event_ptr->getUInt(1), 1UL);
}

BOOST_AUTO_TEST_CASE(checkHideCreditCardInEventWithMultipleStringFields) {
	EventPtr event_ptr(m_event_factory.create(1));
	event_ptr->setString(1, "Here is number 444-4-55 55-66 66-7777 with dashes and spaces!");
	event_ptr->setString(2, "Test with other word boundaries.  ccnum:44 44-5555 66-66 7777!");
	event_ptr->setString(2, "Multiple\r\n lines 4444-5555-6666-7777 credit card\r\nnumber!");
	BOOST_CHECK(HideCreditCardNumbers(*event_ptr));

	Event::ConstIterator it = event_ptr->begin();
	BOOST_REQUIRE(it != event_ptr->end());
	std::string expected_result("Here is number XXXXXXXXXXXXXXXXXXXXXX with dashes and spaces!");
	BOOST_CHECK_EQUAL(expected_result, boost::get<const Event::BlobType&>(it->value).get());
	
	BOOST_REQUIRE(++it != event_ptr->end());
	expected_result = "Test with other word boundaries.  ccnum:XXXXXXXXXXXXXXXXXXXXX!";
	BOOST_CHECK_EQUAL(expected_result, boost::get<const Event::BlobType&>(it->value).get());

	BOOST_REQUIRE(++it != event_ptr->end());
	expected_result = "Multiple\r\n lines XXXXXXXXXXXXXXXXXXX credit card\r\nnumber!";
	BOOST_CHECK_EQUAL(expected_result, boost::get<const Event::BlobType&>(it->value).get());
	
	BOOST_CHECK(++it == event_ptr->end());	// should be no more parameters

	BOOST_CHECK(! HideCreditCardNumbers(*event_ptr));	// should be no more to hide
}

BOOST_AUTO_TEST_SUITE_END()

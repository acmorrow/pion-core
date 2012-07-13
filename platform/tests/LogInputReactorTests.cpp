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
#include <pion/PionScheduler.hpp>
#include <pion/PionUnitTestDefs.hpp>
#include <pion/platform/PionPlatformUnitTest.hpp>
#include <pion/platform/VocabularyManager.hpp>
#include <pion/platform/ReactionEngine.hpp>
#include <pion/platform/CodecFactory.hpp>
#include <pion/platform/ProtocolFactory.hpp>
#include <pion/platform/DatabaseManager.hpp>
#include <boost/regex.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/filesystem/operations.hpp>
#include <fstream>
#include <string>
#include <algorithm>
#include <cctype>

using namespace pion;
using namespace pion::platform;


/// static strings used by these unit tests
static const std::string NEW_OUTPUT_LOG_FILE(LOG_FILE_DIR + "new.log");
static const std::string NEW_INPUT_LOG_FNAME("combined-new");


const std::uint64_t NUM_LINES_IN_DEFAULT_LOG_FILE = 4;
const std::uint64_t TOTAL_LINES_IN_ALL_CLF_LOG_FILES = 7;
const std::uint64_t NUM_LINES_IN_LARGE_LOG_FILE = 20000;

static const std::string expected_urls[] = {
	// from combined.log
	"http://www.example.com/start.html",
	"http://www.atomiclabs.com/",
	"http://www.google.com/",
	"http://www.wikipedia.com/",

	// from comb-log-2.log
	"http://www.example.com/index.html",
	"http://www.amazon.com/",

	// from comb-log-3.log
	"http://xkcd.com/327"
};

static const unsigned int BUF_SIZE = 1023;

// These have external linkage so they can be used as template parameters.
extern const char LogCodec_id[]  = "3f49f2da-bfe3-11dc-8875-0016cb926e68";
extern const char JSONCodec_id[] = "9446b74a-71e4-426c-b965-ae55260375af";
extern const char XMLCodec_id[]  = "f7bb0fd8-3fe0-4227-accb-aaba2440a638";
extern const char LogCodec_file_ext[]  = ".log";
extern const char JSONCodec_file_ext[] = ".json";
extern const char XMLCodec_file_ext[]  = ".xml";


/// fixture for testing LogInputReactor
template<const char* codec_id, const char* file_ext>
class RunningReactionEngineWithLogInputReactor_F {
public:
	RunningReactionEngineWithLogInputReactor_F() :
		m_vocab_mgr(), m_codec_factory(m_vocab_mgr), m_protocol_factory(m_vocab_mgr), m_database_mgr(m_vocab_mgr),
		m_reaction_engine(NULL), m_file_ext(file_ext)
	{
		cleanup_config_files();
		PionPlatformUnitTest::cleanup_cache_files();
		
		m_vocab_mgr.setConfigFile(VOCABS_CONFIG_FILE);
		m_vocab_mgr.openConfigFile();
		m_codec_factory.setConfigFile(CODECS_CONFIG_FILE);
		m_codec_factory.openConfigFile();

		boost::filesystem::remove(NEW_OUTPUT_LOG_FILE);
		BOOST_REQUIRE(! boost::filesystem::exists(NEW_OUTPUT_LOG_FILE)); // Confirm, to avoid accidentally passing tests due to previous output.
		boost::filesystem::remove(LOG_FILE_DIR + NEW_INPUT_LOG_FNAME + file_ext);

		// Create a new (empty) reactor configuration file.
		m_reactor_config_file = CONFIG_FILE_DIR + "reactors1.xml";
		if (boost::filesystem::exists(m_reactor_config_file))
			boost::filesystem::remove(m_reactor_config_file);

		// Create and initialize a ReactionEngine.
		m_reaction_engine = new ReactionEngine(m_vocab_mgr, m_codec_factory, m_protocol_factory, m_database_mgr);
		m_reaction_engine->setConfigFile(m_reactor_config_file);
		m_reaction_engine->setNumThreads(1);
		m_reaction_engine->createConfigFile();

		// Add a LogInputReactor, since that's what we're testing here.
		xmlNodePtr config_ptr = makeLogInputReactorConfig();
		m_log_reader_id = m_reaction_engine->addReactor(config_ptr);
		m_input_codec_id = codec_id;

		// Add a LogOutputReactor and connect it to the LogInputReactor so it has something to send output to.
		config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
			"<Name>DLF Log Writer</Name>"
			"<Plugin>LogOutputReactor</Plugin>"
			"<Workspace>1</Workspace>"
			"<Codec>3f49f2da-bfe3-11dc-8875-0016cb926e68</Codec>"
			"<Filename>../logs/new.log</Filename>");
		m_log_writer_id = m_reaction_engine->addReactor(config_ptr);
		m_reaction_engine->addReactorConnection(m_log_reader_id, m_log_writer_id);

		// Start the ReactionEngine.  (This starts the log writer, but not the log reader.)
		m_reaction_engine->start();
	}
	virtual ~RunningReactionEngineWithLogInputReactor_F() {
		delete m_reaction_engine;
	}
	
	/// cleans up config files in the working directory
	void cleanup_config_files(void) {
		PionPlatformUnitTest::cleanup_vocab_config_files();

		if (boost::filesystem::exists(REACTORS_CONFIG_FILE))
			boost::filesystem::remove(REACTORS_CONFIG_FILE);
		boost::filesystem::copy_file(REACTORS_TEMPLATE_FILE, REACTORS_CONFIG_FILE);

		if (boost::filesystem::exists(CODECS_CONFIG_FILE))
			boost::filesystem::remove(CODECS_CONFIG_FILE);
		boost::filesystem::copy_file(CODECS_TEMPLATE_FILE, CODECS_CONFIG_FILE);
	}

	// Make a LogInputReactor configuration, using defaults for unspecified values.
	xmlNodePtr makeLogInputReactorConfig(const std::string& name = "",
										 const std::string& codec = "",
										 const std::string& directory = "",
										 const std::string& filename = "")
	{
		std::string inner_config_str = "<Plugin>LogInputReactor</Plugin><Workspace>1</Workspace><Name>";
		inner_config_str += !name.empty()?      name :      "Log Reader";
		inner_config_str += "</Name><Codec>";
		inner_config_str += !codec.empty()?     codec :     codec_id;
		inner_config_str += "</Codec><Directory>";
		inner_config_str += !directory.empty()? directory : "../logs";
		inner_config_str += "</Directory><Filename>";
		inner_config_str += !filename.empty()?  filename :  std::string("combined.*\\") + file_ext; // e.g. combined.*\.xml
		inner_config_str += "</Filename>";

		return PionPlatformUnitTest::makeReactorConfigFromString(inner_config_str);
	}
	std::string multiFileRegex(void) {
		return std::string("comb.*\\") + file_ext; // e.g. comb.*\.xml
	}
	void setupForLargeLogFile(void) {
		std::string large_log_file = LOG_FILE_DIR + "large" + file_ext;
		if (! boost::filesystem::exists(large_log_file)) {
			std::ofstream out(large_log_file.c_str());
			std::string before, after;
			if (file_ext == JSONCodec_file_ext) {
				out << "[";
				before = "\n{\"bytes\": ";
				after = "}, ";
			} else if (file_ext == XMLCodec_file_ext) {
				out << "<Events>\n";
				before = "<Event><bytes>";
				after = "</bytes></Event>\n";
			} else {
				before = "- - - [] \"\" - ";
				after = " \"\" \"\"\n";
			}
			for (unsigned int i = 0; i < NUM_LINES_IN_LARGE_LOG_FILE; ++i) {
				out << before << i << after;
			}
			if (file_ext == JSONCodec_file_ext) {
				out.seekp(-2, std::ios::cur);  // Back up over final comma.
				out << "\n]\n";
			} else if (file_ext == XMLCodec_file_ext) {
				out << "</Events>\n";
			}
		}

		// Reconfigure the LogInputReactor to search for the large log file.
		xmlNodePtr config_ptr = makeLogInputReactorConfig("", "", "", std::string("large") + file_ext);
		m_reaction_engine->setReactorConfig(m_log_reader_id, config_ptr);
	}
	void makeNewLogFileByCopying(const std::string& fname_of_file_to_copy) {
		boost::filesystem::copy_file(LOG_FILE_DIR + fname_of_file_to_copy + file_ext,
									 LOG_FILE_DIR + NEW_INPUT_LOG_FNAME + file_ext);
	}
	void getLogsConsumed(const std::string& reactor_id, std::vector<std::string>& logs_consumed) {
		Reactor::QueryBranches branches;
		Reactor::QueryParams params;
		std::ostringstream ss;
		
		logs_consumed.clear();
		m_reaction_engine->query(reactor_id, ss, branches, params);
		const std::string response_str(ss.str());
		std::string::const_iterator start = response_str.begin();
		std::string::const_iterator end = response_str.end();

		boost::regex rx("<ConsumedLog>([^<]+?)</ConsumedLog>");
		boost::match_results<std::string::const_iterator> rx_matches;
		while (boost::regex_search(start, end, rx_matches, rx)) {
			BOOST_REQUIRE_EQUAL(rx_matches.size(), 2U);
			logs_consumed.push_back(std::string(rx_matches[1].first, rx_matches[1].second));
			start = rx_matches[1].second;
		}
	}
	bool waitToConsumeLogs(const std::string& reactor_id,
		std::vector<std::string>& logs_expected,
		const std::uint32_t wait_seconds = 1)
	{
		const int num_checks_allowed = 10 * wait_seconds;
		std::vector<std::string> logs_consumed;
		int i = 0;
		while (true) {
			getLogsConsumed(reactor_id, logs_consumed);
			bool consumed_all_logs = true;
			for (std::vector<std::string>::iterator name_it = logs_expected.begin();
				name_it != logs_expected.end(); ++name_it)
			{
				bool consumed_log = false;
				for (std::vector<std::string>::iterator it = logs_consumed.begin();
					it != logs_consumed.end(); ++it)
				{
					if (it->find(*name_it) != std::string::npos) {
						consumed_log = true;
						break;
					}
				}
				if (!consumed_log) {
					consumed_all_logs = false;
					break;
				}
			}
			if (consumed_all_logs) return true;
			if (++i > num_checks_allowed) break;
			pion::PionScheduler::sleep(0, 100000000); // 0.1 seconds
		}

		return false;
	}
	

	VocabularyManager	m_vocab_mgr;
	CodecFactory		m_codec_factory;
	ProtocolFactory		m_protocol_factory;
	DatabaseManager		m_database_mgr;
	ReactionEngine*		m_reaction_engine;
	std::string			m_reactor_config_file;
	std::string			m_log_reader_id;
	std::string			m_log_writer_id;
	char				m_buf[BUF_SIZE + 1];
	std::string			m_input_codec_id;
	std::string			m_file_ext;
};


typedef boost::mpl::list<RunningReactionEngineWithLogInputReactor_F<LogCodec_id, LogCodec_file_ext>,
#ifdef PION_HAVE_JSON
						 RunningReactionEngineWithLogInputReactor_F<JSONCodec_id, JSONCodec_file_ext>,
#endif
						 RunningReactionEngineWithLogInputReactor_F<XMLCodec_id, XMLCodec_file_ext> > codec_fixture_list;

BOOST_AUTO_TEST_SUITE_FIXTURE_TEMPLATE(RunningReactionEngineWithLogInputReactor_S, codec_fixture_list)

/*
BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(demonstrateLeak) {
	// start the log reader reactor
	m_reaction_engine->startReactor(m_log_reader_id);

	PionScheduler::sleep(0, 100000000);
}
*/

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkNumberOfEventsProcessed) {
	// Start the LogInputReactor.
	F::m_reaction_engine->startReactor(F::m_log_reader_id);

	// Wait up to one second for the LogInputReactor to finish consuming the log file.
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id, NUM_LINES_IN_DEFAULT_LOG_FILE);

	// Confirm that the LogInputReactor has the expected number of input events and output events.
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(F::m_log_reader_id),  NUM_LINES_IN_DEFAULT_LOG_FILE);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(F::m_log_reader_id), NUM_LINES_IN_DEFAULT_LOG_FILE);
}

// Check that the LogInputReactor correctly handled at least one value (specifically, the referrer URL) for every input line.
BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(spotCheckEvents) {
	// Start the LogInputReactor.
	F::m_reaction_engine->startReactor(F::m_log_reader_id);

	// Wait up to one second for the LogInputReactor to finish consuming the log file.
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id, NUM_LINES_IN_DEFAULT_LOG_FILE);

	// Confirm that the new log file has some expected values in it.
	F::m_reaction_engine->stopReactor(F::m_log_writer_id);
	std::ifstream log_stream(NEW_OUTPUT_LOG_FILE.c_str());
	BOOST_REQUIRE(log_stream.is_open());
	for (unsigned int i = 0; i < NUM_LINES_IN_DEFAULT_LOG_FILE; ++i) {
		log_stream.getline(F::m_buf, BUF_SIZE);
		BOOST_CHECK(boost::regex_search(F::m_buf, boost::regex(expected_urls[i].c_str())));
	}
	BOOST_CHECK(! log_stream.getline(F::m_buf, BUF_SIZE));
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(testEmptyLogFile) {
	// Reconfigure the LogInputReactor to search for the empty log file.
	xmlNodePtr config_ptr = this->makeLogInputReactorConfig("", "", "", "comb-log-empty.*");
	F::m_reaction_engine->setReactorConfig(F::m_log_reader_id, config_ptr);

	// Start the LogInputReactor.
	F::m_reaction_engine->startReactor(F::m_log_reader_id);

	// Wait until we've finished consuming the log
	std::vector<std::string> logs_expected;
	logs_expected.push_back("comb-log-empty");
	BOOST_REQUIRE(this->waitToConsumeLogs(F::m_log_reader_id, logs_expected));

	// Confirm that the number of input events and output events is zero.
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(F::m_log_reader_id), static_cast<std::uint64_t>(0));
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(F::m_log_reader_id), static_cast<std::uint64_t>(0));

	// Stop the OutputLogReactor.  If, as expected, it's empty, it will be deleted.
	F::m_reaction_engine->stopReactor(F::m_log_writer_id);

	// Confirm that the output log file is not present.
	BOOST_CHECK(!boost::filesystem::exists(NEW_OUTPUT_LOG_FILE));
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkNumberOfEventsProcessedForMultipleLogFiles) {
	// Reconfigure the LogInputReactor to search for multiple log files.
	xmlNodePtr config_ptr = this->makeLogInputReactorConfig("", "", "", this->multiFileRegex());
	F::m_reaction_engine->setReactorConfig(F::m_log_reader_id, config_ptr);

	// Start the LogInputReactor.
	F::m_reaction_engine->startReactor(F::m_log_reader_id);

	// Wait up to one second for the LogInputReactor to finish consuming all the log files.
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id, TOTAL_LINES_IN_ALL_CLF_LOG_FILES);

	// Confirm that the Reactor has the expected number of input events and output events.
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(F::m_log_reader_id), TOTAL_LINES_IN_ALL_CLF_LOG_FILES);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(F::m_log_reader_id), TOTAL_LINES_IN_ALL_CLF_LOG_FILES);
}

// Check that the LogInputReactor correctly handled at least one value (specifically, the referrer URL) for every input line.
BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(spotCheckEventsForMultipleLogFiles) {
	// Reconfigure the LogInputReactor to search for multiple log files.
	xmlNodePtr config_ptr = this->makeLogInputReactorConfig("", "", "", this->multiFileRegex());
	F::m_reaction_engine->setReactorConfig(F::m_log_reader_id, config_ptr);

	// Start the LogInputReactor.
	F::m_reaction_engine->startReactor(F::m_log_reader_id);

	// Wait up to one second for the LogInputReactor to finish consuming all the log files.
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id, TOTAL_LINES_IN_ALL_CLF_LOG_FILES);

	// Stop the LogOutputReactor and extract all the URLs from the output log.
	F::m_reaction_engine->stopReactor(F::m_log_writer_id);
	std::ifstream log_stream(NEW_OUTPUT_LOG_FILE.c_str());
	BOOST_REQUIRE(log_stream.is_open());
	std::list<std::string> read_urls_list;
	for (unsigned int i = 0; i < TOTAL_LINES_IN_ALL_CLF_LOG_FILES; ++i) {
		log_stream.getline(F::m_buf, BUF_SIZE);

		// Extract the URL and add it to the list.
		boost::regex expected_line(".*\"(http://[^\"]*)\".*");
		boost::cmatch match_results;
		BOOST_CHECK(boost::regex_match(F::m_buf, match_results, expected_line));
		read_urls_list.push_back(match_results[1].str());
	}
	BOOST_CHECK(! log_stream.getline(F::m_buf, BUF_SIZE));

	// Confirm that the extracted URLs are the expected URLs (possibly reordered).
	std::list<std::string> expected_urls_list(expected_urls, expected_urls + TOTAL_LINES_IN_ALL_CLF_LOG_FILES);
	expected_urls_list.sort();
	read_urls_list.sort();
	BOOST_CHECK(expected_urls_list == read_urls_list);
}

/*
BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkConsumedFilesSkippedAfterRestart) {
	// Start the LogInputReactor.
	F::m_reaction_engine->startReactor(F::m_log_reader_id);

	// Wait up to one second for the LogInputReactor to finish consuming the log file.
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id, NUM_LINES_IN_DEFAULT_LOG_FILE);

	// Confirm that the Reactor has the expected number of input events and output events.
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(F::m_log_reader_id), NUM_LINES_IN_DEFAULT_LOG_FILE);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(F::m_log_reader_id), NUM_LINES_IN_DEFAULT_LOG_FILE);

	// Stop the ReactionEngine and create a new log file for the LogInputReactor to consume.
	F::m_reaction_engine->stop();
	this->makeNewLogFileByCopying("comb-log-2");
	const int num_lines_in_new_input_log_file = 2;

	// Restart the ReactionEngine and the LogInputReactor.
	F::m_reaction_engine->start();
	std::uint64_t events_in_at_start = F::m_reaction_engine->getEventsIn(F::m_log_reader_id);
	std::uint64_t events_out_at_start = F::m_reaction_engine->getEventsOut(F::m_log_reader_id);
	F::m_reaction_engine->startReactor(F::m_log_reader_id);

	// Compute the expected totals of input events and output events.  After being restarted, the Reactor 
	// should consume only the events from the new file, thus the totals should only increase by 2.
	std::uint64_t expected_events_in  = events_in_at_start  + num_lines_in_new_input_log_file;
	std::uint64_t expected_events_out = events_out_at_start + num_lines_in_new_input_log_file;

	// Wait up to one second for the expected number of input events.
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id, expected_events_in);

	// Confirm the expected totals of input events and output events.
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(F::m_log_reader_id),  expected_events_in);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(F::m_log_reader_id), expected_events_out);
}
*/

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkConsumedFilesSkippedAfterEngineReloaded) {
	// Start the LogInputReactor.
	F::m_reaction_engine->startReactor(F::m_log_reader_id);

	// Wait up to one second for the LogInputReactor to finish consuming the log file.
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id, NUM_LINES_IN_DEFAULT_LOG_FILE);

	// Confirm that the Reactor has the expected number of input events and output events.
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(F::m_log_reader_id), NUM_LINES_IN_DEFAULT_LOG_FILE);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(F::m_log_reader_id), NUM_LINES_IN_DEFAULT_LOG_FILE);

	// Delete the ReactionEngine.  (This is needed to enable creating a new LogInputReactor with the same ID, 
	// because IDs can only be specified in a configuration file, configuration files can only be read if 
	// they're not already open, and they can only be closed by ConfigManager::~ConfigManager().)
	delete F::m_reaction_engine;

	// Create a new log file for the LogInputReactor to consume.
	this->makeNewLogFileByCopying("comb-log-2");
	const int num_lines_in_new_input_log_file = 2;

	// Create a new ReactionEngine, using the config file created in the constructor.
	F::m_reaction_engine = new ReactionEngine(F::m_vocab_mgr, F::m_codec_factory, F::m_protocol_factory, F::m_database_mgr);
	F::m_reaction_engine->setConfigFile(F::m_reactor_config_file);
	F::m_reaction_engine->setNumThreads(1);
	F::m_reaction_engine->openConfigFile();

	// Start the LogInputReactor.  (The ReactionEngine and LogOutputReactor were started by openConfigFile().)
	F::m_reaction_engine->startReactor(F::m_log_reader_id);

	// The LogInputReactor should consume only the events from the new file.
	std::uint64_t expected_events_in  = num_lines_in_new_input_log_file;
	std::uint64_t expected_events_out = num_lines_in_new_input_log_file;

	// Wait up to one second for the expected number of input events.
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id, expected_events_in);

	// Confirm the expected totals of input events and output events.
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(F::m_log_reader_id),  expected_events_in);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(F::m_log_reader_id), expected_events_out);
}

/*

// These two tests are commented out for now, because the calibration isn't good enough to 
// force the desired behavior for all configurations and platforms.

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkPartiallyConsumedFileResumedAfterRestartingReactor) {
	this->setupForLargeLogFile();

	// Start the LogInputReactor.
	F::m_reaction_engine->startReactor(F::m_log_reader_id);

	// Calibrate.
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id, NUM_LINES_IN_LARGE_LOG_FILE / 100); // i.e. less than 1% read

	// Stop and restart the LogInputReactor 10 times.
	std::uint64_t prev_num_events_in = 0;
	for (unsigned int i = 0; i < 10; ++i) {
		PionScheduler::sleep(0, num_nsec);
		F::m_reaction_engine->stopReactor(F::m_log_reader_id);
		std::uint64_t num_events_in = F::m_reaction_engine->getEventsIn(F::m_log_reader_id);
		BOOST_CHECK_GT(num_events_in, prev_num_events_in); // i.e. at least one event was read
		prev_num_events_in = num_events_in;
		F::m_reaction_engine->startReactor(F::m_log_reader_id);
	}

	// Wait up to 20 seconds for the LogInputReactor to finish consuming the log file.
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id, NUM_LINES_IN_LARGE_LOG_FILE, 20);

	// Confirm that the LogInputReactor has the expected number of input events and output events.
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(F::m_log_reader_id), static_cast<std::uint64_t>(NUM_LINES_IN_LARGE_LOG_FILE));
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(F::m_log_reader_id), static_cast<std::uint64_t>(NUM_LINES_IN_LARGE_LOG_FILE));
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkPartiallyConsumedFileResumedAfterRestartingEngine) {
	this->setupForLargeLogFile();

	// Start the LogInputReactor.
	F::m_reaction_engine->startReactor(F::m_log_reader_id);

	// Calibrate.
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id, NUM_LINES_IN_LARGE_LOG_FILE / 100); // i.e. less than 1% read

	// Stop and restart the ReactorEngine 10 times.
	std::uint64_t prev_num_events_in = 0;
	for (unsigned int i = 0; i < 10; ++i) {
		PionScheduler::sleep(0, num_nsec);

		// If the LogInputReactor is not stopped first, bad assertion exceptions are often thrown in LogOutputReactor::operator().
		F::m_reaction_engine->stopReactor(F::m_log_reader_id);

		F::m_reaction_engine->stop();
		std::uint64_t num_events_in = F::m_reaction_engine->getEventsIn(F::m_log_reader_id);
		BOOST_CHECK(num_events_in > prev_num_events_in); // i.e. at least one event was read
		prev_num_events_in = num_events_in;
		F::m_reaction_engine->start();
		F::m_reaction_engine->startReactor(F::m_log_writer_id);
		F::m_reaction_engine->startReactor(F::m_log_reader_id);
	}

	// Wait up to 20 seconds for the LogInputReactor to finish consuming the log file.
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id, NUM_LINES_IN_LARGE_LOG_FILE, 20);

	// Confirm that the Reactor has the expected number of input events and output events.
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(F::m_log_reader_id), static_cast<std::uint64_t>(NUM_LINES_IN_LARGE_LOG_FILE));
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(F::m_log_reader_id), static_cast<std::uint64_t>(NUM_LINES_IN_LARGE_LOG_FILE));
}
*/

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkPartiallyConsumedFileResumedAfterEngineReloaded) {
	this->setupForLargeLogFile();

	// Start the LogInputReactor.
	F::m_reaction_engine->startReactor(F::m_log_reader_id);

	// Run until at least a few events have been read.
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id, 10UL);

	// Stop the LogInputReactor and save the numbers of input events and output events.
	F::m_reaction_engine->stopReactor(F::m_log_reader_id);
	std::uint64_t events_in_before_delete = F::m_reaction_engine->getEventsIn(F::m_log_reader_id);
	std::uint64_t events_out_before_delete = F::m_reaction_engine->getEventsOut(F::m_log_reader_id);

	// Delete the ReactionEngine.  (This is needed to enable creating a new LogInputReactor with the same ID, 
	// because IDs can only be specified in a configuration file, configuration files can only be read if 
	// they're not already open, and they can only be closed by ConfigManager::~ConfigManager().)
	delete F::m_reaction_engine;

	// Create a new ReactionEngine, using the config file created in the constructor.
	F::m_reaction_engine = new ReactionEngine(F::m_vocab_mgr, F::m_codec_factory, F::m_protocol_factory, F::m_database_mgr);
	F::m_reaction_engine->setConfigFile(F::m_reactor_config_file);
	F::m_reaction_engine->setNumThreads(1);
	F::m_reaction_engine->openConfigFile();

	// Start the LogInputReactor.  (The ReactionEngine and LogOutputReactor were started by openConfigFile().)
	F::m_reaction_engine->startReactor(F::m_log_reader_id);

	// Compute the expected numbers of input events and output events (which should be
	// the number of lines in the log file that haven't yet been read).
	std::uint64_t expected_events_in  = NUM_LINES_IN_LARGE_LOG_FILE - events_in_before_delete;
	std::uint64_t expected_events_out = NUM_LINES_IN_LARGE_LOG_FILE - events_out_before_delete;

	// Wait up to 20 seconds for the expected number of input events.
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id, expected_events_in, 20);

	// Confirm that the Reactor has the expected number of input events and output events.
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(F::m_log_reader_id),  expected_events_in);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(F::m_log_reader_id), expected_events_out);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkNumberOfEventsProcessedForMultipleReaders) {
	// Make another LogInputReactor.
	xmlNodePtr config_ptr = this->makeLogInputReactorConfig();
	std::string log_reader_id_2 = F::m_reaction_engine->addReactor(config_ptr);
	F::m_reaction_engine->addReactorConnection(log_reader_id_2, F::m_log_writer_id);

	// Start the LogInputReactors.
	F::m_reaction_engine->startReactor(F::m_log_reader_id);
	F::m_reaction_engine->startReactor(log_reader_id_2);

	// Wait up to one second for the LogInputReactors to finish consuming the log file.
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id, NUM_LINES_IN_DEFAULT_LOG_FILE);
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, log_reader_id_2, NUM_LINES_IN_DEFAULT_LOG_FILE);

	// Confirm that both LogInputReactors have the expected number of input events and output events.
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(F::m_log_reader_id),  NUM_LINES_IN_DEFAULT_LOG_FILE);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(F::m_log_reader_id), NUM_LINES_IN_DEFAULT_LOG_FILE);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(log_reader_id_2),  NUM_LINES_IN_DEFAULT_LOG_FILE);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(log_reader_id_2), NUM_LINES_IN_DEFAULT_LOG_FILE);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkNumberOfEventsProcessedForMultipleReadersAndMultipleLogFiles) {
	// Reconfigure the LogInputReactor to search for multiple log files.
	xmlNodePtr config_ptr = this->makeLogInputReactorConfig("", "", "", this->multiFileRegex());
	F::m_reaction_engine->setReactorConfig(F::m_log_reader_id, config_ptr);

	// Make another LogInputReactor to search for multiple log files.
	std::string log_reader_id_2 = F::m_reaction_engine->addReactor(config_ptr);
	F::m_reaction_engine->addReactorConnection(log_reader_id_2, F::m_log_writer_id);

	// Start the LogInputReactors.
	F::m_reaction_engine->startReactor(F::m_log_reader_id);
	F::m_reaction_engine->startReactor(log_reader_id_2);

	// Wait up to one second for the LogInputReactors to finish consuming the log file.
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id, TOTAL_LINES_IN_ALL_CLF_LOG_FILES);
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, log_reader_id_2, TOTAL_LINES_IN_ALL_CLF_LOG_FILES);

	// Confirm that both LogInputReactors have the expected number of input events and output events.
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(F::m_log_reader_id),  TOTAL_LINES_IN_ALL_CLF_LOG_FILES);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(F::m_log_reader_id), TOTAL_LINES_IN_ALL_CLF_LOG_FILES);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(log_reader_id_2),  TOTAL_LINES_IN_ALL_CLF_LOG_FILES);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(log_reader_id_2), TOTAL_LINES_IN_ALL_CLF_LOG_FILES);
}

/*
BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkConsumedFilesSkippedAfterRestartForMultipleReaders) {
	// Make another LogInputReactor.
	xmlNodePtr config_ptr = this->makeLogInputReactorConfig();
	std::string log_reader_id_2 = F::m_reaction_engine->addReactor(config_ptr);

	// Start the LogInputReactors.
	F::m_reaction_engine->startReactor(F::m_log_reader_id);
	F::m_reaction_engine->startReactor(log_reader_id_2);

	// Wait up to one second for the LogInputReactors to finish consuming the log file.
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id, NUM_LINES_IN_DEFAULT_LOG_FILE);
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, log_reader_id_2, NUM_LINES_IN_DEFAULT_LOG_FILE);

	// Confirm that both LogInputReactors have the expected number of input events and output events.
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(F::m_log_reader_id),  NUM_LINES_IN_DEFAULT_LOG_FILE);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(F::m_log_reader_id), NUM_LINES_IN_DEFAULT_LOG_FILE);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(log_reader_id_2),  NUM_LINES_IN_DEFAULT_LOG_FILE);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(log_reader_id_2), NUM_LINES_IN_DEFAULT_LOG_FILE);

	// Stop the ReactionEngine and create a new log file for the LogInputReactors to consume.
	F::m_reaction_engine->stop();
	this->makeNewLogFileByCopying("comb-log-2");
	const int num_lines_in_new_input_log_file = 2;

	// Restart the ReactionEngine, check and save the number of input and output events, and restart the LogInputReactors.
	F::m_reaction_engine->start();
	std::uint64_t events_in_at_start = F::m_reaction_engine->getEventsIn(F::m_log_reader_id);
	std::uint64_t events_out_at_start = F::m_reaction_engine->getEventsOut(F::m_log_reader_id);
	BOOST_CHECK_EQUAL(events_in_at_start, NUM_LINES_IN_DEFAULT_LOG_FILE);
	BOOST_CHECK_EQUAL(events_out_at_start, NUM_LINES_IN_DEFAULT_LOG_FILE);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(log_reader_id_2), events_in_at_start);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(log_reader_id_2), events_out_at_start);
	F::m_reaction_engine->startReactor(F::m_log_reader_id);
	F::m_reaction_engine->startReactor(log_reader_id_2);

	// Compute the expected totals of input events and output events.  After being restarted, the LogInputReactors 
	// should consume only the events from the new file, thus the totals should only increase by 2.
	std::uint64_t expected_events_in  = events_in_at_start  + num_lines_in_new_input_log_file;
	std::uint64_t expected_events_out = events_out_at_start + num_lines_in_new_input_log_file;

	// Wait up to one second for the expected number of input events.
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id, expected_events_in);
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, log_reader_id_2, expected_events_in);

	// Confirm that both LogInputReactors have the expected number of input events and output events.
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(F::m_log_reader_id),  expected_events_in);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(F::m_log_reader_id), expected_events_out);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(log_reader_id_2),  expected_events_in);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(log_reader_id_2), expected_events_out);
}
*/

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkConsumedFilesSkippedAfterEngineReloadedForMultipleReaders) {
	// Make another LogInputReactor.
	xmlNodePtr config_ptr = this->makeLogInputReactorConfig();
	std::string log_reader_id_2 = F::m_reaction_engine->addReactor(config_ptr);

	// Start the LogInputReactors.
	F::m_reaction_engine->startReactor(F::m_log_reader_id);
	F::m_reaction_engine->startReactor(log_reader_id_2);

	// Wait up to one second for the LogInputReactors to finish consuming the log file.
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id, NUM_LINES_IN_DEFAULT_LOG_FILE);
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, log_reader_id_2, NUM_LINES_IN_DEFAULT_LOG_FILE);

	// Confirm that both LogInputReactors have the expected number of input events and output events.
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(F::m_log_reader_id),  NUM_LINES_IN_DEFAULT_LOG_FILE);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(F::m_log_reader_id), NUM_LINES_IN_DEFAULT_LOG_FILE);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(log_reader_id_2),  NUM_LINES_IN_DEFAULT_LOG_FILE);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(log_reader_id_2), NUM_LINES_IN_DEFAULT_LOG_FILE);

	// Delete the ReactionEngine.  (This is needed to enable creating a new LogInputReactor with the same ID, 
	// because IDs can only be specified in a configuration file, configuration files can only be read if 
	// they're not already open, and they can only be closed by ConfigManager::~ConfigManager().)
	delete F::m_reaction_engine;

	// Create a new log file for the LogInputReactor to consume.
	this->makeNewLogFileByCopying("comb-log-2");
	const int num_lines_in_new_input_log_file = 2;

	// Create a new ReactionEngine, using the config file created in the constructor.
	F::m_reaction_engine = new ReactionEngine(F::m_vocab_mgr, F::m_codec_factory, F::m_protocol_factory, F::m_database_mgr);
	F::m_reaction_engine->setConfigFile(F::m_reactor_config_file);
	F::m_reaction_engine->setNumThreads(1);
	F::m_reaction_engine->openConfigFile();

	// Start the LogInputReactors.  (The ReactionEngine and LogOutputReactor were started by openConfigFile().)
	F::m_reaction_engine->startReactor(F::m_log_reader_id);
	F::m_reaction_engine->startReactor(log_reader_id_2);

	// The LogInputReactors should consume only the events from the new file.
	std::uint64_t expected_events_in  = num_lines_in_new_input_log_file;
	std::uint64_t expected_events_out = num_lines_in_new_input_log_file;

	// Wait up to one second for the expected number of input events.
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id, expected_events_in);
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, log_reader_id_2, expected_events_in);

	// Confirm that both LogInputReactors have the expected number of input events and output events.
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(F::m_log_reader_id),  expected_events_in);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(F::m_log_reader_id), expected_events_out);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(log_reader_id_2),  expected_events_in);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(log_reader_id_2), expected_events_out);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkHistoryCacheUpdating) {
	// Start the LogInputReactor.
	F::m_reaction_engine->startReactor(F::m_log_reader_id);

	// Wait up to one second for the LogInputReactor to finish consuming the default log file.
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id, NUM_LINES_IN_DEFAULT_LOG_FILE);

	// Wait until we've finished consuming the log
	std::vector<std::string> logs_expected;
	logs_expected.push_back("combined");
	BOOST_REQUIRE(this->waitToConsumeLogs(F::m_log_reader_id, logs_expected));

	// Confirm that the history cache exists and has the default log file and no others.
	std::string history_cache_filename = CONFIG_FILE_DIR + F::m_log_reader_id + ".cache";
	BOOST_REQUIRE(boost::filesystem::exists(history_cache_filename));
	std::ifstream history_cache(history_cache_filename.c_str());
	BOOST_REQUIRE(history_cache);
	std::string default_log_file = std::string("combined") + F::m_file_ext;
	BOOST_CHECK(history_cache.getline(F::m_buf, BUF_SIZE));
	BOOST_CHECK(strcmp(F::m_buf, default_log_file.c_str()) == 0);
	BOOST_CHECK(! history_cache.getline(F::m_buf, BUF_SIZE));
	history_cache.close();

	// Create a new log file for the LogInputReactor to consume.
	this->makeNewLogFileByCopying("comb-log-2");
	const int num_lines_in_new_input_log_file = 2;
	std::string new_log_file = NEW_INPUT_LOG_FNAME + F::m_file_ext;

	// Wait up to 2 seconds for the LogInputReactor to finish consuming the new log file.
	// (DEFAULT_FREQUENCY, the time to wait until checking for new log files, is 1 second.)
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id, NUM_LINES_IN_DEFAULT_LOG_FILE + num_lines_in_new_input_log_file, 2);

	// Wait until we've finished consuming the log
	logs_expected.push_back("combined-new");
	BOOST_REQUIRE(this->waitToConsumeLogs(F::m_log_reader_id, logs_expected));

	// Confirm that the history cache exists and has the expected two log files and no others.
	history_cache.clear();
	history_cache.open(history_cache_filename.c_str());
	BOOST_REQUIRE(history_cache);
	std::string s1, s2, s3;
	history_cache >> s1 >> s2 >> s3;
	BOOST_CHECK(((s1 == default_log_file) && (s2 == new_log_file)) || ((s1 == new_log_file) && (s2 == default_log_file)));
	BOOST_CHECK(s3.empty());
	history_cache.close();

	// Delete the new log file.
	boost::filesystem::remove(LOG_FILE_DIR + NEW_INPUT_LOG_FNAME + F::m_file_ext);

/*
 * this makes tests too slow
 *
	// Wait 1.5 seconds, to give the LogInputReactor a chance to update the history cache.
	// (DEFAULT_FREQUENCY, the time to wait until checking for new log files, is 1 second.)
	PionScheduler::sleep(0, 1500000000);

	// Confirm that the history cache exists and has the default log file and no others.
	history_cache.clear();
	history_cache.open(history_cache_filename.c_str());
	BOOST_REQUIRE(history_cache);
	BOOST_CHECK(history_cache.getline(F::m_buf, BUF_SIZE));
	BOOST_CHECK(strcmp(F::m_buf, default_log_file.c_str()) == 0);
	BOOST_CHECK(! history_cache.getline(F::m_buf, BUF_SIZE));
	history_cache.close();
*/
}

BOOST_AUTO_TEST_SUITE_END()


/// fixture for testing LogInputReactor with compressed log files
template<const char* codec_id, const char* file_ext, const char* compression_ext>
class LogInputReactorInterfaceForCompressedLogFiles_F : public RunningReactionEngineWithLogInputReactor_F<codec_id, file_ext> {
public:
	LogInputReactorInterfaceForCompressedLogFiles_F() {}
	virtual ~LogInputReactorInterfaceForCompressedLogFiles_F() {}

	std::string compressedFileRegex(const std::string& fname, bool make_uppercase) {
		std::string file_ext_2 = compression_ext;
		if (make_uppercase)
			std::transform(file_ext_2.begin(), file_ext_2.end(), file_ext_2.begin(), (int (*)(int))std::toupper);
		return fname + "\\" + file_ext + "\\" + file_ext_2; // e.g. .*\.xml\.gz
	}
};

extern const char gzip_file_ext[]  = ".gz";
extern const char bzip2_file_ext[] = ".bz2";

typedef boost::mpl::list<LogInputReactorInterfaceForCompressedLogFiles_F<LogCodec_id,  LogCodec_file_ext,  gzip_file_ext>,
						 LogInputReactorInterfaceForCompressedLogFiles_F<LogCodec_id,  LogCodec_file_ext,  bzip2_file_ext>,
#ifdef PION_HAVE_JSON
						 LogInputReactorInterfaceForCompressedLogFiles_F<JSONCodec_id, JSONCodec_file_ext, gzip_file_ext>,
						 LogInputReactorInterfaceForCompressedLogFiles_F<JSONCodec_id, JSONCodec_file_ext, bzip2_file_ext>,
#endif
						 LogInputReactorInterfaceForCompressedLogFiles_F<XMLCodec_id,  XMLCodec_file_ext,  gzip_file_ext>,
						 LogInputReactorInterfaceForCompressedLogFiles_F<XMLCodec_id,  XMLCodec_file_ext,  bzip2_file_ext>
> compressed_log_fixture_list;

BOOST_AUTO_TEST_SUITE_FIXTURE_TEMPLATE(LogInputReactorInterfaceForCompressedLogFiles_S, compressed_log_fixture_list)

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(testReadingCompressedLogs) {
	// Reconfigure the LogInputReactor to search for compressed default log file, e.g. combined.json.GZ, if file_ext == ".json".
	xmlNodePtr config_ptr = this->makeLogInputReactorConfig("", "", "../compressed-logs", this->compressedFileRegex("combined", true));
	F::m_reaction_engine->setReactorConfig(F::m_log_reader_id, config_ptr);

	// Start the LogInputReactor.
	F::m_reaction_engine->startReactor(F::m_log_reader_id);

	// Wait up to one second for the LogInputReactor to finish consuming the log file.
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id, NUM_LINES_IN_DEFAULT_LOG_FILE);

	// Confirm that the LogInputReactor has the expected number of input events and output events.
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(F::m_log_reader_id),  NUM_LINES_IN_DEFAULT_LOG_FILE);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(F::m_log_reader_id), NUM_LINES_IN_DEFAULT_LOG_FILE);

	// Confirm that the new log file has some expected values in it.
	F::m_reaction_engine->stopReactor(F::m_log_writer_id);
	std::ifstream log_stream(NEW_OUTPUT_LOG_FILE.c_str());
	BOOST_REQUIRE(log_stream.is_open());
	for (unsigned int i = 0; i < NUM_LINES_IN_DEFAULT_LOG_FILE; ++i) {
		log_stream.getline(F::m_buf, BUF_SIZE);
		BOOST_CHECK(boost::regex_search(F::m_buf, boost::regex(expected_urls[i].c_str())));
	}
	BOOST_CHECK(! log_stream.getline(F::m_buf, BUF_SIZE));
}

/*
BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkPartiallyConsumedCompressedFileResumedAfterEngineReloaded) {
	// Reconfigure the LogInputReactor to search for compressed large log file, e.g. large.json.gz, if file_ext == ".json".
	xmlNodePtr config_ptr = this->makeLogInputReactorConfig("", "", "../compressed-logs", this->compressedFileRegex("large", false));
	F::m_reaction_engine->setReactorConfig(F::m_log_reader_id, config_ptr);

	// Start the LogInputReactor.
	F::m_reaction_engine->startReactor(F::m_log_reader_id);

	// Run until at least a few events have been read.
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id, 10UL);

	// Stop the LogInputReactor and save the numbers of input events and output events.
	F::m_reaction_engine->stopReactor(F::m_log_reader_id);
	std::uint64_t events_in_before_delete = F::m_reaction_engine->getEventsIn(F::m_log_reader_id);
	std::uint64_t events_out_before_delete = F::m_reaction_engine->getEventsOut(F::m_log_reader_id);

	// Delete the ReactionEngine.  (This is needed to enable creating a new LogInputReactor with the same ID, 
	// because IDs can only be specified in a configuration file, configuration files can only be read if 
	// they're not already open, and they can only be closed by ConfigManager::~ConfigManager().)
	delete F::m_reaction_engine;

	// Create a new ReactionEngine, using the config file created in the constructor.
	F::m_reaction_engine = new ReactionEngine(F::m_vocab_mgr, F::m_codec_factory, F::m_protocol_factory, F::m_database_mgr);
	F::m_reaction_engine->setConfigFile(F::m_reactor_config_file);
	F::m_reaction_engine->setNumThreads(1);
	F::m_reaction_engine->openConfigFile();

	// Start the LogInputReactor.  (The ReactionEngine and LogOutputReactor were started by openConfigFile().)
	F::m_reaction_engine->startReactor(F::m_log_reader_id);

	// Compute the expected numbers of input events and output events (which should be
	// the number of lines in the log file that haven't yet been read).
	std::uint64_t expected_events_in  = NUM_LINES_IN_LARGE_LOG_FILE - events_in_before_delete;
	std::uint64_t expected_events_out = NUM_LINES_IN_LARGE_LOG_FILE - events_out_before_delete;

	// Wait up to 20 seconds for the expected number of input events.
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id, expected_events_in, 20);

	// Confirm that the Reactor has the expected number of input events and output events.
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(F::m_log_reader_id),  expected_events_in);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(F::m_log_reader_id), expected_events_out);
}
*/

BOOST_AUTO_TEST_SUITE_END()


/*
// The following tests are commented out for now, because they sometimes fail,
// simply because the log file is consumed too quickly.


/// fixture for testing file position caching with multiple LogInputReactors
template<const char* codec_id, const char* file_ext>
class TwoRunningLogInputReactorsReadingLargeFile_F : public RunningReactionEngineWithLogInputReactor_F<codec_id, file_ext> {
public:
	TwoRunningLogInputReactorsReadingLargeFile_F() {
		this->setupForLargeLogFile();

		// Make another LogInputReactor with the same configuration as the first, so it will also read the large log file.
		// (Note that this Reactor will have no output connection, since it's not actually needed for the test,
		// but one could be added for debugging purposes if desired.)
		xmlNodePtr config_ptr_2 = this->m_reaction_engine->getPluginConfig(this->m_log_reader_id)->children;
		this->m_log_reader_id_2 = this->m_reaction_engine->addReactor(config_ptr_2);

		// Start the LogInputReactors.
		this->m_reaction_engine->startReactor(this->m_log_reader_id);
		this->m_reaction_engine->startReactor(this->m_log_reader_id_2);

		// Calibrate.
		this->m_num_nsec = 10000000; // 0.01 seconds (Start small, since we need to run as many as 15 intervals before either Reactor finishes the file.)
		PionScheduler::sleep(0, this->m_num_nsec);
		while (this->m_reaction_engine->getEventsIn(this->m_log_reader_id) < NUM_LINES_IN_LARGE_LOG_FILE / 100) { // i.e. less than 1% read
			this->m_num_nsec *= 2;
			if (this->m_num_nsec >= 1000000000) { // 1 second
				BOOST_FAIL("LogInputReactor was taking too long to start reading a log file.");
			}
			PionScheduler::sleep(0, this->m_num_nsec);
		}
		this->m_num_nsec /= 2;

		// The second LogInputReactor should have read something by now.
		BOOST_CHECK(this->m_reaction_engine->getEventsIn(this->m_log_reader_id_2) != 0);
	}
	virtual ~TwoRunningLogInputReactorsReadingLargeFile_F() {
	}

	std::uint32_t m_num_nsec;
	std::string m_log_reader_id_2;
};

typedef boost::mpl::list<TwoRunningLogInputReactorsReadingLargeFile_F<LogCodec_id, LogCodec_file_ext>,
						 TwoRunningLogInputReactorsReadingLargeFile_F<JSONCodec_id, JSONCodec_file_ext>,
						 TwoRunningLogInputReactorsReadingLargeFile_F<XMLCodec_id, XMLCodec_file_ext> > codec_fixture_list_2;

BOOST_AUTO_TEST_SUITE_FIXTURE_TEMPLATE(TwoRunningLogInputReactorsReadingLargeFile_S, codec_fixture_list_2)

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkPartiallyConsumedFileResumedAfterRestartingReactors) {
	// Stop and restart both LogInputReactors 3 times.
	for (unsigned int i = 0; i < 3; ++i) {
		PionScheduler::sleep(0, F::m_num_nsec); // both Reactors running
		F::m_reaction_engine->stopReactor(F::m_log_reader_id);
		BOOST_CHECK(F::m_reaction_engine->getEventsIn(F::m_log_reader_id) != NUM_LINES_IN_LARGE_LOG_FILE);
		PionScheduler::sleep(0, F::m_num_nsec); // second Reactor only running
		F::m_reaction_engine->stopReactor(F::m_log_reader_id_2);
		BOOST_CHECK(F::m_reaction_engine->getEventsIn(F::m_log_reader_id_2) != NUM_LINES_IN_LARGE_LOG_FILE);
		F::m_reaction_engine->startReactor(F::m_log_reader_id);
		PionScheduler::sleep(0, F::m_num_nsec); // first Reactor only running
		F::m_reaction_engine->startReactor(F::m_log_reader_id_2);
	}

	// Wait up to 20 seconds for the LogInputReactors to finish consuming the log file.
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id, NUM_LINES_IN_LARGE_LOG_FILE, 20);
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id_2, NUM_LINES_IN_LARGE_LOG_FILE, 20);

	// Confirm that both LogInputReactors have the expected number of input events and output events.
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(F::m_log_reader_id),  NUM_LINES_IN_LARGE_LOG_FILE);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(F::m_log_reader_id), NUM_LINES_IN_LARGE_LOG_FILE);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(F::m_log_reader_id_2),  NUM_LINES_IN_LARGE_LOG_FILE);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(F::m_log_reader_id_2), NUM_LINES_IN_LARGE_LOG_FILE);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkPartiallyConsumedFileResumedAfterRestartingEngine) {
	// Stop and restart the ReactorEngine 10 times.
	for (unsigned int i = 0; i < 10; ++i) {
		PionScheduler::sleep(0, F::m_num_nsec);

		// If the LogInputReactor is not stopped first, bad assertion exceptions are often thrown in LogOutputReactor::operator().
		F::m_reaction_engine->stopReactor(F::m_log_reader_id);

		F::m_reaction_engine->stop();
		BOOST_CHECK(F::m_reaction_engine->getEventsIn(F::m_log_reader_id) != NUM_LINES_IN_LARGE_LOG_FILE);
		BOOST_CHECK(F::m_reaction_engine->getEventsIn(F::m_log_reader_id_2) != NUM_LINES_IN_LARGE_LOG_FILE);
		F::m_reaction_engine->start();
		F::m_reaction_engine->startReactor(F::m_log_writer_id);
		F::m_reaction_engine->startReactor(F::m_log_reader_id);
		F::m_reaction_engine->startReactor(F::m_log_reader_id_2);
	}

	// Wait up to 20 seconds for the LogInputReactors to finish consuming the log file.
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id, NUM_LINES_IN_LARGE_LOG_FILE, 20);
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id_2, NUM_LINES_IN_LARGE_LOG_FILE, 20);

	// Confirm that both LogInputReactors have the expected number of input events and output events.
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(F::m_log_reader_id),  NUM_LINES_IN_LARGE_LOG_FILE);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(F::m_log_reader_id), NUM_LINES_IN_LARGE_LOG_FILE);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(F::m_log_reader_id_2),  NUM_LINES_IN_LARGE_LOG_FILE);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(F::m_log_reader_id_2), NUM_LINES_IN_LARGE_LOG_FILE);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkPartiallyConsumedFileResumedAfterEngineReloaded) {
	// Stop the LogInputReactors and save the numbers of input events and output events.
	F::m_reaction_engine->stopReactor(F::m_log_reader_id);
	F::m_reaction_engine->stopReactor(F::m_log_reader_id_2);
	std::uint64_t events_in_before_delete_1  = F::m_reaction_engine->getEventsIn(F::m_log_reader_id);
	std::uint64_t events_out_before_delete_1 = F::m_reaction_engine->getEventsOut(F::m_log_reader_id);
	std::uint64_t events_in_before_delete_2  = F::m_reaction_engine->getEventsIn(F::m_log_reader_id_2);
	std::uint64_t events_out_before_delete_2 = F::m_reaction_engine->getEventsOut(F::m_log_reader_id_2);

	// Delete the ReactionEngine.  (This is needed to enable creating new LogInputReactors with the same IDs, 
	// because IDs can only be specified in a configuration file, configuration files can only be read if 
	// they're not already open, and they can only be closed by ConfigManager::~ConfigManager().)
	delete F::m_reaction_engine;

	// Create a new ReactionEngine, using the config file created in the constructor.
	F::m_reaction_engine = new ReactionEngine(F::m_vocab_mgr, F::m_codec_factory, F::m_protocol_factory, F::m_database_mgr);
	F::m_reaction_engine->setConfigFile(F::m_reactor_config_file);
	F::m_reaction_engine->setNumThreads(1);
	F::m_reaction_engine->openConfigFile();

	// Start the LogInputReactors.  (The ReactionEngine and LogOutputReactor were started by openConfigFile().)
	F::m_reaction_engine->startReactor(F::m_log_reader_id);
	F::m_reaction_engine->startReactor(F::m_log_reader_id_2);

	// Compute the expected numbers of input events and output events (which should be
	// the number of lines in the log file that haven't yet been read).
	std::uint64_t expected_events_in_1  = NUM_LINES_IN_LARGE_LOG_FILE - events_in_before_delete_1;
	std::uint64_t expected_events_out_1 = NUM_LINES_IN_LARGE_LOG_FILE - events_out_before_delete_1;
	std::uint64_t expected_events_in_2  = NUM_LINES_IN_LARGE_LOG_FILE - events_in_before_delete_2;
	std::uint64_t expected_events_out_2 = NUM_LINES_IN_LARGE_LOG_FILE - events_out_before_delete_2;

	// Wait up to 20 seconds for the expected number of input events.
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id, expected_events_in_1, 20);
	PionPlatformUnitTest::checkReactorEventsIn(*F::m_reaction_engine, F::m_log_reader_id_2, expected_events_in_2, 20);

	// Confirm that both LogInputReactors have the expected number of input events and output events.
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(F::m_log_reader_id),  expected_events_in_1);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(F::m_log_reader_id), expected_events_out_1);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsIn(F::m_log_reader_id_2),  expected_events_in_2);
	BOOST_CHECK_EQUAL(F::m_reaction_engine->getEventsOut(F::m_log_reader_id_2), expected_events_out_2);
}

BOOST_AUTO_TEST_SUITE_END()

*/


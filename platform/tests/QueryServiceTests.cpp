// ------------------------------------------------------------------
// pion-net: a C++ framework for building lightweight HTTP interfaces
// ------------------------------------------------------------------
// Copyright (C) 2007-2011 Atomic Labs, Inc.  (http://www.atomiclabs.com)
//
// Distributed under the Boost Software License, Version 1.0.
// See http://www.boost.org/LICENSE_1_0.txt
//

#include <string>
#include <fstream>
#include <boost/test/unit_test.hpp>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/find.hpp>
#include <pion/PionConfig.hpp>
#include <pion/PionPlugin.hpp>
#include <pion/PionScheduler.hpp>
#include <pion/PionUnitTestDefs.hpp>
#include <pion/platform/PionPlatformUnitTest.hpp>
#include <pion/net/HTTPRequest.hpp>
#include <pion/net/HTTPResponse.hpp>
#include <pion/net/WebService.hpp>
#include <pion/net/TCPStream.hpp>
#include "../server/PlatformConfig.hpp"

using namespace pion;
using namespace pion::net;

#if defined(PION_XCODE)
	static const std::string PATH_TO_PLUGINS(".");
#else
	// same for Unix and Windows
	static const std::string PATH_TO_PLUGINS("../services/.libs");
#endif

static const std::string NEW_OUTPUT_LOG_FILE(LOG_FILE_DIR + "new.log");
static const std::uint64_t ONE_SECOND = 1000000000; // in nsec
static const std::uint64_t NUM_LINES_IN_DEFAULT_LOG_FILE = 4;
static const std::uint64_t NUM_LINES_IN_COMB_LOG_2 = 2;
static const std::string TIMESTAMPED_EXPECTED_A_FILE(LOG_FILE_DIR + "timestamped_expected_a.log");
static const std::string LOG_EXPECTED_A_FILE(LOG_FILE_DIR + "log_expected_a.log");


BOOST_AUTO_TEST_CASE(checkOpenQueryService) {
	PionPlugin::addPluginDirectory(PATH_TO_PLUGINS);
	PionPluginPtr<WebService> ppp;
	BOOST_CHECK_NO_THROW(ppp.open("QueryService"));
}


struct PluginPtrWithQueryServiceLoaded_F : public PionPluginPtr<pion::server::PlatformService> {
	PluginPtrWithQueryServiceLoaded_F() { 
		PionPlugin::addPluginDirectory(PATH_TO_PLUGINS);
		s = NULL;
		open("QueryService");
	}
	~PluginPtrWithQueryServiceLoaded_F() {
		if (s) destroy(s);
	}

	pion::server::PlatformService* s;
};

BOOST_FIXTURE_TEST_SUITE(PluginPtrWithQueryServiceLoaded_S, PluginPtrWithQueryServiceLoaded_F)

BOOST_AUTO_TEST_CASE(checkIsOpenReturnsTrue) {
	BOOST_CHECK(is_open());
}

BOOST_AUTO_TEST_CASE(checkGetPluginNameReturnsPluginName) {
	BOOST_CHECK_EQUAL(getPluginName(), "QueryService");
}

BOOST_AUTO_TEST_CASE(checkCreateReturnsSomething) {
	BOOST_CHECK((s = create()) != NULL);
}

BOOST_AUTO_TEST_CASE(checkDestroyDoesntThrowExceptionAfterCreate) {
	s = create();
	BOOST_CHECK_NO_THROW(destroy(s));
	s = NULL;
}

BOOST_AUTO_TEST_CASE(checkSetOptionThrowsExpectedException) {
	s = create();
	BOOST_CHECK_THROW(s->setOption("notAnOption", "some value"), WebService::UnknownOptionException);
}

BOOST_AUTO_TEST_SUITE_END()


class PlatformReadyForQueryServiceRequests_F {
public:
	PlatformReadyForQueryServiceRequests_F() {
		PionPlatformUnitTest::cleanup_platform_config_files();
		PionPlatformUnitTest::cleanup_cache_files();
		PionPlatformUnitTest::cleanup_log_files();
		BOOST_CHECK(timestampedLogFiles().size() == 0);
		boost::filesystem::remove(get_log_file_dir() + "combined-2.log");

		m_platform_cfg.setConfigFile(PLATFORM_CONFIG_FILE);
		m_platform_cfg.openConfigFile();
	}
	~PlatformReadyForQueryServiceRequests_F() {
	}
	std::vector<std::string> timestampedLogFiles(void) {
		std::vector<std::string> timestamped_log_files;
		boost::filesystem::path dir_path(LOG_FILE_DIR);
		for (boost::filesystem::directory_iterator itr(dir_path); itr != boost::filesystem::directory_iterator(); ++itr) {
			std::string basename = boost::filesystem::basename(itr->path());
			if (basename.substr(0, 6) == "new-20") {
# if defined(BOOST_FILESYSTEM_VERSION) && BOOST_FILESYSTEM_VERSION >= 3
                timestamped_log_files.push_back(itr->path().string());
#else
                timestamped_log_files.push_back(itr->path().file_string());
#endif 
			}
		}
		return timestamped_log_files;
	}
	void confirmDefaultQueryBehavior(const std::string& resource, int expected_events_out) {
		// Connect a stream to localhost.
		TCPStream tcp_stream(m_platform_cfg.getServiceManager().getIOService());
		boost::system::error_code ec = tcp_stream.connect(boost::asio::ip::address::from_string("127.0.0.1"),
			m_platform_cfg.getServiceManager().getPort());
		BOOST_REQUIRE(! ec);

		HTTPRequest http_request;
		http_request.setResource(resource);
		http_request.send(tcp_stream.rdbuf()->getConnection(), ec);
		BOOST_REQUIRE(! ec);

		// Receive the response from the server.
		HTTPResponse http_response(http_request);
		http_response.receive(tcp_stream.rdbuf()->getConnection(), ec);
		BOOST_REQUIRE(! ec);

		// Check that the query response had the expected value for EventsOut.
		std::string response_content(http_response.getContent());
		std::ostringstream expected_substring;
		expected_substring << "<EventsOut>" << expected_events_out << "</EventsOut>";
		BOOST_CHECK(response_content.find(expected_substring.str()) != std::string::npos);

		// Check that the log file was not timestamped.
		std::vector<std::string> timestamped_log_files = timestampedLogFiles();
		BOOST_CHECK(timestamped_log_files.size() == 0);
	}
	HTTPResponsePtr sendRequestAndGetResponse(const std::string& resource, const std::string& request_method = "") {
		// Connect a stream to localhost.
		TCPStream tcp_stream(m_platform_cfg.getServiceManager().getIOService());
		m_ec = tcp_stream.connect(boost::asio::ip::address::from_string("127.0.0.1"),
			m_platform_cfg.getServiceManager().getPort());
		BOOST_REQUIRE(! m_ec);
		
		// Send the request to the server.
		HTTPRequest http_request;
		http_request.setResource(resource);
		if (! request_method.empty())
			http_request.setMethod(request_method);
		http_request.send(tcp_stream.rdbuf()->getConnection(), m_ec);
		BOOST_REQUIRE(! m_ec);
		
		// Get the response from the server.
		HTTPResponsePtr response_ptr(new HTTPResponse(http_request));
		response_ptr->receive(tcp_stream.rdbuf()->getConnection(), m_ec);
		BOOST_REQUIRE(! m_ec);
		
		return response_ptr;
	}
	std::uint64_t extractNumEventsInFromResponse(const HTTPResponsePtr& response_ptr) {
		std::string response_content(response_ptr->getContent());
		boost::smatch rx_matches;
		BOOST_REQUIRE(boost::regex_match(response_content, rx_matches, boost::regex(".*<EventsIn>(\\d+)</EventsIn>.*")));
		return boost::lexical_cast<std::uint64_t>(rx_matches[1]);
	}

	pion::server::PlatformConfig		m_platform_cfg;
	boost::system::error_code			m_ec;
};

BOOST_FIXTURE_TEST_SUITE(PlatformReadyForQueryServiceRequests_S, PlatformReadyForQueryServiceRequests_F)

BOOST_AUTO_TEST_CASE(testMinimalQueryToLogInputReactor) {
	// Send minimal query request to a LogInputReactor.
	std::string reactor_id = "c7a9f95a-e305-11dc-98ce-0016cb926e68";
	HTTPResponsePtr response_ptr = sendRequestAndGetResponse("/query/reactors/" + reactor_id);

	// Check the response.
	std::string response_content(response_ptr->getContent());
	std::string expected_response = 
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	"<PionStats xmlns=\"http://purl.org/pion/config\">\n"
		"<Reactor id=\"" + reactor_id + "\">\n"
			"<Running>false</Running>\n"
			"<EventsIn>0</EventsIn>\n"
			"<EventsOut>0</EventsOut>\n"
			"<CurrentLog></CurrentLog>\n"
		"</Reactor>\n"
	"</PionStats>\n";
	BOOST_CHECK_EQUAL(response_content, expected_response);
}

BOOST_AUTO_TEST_CASE(testMinimalQueryToDatabaseOutputReactor) {
	// Send minimal query request to a DatabaseOutputReactor.
	std::string reactor_id = "a8928460-eb0c-11dc-9b68-0019e3f89cd2";
	HTTPResponsePtr response_ptr = sendRequestAndGetResponse("/query/reactors/" + reactor_id);

	// Check the response.
	std::string response_content(response_ptr->getContent());
	std::string expected_response = 
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	"<PionStats xmlns=\"http://purl.org/pion/config\">\n"
		"<Reactor id=\"" + reactor_id + "\">\n"
			"<Running>true</Running>\n"
			"<EventsIn>0</EventsIn>\n"
			"<EventsOut>0</EventsOut>\n"
			"<Inserter>\n"
			"<EventsQueued>0</EventsQueued>\n"
			"<KeyCacheSize>0</KeyCacheSize>\n"
			"</Inserter>\n"
			"<Database>e75d88f0-e7df-11dc-a76c-0016cb926e68</Database>\n"
			"<Table>clickstream</Table>\n"
		"</Reactor>\n"
	"</PionStats>\n";
	BOOST_CHECK_EQUAL(response_content, expected_response);
}

BOOST_AUTO_TEST_CASE(testMinimalQueryToLogOutputReactor) {
	// Send minimal query request to LogOutputReactor.
	std::string reactor_id = "a92b7278-e306-11dc-85f0-0016cb926e68";
	HTTPResponsePtr response_ptr = sendRequestAndGetResponse("/query/reactors/" + reactor_id);

	// Check the response.
	std::string response_content(response_ptr->getContent());
	std::string expected_response = 
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	"<PionStats xmlns=\"http://purl.org/pion/config\">\n"
		"<Reactor id=\"" + reactor_id + "\">\n"
			"<Running>true</Running>\n"
			"<EventsIn>0</EventsIn>\n"
			"<EventsOut>0</EventsOut>\n"
		"</Reactor>\n"
	"</PionStats>\n";
	BOOST_CHECK_EQUAL(response_content, expected_response);
}

BOOST_AUTO_TEST_CASE(testMinimalQueryToReactorWithoutQueryOverride) {
	// Send minimal query request to FilterReactor.  (FilterReactor::query() is not defined.)
	std::string reactor_id = "0cc21558-cf84-11dc-a9e0-0019e3f89cd2";
	HTTPResponsePtr response_ptr = sendRequestAndGetResponse("/query/reactors/" + reactor_id);

	// Check the response.
	std::string response_content(response_ptr->getContent());
	std::string expected_response = 
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	"<PionStats xmlns=\"http://purl.org/pion/config\">\n"
		"<Reactor id=\"" + reactor_id + "\">\n"
			"<Running>true</Running>\n"
			"<EventsIn>0</EventsIn>\n"
			"<EventsOut>0</EventsOut>\n"
		"</Reactor>\n"
	"</PionStats>\n";
	BOOST_CHECK_EQUAL(response_content, expected_response);
}

BOOST_AUTO_TEST_CASE(testQueryUsingPostMethod) {
	// Send POST query request to a LogInputReactor.
	std::string reactor_id = "c7a9f95a-e305-11dc-98ce-0016cb926e68";
	HTTPResponsePtr response_ptr = sendRequestAndGetResponse("/query/reactors/" + reactor_id, "POST");

	// Check the response.
	BOOST_CHECK_EQUAL(response_ptr->getStatusCode(), HTTPTypes::RESPONSE_CODE_METHOD_NOT_ALLOWED);
	BOOST_CHECK_EQUAL(response_ptr->getHeader("Allow"), "GET");
}

BOOST_AUTO_TEST_CASE(testQueryUsingPutMethod) {
	// Send PUT query request to a LogInputReactor.
	std::string reactor_id = "c7a9f95a-e305-11dc-98ce-0016cb926e68";
	HTTPResponsePtr response_ptr = sendRequestAndGetResponse("/query/reactors/" + reactor_id, "PUT");

	// Check the response.
	BOOST_CHECK_EQUAL(response_ptr->getStatusCode(), HTTPTypes::RESPONSE_CODE_METHOD_NOT_ALLOWED);
	BOOST_CHECK_EQUAL(response_ptr->getHeader("Allow"), "GET");
}

BOOST_AUTO_TEST_CASE(testQueryUsingDeleteMethod) {
	// Send DELETE query request to a LogInputReactor.
	std::string reactor_id = "c7a9f95a-e305-11dc-98ce-0016cb926e68";
	HTTPResponsePtr response_ptr = sendRequestAndGetResponse("/query/reactors/" + reactor_id, "DELETE");

	// Check the response.
	BOOST_CHECK_EQUAL(response_ptr->getStatusCode(), HTTPTypes::RESPONSE_CODE_METHOD_NOT_ALLOWED);
	BOOST_CHECK_EQUAL(response_ptr->getHeader("Allow"), "GET");
}

BOOST_AUTO_TEST_CASE(testQueryUsingHeadMethod) {
	// Send HEAD query request to a LogInputReactor.
	std::string reactor_id = "c7a9f95a-e305-11dc-98ce-0016cb926e68";
	HTTPResponsePtr response_ptr = sendRequestAndGetResponse("/query/reactors/" + reactor_id, "HEAD");

	// Check the response.
	BOOST_CHECK_EQUAL(response_ptr->getStatusCode(), HTTPTypes::RESPONSE_CODE_METHOD_NOT_ALLOWED);
	BOOST_CHECK_EQUAL(response_ptr->getHeader("Allow"), "GET");
}

BOOST_AUTO_TEST_CASE(testSendRotateQueryToLogOutputReactorWithNoInput) {
	// Send request to the LogOutputReactor to rotate its output file.
	std::string reactor_id = "a92b7278-e306-11dc-85f0-0016cb926e68";
	HTTPResponsePtr response_ptr = sendRequestAndGetResponse("/query/reactors/" + reactor_id + "/rotate");

	// Check the response.  We expect the response to be the same as for testMinimalQueryToDatabaseOutputReactor,
	// even though this time the query has an additional branch requesting log file rotation.
	std::string response_content(response_ptr->getContent());
	std::string expected_response = 
		"<Reactor id=\"" + reactor_id + "\">\n"
			"<Running>true</Running>\n"
			"<EventsIn>0</EventsIn>\n"
			"<EventsOut>0</EventsOut>\n"
		"</Reactor>\n";
	BOOST_CHECK_EQUAL(response_content, expected_response);

	// Check that the log file is empty (since no data is flowing).
	BOOST_CHECK_EQUAL(boost::filesystem::file_size(NEW_OUTPUT_LOG_FILE), 0UL);

	// Check that the log file was not timestamped (since it is empty).
	std::vector<std::string> timestamped_log_files = timestampedLogFiles();
	BOOST_CHECK(timestamped_log_files.size() == 0);
}

BOOST_AUTO_TEST_CASE(testSendRotateQueryToLogOutputReactorWithSomeInput) {
	// Start a LogInputReactor, which will send its output to the already running LogOutputReactor.
	std::string log_input_reactor_id = "c7a9f95a-e305-11dc-98ce-0016cb926e68";
	std::string log_output_reactor_id = "a92b7278-e306-11dc-85f0-0016cb926e68";
	m_platform_cfg.getReactionEngine().startReactor(log_input_reactor_id);

	PionPlatformUnitTest::checkReactorEventsIn(m_platform_cfg.getReactionEngine(), log_output_reactor_id, NUM_LINES_IN_DEFAULT_LOG_FILE, 5);

	// Send request to the LogOutputReactor to rotate its output file.
	HTTPResponsePtr response_ptr = sendRequestAndGetResponse("/query/reactors/" + log_output_reactor_id + "/rotate");

	// Extract from the query response the number of Events the LogOutputReactor had received at the time of rotation.
	std::uint64_t num_events_in_at_rotation = extractNumEventsInFromResponse(response_ptr);
	BOOST_CHECK_EQUAL(num_events_in_at_rotation, NUM_LINES_IN_DEFAULT_LOG_FILE);
	
	// Check that the log file was timestamped.
	std::vector<std::string> timestamped_log_files = timestampedLogFiles();
	BOOST_CHECK(timestamped_log_files.size() == 1);

	// Check that the timestamped log file has the expected contents.
	BOOST_CHECK(PionUnitTest::check_files_match(timestamped_log_files[0], TIMESTAMPED_EXPECTED_A_FILE));

	// Check that a new log file was created and is empty.
	BOOST_CHECK(boost::filesystem::exists(NEW_OUTPUT_LOG_FILE));
	BOOST_CHECK_EQUAL(boost::filesystem::file_size(NEW_OUTPUT_LOG_FILE), 0UL);

	// Create another log file for the LogInputReactor to find, and give it time to find and process it.
	boost::filesystem::copy_file(get_log_file_dir() + "comb-log-2.log", get_log_file_dir() + "combined-2.log");
	int expected_total_events_in = NUM_LINES_IN_DEFAULT_LOG_FILE + NUM_LINES_IN_COMB_LOG_2;
	PionPlatformUnitTest::checkReactorEventsIn(m_platform_cfg.getReactionEngine(), log_output_reactor_id, expected_total_events_in, 5);

	// Stop the LogOutputReactor and check if the new log file that was just created has the expected contents.
	m_platform_cfg.getReactionEngine().stopReactor(log_output_reactor_id);
	BOOST_CHECK(PionUnitTest::check_files_match(NEW_OUTPUT_LOG_FILE, LOG_EXPECTED_A_FILE));
}

BOOST_AUTO_TEST_CASE(testRotateQueryForCurrentlyGrowingLogFile) {
	// Add a "perpetual" LogInputReactor, i.e. with JustOne set to true.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Name>Perpetual CLF Log Reader</Name>"
		"<Plugin>LogInputReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Codec>3f49f2da-bfe3-11dc-8875-0016cb926e68</Codec>"
		"<Directory>../logs</Directory>"
		"<Filename>combined\\.log</Filename>"
		"<JustOne>true</JustOne>");
	std::string log_input_reactor_id = m_platform_cfg.getReactionEngine().addReactor(config_ptr);

	// Connect the new LogInputReactor to a LogOutputReactor and start it.  (The latter should already be running.)
	std::string log_output_reactor_id = "a92b7278-e306-11dc-85f0-0016cb926e68";
	m_platform_cfg.getReactionEngine().addReactorConnection(log_input_reactor_id, log_output_reactor_id);
	m_platform_cfg.getReactionEngine().startReactor(log_input_reactor_id);

	// Wait for a few Events to be processed.
	PionPlatformUnitTest::checkReactorEventsIn(m_platform_cfg.getReactionEngine(), log_output_reactor_id, 10UL, 5);

	// Send request to the LogOutputReactor to rotate its output file.
	HTTPResponsePtr response_ptr = sendRequestAndGetResponse("/query/reactors/" + log_output_reactor_id + "/rotate");

	// Extract from the query response the number of Events the LogOutputReactor had received at the time of rotation.
	std::uint64_t num_events_in_at_rotation = extractNumEventsInFromResponse(response_ptr);
	BOOST_CHECK_GE(num_events_in_at_rotation, 10UL);

	// Check that the log file was timestamped.
	std::vector<std::string> timestamped_log_files = timestampedLogFiles();
	BOOST_CHECK(timestamped_log_files.size() == 1);

	// Check that a new log file was created.
	BOOST_CHECK(boost::filesystem::exists(NEW_OUTPUT_LOG_FILE));

	// Wait for a few more Events to be processed and stop the LogInputReactor.
	PionPlatformUnitTest::checkReactorEventsIn(m_platform_cfg.getReactionEngine(), log_output_reactor_id, num_events_in_at_rotation + 10UL, 5);
	m_platform_cfg.getReactionEngine().stopReactor(log_input_reactor_id);
	std::uint64_t num_events_from_log_input_reactor = m_platform_cfg.getReactionEngine().getEventsOut(log_input_reactor_id);

	// Wait until the LogOutputReactor has processed all Events sent by the LogInputReactor and then stop it.
	PionPlatformUnitTest::checkReactorEventsIn(m_platform_cfg.getReactionEngine(), log_output_reactor_id, num_events_from_log_input_reactor, 5);
	m_platform_cfg.getReactionEngine().stopReactor(log_output_reactor_id);

	// Count the number of lines in the timestamped file.
	std::uint64_t num_events_in_timestamped_log_file = -4; // don't count header lines
	std::ifstream log_file_1(timestamped_log_files[0].c_str());
	BOOST_REQUIRE(log_file_1.is_open());
	const unsigned int BUF_SIZE = 1023;
	char buf[BUF_SIZE + 1];
	while (log_file_1.getline(buf, BUF_SIZE)) {
		num_events_in_timestamped_log_file++;
	}
	log_file_1.close();

	// Count the number of lines in the current log file.
	std::uint64_t num_events_in_most_current_log_file = -4; // don't count header lines
	std::ifstream log_file_2(NEW_OUTPUT_LOG_FILE.c_str());
	BOOST_REQUIRE(log_file_2.is_open());
	while (log_file_2.getline(buf, BUF_SIZE)) {
		num_events_in_most_current_log_file++;
	}
	log_file_2.close();

	// Check that each log file has at least 10 Events, since at least 10 Events were received both before and after the query.
	BOOST_CHECK_GE(num_events_in_timestamped_log_file, 10UL);
	BOOST_CHECK_GE(num_events_in_most_current_log_file, 10UL);

	// Check that the total number of lines in the two log files is the same as the total number of Events sent.
	BOOST_CHECK_EQUAL(num_events_in_timestamped_log_file + num_events_in_most_current_log_file, num_events_from_log_input_reactor);
}

#ifdef PION_HAVE_YAJL
BOOST_AUTO_TEST_CASE(testRotateQueryWithJsonCodec) {
	// Add a LogOutputReactor with an JSONCodec.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Name>JSON Log Writer</Name>"
		"<Plugin>LogOutputReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Codec>9446b74a-71e4-426c-b965-ae55260375af</Codec>"
		"<Filename>../logs/new.json</Filename>");
	std::string xml_log_output_reactor_id = m_platform_cfg.getReactionEngine().addReactor(config_ptr);

	// Connect a LogInputReactor to the new LogOutputReactor and start it.  (The latter should already be running.)
	std::string log_input_reactor_id = "c7a9f95a-e305-11dc-98ce-0016cb926e68";
	m_platform_cfg.getReactionEngine().addReactorConnection(log_input_reactor_id, xml_log_output_reactor_id);
	m_platform_cfg.getReactionEngine().startReactor(log_input_reactor_id);

	PionPlatformUnitTest::checkReactorEventsIn(m_platform_cfg.getReactionEngine(), xml_log_output_reactor_id, NUM_LINES_IN_DEFAULT_LOG_FILE, 5);

	// Send request to the LogOutputReactor to rotate its output file.
	HTTPResponsePtr response_ptr = sendRequestAndGetResponse("/query/reactors/" + xml_log_output_reactor_id + "/rotate");

	// Extract from the query response the number of Events the LogOutputReactor had received at the time of rotation.
	std::uint64_t num_events_in_at_rotation = extractNumEventsInFromResponse(response_ptr);
	BOOST_CHECK_EQUAL(num_events_in_at_rotation, NUM_LINES_IN_DEFAULT_LOG_FILE);
	
	// Check that the log file was timestamped.
	std::vector<std::string> timestamped_log_files = timestampedLogFiles();
	BOOST_CHECK(timestamped_log_files.size() == 1);

	// Check that the timestamped log file has the expected size.
	BOOST_CHECK_EQUAL(boost::filesystem::file_size(timestamped_log_files[0]), 452UL);

	// Check that a new log file was created and is empty.
	std::string log_output_file = get_log_file_dir() + "new.json";
	BOOST_CHECK(boost::filesystem::exists(log_output_file));
	BOOST_CHECK_EQUAL(boost::filesystem::file_size(log_output_file), 0UL);

	// Create another log file for the LogInputReactor to find, and give it time to find and process it.
	boost::filesystem::copy_file(get_log_file_dir() + "comb-log-2.log", get_log_file_dir() + "combined-2.log");
	int expected_total_events_in = NUM_LINES_IN_DEFAULT_LOG_FILE + NUM_LINES_IN_COMB_LOG_2;
	PionPlatformUnitTest::checkReactorEventsIn(m_platform_cfg.getReactionEngine(), xml_log_output_reactor_id, expected_total_events_in, 5);

	// Stop the LogOutputReactor and check if the new log file that was just created has the expected size.
	m_platform_cfg.getReactionEngine().stopReactor(xml_log_output_reactor_id);
	BOOST_CHECK_EQUAL(boost::filesystem::file_size(log_output_file), 228UL);
}
#endif

BOOST_AUTO_TEST_CASE(testRotateQueryWithXmlCodec) {
	// Add a LogOutputReactor with an XMLCodec.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Name>XML Log Writer</Name>"
		"<Plugin>LogOutputReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Codec>f7bb0fd8-3fe0-4227-accb-aaba2440a638</Codec>"
		"<Filename>../logs/new.xml</Filename>");
	std::string xml_log_output_reactor_id = m_platform_cfg.getReactionEngine().addReactor(config_ptr);

	// Connect a LogInputReactor to the new LogOutputReactor and start it.  (The latter should already be running.)
	std::string log_input_reactor_id = "c7a9f95a-e305-11dc-98ce-0016cb926e68";
	m_platform_cfg.getReactionEngine().addReactorConnection(log_input_reactor_id, xml_log_output_reactor_id);
	m_platform_cfg.getReactionEngine().startReactor(log_input_reactor_id);

	PionPlatformUnitTest::checkReactorEventsIn(m_platform_cfg.getReactionEngine(), xml_log_output_reactor_id, NUM_LINES_IN_DEFAULT_LOG_FILE, 5);

	// Send request to the LogOutputReactor to rotate its output file.
	HTTPResponsePtr response_ptr = sendRequestAndGetResponse("/query/reactors/" + xml_log_output_reactor_id + "/rotate");

	// Extract from the query response the number of Events the LogOutputReactor had received at the time of rotation.
	std::uint64_t num_events_in_at_rotation = extractNumEventsInFromResponse(response_ptr);
	BOOST_CHECK_EQUAL(num_events_in_at_rotation, NUM_LINES_IN_DEFAULT_LOG_FILE);
	
	// Check that the log file was timestamped.
	std::vector<std::string> timestamped_log_files = timestampedLogFiles();
	BOOST_CHECK(timestamped_log_files.size() == 1);

	// Check that the timestamped log file has the expected size.
	BOOST_CHECK_EQUAL(boost::filesystem::file_size(timestamped_log_files[0]), 619UL);

	// Check that a new log file was created and is empty.
	std::string log_output_file = get_log_file_dir() + "new.xml";
	BOOST_CHECK(boost::filesystem::exists(log_output_file));
	BOOST_CHECK_EQUAL(boost::filesystem::file_size(log_output_file), 0UL);

	// Create another log file for the LogInputReactor to find, and give it time to find and process it.
	boost::filesystem::copy_file(get_log_file_dir() + "comb-log-2.log", get_log_file_dir() + "combined-2.log");
	int expected_total_events_in = NUM_LINES_IN_DEFAULT_LOG_FILE + NUM_LINES_IN_COMB_LOG_2;
	PionPlatformUnitTest::checkReactorEventsIn(m_platform_cfg.getReactionEngine(), xml_log_output_reactor_id, expected_total_events_in, 5);

	// Stop the LogOutputReactor and check if the new log file that was just created has the expected size.
	m_platform_cfg.getReactionEngine().stopReactor(xml_log_output_reactor_id);
	BOOST_CHECK_EQUAL(boost::filesystem::file_size(log_output_file), 339UL);
}

BOOST_AUTO_TEST_CASE(testRotateQueryForUnchangedLogFile) {
	// Start the LogInputReactor, which will send its output to the already running LogOutputReactor.
	std::string log_input_reactor_id = "c7a9f95a-e305-11dc-98ce-0016cb926e68";
	std::string log_output_reactor_id = "a92b7278-e306-11dc-85f0-0016cb926e68";
	m_platform_cfg.getReactionEngine().startReactor(log_input_reactor_id);

	PionPlatformUnitTest::checkReactorEventsIn(m_platform_cfg.getReactionEngine(), log_output_reactor_id, NUM_LINES_IN_DEFAULT_LOG_FILE, 5);

	// Send request to the LogOutputReactor to rotate its output file.
	HTTPResponsePtr response_ptr = sendRequestAndGetResponse("/query/reactors/" + log_output_reactor_id + "/rotate");

	// Extract from the query response the number of Events the LogOutputReactor had received at the time of rotation.
	std::uint64_t num_events_in_at_rotation = extractNumEventsInFromResponse(response_ptr);
	BOOST_CHECK_EQUAL(num_events_in_at_rotation, NUM_LINES_IN_DEFAULT_LOG_FILE);
	
	// Check that the log file was timestamped.
	std::vector<std::string> timestamped_log_files = timestampedLogFiles();
	BOOST_CHECK(timestamped_log_files.size() == 1);

	// Check that the timestamped log file has the expected contents.
	BOOST_CHECK(PionUnitTest::check_files_match(timestamped_log_files[0], TIMESTAMPED_EXPECTED_A_FILE));

	// Check that a new log file was created and is empty.
	BOOST_CHECK(boost::filesystem::exists(NEW_OUTPUT_LOG_FILE));
	BOOST_CHECK_EQUAL(boost::filesystem::file_size(NEW_OUTPUT_LOG_FILE), 0UL);

	// Wait briefly and send another request to the LogOutputReactor to rotate its output file.
	PionScheduler::sleep(0, ONE_SECOND); // Enough to avoid a timestamp conflict.
	HTTPResponsePtr response_ptr_2 = sendRequestAndGetResponse("/query/reactors/" + log_output_reactor_id + "/rotate");

	// Check that the log file is empty (since no more Events were available since the previous query).
	BOOST_CHECK_EQUAL(boost::filesystem::file_size(NEW_OUTPUT_LOG_FILE), 0UL);

	// Check that no additional timestamped file exists (and the old one is still there).
	std::vector<std::string> timestamped_log_files_2 = timestampedLogFiles();
	BOOST_CHECK(timestamped_log_files == timestamped_log_files_2);

	// Check that the response to the second query was the same as to the first query.
	BOOST_CHECK_EQUAL(response_ptr->getContent(), response_ptr_2->getContent());
}

BOOST_AUTO_TEST_CASE(testRapidFireRotateQueries) {
	// Add a "perpetual" LogInputReactor, i.e. with JustOne set to true.
	xmlNodePtr config_ptr = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Name>Perpetual CLF Log Reader</Name>"
		"<Plugin>LogInputReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Codec>3f49f2da-bfe3-11dc-8875-0016cb926e68</Codec>"
		"<Directory>../logs</Directory>"
		"<Filename>combined\\.log</Filename>"
		"<JustOne>true</JustOne>");
	std::string log_input_reactor_id = m_platform_cfg.getReactionEngine().addReactor(config_ptr);

	// Connect the new LogInputReactor to a LogOutputReactor and start it.  (The latter should already be running.)
	std::string log_output_reactor_id = "a92b7278-e306-11dc-85f0-0016cb926e68";
	m_platform_cfg.getReactionEngine().addReactorConnection(log_input_reactor_id, log_output_reactor_id);
	m_platform_cfg.getReactionEngine().startReactor(log_input_reactor_id);

	// Send several consecutive rotate requests to try to force a timestamp conflict.
	HTTPResponsePtr response_ptr = sendRequestAndGetResponse("/query/reactors/" + log_output_reactor_id + "/rotate");
	std::uint64_t num_events_in_at_rotation = extractNumEventsInFromResponse(response_ptr);
	bool expected_error_logged = false;
	for (int i = 0; i < 10; ++i) {
		// Wait for at least one more Event, so that the LogOutputReactor will attempt to rotate the log file.
		PionPlatformUnitTest::checkReactorEventsIn(m_platform_cfg.getReactionEngine(), log_output_reactor_id, num_events_in_at_rotation + 1UL, 5);

		response_ptr = sendRequestAndGetResponse("/query/reactors/" + log_output_reactor_id + "/rotate");
		std::string response_content(response_ptr->getContent());
		if (response_content.find("Unable to rotate log file:") != std::string::npos) {
			expected_error_logged = true;
			break;
		}

		// There was no conflict reported, so update the number of Events and try again.
		num_events_in_at_rotation = extractNumEventsInFromResponse(response_ptr);
	}

	// This is just a warning, because the test can't guarantee that a conflict will occur.
	BOOST_WARN(expected_error_logged);
}

BOOST_AUTO_TEST_CASE(testNonRotateQueryToLogOutputReactor) {
	// Start the LogInputReactor, which will send its output to the already running LogOutputReactor.
	std::string log_input_reactor_id = "c7a9f95a-e305-11dc-98ce-0016cb926e68";
	std::string log_output_reactor_id = "a92b7278-e306-11dc-85f0-0016cb926e68";
	m_platform_cfg.getReactionEngine().startReactor(log_input_reactor_id);

	PionPlatformUnitTest::checkReactorEventsIn(m_platform_cfg.getReactionEngine(), log_output_reactor_id, NUM_LINES_IN_DEFAULT_LOG_FILE, 5);

	confirmDefaultQueryBehavior("/query/reactors/" + log_output_reactor_id, NUM_LINES_IN_DEFAULT_LOG_FILE);

	confirmDefaultQueryBehavior("/query/reactors/" + log_output_reactor_id + "/junk", NUM_LINES_IN_DEFAULT_LOG_FILE);
}

BOOST_AUTO_TEST_SUITE_END()

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

#include <sstream>
#include <fstream>
#include <thread>
#include <boost/test/unit_test.hpp>
#include <boost/filesystem/operations.hpp>
#include <pion/PionConfig.hpp>
#include <pion/PionScheduler.hpp>
#include <pion/PionUnitTestDefs.hpp>
#include <pion/platform/PionPlatformUnitTest.hpp>
#include <pion/net/TCPStream.hpp>
#include <pion/net/HTTPRequest.hpp>
#include "../server/PlatformConfig.hpp"

using namespace pion;
using namespace pion::net;
using namespace pion::platform;
using namespace pion::server;


/// static strings used by these unit tests
static const std::string COMMON_LOG_FILE(LOG_FILE_DIR + "common.log");


/// interface class for FeedService tests
class FeedServiceTestInterface_F {
public:
	FeedServiceTestInterface_F()
		: m_common_id("a174c3b0-bfcd-11dc-9db2-0016cb926e68"),
		m_combined_id("3f49f2da-bfe3-11dc-8875-0016cb926e68"),
		m_ie_filter_id("153f6c40-cb78-11dc-8fa0-0019e3f89cd2"),
		m_do_nothing_id("0cc21558-cf84-11dc-a9e0-0019e3f89cd2")
	{
		PionPlatformUnitTest::cleanup_platform_config_files();

		// start the ServiceManager, etc.
		m_platform_cfg.setConfigFile(PLATFORM_CONFIG_FILE);
		m_platform_cfg.openConfigFile();
	}
	
	virtual ~FeedServiceTestInterface_F() {}
	
	void getConnectionInfo(unsigned int& num_reactors, unsigned int& num_input,
						   unsigned int& num_output)
	{
		const boost::regex reactor_conn_regex("<Type>reactor</Type>");
		const boost::regex input_conn_regex("<Type>input</Type>");
		const boost::regex output_conn_regex("<Type>output</Type>");
		const unsigned int BUF_SIZE = 1023;
		char buf[BUF_SIZE+1];
		
		// initialize values to zero
		num_reactors = num_input = num_output = 0;

		// count the number of each type of connection
		std::stringstream ss;
		m_platform_cfg.getReactionEngine().writeConnectionsXML(ss);
		while (ss.getline(buf, BUF_SIZE)) {
			if (boost::regex_search(buf, reactor_conn_regex))
				++num_reactors;
			if (boost::regex_search(buf, input_conn_regex))
				++num_input;
			if (boost::regex_search(buf, output_conn_regex))
				++num_output;
		}
	}
	
	void checkConnectionInfo(unsigned int expected_reactors, unsigned int expected_input,
							 unsigned int expected_output)
	{
		unsigned int num_reactors, num_input, num_output;
		for (int i = 0; i < 10; ++i) {
			getConnectionInfo(num_reactors, num_input, num_output);
			if (num_reactors == expected_reactors && num_input == expected_input
				&& num_output == expected_output)
			{
				break;
			}
			PionScheduler::sleep(0, 100000000); // 0.1 seconds
		}
		BOOST_CHECK_EQUAL(num_reactors, expected_reactors);
		BOOST_CHECK_EQUAL(num_input, expected_input);
		BOOST_CHECK_EQUAL(num_output, expected_output);
	}
	
	PlatformConfig		m_platform_cfg;
	const std::string	m_common_id;
	const std::string	m_combined_id;
	const std::string	m_ie_filter_id;
	const std::string	m_do_nothing_id;
};

BOOST_FIXTURE_TEST_SUITE(FeedServiceTestInterface_S, FeedServiceTestInterface_F)

BOOST_AUTO_TEST_CASE(checkFeedServiceReactorConnections) {
	unsigned int num_reactors, num_input, num_output;
	
	// initially, there should be only two connections of type "reactor"
	getConnectionInfo(num_reactors, num_input, num_output);
	BOOST_CHECK_EQUAL(num_reactors, static_cast<unsigned int>(8));
	BOOST_CHECK_EQUAL(num_input, static_cast<unsigned int>(0));
	BOOST_CHECK_EQUAL(num_output, static_cast<unsigned int>(0));
	
	// connect a stream to localhost
	TCPStream output_tcp_stream(m_platform_cfg.getServiceManager().getIOService());
	boost::system::error_code ec;
	ec = output_tcp_stream.connect(boost::asio::ip::address::from_string("127.0.0.1"),
		m_platform_cfg.getServiceManager().getPort());
	BOOST_REQUIRE(! ec);

	// request an output feed from the "do nothing" reactor
	HTTPRequest feed_request;
	feed_request.setResource("/feed/" + m_do_nothing_id + "/" + m_common_id);
	feed_request.send(output_tcp_stream.rdbuf()->getConnection(), ec);
	BOOST_REQUIRE(! ec);
	
	// re-check the connections recognized
	checkConnectionInfo(8, 0, 1);
	
	// connect a stream to localhost
	TCPStream input_tcp_stream(m_platform_cfg.getServiceManager().getIOService());
	ec = input_tcp_stream.connect(boost::asio::ip::address::from_string("127.0.0.1"),
		m_platform_cfg.getServiceManager().getPort());
	BOOST_REQUIRE(! ec);
	
	// establish an input feed to the "do nothing" reactor
	feed_request.setMethod(HTTPTypes::REQUEST_METHOD_POST);
	feed_request.send(input_tcp_stream.rdbuf()->getConnection(), ec);
	BOOST_REQUIRE(! ec);
	
	// re-check the connections recognized
	checkConnectionInfo(8, 1, 1);

	// send data from the common log file to the input connection
	std::ifstream common_log;
	common_log.open(COMMON_LOG_FILE.c_str(), std::ios::in);
	BOOST_REQUIRE(common_log.is_open());
	input_tcp_stream << common_log.rdbuf();
	input_tcp_stream.flush();
	
	// disconnect the input stream
	input_tcp_stream.close();
	
	// re-check the connections recognized
	checkConnectionInfo(8, 0, 1);
	
	// read data in from the output stream
	bool found_it = false;
	const unsigned int BUF_SIZE = 1023;
	char buf[BUF_SIZE+1];
	while (output_tcp_stream.getline(buf, BUF_SIZE)) {
		// look for the last Event
		std::string str(buf);
		if (str.find("10.0.141.122") != std::string::npos) {
			found_it = true;
			break;
		}
	}
	BOOST_CHECK(found_it);
}

BOOST_AUTO_TEST_SUITE_END()


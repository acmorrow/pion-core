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

#include <iostream>
#include <string>
#include <libxml/xmlerror.h>
#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <pion/PionConfig.hpp>
#include <pion/PionPlugin.hpp>

#ifdef PION_HAVE_SSL
	#include <openssl/ssl.h>
//#ifdef _MSC_VER
//	#include <openssl/applink.c>
//#endif
#endif

#define BOOST_TEST_MODULE pion-platform-unit-tests
#include <boost/test/unit_test.hpp>

#include <pion/PionUnitTestDefs.hpp>
#include <pion/platform/PionPlatformUnitTest.hpp>

struct PionPlatformUnitTestsConfig {
	PionPlatformUnitTestsConfig() {
		std::cout << "global setup specific to pion-platform\n";

		// argc and argv do not include parameters handled by the boost unit test framework, such as --log_level.
		int argc = boost::unit_test::framework::master_test_suite().argc;
		char** argv = boost::unit_test::framework::master_test_suite().argv;

		bool verbose = false;
		if (argc > 1) {
			if (argv[1][0] == '-' && argv[1][1] == 'v') {
				verbose = true;
			}
		}

		pion::PionLogger config_log_ptr = PION_GET_LOGGER("config");
		if (verbose) {
			PION_LOG_SETLEVEL_INFO(config_log_ptr);
		} else {
			xmlSetGenericErrorFunc(NULL, PionUnitTest::doNothing);
			std::cout << "Use '-v' to enable logging from libxml2.\n";
			PION_LOG_SETLEVEL_ERROR(config_log_ptr);
		}

		pion::PionPlugin::resetPluginDirectories();
#if defined(PION_XCODE)
		pion::PionPlugin::addPluginDirectory("../../bin/Debug");
#else
		// same for Unix and Windows (including Cygwin)
		pion::PionPlugin::addPluginDirectory("../codecs/.libs");
		pion::PionPlugin::addPluginDirectory("../reactors/.libs");
		pion::PionPlugin::addPluginDirectory("../databases/.libs");
		pion::PionPlugin::addPluginDirectory("../services/.libs");
		pion::PionPlugin::addPluginDirectory("../protocols/.libs");
		pion::PionPlugin::addPluginDirectory("../../net/services/.libs");
#endif

#ifdef PION_HAVE_SSL
		CRYPTO_malloc_init();
		SSL_library_init();	// TODO: asio should handle this, why not?!?!
#endif
	}
	~PionPlatformUnitTestsConfig() {
		std::cout << "global teardown specific to pion-platform\n";
	}
};

BOOST_GLOBAL_FIXTURE(PionUnitTestsConfig);
BOOST_GLOBAL_FIXTURE(PionPlatformUnitTestsConfig);


/// returns the path to the unit test log file directory
const std::string& get_log_file_dir(void)
{
#if defined(PION_XCODE)
	static const std::string TESTS_LOG_FILE_DIR("../../platform/tests/logs/");
#else
	static const std::string TESTS_LOG_FILE_DIR("logs/");
#endif

	return TESTS_LOG_FILE_DIR;
}

/// returns the path to the directory of compressed log files for unit tests
const std::string& get_compressed_log_file_dir(void)
{
#if defined(PION_XCODE)
	static const std::string TESTS_COMPR_LOG_FILE_DIR("../../platform/tests/compressed-logs/");
#else
	static const std::string TESTS_COMPR_LOG_FILE_DIR("compressed-logs/");
#endif

	return TESTS_COMPR_LOG_FILE_DIR;
}

/// returns the path to the unit test config file directory
const std::string& get_config_file_dir(void)
{
#if defined(PION_XCODE)
	static const std::string TESTS_CONFIG_FILE_DIR("../../platform/tests/config/");
#else
	static const std::string TESTS_CONFIG_FILE_DIR("config/");
#endif

	return TESTS_CONFIG_FILE_DIR;
}

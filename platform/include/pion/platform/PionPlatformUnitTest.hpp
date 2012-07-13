// ------------------------------------------------------------------------
// Pion is a development platform for building Reactors that process Events
// ------------------------------------------------------------------------
// Copyright (C) 2009-2011 Atomic Labs, Inc.  (http://www.atomiclabs.com)
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

#ifndef __PION_PIONPLATFORMUNITTEST_HEADER__
#define __PION_PIONPLATFORMUNITTEST_HEADER__

#include <fstream>
#include <functional>
#include <libxml/tree.h>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <pion/PionScheduler.hpp>
#include <pion/PionLogger.hpp>
#include <pion/platform/Event.hpp>
#include <pion/platform/Codec.hpp>
#include <pion/platform/ReactionEngine.hpp>
#include <pion/platform/ConfigManager.hpp>
#include <pion/net/TCPStream.hpp>
#include <pion/net/HTTPRequest.hpp>
#include <pion/net/HTTPResponse.hpp>
#include "../server/PlatformConfig.hpp"

/// returns the path to the unit test config file directory
const std::string& get_config_file_dir(void);

/// returns the path to the unit test log file directory
const std::string& get_log_file_dir(void);

const std::string CONFIG_FILE_DIR(get_config_file_dir());
const std::string LOG_FILE_DIR(get_log_file_dir());

const std::string VOCABS_CONFIG_FILE(CONFIG_FILE_DIR + "vocabularies.xml");
const std::string VOCABS_TEMPLATE_FILE(CONFIG_FILE_DIR + "vocabularies.tmpl");
const std::string CODECS_CONFIG_FILE(CONFIG_FILE_DIR + "codecs.xml");
#ifdef PION_HAVE_JSON
const std::string CODECS_TEMPLATE_FILE(CONFIG_FILE_DIR + "codecs.tmpl");
#else
const std::string CODECS_TEMPLATE_FILE(CONFIG_FILE_DIR + "codecs-no-json.tmpl");
#endif
const std::string DATABASES_CONFIG_FILE(CONFIG_FILE_DIR + "databases.xml");
const std::string DATABASES_TEMPLATE_FILE(CONFIG_FILE_DIR + "databases.tmpl");
const std::string REACTORS_CONFIG_FILE(CONFIG_FILE_DIR + "reactors.xml");
const std::string REACTORS_TEMPLATE_FILE(CONFIG_FILE_DIR + "reactors.tmpl");
const std::string PROTOCOLS_CONFIG_FILE(CONFIG_FILE_DIR + "protocols.xml");
const std::string PROTOCOLS_TEMPLATE_FILE(CONFIG_FILE_DIR + "protocols.tmpl");
const std::string USERS_CONFIG_FILE(CONFIG_FILE_DIR + "users.xml");
const std::string USERS_TEMPLATE_FILE(CONFIG_FILE_DIR + "users.tmpl");
const std::string SERVICES_CONFIG_FILE(CONFIG_FILE_DIR + "services.xml");
const std::string SERVICES_TEMPLATE_FILE(CONFIG_FILE_DIR + "services.tmpl");
const std::string PLATFORM_CONFIG_FILE(CONFIG_FILE_DIR + "platform.xml");
const std::string PLATFORM_TEMPLATE_FILE(CONFIG_FILE_DIR + "platform.tmpl");
const std::string DBENGINES_CONFIG_FILE(CONFIG_FILE_DIR + "dbengines.xml");
const std::string DBENGINES_TEMPLATE_FILE(CONFIG_FILE_DIR + "dbengines.tmpl");


struct PionPlatformUnitTest
{
	/// returns the path to the unit test vocabulary config path
	static const std::string& get_vocabulary_path(void)
	{
		static const std::string TESTS_VOCABULARY_PATH("config/vocabularies/");
		return TESTS_VOCABULARY_PATH;
	}

	/// cleans up vocabulary config files in the tests config directory
	static void cleanup_vocab_config_files(void)
	{
		if (boost::filesystem::exists(VOCABS_CONFIG_FILE))
			boost::filesystem::remove(VOCABS_CONFIG_FILE);
		boost::filesystem::copy_file(VOCABS_TEMPLATE_FILE, VOCABS_CONFIG_FILE);

		// Copy all *.tmpl files in the "vocabularies" subdirectory of the tests config directory to *.xml files.
		boost::filesystem::path vocab_dir_path(get_vocabulary_path());
		for (boost::filesystem::directory_iterator itr(vocab_dir_path); itr != boost::filesystem::directory_iterator(); ++itr) {
			if (boost::filesystem::extension(itr->path()) == ".tmpl") {
				boost::filesystem::path xml_config_file = boost::filesystem::change_extension(itr->path(), ".xml");
				if (boost::filesystem::exists(xml_config_file))
					boost::filesystem::remove(xml_config_file);
				boost::filesystem::copy_file(itr->path(), xml_config_file);
			}
		}
	}

	/// cleans up platform config files in the working directory
	static void cleanup_platform_config_files(void)
	{
		cleanup_vocab_config_files();
		
		if (boost::filesystem::exists(REACTORS_CONFIG_FILE))
			boost::filesystem::remove(REACTORS_CONFIG_FILE);
		boost::filesystem::copy_file(REACTORS_TEMPLATE_FILE, REACTORS_CONFIG_FILE);
		
		if (boost::filesystem::exists(CODECS_CONFIG_FILE))
			boost::filesystem::remove(CODECS_CONFIG_FILE);
		boost::filesystem::copy_file(CODECS_TEMPLATE_FILE, CODECS_CONFIG_FILE);
		
		if (boost::filesystem::exists(PROTOCOLS_CONFIG_FILE))
			boost::filesystem::remove(PROTOCOLS_CONFIG_FILE);
		boost::filesystem::copy_file(PROTOCOLS_TEMPLATE_FILE, PROTOCOLS_CONFIG_FILE);

		if (boost::filesystem::exists(DATABASES_CONFIG_FILE))
			boost::filesystem::remove(DATABASES_CONFIG_FILE);
		boost::filesystem::copy_file(DATABASES_TEMPLATE_FILE, DATABASES_CONFIG_FILE);
		
		if (boost::filesystem::exists(PLATFORM_CONFIG_FILE))
			boost::filesystem::remove(PLATFORM_CONFIG_FILE);
		boost::filesystem::copy_file(PLATFORM_TEMPLATE_FILE, PLATFORM_CONFIG_FILE);
		
		if (boost::filesystem::exists(SERVICES_CONFIG_FILE))
			boost::filesystem::remove(SERVICES_CONFIG_FILE);
		boost::filesystem::copy_file(SERVICES_TEMPLATE_FILE, SERVICES_CONFIG_FILE);

		if (boost::filesystem::exists(USERS_CONFIG_FILE))
			boost::filesystem::remove(USERS_CONFIG_FILE);
		boost::filesystem::copy_file(USERS_TEMPLATE_FILE, USERS_CONFIG_FILE);

		if (boost::filesystem::exists(DBENGINES_CONFIG_FILE))
			boost::filesystem::remove(DBENGINES_CONFIG_FILE);
		boost::filesystem::copy_file(DBENGINES_TEMPLATE_FILE, DBENGINES_CONFIG_FILE);
	}

	static void cleanup_cache_files(void)
	{
		boost::filesystem::path dir_path(CONFIG_FILE_DIR);
		for (boost::filesystem::directory_iterator itr(dir_path); itr != boost::filesystem::directory_iterator(); ++itr) {
			if (boost::filesystem::extension(itr->path()) == ".cache") {
				boost::filesystem::remove(itr->path());
			}
		}
	}

	static void cleanup_backup_files(void)
	{
		boost::filesystem::path dir_path(CONFIG_FILE_DIR);
		for (boost::filesystem::directory_iterator itr(dir_path); itr != boost::filesystem::directory_iterator(); ++itr) {
			if (boost::filesystem::extension(itr->path()) == ".bak") {
				boost::filesystem::remove(itr->path());
			}
		}
	}

	// Deletes all files starting with "new" in the test logs directory.
	static void cleanup_log_files(void)
	{
		boost::filesystem::path dir_path(get_log_file_dir());
		for (boost::filesystem::directory_iterator itr(dir_path); itr != boost::filesystem::directory_iterator(); ++itr) {
			std::string basename = boost::filesystem::basename(itr->path());
			if (basename.substr(0, 3) == "new") {
				boost::filesystem::remove(itr->path());
			}
		}
	}

	static void checkReactorEventsIn(pion::platform::ReactionEngine& reaction_engine,
		const std::string& reactor_id, const std::uint64_t expected_value,
		const std::uint32_t wait_seconds = 1)
	{
		// wait up to one second for the number to exceed the expected value
		const int num_checks_allowed = 10 * wait_seconds;
		for (int i = 0; i < num_checks_allowed; ++i) {
			pion::PionScheduler::sleep(0, 100000000); // 0.1 seconds
			if (reaction_engine.getEventsIn(reactor_id) >= expected_value) break;
		}
		BOOST_REQUIRE_GE( reaction_engine.getEventsIn(reactor_id), expected_value );
	}

	static void checkReactorEventsOut(pion::platform::ReactionEngine& reaction_engine,
		const std::string& reactor_id, const std::uint64_t expected_value,
		const std::uint32_t wait_seconds = 1)
	{
		// wait up to one second for the number to exceed the expected value
		const int num_checks_allowed = 10 * wait_seconds;
		for (int i = 0; i < num_checks_allowed; ++i) {
			pion::PionScheduler::sleep(0, 100000000); // 0.1 seconds
			if (reaction_engine.getEventsOut(reactor_id) >= expected_value) break;
		}
		BOOST_REQUIRE_GE( reaction_engine.getEventsOut(reactor_id), expected_value );
	}
	
	static std::uint64_t feedFileToReactor(pion::platform::ReactionEngine& reaction_engine,
		const std::string& reactor_id, pion::platform::Codec& codec_ref, const std::string& log_file)
	{
		std::ifstream in(log_file.c_str(), std::ios::in);
		BOOST_REQUIRE(in.is_open());
	
		std::uint64_t events_read = 0;
		pion::platform::EventPtr event_ptr;
		pion::platform::EventFactory event_factory;

		// push events from the log file into the clickstream sessionizer reactor
		event_factory.create(event_ptr, codec_ref.getEventType());
		while (codec_ref.read(in, *event_ptr)) {
			++events_read;
			reaction_engine.send(reactor_id, event_ptr);
			event_factory.create(event_ptr, codec_ref.getEventType());
		}
		
		in.close();
	
		return events_read;
	}

	// From a string representation of a Reactor configuration, obtain an xmlNodePtr that
	// points to a list of all the child nodes needed by Reactor::setConfig().
	static xmlNodePtr makeReactorConfigFromString(const std::string& inner_config_str) {
		std::string config_str = std::string("<PionConfig><Reactor>") + inner_config_str + "</Reactor></PionConfig>";
		xmlNodePtr config_ptr = pion::platform::ConfigManager::createResourceConfig("Reactor", config_str.c_str(), config_str.size());
		BOOST_REQUIRE(config_ptr);
		return config_ptr;
	}

	static pion::net::HTTPResponsePtr sendRequestAndGetResponse(
		pion::server::PlatformConfig& platform_cfg,
		pion::net::HTTPRequest request,
		const std::string& request_method = "GET")
	{
		// Connect a stream to localhost.
		pion::net::TCPStream tcp_stream(platform_cfg.getServiceManager().getIOService());
		boost::system::error_code ec = tcp_stream.connect(boost::asio::ip::address::from_string("127.0.0.1"),
														  platform_cfg.getServiceManager().getPort());
		BOOST_REQUIRE(! ec);

		// Send the request to the server.
		request.setMethod(request_method);
		request.send(tcp_stream.rdbuf()->getConnection(), ec);
		BOOST_REQUIRE(! ec);

		// Get the response from the server.
		pion::net::HTTPResponsePtr response_ptr(new pion::net::HTTPResponse(request));
		response_ptr->receive(tcp_stream.rdbuf()->getConnection(), ec);
		BOOST_REQUIRE(! ec);
		
		return response_ptr;
	}

#if defined(PION_USE_LOG4CPLUS)
	static void setupXmlLogService(pion::server::PlatformConfig& platform_cfg) {
		// Create a CircularBufferAppender so that XMLLogService can use it.
		pion::PionLogAppenderPtr appender(new pion::CircularBufferAppender);
		appender->setName("CircularBufferAppender");
		log4cplus::Logger::getRoot().addAppender(appender);

		// Create and load a services config file with an XMLLogService, so that we can check that regex exceptions are logged.
		boost::filesystem::ofstream services_config_file;
		services_config_file.open(SERVICES_CONFIG_FILE);
		services_config_file
			<< "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			<< "<PionConfig xmlns=\"http://purl.org/pion/config\" pion_version=\"tests\">\n"
			<< "    <Server id=\"main-server\">\n"
			<< "        <Port>0</Port>\n"
			<< "        <PlatformService id=\"xml-log-service\">\n"
			<< "            <Name>XML Log Service</Name>\n"
			<< "            <Plugin>XMLLogService</Plugin>\n"
			<< "            <Resource>/xmllog</Resource>\n"
			<< "        </PlatformService>\n"
			<< "    </Server>\n"
			<< "</PionConfig>\n";
		services_config_file.close();
		platform_cfg.getServiceManager().setConfigFile(SERVICES_CONFIG_FILE);
		platform_cfg.getServiceManager().openConfigFile();
	}

	static void getXmlLogMessages(pion::server::PlatformConfig& platform_cfg, std::vector<std::string>& messages) {
		messages.clear();
		pion::net::HTTPResponsePtr response_ptr = PionPlatformUnitTest::sendRequestAndGetResponse(platform_cfg, pion::net::HTTPRequest("/xmllog"));
		xmlDocPtr doc_ptr = xmlParseMemory(response_ptr->getContent(), response_ptr->getContentLength());
		BOOST_REQUIRE(doc_ptr);
		xmlNodePtr node_ptr = xmlDocGetRootElement(doc_ptr);
		BOOST_REQUIRE(node_ptr);
		if (node_ptr->children) {
			for (xmlNodePtr cur_node = node_ptr->children; cur_node != NULL; cur_node = cur_node->next) {
				xmlNodePtr msg_node = pion::platform::ConfigManager::findConfigNodeByName("Message", cur_node->children);
				xmlChar* xml_char_ptr = xmlNodeGetContent(msg_node);
				BOOST_REQUIRE(xml_char_ptr);
				messages.push_back(reinterpret_cast<char*>(xml_char_ptr));
			}
		}
	}
#endif
};

#endif

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

#include <functional>
#include <pion/PionConfig.hpp>
#include <pion/PionPlugin.hpp>
#include <pion/PionUnitTestDefs.hpp>
#include <pion/platform/PionPlatformUnitTest.hpp>
#include <pion/platform/PluginConfig.hpp>
#include <pion/platform/CodecFactory.hpp>
#include <pion/platform/ProtocolFactory.hpp>
#include <pion/platform/DatabaseManager.hpp>
#include <pion/platform/ReactionEngine.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/mpl/list.hpp>
#include <boost/cast.hpp>
#include <libxml/tree.h>
#include <fstream>
#include <string>

using namespace pion;
using namespace pion::platform;


VocabularyManager g_vocab_mgr;
CodecFactory g_codec_factory(g_vocab_mgr);
ProtocolFactory g_protocol_factory(g_vocab_mgr);
DatabaseManager g_database_mgr(g_vocab_mgr);

class PluginConfigFixture {
public:
	PluginConfigFixture(const std::string& concrete_plugin_class) {
		PionPlatformUnitTest::cleanup_platform_config_files();
		m_concrete_plugin_class = concrete_plugin_class;
		BOOST_REQUIRE_NO_THROW(m_config_ptr = ConfigManager::createPluginConfig(m_concrete_plugin_class));
	}
	virtual ~PluginConfigFixture(void) {
		xmlFreeNodeList(m_config_ptr);
	}

	// Should contain the configuration for one plugin of the appropriate type, with no dependencies on any plugins of other types.
	virtual void setupSimpleConfigFile(const std::string& config_file_path) = 0;

	xmlNodePtr m_config_ptr;
	std::string m_concrete_plugin_class;
};

class DCCodecFactory : public CodecFactory, public PluginConfigFixture {
public:
	DCCodecFactory(void) : CodecFactory(g_vocab_mgr), PluginConfigFixture("LogCodec") {
		g_vocab_mgr.setConfigFile(VOCABS_CONFIG_FILE);
		g_vocab_mgr.openConfigFile();

		xmlNodePtr event_type_node = xmlNewNode(NULL, reinterpret_cast<const xmlChar*>("EventType"));
		xmlNodeSetContent(event_type_node,  reinterpret_cast<const xmlChar*>("urn:vocab:clickstream#http-event"));
		xmlAddNextSibling(m_config_ptr, event_type_node);
	}

	void setupSimpleConfigFile(const std::string& config_file_path) {
		std::ofstream out(config_file_path.c_str());
		out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			<< "<PionConfig xmlns=\"http://purl.org/pion/config\" pion_version=\"tests\">\n"
			<< "\t<Codec id=\"dba9eac2-d8bb-11dc-bebe-001cc02bd66b\">\n"
			<< "\t\t<Name>Just the clf-date</Name>\n"
			<< "\t\t<Comment>Codec for just reading clf-dates</Comment>\n"
			<< "\t\t<Plugin>LogCodec</Plugin>\n"
			<< "\t\t<EventType>urn:vocab:clickstream#http-event</EventType>\n"
			<< "\t\t<Flush>true</Flush>\n"
			<< "\t\t<Field term=\"urn:vocab:clickstream#clf-date\" start=\"&quot;\" end=\"&quot;\">clf-date</Field>\n"
			<< "\t</Codec>\n"
			<< "</PionConfig>\n";
	};

	typedef Codec plugin_type;
};

class DCDatabaseManager : public DatabaseManager, public PluginConfigFixture {
public:
	DCDatabaseManager(void) : DatabaseManager(g_vocab_mgr), PluginConfigFixture("SQLiteDatabase") {
		xmlNodePtr node = xmlNewNode(NULL, reinterpret_cast<const xmlChar*>("Filename"));
		xmlNodeSetContent(node, reinterpret_cast<const xmlChar*>("some_file"));
		xmlAddNextSibling(m_config_ptr, node);
	}

	void setupSimpleConfigFile(const std::string& config_file_path) {
		std::ofstream out(config_file_path.c_str());
		out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			<< "<PionConfig xmlns=\"http://purl.org/pion/config\" pion_version=\"tests\">\n"
			<< "\t<Database id=\"e75d88f0-e7df-11dc-a76c-0016cb926e68\">\n"
			<< "\t\t<Name>Embedded Storage Database</Name>\n"
			<< "\t\t<Comment>Embedded SQLite database for storing events</Comment>\n"
			<< "\t\t<Plugin>SQLiteDatabase</Plugin>\n"
			<< "\t\t<Filename>../logs/clickstream.db</Filename>\n"
			<< "\t</Database>\n"
			<< "</PionConfig>\n";
	};

	typedef Database plugin_type;
};

class DCReactionEngine : public ReactionEngine, public PluginConfigFixture {
public:
	DCReactionEngine(void) : ReactionEngine(g_vocab_mgr, g_codec_factory, g_protocol_factory, g_database_mgr),
							 PluginConfigFixture("FilterReactor") {
		xmlNodePtr node = xmlNewNode(NULL, reinterpret_cast<const xmlChar*>("Workspace"));
		xmlNodeSetContent(node, reinterpret_cast<const xmlChar*>("1"));
		xmlAddNextSibling(m_config_ptr, node);
	}

	void setupSimpleConfigFile(const std::string& config_file_path) {
		std::ofstream out(config_file_path.c_str());
		out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			<< "<PionConfig xmlns=\"http://purl.org/pion/config\" pion_version=\"tests\">\n"
			<< "\t<Reactor id=\"0cc21558-cf84-11dc-a9e0-0019e3f89cd2\">\n"
			<< "\t\t<Name>Do Nothing</Name>\n"
			<< "\t\t<Comment>Filter that does nothing</Comment>\n"
			<< "\t\t<Plugin>FilterReactor</Plugin>\n"
			<< "\t\t<Workspace>1</Workspace>\n"
			<< "\t</Reactor>\n"
			<< "</PionConfig>\n";
	};

	typedef Reactor plugin_type;
};

template <typename DefaultConstructablePluginConfig>
class PluginConfig_F : public DefaultConstructablePluginConfig {
public:
	PluginConfig_F() {
		m_config_file_path = CONFIG_FILE_DIR + this->m_config_file;
	}
	~PluginConfig_F() {
	}

	typedef DefaultConstructablePluginConfig DCPluginConfig;
	std::string m_config_file_path;
};

typedef boost::mpl::list<PluginConfig_F<DCCodecFactory>, PluginConfig_F<DCDatabaseManager>, PluginConfig_F<DCReactionEngine> > PluginConfig_fixture_list;

BOOST_AUTO_TEST_SUITE_FIXTURE_TEMPLATE(PluginConfig_S, PluginConfig_fixture_list)

// configIsOpen is inherited from ConfigManager
// ConfigManagerTests tests the static methods of ConfigManager, but PluginConfigTests is a good 
// place to test a lot of the non-static methods, since ConfigManager has a protected constructor.
BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkConfigIsOpen) {
	BOOST_CHECK(!F::configIsOpen());
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkConfigFileAccessors) {
	BOOST_CHECK_NO_THROW(F::setConfigFile("abc.xml"));
	BOOST_CHECK_EQUAL(F::getConfigFile(), "abc.xml");
	BOOST_CHECK_NO_THROW(F::setConfigFile("xyz.xml"));
	BOOST_CHECK_EQUAL(F::getConfigFile(), "xyz.xml");
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkCreateConfigFile) {
	boost::filesystem::remove(F::m_config_file_path);
	BOOST_CHECK(!boost::filesystem::exists(F::m_config_file_path));
	BOOST_CHECK_NO_THROW(F::setConfigFile(F::m_config_file_path));
	BOOST_CHECK_NO_THROW(F::createConfigFile());
	BOOST_CHECK(boost::filesystem::exists(F::m_config_file_path));
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkOpenConfigFile) {
	this->setupSimpleConfigFile(F::m_config_file_path);
	BOOST_CHECK_NO_THROW(F::setConfigFile(F::m_config_file_path));
	BOOST_CHECK_NO_THROW(F::openConfigFile());
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkSetPluginConfig) {
	xmlNodePtr comment_node = xmlNewNode(NULL, reinterpret_cast<const xmlChar*>("Comment"));
	xmlNodeSetContent(comment_node,  reinterpret_cast<const xmlChar*>("A new comment"));

	BOOST_CHECK_THROW(F::setPluginConfig("some_id", comment_node), ConfigManager::ConfigNotOpenException);
	xmlFreeNodeList(comment_node);
}

// TODO: write some tests that check that updateVocabulary() actually does something
BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkUpdateVocabulary) {
	BOOST_CHECK_NO_THROW(F::updateVocabulary());
}

BOOST_AUTO_TEST_SUITE_END()


static int g_num_times_mock_called = 0;

void MockCallback(void) {
	g_num_times_mock_called++;
}


template <typename DefaultConstructablePluginConfig>
class OpenPluginConfig_F : public PluginConfig_F<DefaultConstructablePluginConfig> {
public:
	OpenPluginConfig_F() {
		this->m_plugin = NULL;
		this->setupSimpleConfigFile(this->m_config_file_path);
		BOOST_CHECK_NO_THROW(this->setConfigFile(this->m_config_file_path));
		BOOST_CHECK_NO_THROW(PluginConfig_F<DefaultConstructablePluginConfig>::openConfigFile());
	}
	~OpenPluginConfig_F() {
	}

	typename DefaultConstructablePluginConfig::plugin_type* m_plugin;
};


typedef boost::mpl::list<OpenPluginConfig_F<DCCodecFactory>, OpenPluginConfig_F<DCDatabaseManager>, OpenPluginConfig_F<DCReactionEngine> > OpenPluginConfig_fixture_list;

BOOST_AUTO_TEST_SUITE_FIXTURE_TEMPLATE(OpenPluginConfig_S, OpenPluginConfig_fixture_list)

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkConfigIsOpen) {
	BOOST_CHECK(F::configIsOpen());
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkOpenConfigFileAgain) {
	BOOST_CHECK_NO_THROW(F::openConfigFile());
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkAddPlugin) {
	std::string id;
	BOOST_CHECK_NO_THROW(id = F::addPlugin(F::m_config_ptr));
	BOOST_CHECK(id != "");
	BOOST_CHECK(F::hasPlugin(id));
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkRemovePlugin) {
	BOOST_CHECK_THROW(F::removePlugin("123"), typename PluginManager<typename F::plugin_type>::PluginNotFoundException);

	std::string id = F::addPlugin(F::m_config_ptr);
	BOOST_CHECK_NO_THROW(F::removePlugin(id));
	BOOST_CHECK(!F::hasPlugin(id));

	BOOST_CHECK_THROW(F::removePlugin(id), typename PluginManager<typename F::plugin_type>::PluginNotFoundException);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkHasPlugin) {
	BOOST_CHECK(!F::hasPlugin("bogus_id"));

	std::string id = F::addPlugin(F::m_config_ptr);
	BOOST_CHECK(F::hasPlugin(id));

	F::removePlugin(id);
	BOOST_CHECK(!F::hasPlugin(id));
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkGetWithUnknownId) {
	BOOST_CHECK(F::m_plugins.get("abc") == NULL);
	BOOST_CHECK(F::m_plugins.getLibPtr("abc").getPluginName() == "");
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkGetIdReturnsId) {
	std::string id = F::addPlugin(F::m_config_ptr);

	BOOST_CHECK_NO_THROW(this->m_plugin = F::m_plugins.get(id));
	BOOST_CHECK_EQUAL(this->m_plugin->getId(), id);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkGetPluginNameReturnsPluginName) {
	std::string id = F::addPlugin(F::m_config_ptr);

	PionPluginPtr<typename F::plugin_type> p = F::m_plugins.getLibPtr(id);
	BOOST_CHECK_EQUAL(p.getPluginName(), this->m_concrete_plugin_class);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkRegisterForUpdates) {
	BOOST_CHECK_NO_THROW(F::registerForUpdates(std::bind(&MockCallback)));

	BOOST_CHECK_NO_THROW(F::setConfigFile(F::m_config_file_path));
	BOOST_CHECK_NO_THROW(F::openConfigFile());

	g_num_times_mock_called = 0;
	std::string id = F::addPlugin(F::m_config_ptr);
	BOOST_CHECK(g_num_times_mock_called == 1);

	xmlNodePtr comment_node = xmlNewNode(NULL, reinterpret_cast<const xmlChar*>("Comment"));
	xmlNodeSetContent(comment_node,  reinterpret_cast<const xmlChar*>("A new comment"));
	xmlAddNextSibling(this->m_config_ptr, comment_node);
	BOOST_CHECK_NO_THROW(F::setPluginConfig(id, F::m_config_ptr));
	BOOST_CHECK(g_num_times_mock_called == 2);
}

BOOST_AUTO_TEST_SUITE_END()

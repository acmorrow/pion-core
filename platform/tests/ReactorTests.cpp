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
#include <pion/PionPlugin.hpp>
#include <pion/PionScheduler.hpp>
#include <pion/PionUnitTestDefs.hpp>
#include <pion/platform/PionPlatformUnitTest.hpp>
#include <pion/platform/VocabularyManager.hpp>
#include <pion/platform/ReactionEngine.hpp>
#include <pion/platform/CodecFactory.hpp>
#include <pion/platform/ProtocolFactory.hpp>
#include <pion/platform/DatabaseManager.hpp>
#include <boost/regex.hpp>
#include <boost/cstdint.hpp>
#include <boost/thread/thread.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/mpl/list.hpp>
#include <boost/mpl/transform.hpp>
#include <boost/mpl/lambda.hpp>
#include <fstream>
#include <string>

using namespace pion;
using namespace pion::platform;
//using namespace boost::mpl;	// Can't use either of these, because they make ...
//using boost::mpl::_1;			// ... event.hpp confused about which _1 to use.
using boost::mpl::transform1;
using boost::mpl::lambda;


/// static strings used by these unit tests
static const std::string COMBINED_LOG_FILE(LOG_FILE_DIR + "combined.log");
static const std::string NEW_LOG_FILE(LOG_FILE_DIR + "new.log");
static const std::string NEW_DATABASE_FILE(LOG_FILE_DIR + "clickstream.db");
static const std::string RSS_REQUEST_LOG_FILE(LOG_FILE_DIR + "rss_request.xml");
static const std::string RSS_CHANNELS_LOG_FILE(LOG_FILE_DIR + "rss_channels.xml");
static const std::string RSS_CHANNELS_EXPECTED_FILE(LOG_FILE_DIR + "rss_channels_expected.xml");
static const std::string RSS_ITEMS_LOG_FILE(LOG_FILE_DIR + "rss_items.xml");
static const std::string RSS_ITEMS_EXPECTED_FILE(LOG_FILE_DIR + "rss_items_expected.xml");
static const std::string ATOM_REQUEST_LOG_FILE(LOG_FILE_DIR + "atom_request.xml");
static const std::string ATOM_FEEDS_LOG_FILE(LOG_FILE_DIR + "atom_feeds.xml");
static const std::string ATOM_FEEDS_EXPECTED_FILE(LOG_FILE_DIR + "atom_feeds_expected.xml");
static const std::string ATOM_ENTRIES_LOG_FILE(LOG_FILE_DIR + "atom_entries.xml");
static const std::string ATOM_ENTRIES_EXPECTED_FILE(LOG_FILE_DIR + "atom_entries_expected.xml");


/// cleans up reactor config files in the working directory
void cleanup_reactor_config_files(void)
{
	PionPlatformUnitTest::cleanup_vocab_config_files();

	if (boost::filesystem::exists(REACTORS_CONFIG_FILE))
		boost::filesystem::remove(REACTORS_CONFIG_FILE);
	boost::filesystem::copy_file(REACTORS_TEMPLATE_FILE, REACTORS_CONFIG_FILE);

	if (boost::filesystem::exists(CODECS_CONFIG_FILE))
		boost::filesystem::remove(CODECS_CONFIG_FILE);
	boost::filesystem::copy_file(CODECS_TEMPLATE_FILE, CODECS_CONFIG_FILE);

	if (boost::filesystem::exists(DATABASES_CONFIG_FILE))
		boost::filesystem::remove(DATABASES_CONFIG_FILE);
	boost::filesystem::copy_file(DATABASES_TEMPLATE_FILE, DATABASES_CONFIG_FILE);
}


class FilterReactor         { public: static std::string name() { return "FilterReactor"; } };
class TransformReactor      { public: static std::string name() { return "TransformReactor"; } };
class LogInputReactor       { public: static std::string name() { return "LogInputReactor"; } };
class LogOutputReactor      { public: static std::string name() { return "LogOutputReactor"; } };
class DatabaseOutputReactor { public: static std::string name() { return "DatabaseOutputReactor"; } };
class ScriptReactor         { public: static std::string name() { return "ScriptReactor"; } };
#ifdef PION_HAVE_PYTHON
class PythonReactor         { public: static std::string name() { return "PythonReactor"; } };
#endif // PION_HAVE_PYTHON

typedef boost::mpl::list<
	FilterReactor
	, TransformReactor
	, LogInputReactor
	, LogOutputReactor
	, DatabaseOutputReactor
	, ScriptReactor
#ifdef PION_HAVE_PYTHON
	, PythonReactor
#endif // PION_HAVE_PYTHON
> reactor_list;

template<typename plugin_class>
class PluginPtrReadyToOpenReactor_F : public PionPluginPtr<Reactor> {
public:
	PluginPtrReadyToOpenReactor_F() {
		m_plugin_name = plugin_class::name();
	}
	~PluginPtrReadyToOpenReactor_F() {
	}

	std::string m_plugin_name;
};

// result is boost::mpl::list<PluginPtrReadyToOpenReactor_F<FilterReactor>, PluginPtrReadyToOpenReactor_F<TransformReactor>, ...>
typedef transform1< reactor_list, lambda<PluginPtrReadyToOpenReactor_F<boost::mpl::_1> >::type >::type
		PluginPtrReadyToOpenReactor_F_list;

// PluginPtrReadyToOpenReactor_S contains tests that should pass for any type of Reactor
BOOST_AUTO_TEST_SUITE_FIXTURE_TEMPLATE(PluginPtrReadyToOpenReactor_S, PluginPtrReadyToOpenReactor_F_list)

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkOpenReactor) {
	BOOST_CHECK_NO_THROW(F::open(F::m_plugin_name));
}

BOOST_AUTO_TEST_SUITE_END()


template<typename plugin_class>
class PluginPtrWithReactorLoaded_F : public PluginPtrReadyToOpenReactor_F<plugin_class> {
public:
	PluginPtrWithReactorLoaded_F() {
		this->open(this->m_plugin_name);
		m_reactor = NULL;
	}
	~PluginPtrWithReactorLoaded_F() {
		if (m_reactor) this->destroy(m_reactor);
	}

	Reactor* m_reactor;
};

typedef transform1< reactor_list, lambda<PluginPtrWithReactorLoaded_F<boost::mpl::_1> >::type >::type
		PluginPtrWithReactorLoaded_F_list;

// PluginPtrWithReactorLoaded_S contains tests that should pass for any type of Reactor
BOOST_AUTO_TEST_SUITE_FIXTURE_TEMPLATE(PluginPtrWithReactorLoaded_S, PluginPtrWithReactorLoaded_F_list)

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkIsOpenReturnsTrue) {
	BOOST_CHECK(F::is_open());
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkGetPluginNameReturnsPluginName) {
	BOOST_CHECK_EQUAL(F::getPluginName(), F::m_plugin_name);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkCreateReturnsSomething) {
	BOOST_CHECK((F::m_reactor = F::create()) != NULL);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkDestroyDoesntThrowExceptionAfterCreate) {
	F::m_reactor = F::create();
	BOOST_CHECK_NO_THROW(F::destroy(F::m_reactor));
	F::m_reactor = NULL;
}

BOOST_AUTO_TEST_SUITE_END()


/// interface class for ReactionEngine tests
class ReactionEngineTestInterface_F {
public:
	ReactionEngineTestInterface_F()
		: m_vocab_mgr(), m_codec_factory(m_vocab_mgr), m_protocol_factory(m_vocab_mgr), m_database_mgr(m_vocab_mgr),
		m_reaction_engine(m_vocab_mgr, m_codec_factory, m_protocol_factory, m_database_mgr),
		m_combined_id("3f49f2da-bfe3-11dc-8875-0016cb926e68"),
		m_rss_request_id("4e97184e-0e66-11de-a6a9-0019d185f6fc"),
		m_ie_filter_id("153f6c40-cb78-11dc-8fa0-0019e3f89cd2"),
		m_do_nothing_id("0cc21558-cf84-11dc-a9e0-0019e3f89cd2"),
		m_ie_or_firefox_filter_id("183a2b12-caf8-11dd-ba3c-0019d185f6fc"),
		m_not_ie_from_google_filter_id("105ee482-caf8-11dd-8a85-0019d185f6fc"),
		m_log_reader_id("c7a9f95a-e305-11dc-98ce-0016cb926e68"),
		m_log_writer_id("a92b7278-e306-11dc-85f0-0016cb926e68"),
		m_clickstream_id("a8928460-eb0c-11dc-9b68-0019e3f89cd2"),
		m_embedded_db_id("e75d88f0-e7df-11dc-a76c-0016cb926e68"),
		m_rss_channels_fission_id("fd69757c-0e8b-11de-8031-0019d185f6fc"),
		m_rss_channels_log_id("0a4757fa-0e8c-11de-8f1d-0019d185f6fc"),
		m_rss_items_fission_id("5f5ba5b6-1021-11de-b1a4-0019d185f6fc"),
		m_rss_items_log_id("5ff1bf10-1021-11de-bfa7-0019d185f6fc"),
		m_atom_feeds_fission_id("57137402-13d6-11de-9b7e-001cc02bd66b"),
		m_atom_feeds_log_id("5763fd50-13d6-11de-bde1-001cc02bd66b"),
		m_atom_entries_fission_id("58115b62-13d6-11de-8e09-001cc02bd66b"),
		m_atom_entries_log_id("8c7451fc-13d6-11de-8672-001cc02bd66b")
	{
		cleanup_reactor_config_files();
		
		m_vocab_mgr.setConfigFile(VOCABS_CONFIG_FILE);
		m_vocab_mgr.openConfigFile();
		m_codec_factory.setConfigFile(CODECS_CONFIG_FILE);
		m_codec_factory.openConfigFile();
		m_database_mgr.setConfigFile(DATABASES_CONFIG_FILE);
		m_database_mgr.openConfigFile();
		
		m_combined_codec = m_codec_factory.getCodec(m_combined_id);
		BOOST_CHECK(m_combined_codec);

		m_request_content_codec = m_codec_factory.getCodec(m_rss_request_id);
		BOOST_CHECK(m_request_content_codec);

		m_reaction_engine.start();
	}
	virtual ~ReactionEngineTestInterface_F() { m_reaction_engine.stop(); }

	std::string getReactorConfigOptionFromConfigFile(const std::string& reactor_id, const std::string& option_name) {
		// Parse the entire config file.
		std::ifstream in(REACTORS_CONFIG_FILE.c_str());
		std::string file_contents;
		char c;
		while (in.get(c)) file_contents += c;
		xmlDocPtr doc_ptr = xmlParseMemory(file_contents.c_str(), file_contents.size());
		BOOST_REQUIRE(doc_ptr);
		xmlNodePtr node_ptr = xmlDocGetRootElement(doc_ptr);
		BOOST_REQUIRE(node_ptr);
		BOOST_REQUIRE(node_ptr->children);

		// Get the node for the specified Reactor.	
		xmlNodePtr reactor_node = ConfigManager::findConfigNodeByAttr("Reactor",
																	  "id", reactor_id,
																	  node_ptr->children);
		BOOST_REQUIRE(reactor_node);

		// Return the value of the specified option for the specified Reactor.
		std::string option_value;
		ConfigManager::getConfigOption(option_name, option_value, reactor_node->children);
		return option_value;
	}

	
	VocabularyManager	m_vocab_mgr;
	CodecFactory		m_codec_factory;
	ProtocolFactory		m_protocol_factory;
	DatabaseManager		m_database_mgr;
	ReactionEngine		m_reaction_engine;
	const std::string	m_combined_id;
	const std::string	m_rss_request_id;
	const std::string	m_ie_filter_id;
	const std::string	m_do_nothing_id;
	const std::string	m_ie_or_firefox_filter_id;
	const std::string	m_not_ie_from_google_filter_id;
	const std::string	m_log_reader_id;
	const std::string	m_log_writer_id;
	const std::string	m_clickstream_id;
	const std::string	m_embedded_db_id;
	const std::string	m_rss_channels_fission_id;
	const std::string	m_rss_channels_log_id;
	const std::string	m_rss_items_fission_id;
	const std::string	m_rss_items_log_id;
	const std::string	m_atom_feeds_fission_id;
	const std::string	m_atom_feeds_log_id;
	const std::string	m_atom_entries_fission_id;
	const std::string	m_atom_entries_log_id;
	CodecPtr			m_combined_codec;
	CodecPtr			m_request_content_codec;
};


/// fixture for some basic ReactionEngine tests
class ReactionEngineBasicTests_F
	: public ReactionEngineTestInterface_F
{
public:
	ReactionEngineBasicTests_F() {
		boost::filesystem::remove(REACTORS_CONFIG_FILE);
		m_reaction_engine.setConfigFile(REACTORS_CONFIG_FILE);
		m_reaction_engine.createConfigFile();
	}
	virtual ~ReactionEngineBasicTests_F() {}
};

BOOST_FIXTURE_TEST_SUITE(ReactionEngineBasicTests_S, ReactionEngineBasicTests_F)

BOOST_AUTO_TEST_CASE(checkAddConnectionForMissingReactor) {
	BOOST_CHECK_THROW(m_reaction_engine.addReactorConnection("bad_id", "another_bad_id"),
					  ReactionEngine::ReactorNotFoundException);
}

BOOST_AUTO_TEST_CASE(checkRemoveConnectionForMissingReactor) {
	BOOST_CHECK_THROW(m_reaction_engine.removeReactorConnection("bad_id", "another_bad_id"),
					  ReactionEngine::ReactorNotFoundException);
}

BOOST_AUTO_TEST_CASE(checkAddReactorMinimalConfig) {
	std::string config_str = "<PionConfig><Reactor><Plugin>FilterReactor</Plugin><Workspace>1</Workspace></Reactor></PionConfig>";
	xmlNodePtr config_ptr = ConfigManager::createResourceConfig("Reactor", config_str.c_str(), config_str.size());
	std::string reactor_id = m_reaction_engine.addReactor(config_ptr);
	xmlFreeNodeList(config_ptr);
	BOOST_CHECK(! reactor_id.empty());
}

BOOST_AUTO_TEST_CASE(checkAddReactorSimpleConfig) {
	std::string config_str = "<PionConfig><Reactor><Plugin>FilterReactor</Plugin><Workspace>1</Workspace><Name>Filter Reactor 1</Name></Reactor></PionConfig>";
	xmlNodePtr config_ptr = ConfigManager::createResourceConfig("Reactor", config_str.c_str(), config_str.size());
	std::string reactor_id = m_reaction_engine.addReactor(config_ptr);
	xmlFreeNodeList(config_ptr);
	BOOST_CHECK(! reactor_id.empty());
}

BOOST_AUTO_TEST_CASE(checkSetReactorLocationWithoutPreExistingCoords) {
	// Create a Reactor without X or Y coordinates.
	xmlNodePtr orig_config = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>FilterReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Name>Filter Reactor 1</Name>"
	);
	std::string reactor_id = m_reaction_engine.addReactor(orig_config);
	xmlFreeNodeList(orig_config);

	// Update the Reactor's UI location.
	xmlNodePtr new_config = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>FilterReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Name>Filter Reactor 1</Name>"
		"<X>75</X>"
		"<Y>50</Y>"
	);
	BOOST_CHECK_NO_THROW(m_reaction_engine.setReactorLocation(reactor_id, new_config));
	xmlFreeNodeList(new_config);

	// Get the configuration from the ReactionEngine and check that the location parameters are present.
	xmlNodePtr retrieved_config = m_reaction_engine.getPluginConfig(reactor_id);
	std::string config_option_str;
	ConfigManager::getConfigOption("X", config_option_str, retrieved_config->children);
	BOOST_CHECK_EQUAL(config_option_str, "75");
	ConfigManager::getConfigOption("Y", config_option_str, retrieved_config->children);
	BOOST_CHECK_EQUAL(config_option_str, "50");
	xmlFreeNodeList(retrieved_config);
}

BOOST_AUTO_TEST_CASE(checkSetReactorLocationWithPreExistingCoords) {
	// Create a Reactor with X and Y coordinates.
	xmlNodePtr orig_config = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>FilterReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Name>Filter Reactor 1</Name>"
		"<X>40</X>"
		"<Y>90</Y>"
	);
	std::string reactor_id = m_reaction_engine.addReactor(orig_config);
	xmlFreeNodeList(orig_config);

	// Update the Reactor's UI location.
	xmlNodePtr new_config = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>FilterReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Name>Filter Reactor 1</Name>"
		"<X>75</X>"
		"<Y>50</Y>"
	);
	BOOST_CHECK_NO_THROW(m_reaction_engine.setReactorLocation(reactor_id, new_config));
	xmlFreeNodeList(new_config);

	// Get the configuration from the ReactionEngine and check that the location parameters have changed.
	xmlNodePtr retrieved_config = m_reaction_engine.getPluginConfig(reactor_id);
	std::string config_option_str;
	ConfigManager::getConfigOption("X", config_option_str, retrieved_config->children);
	BOOST_CHECK_EQUAL(config_option_str, "75");
	ConfigManager::getConfigOption("Y", config_option_str, retrieved_config->children);
	BOOST_CHECK_EQUAL(config_option_str, "50");
	xmlFreeNodeList(retrieved_config);
}

BOOST_AUTO_TEST_CASE(checkSetReactorLocationIgnoresNonLocationChanges) {
	// Create a Reactor with X and Y coordinates.
	xmlNodePtr orig_config = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>FilterReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Name>Filter Reactor 1</Name>"
		"<X>40</X>"
		"<Y>90</Y>"
	);
	std::string reactor_id = m_reaction_engine.addReactor(orig_config);
	xmlFreeNodeList(orig_config);

	// Call setReactorLocation() with a config that also has a new name.
	xmlNodePtr new_config = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>FilterReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Name>Extra Spiffy Filter Reactor</Name>"
		"<X>75</X>"
		"<Y>50</Y>"
	);
	BOOST_CHECK_NO_THROW(m_reaction_engine.setReactorLocation(reactor_id, new_config));
	xmlFreeNodeList(new_config);

	// Get the configuration from the ReactionEngine and check that the location parameters have changed but the name hasn't.
	xmlNodePtr retrieved_config = m_reaction_engine.getPluginConfig(reactor_id);
	std::string config_option_str;
	ConfigManager::getConfigOption("X", config_option_str, retrieved_config->children);
	BOOST_CHECK_EQUAL(config_option_str, "75");
	ConfigManager::getConfigOption("Y", config_option_str, retrieved_config->children);
	BOOST_CHECK_EQUAL(config_option_str, "50");
	ConfigManager::getConfigOption("Name", config_option_str, retrieved_config->children);
	BOOST_CHECK_EQUAL(config_option_str, "Filter Reactor 1");
	xmlFreeNodeList(retrieved_config);
}

BOOST_AUTO_TEST_CASE(checkSetReactorLocationDoesntCallSetConfig) {
	// Create a LogOutputReactor (because LogOutputReactor::setConfig() actually does some verification.)
	xmlNodePtr orig_config = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>LogOutputReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Codec>23f68d5a-bfec-11dc-81a7-0016cb926e68</Codec>"
		"<Filename>../logs/new.log</Filename>"
	);
	std::string reactor_id = m_reaction_engine.addReactor(orig_config);
	xmlFreeNodeList(orig_config);

	// Create a configuration that should cause LogOutputReactor::setConfig() to throw (due to missing elements).
	xmlNodePtr bad_config = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>LogOutputReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<X>75</X>"
		"<Y>50</Y>"
	);

	// Confirm that setReactorConfig() throws an exception.
	BOOST_CHECK_THROW(m_reaction_engine.setReactorConfig(reactor_id, bad_config), PionException);

	// Check that setReactorLocation() doesn't throw an exception.
	// (The point of this test is to confirm that LogOutputReactor::setConfig()
	// is not getting called, since the whole point of setReactorLocation() is to bypass
	// potentially expensive calls to Reactor specific implementations of setConfig().)
	BOOST_CHECK_NO_THROW(m_reaction_engine.setReactorLocation(reactor_id, bad_config));

	xmlFreeNodeList(bad_config);
}

BOOST_AUTO_TEST_CASE(checkSetReactorLocationUpdatesConfigFile) {
	// Create a Reactor without X or Y coordinates.
	xmlNodePtr orig_config = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>FilterReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Name>Filter Reactor 1</Name>"
	);
	std::string reactor_id = m_reaction_engine.addReactor(orig_config);
	xmlFreeNodeList(orig_config);

	// Update the Reactor's UI location.
	xmlNodePtr new_config = PionPlatformUnitTest::makeReactorConfigFromString(
		"<Plugin>FilterReactor</Plugin>"
		"<Workspace>1</Workspace>"
		"<Name>Filter Reactor 1</Name>"
		"<X>125</X>"
		"<Y>150</Y>"
	);
	BOOST_CHECK_NO_THROW(m_reaction_engine.setReactorLocation(reactor_id, new_config));
	xmlFreeNodeList(new_config);

	// Check that the new coordinates were saved in the Reactors config file.
	std::ifstream in(REACTORS_CONFIG_FILE.c_str());
	std::string file_contents;
	char c;
	while (in.get(c)) file_contents += c;
	BOOST_CHECK(file_contents.find("<X>125</X>") != std::string::npos);
	BOOST_CHECK(file_contents.find("<Y>150</Y>") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(checkAddWorkspace) {
	std::string config_str = "<PionConfig><Workspace><Name>My First Workspace</Name></Workspace></PionConfig>";
	std::string workspace_id = m_reaction_engine.addWorkspace(config_str.c_str(), config_str.size());
	BOOST_CHECK(! workspace_id.empty());
}

BOOST_AUTO_TEST_SUITE_END()


/// fixture for Reactor connection tests
class ReactionEngineConnectionTests_F
	: public ReactionEngineBasicTests_F
{
public:
	ReactionEngineConnectionTests_F() {
		xmlNodePtr config_ptr(ConfigManager::createPluginConfig("FilterReactor"));
		xmlNodePtr node = xmlNewNode(NULL, reinterpret_cast<const xmlChar*>("Workspace"));
		xmlNodeSetContent(node, reinterpret_cast<const xmlChar*>("1"));
		xmlAddNextSibling(config_ptr, node);

		filter_one_id = m_reaction_engine.addReactor(config_ptr);
		BOOST_REQUIRE(! filter_one_id.empty());

		filter_two_id = m_reaction_engine.addReactor(config_ptr);
		BOOST_REQUIRE(! filter_two_id.empty());

		xmlFreeNodeList(config_ptr);
	}
	virtual ~ReactionEngineConnectionTests_F() {}
	
	std::string filter_one_id;
	std::string filter_two_id;
};

BOOST_FIXTURE_TEST_SUITE(ReactionEngineConnectionTests_S, ReactionEngineConnectionTests_F)

BOOST_AUTO_TEST_CASE(checkAddThenRemoveReactorConnection) {
	m_reaction_engine.addReactorConnection(filter_one_id, filter_two_id);
	
	// check config file
	// ...
	
	m_reaction_engine.removeReactorConnection(filter_one_id, filter_two_id);
	
	// check config file
	// ...
}

BOOST_AUTO_TEST_SUITE_END()


/// fixture for testing reactors (with the ReactionEngine already running)
class ReactionEngineAlreadyRunningTests_F
	: public ReactionEngineTestInterface_F
{
public:
	ReactionEngineAlreadyRunningTests_F()
		: m_finished_log_callbacks(0U)
	{
		PionPlatformUnitTest::cleanup_cache_files();
		boost::filesystem::remove(NEW_LOG_FILE);
		boost::filesystem::remove(NEW_DATABASE_FILE);
		boost::filesystem::remove(RSS_CHANNELS_LOG_FILE);
		boost::filesystem::remove(RSS_ITEMS_LOG_FILE);
		boost::filesystem::remove(ATOM_FEEDS_LOG_FILE);
		boost::filesystem::remove(ATOM_ENTRIES_LOG_FILE);
		m_reaction_engine.setConfigFile(REACTORS_CONFIG_FILE);
		m_reaction_engine.openConfigFile();
	}
	virtual ~ReactionEngineAlreadyRunningTests_F() {}

	void finishedLog(const std::string& reactor_id, const std::string& signal_id, void *ptr) {
		BOOST_CHECK_EQUAL(reactor_id, m_log_reader_id);
		BOOST_CHECK_EQUAL(signal_id, "FinishedLog");
		BOOST_REQUIRE(ptr);
		std::string *str_ptr = (std::string *) ptr;
		BOOST_CHECK_NE(*str_ptr, "");
		++m_finished_log_callbacks;
	}
	
	unsigned long m_finished_log_callbacks;
};


BOOST_FIXTURE_TEST_SUITE(ReactionEngineAlreadyRunningTests_S, ReactionEngineAlreadyRunningTests_F)

BOOST_AUTO_TEST_CASE(checkSetReactorWorkspace) {
	// get the current configuration for the Reactor
	xmlNodePtr reactor_config = m_reaction_engine.getPluginConfig(m_ie_filter_id);
	BOOST_REQUIRE(reactor_config);
	
	// add a "Workspace" node
	xmlNodePtr workspace_node = xmlNewNode(NULL, reinterpret_cast<const xmlChar*>("Workspace"));
	xmlNodeSetContent(workspace_node,  reinterpret_cast<const xmlChar*>("Log Workspace"));
	xmlAddNextSibling(reactor_config->last, workspace_node);

	// update the Reactor's config
	BOOST_CHECK_NO_THROW(m_reaction_engine.setReactorConfig(m_ie_filter_id,
															reactor_config->children));
	xmlFreeNodeList(reactor_config);
	
	// check config file
	// ...
}

BOOST_AUTO_TEST_CASE(checkReactorOfTypeCollectionIsNotRunning) {
	std::ostringstream out;
	m_reaction_engine.writeStatsXML(out, m_log_reader_id);
	BOOST_CHECK(boost::regex_search(out.str(), boost::regex("<Running>false</Running>")));
}

BOOST_AUTO_TEST_CASE(checkReactorNotOfTypeCollectionIsRunning) {
	std::ostringstream out;
	m_reaction_engine.writeStatsXML(out, m_log_writer_id);
	BOOST_CHECK(boost::regex_search(out.str(), boost::regex("<Running>true</Running>")));
}

BOOST_AUTO_TEST_CASE(checkStartReactorOfTypeCollection) {
	// The Reactor should not be running yet (tested in checkReactorOfTypeCollectionIsNotRunning).
	m_reaction_engine.startReactor(m_log_reader_id);

	// Check that writeStatsXML() reports that it's running.
	std::ostringstream out;
	m_reaction_engine.writeStatsXML(out, m_log_reader_id);
	BOOST_CHECK(boost::regex_search(out.str(), boost::regex("<Running>true</Running>")));

	// Check that the correct run status was saved in the config file.
	BOOST_CHECK_EQUAL(getReactorConfigOptionFromConfigFile(m_log_reader_id, "Running"), "true");
}

BOOST_AUTO_TEST_CASE(checkReactorSignalCallbacks) {
	// subscribe to signal
	boost::signals::scoped_connection conn =
		m_reaction_engine.subscribe(m_log_reader_id, "FinishedLog",
		std::bind(&ReactionEngineAlreadyRunningTests_F::finishedLog, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

	// The Reactor should not be running yet (tested in checkReactorOfTypeCollectionIsNotRunning).
	m_reaction_engine.startReactor(m_log_reader_id);

	// wait up to one second for the reactor to finish reading the log file
	const unsigned long EXPECTED_LOGS = 1U;
	const int num_checks_allowed = 10;
	for (int i = 0; i < num_checks_allowed; ++i) {
		pion::PionScheduler::sleep(0, 100000000); // 0.1 seconds
		if (m_finished_log_callbacks >= EXPECTED_LOGS) break;
	}
	BOOST_REQUIRE_GE(m_finished_log_callbacks, EXPECTED_LOGS);
}

BOOST_AUTO_TEST_CASE(checkStopReactorOfTypeCollection) {
	// The Reactor should not be running yet (tested in checkReactorOfTypeCollectionIsNotRunning).
	m_reaction_engine.stopReactor(m_log_reader_id);

	// Check that writeStatsXML() reports that it's not running.
	std::ostringstream out;
	m_reaction_engine.writeStatsXML(out, m_log_reader_id);
	BOOST_CHECK(boost::regex_search(out.str(), boost::regex("<Running>false</Running>")));

	// Check that the correct run status was saved in the config file.
	BOOST_CHECK_EQUAL(getReactorConfigOptionFromConfigFile(m_log_reader_id, "Running"), "false");
}

BOOST_AUTO_TEST_CASE(checkStartReactorNotOfTypeCollection) {
	// The Reactor should already be running (tested in checkReactorNotOfTypeCollectionIsRunning).
	m_reaction_engine.startReactor(m_log_writer_id);

	// Check that writeStatsXML() still reports that it's running.
	std::ostringstream out;
	m_reaction_engine.writeStatsXML(out, m_log_writer_id);
	BOOST_CHECK(boost::regex_search(out.str(), boost::regex("<Running>true</Running>")));

	// Check that the correct run status was saved in the config file.
	BOOST_CHECK_EQUAL(getReactorConfigOptionFromConfigFile(m_log_writer_id, "Running"), "true");
}

BOOST_AUTO_TEST_CASE(checkStopReactorNotOfTypeCollection) {
	// The Reactor should already be running (tested in checkReactorNotOfTypeCollectionIsRunning).
	m_reaction_engine.stopReactor(m_log_writer_id);

	// Check that writeStatsXML() reports that it's not running.
	std::ostringstream out;
	m_reaction_engine.writeStatsXML(out, m_log_writer_id);
	BOOST_CHECK(boost::regex_search(out.str(), boost::regex("<Running>false</Running>")));

	// Check that the correct run status was saved in the config file.
	BOOST_CHECK_EQUAL(getReactorConfigOptionFromConfigFile(m_log_writer_id, "Running"), "false");
}

BOOST_AUTO_TEST_CASE(checkNumberofIERequestsInLogFile) {
	// start the log reader reactor
	m_reaction_engine.startReactor(m_log_reader_id);
	
	// make sure that the log input reactor has consumed all of the events
	PionPlatformUnitTest::checkReactorEventsIn(m_reaction_engine, m_log_reader_id, 4UL);
	PionPlatformUnitTest::checkReactorEventsOut(m_reaction_engine, m_log_reader_id, 4UL);

	// check the IE or Firefox Filter Reactor (match_all_comparisons = false)
	PionPlatformUnitTest::checkReactorEventsIn(m_reaction_engine, m_ie_or_firefox_filter_id, 4UL);
	PionPlatformUnitTest::checkReactorEventsOut(m_reaction_engine, m_ie_or_firefox_filter_id, 2UL);

	// check the Not IE From Google Filter Reactor (match_all_comparisons = true)
	PionPlatformUnitTest::checkReactorEventsIn(m_reaction_engine, m_not_ie_from_google_filter_id, 4UL);
	PionPlatformUnitTest::checkReactorEventsOut(m_reaction_engine, m_not_ie_from_google_filter_id, 1UL);

	// make sure that the log output reactor has written all of the events
	PionPlatformUnitTest::checkReactorEventsIn(m_reaction_engine, m_log_writer_id, 4UL);
	PionPlatformUnitTest::checkReactorEventsOut(m_reaction_engine, m_log_writer_id, 4UL);


	// push events from the log file into the IE filter reactor
	boost::uint64_t events_read = PionPlatformUnitTest::feedFileToReactor(
		m_reaction_engine, m_ie_filter_id, *m_combined_codec, COMBINED_LOG_FILE);
		
	// make sure that four events were read from the log
	BOOST_CHECK_EQUAL(events_read, static_cast<boost::uint64_t>(4));
	
	// check the IE Filter Reactor
	PionPlatformUnitTest::checkReactorEventsIn(m_reaction_engine, m_ie_filter_id, 4UL);
	PionPlatformUnitTest::checkReactorEventsOut(m_reaction_engine, m_ie_filter_id, 1UL);

	// check the do nothing Reactor
	PionPlatformUnitTest::checkReactorEventsIn(m_reaction_engine, m_do_nothing_id, 1UL);
	PionPlatformUnitTest::checkReactorEventsOut(m_reaction_engine, m_do_nothing_id, 1UL);


	// check the total number of operations (sum of getEventsIn() for each reactor)
	BOOST_CHECK_EQUAL(m_reaction_engine.getEventsIn(m_log_reader_id), 4UL);
	BOOST_CHECK_EQUAL(m_reaction_engine.getEventsIn(m_ie_or_firefox_filter_id), 4UL);
	BOOST_CHECK_EQUAL(m_reaction_engine.getEventsIn(m_not_ie_from_google_filter_id), 4UL);
	BOOST_CHECK_EQUAL(m_reaction_engine.getEventsIn(m_log_writer_id), 4UL);
	BOOST_CHECK_EQUAL(m_reaction_engine.getEventsIn(m_ie_filter_id), 4UL);
	BOOST_CHECK_EQUAL(m_reaction_engine.getEventsIn(m_do_nothing_id), 1UL);
	BOOST_CHECK_EQUAL(m_reaction_engine.getEventsIn(m_clickstream_id), 0UL);

	BOOST_CHECK_EQUAL(m_reaction_engine.getTotalOperations(), static_cast<boost::uint64_t>(21));
	
	//
	// check the contents of the new log file
	//
	
	// open up the log file
	std::ifstream log_stream;
	log_stream.open(NEW_LOG_FILE.c_str(), std::ios::in);
	BOOST_REQUIRE(log_stream.is_open());
	
	// read each line
	bool found_it = false;
	boost::regex last_event_regex(".30/Jan/2008.*10.0.141.122.*pion.*wikipedia.*200");
	const unsigned int BUF_SIZE = 1023;
	char buf[BUF_SIZE+1];
	while (log_stream.getline(buf, BUF_SIZE)) {
		// look for the last Event
		if (boost::regex_search(buf, last_event_regex)) {
			found_it = true;
			break;
		}
	}
	BOOST_CHECK(found_it);
}

BOOST_AUTO_TEST_CASE(checkExtractRSSChannelsUsingFissionReactor) {

	// push events from the log file into the IE filter reactor
	boost::uint64_t events_read = PionPlatformUnitTest::feedFileToReactor(
		m_reaction_engine, m_rss_channels_fission_id, *m_request_content_codec, RSS_REQUEST_LOG_FILE);
		
	// make sure that the event was read from the log
	BOOST_CHECK_EQUAL(events_read, static_cast<boost::uint64_t>(1));
	
	// check the RSS Fission Reactor
	PionPlatformUnitTest::checkReactorEventsIn(m_reaction_engine, m_rss_channels_fission_id, 1UL);
	PionPlatformUnitTest::checkReactorEventsOut(m_reaction_engine, m_rss_channels_fission_id, 1UL);

	// check the RSS Output Log Reactor
	PionPlatformUnitTest::checkReactorEventsIn(m_reaction_engine, m_rss_channels_log_id, 1UL);
	PionPlatformUnitTest::checkReactorEventsOut(m_reaction_engine, m_rss_channels_log_id, 1UL);

	// stop the log reactor to flush its output stream
	m_reaction_engine.stopReactor(m_rss_channels_log_id);

	// make sure that the output files match what is expected
	BOOST_CHECK(PionUnitTest::check_files_exact_match(RSS_CHANNELS_LOG_FILE, RSS_CHANNELS_EXPECTED_FILE, true));
}

BOOST_AUTO_TEST_CASE(checkExtractRSSItemsUsingFissionReactor) {

	// push events from the log file into the IE filter reactor
	boost::uint64_t events_read = PionPlatformUnitTest::feedFileToReactor(
		m_reaction_engine, m_rss_items_fission_id, *m_request_content_codec, RSS_REQUEST_LOG_FILE);
		
	// make sure that the event was read from the log
	BOOST_CHECK_EQUAL(events_read, static_cast<boost::uint64_t>(1));
	
	// check the RSS Fission Reactor
	PionPlatformUnitTest::checkReactorEventsIn(m_reaction_engine, m_rss_items_fission_id, 1UL);
	PionPlatformUnitTest::checkReactorEventsOut(m_reaction_engine, m_rss_items_fission_id, 1UL);

	// check the RSS Output Log Reactor
	PionPlatformUnitTest::checkReactorEventsIn(m_reaction_engine, m_rss_items_log_id, 1UL);
	PionPlatformUnitTest::checkReactorEventsOut(m_reaction_engine, m_rss_items_log_id, 1UL);

	// stop the log reactor to flush its output stream
	m_reaction_engine.stopReactor(m_rss_items_log_id);

	// make sure that the output files match what is expected
	BOOST_CHECK(PionUnitTest::check_files_exact_match(RSS_ITEMS_LOG_FILE, RSS_ITEMS_EXPECTED_FILE, true));
}

BOOST_AUTO_TEST_CASE(checkExtractAtomFeedsUsingFissionReactor) {

	// push events from the log file into the IE filter reactor
	boost::uint64_t events_read = PionPlatformUnitTest::feedFileToReactor(
		m_reaction_engine, m_atom_feeds_fission_id, *m_request_content_codec, ATOM_REQUEST_LOG_FILE);
		
	// make sure that the event was read from the log
	BOOST_CHECK_EQUAL(events_read, static_cast<boost::uint64_t>(1));
	
	// check the Atom Fission Reactor
	PionPlatformUnitTest::checkReactorEventsIn(m_reaction_engine, m_atom_feeds_fission_id, 1UL);
	PionPlatformUnitTest::checkReactorEventsOut(m_reaction_engine, m_atom_feeds_fission_id, 1UL);

	// check the Atom Output Log Reactor
	PionPlatformUnitTest::checkReactorEventsIn(m_reaction_engine, m_atom_feeds_log_id, 1UL);
	PionPlatformUnitTest::checkReactorEventsOut(m_reaction_engine, m_atom_feeds_log_id, 1UL);

	// stop the log reactor to flush its output stream
	m_reaction_engine.stopReactor(m_atom_feeds_log_id);

	// make sure that the output files match what is expected
	BOOST_CHECK(PionUnitTest::check_files_exact_match(ATOM_FEEDS_LOG_FILE, ATOM_FEEDS_EXPECTED_FILE, true));
}

BOOST_AUTO_TEST_CASE(checkExtractAtomEntriesUsingFissionReactor) {

	// push events from the log file into the IE filter reactor
	boost::uint64_t events_read = PionPlatformUnitTest::feedFileToReactor(
		m_reaction_engine, m_atom_entries_fission_id, *m_request_content_codec, ATOM_REQUEST_LOG_FILE);
		
	// make sure that the event was read from the log
	BOOST_CHECK_EQUAL(events_read, static_cast<boost::uint64_t>(1));
	
	// check the Atom Fission Reactor
	PionPlatformUnitTest::checkReactorEventsIn(m_reaction_engine, m_atom_entries_fission_id, 1UL);
	PionPlatformUnitTest::checkReactorEventsOut(m_reaction_engine, m_atom_entries_fission_id, 1UL);

	// check the Atom Output Log Reactor
	PionPlatformUnitTest::checkReactorEventsIn(m_reaction_engine, m_atom_entries_log_id, 1UL);
	PionPlatformUnitTest::checkReactorEventsOut(m_reaction_engine, m_atom_entries_log_id, 1UL);

	// stop the log reactor to flush its output stream
	m_reaction_engine.stopReactor(m_atom_entries_log_id);

	// make sure that the output files match what is expected
	BOOST_CHECK(PionUnitTest::check_files_exact_match(ATOM_ENTRIES_LOG_FILE, ATOM_ENTRIES_EXPECTED_FILE, true));
}

BOOST_AUTO_TEST_CASE(checkDatabaseOutputReactor) {
	// push events from the log file into the data store reactor
	boost::uint64_t events_read = PionPlatformUnitTest::feedFileToReactor(
		m_reaction_engine, m_clickstream_id, *m_combined_codec, COMBINED_LOG_FILE);
	
	// make sure that four events were read from the log
	BOOST_CHECK_EQUAL(events_read, static_cast<boost::uint64_t>(4));
	
	// make sure that the reactor received all of the events read
	PionPlatformUnitTest::checkReactorEventsOut(m_reaction_engine, m_clickstream_id, events_read);
	
	//
	// check the contents of the new database
	//
	
	// first stop the data store reactor to make sure the events get flushed
	m_reaction_engine.stopReactor(m_clickstream_id);
	
	// open the database
	DatabasePtr db_ptr = m_database_mgr.getDatabase(m_embedded_db_id);
	BOOST_REQUIRE(db_ptr);
	db_ptr->open();
	BOOST_REQUIRE(db_ptr->is_open());
	
	// prepare a query
	QueryPtr query_ptr = db_ptr->addQuery("urn:query:select_all", "SELECT * FROM clickstream");
	BOOST_REQUIRE(query_ptr);
	
	// count the number of records returned
	unsigned int db_records = 0;
	while (query_ptr->run())
		++db_records;
	BOOST_CHECK_EQUAL(db_records, events_read);
}

BOOST_AUTO_TEST_SUITE_END()


class ReactionEngineReadyForReactorConfigFile_F {
public:
	ReactionEngineReadyForReactorConfigFile_F()
		: m_vocab_mgr(), m_codec_factory(m_vocab_mgr), m_protocol_factory(m_vocab_mgr), m_database_mgr(m_vocab_mgr),
		m_reaction_engine(m_vocab_mgr, m_codec_factory, m_protocol_factory, m_database_mgr),
		m_filter_id("0cc21558-cf84-11dc-a9e0-0019e3f89cd2"),
		m_log_reader_id("c7a9f95a-e305-11dc-98ce-0016cb926e68"),
		m_config_filename(CONFIG_FILE_DIR + "reactorsWithStatus.xml")
	{
		cleanup_reactor_config_files();
		
		m_vocab_mgr.setConfigFile(VOCABS_CONFIG_FILE);
		m_vocab_mgr.openConfigFile();
		m_codec_factory.setConfigFile(CODECS_CONFIG_FILE);
		m_codec_factory.openConfigFile();

		m_reaction_engine.setConfigFile(m_config_filename);
		m_reaction_engine.start();
	}
	virtual ~ReactionEngineReadyForReactorConfigFile_F() { m_reaction_engine.stop(); }

	VocabularyManager	m_vocab_mgr;
	CodecFactory		m_codec_factory;
	ProtocolFactory		m_protocol_factory;
	DatabaseManager		m_database_mgr;
	ReactionEngine		m_reaction_engine;
	const std::string	m_filter_id;
	const std::string	m_log_reader_id;
	const std::string	m_config_filename;
};


BOOST_FIXTURE_TEST_SUITE(ReactionEngineReadyForReactorConfigFile_S, ReactionEngineReadyForReactorConfigFile_F)

BOOST_AUTO_TEST_CASE(checkReactorStatusInConfigFileIsUsed) {
	// Create a config file with two Reactors, one a collector (i.e. of type TYPE_COLLECTION), the other not.
	// The Running option is set to true for the collector and false for the non-collector, the opposite
	// of their default behavior.
	std::ofstream out(m_config_filename.c_str());
	out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		<< "<PionConfig xmlns=\"http://purl.org/pion/config\" pion_version=\"tests\">\n"
		<< "  <Reactor id=\"" << m_filter_id << "\">\n"
		<< "    <Name>Do Nothing</Name>\n"
		<< "    <Plugin>FilterReactor</Plugin>\n"
		<< "    <Workspace>1</Workspace>\n"
		<< "    <Running>false</Running>\n"
		<< "  </Reactor>\n"
		<< "  <Reactor id=\"" << m_log_reader_id << "\">\n"
		<< "    <Name>CLF Log Reader</Name>\n"
		<< "    <Plugin>LogInputReactor</Plugin>\n"
		<< "    <Workspace>1</Workspace>\n"
		<< "    <Codec>3f49f2da-bfe3-11dc-8875-0016cb926e68</Codec>\n"
		<< "    <Directory>../logs</Directory>\n"
		<< "    <Filename>combined.*\\.log</Filename>\n"
		<< "    <Running>true</Running>\n"
		<< "  </Reactor>\n"
		<< "</PionConfig>\n";
	out.close();

	// Start the ReactionEngine with the new config file.
	m_reaction_engine.openConfigFile();

	// Check that writeStatsXML() reports that the collector is running and the non-collector isn't.
	std::ostringstream out_1;
	m_reaction_engine.writeStatsXML(out_1, m_log_reader_id);
	BOOST_CHECK(boost::regex_search(out_1.str(), boost::regex("<Running>true</Running>")));
	BOOST_CHECK(! boost::regex_search(out_1.str(), boost::regex("<Running>false</Running>")));
	std::ostringstream out_2;
	m_reaction_engine.writeStatsXML(out_2, m_filter_id);
	BOOST_CHECK(boost::regex_search(out_2.str(), boost::regex("<Running>false</Running>")));
	BOOST_CHECK(! boost::regex_search(out_2.str(), boost::regex("<Running>true</Running>")));
}

BOOST_AUTO_TEST_SUITE_END()

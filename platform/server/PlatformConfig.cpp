// ------------------------------------------------------------------------
// Pion is a development platform for building Reactors that process Events
// ------------------------------------------------------------------------
// Copyright (C) 2007-2008 Atomic Labs, Inc.  (http://www.atomiclabs.com)
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

#include <boost/filesystem/operations.hpp>
#include <pion/PionPlugin.hpp>
#include <pion/PionLogger.hpp>
#include "PlatformConfig.hpp"

using namespace pion::net;
using namespace pion::platform;


namespace pion {		// begin namespace pion
namespace server {		// begin namespace server (Pion Server)

	
// static members of PlatformConfig
	
const std::string			PlatformConfig::DEFAULT_CONFIG_FILE = "platform.xml";
const std::string			PlatformConfig::VERSION_ELEMENT_NAME = "Version";
const std::string			PlatformConfig::PLATFORM_CONFIG_ELEMENT_NAME = "PlatformConfig";
const std::string			PlatformConfig::VOCABULARY_CONFIG_ELEMENT_NAME = "VocabularyConfig";
const std::string			PlatformConfig::CODEC_CONFIG_ELEMENT_NAME = "CodecConfig";
const std::string			PlatformConfig::PROTOCOL_CONFIG_ELEMENT_NAME = "ProtocolConfig";
const std::string			PlatformConfig::DATABASE_CONFIG_ELEMENT_NAME = "DatabaseConfig";
const std::string			PlatformConfig::REACTOR_CONFIG_ELEMENT_NAME = "ReactorConfig";
const std::string			PlatformConfig::SERVICE_CONFIG_ELEMENT_NAME = "ServiceConfig";
const std::string			PlatformConfig::USER_CONFIG_ELEMENT_NAME = "UserConfig";
const std::string			PlatformConfig::LOG_CONFIG_ELEMENT_NAME = "LogConfig";
const std::string			PlatformConfig::VOCABULARY_PATH_ELEMENT_NAME = "VocabularyPath";
const std::string			PlatformConfig::PLUGIN_PATH_ELEMENT_NAME = "PluginPath";
const std::string			PlatformConfig::DATA_DIRECTORY_ELEMENT_NAME = "DataDirectory";
const std::string			PlatformConfig::DEBUG_MODE_ELEMENT_NAME = "DebugMode";
const std::string			PlatformConfig::REACTION_ENGINE_ELEMENT_NAME = "ReactionEngine";
const std::string			PlatformConfig::SERVICE_MANAGER_ELEMENT_NAME = "ServiceManager";
const std::string			PlatformConfig::MAX_THREADS_ELEMENT_NAME = "MaxThreads";
const std::string			PlatformConfig::MULTITHREAD_BRANCHES_ELEMENT_NAME = "MultithreadBranches";
	
		
// PlatformConfig member functions

PlatformConfig::PlatformConfig(void)
	: ConfigManager(DEFAULT_CONFIG_FILE),
	m_vocab_mgr(), m_codec_factory(m_vocab_mgr), 
	m_protocol_factory(m_vocab_mgr), m_database_mgr(m_vocab_mgr),
	m_reaction_engine(m_vocab_mgr, m_codec_factory, m_protocol_factory, m_database_mgr),
	m_service_mgr(m_vocab_mgr, *this), m_user_mgr_ptr(new UserManager),
	m_debug_mode(false)
{
	setLogger(PION_GET_LOGGER("pion.server.PlatformConfig"));
}

void PlatformConfig::openConfigFile(void)
{
	std::lock_guard<std::mutex> platform_lock(m_mutex);

	// just return if it's already open
	if (configIsOpen())
		return;
	
	// open the file and find the "config" root element
	ConfigManager::openConfigFile();

	// check for run as user/group in config file
	ConfigManager::runAsUserGroup();

	#if defined(PION_USE_LOG4CXX) || defined(PION_USE_LOG4CPLUS) || defined(PION_USE_LOG4CPP)
	// configure logging using LogConfig file (if defined)
	if (ConfigManager::getConfigOption(LOG_CONFIG_ELEMENT_NAME, m_log_config_file,
									   m_config_node_ptr->children))
	{
		// initialize logging config using the file specified
		m_log_config_file = ConfigManager::resolveRelativePath(m_log_config_file);
		PION_LOG_CONFIG(m_log_config_file);
	} else {
		m_log_config_file.erase();
	}
	#endif

	#if defined(PION_USE_LOG4CPLUS)
	// Start caching log events, so that even log events occurring during configuration will be available.
	PionLogAppenderPtr appender(new CircularBufferAppender);
	appender->setName("CircularBufferAppender");
	log4cplus::Logger::getRoot().addAppender(appender);
	#endif

	// Step through plugin path definitions
	m_plugin_paths.clear();
	std::string plugin_path;
	xmlNodePtr path_node = m_config_node_ptr->children;
	if (ConfigManager::findConfigNodeByName(PLUGIN_PATH_ELEMENT_NAME, path_node) != NULL) 
		PionPlugin::resetPluginDirectories();
	while ( (path_node = ConfigManager::findConfigNodeByName(PLUGIN_PATH_ELEMENT_NAME, path_node)) != NULL)
	{
		// get the plug-in path value
		xmlChar *xml_char_ptr = xmlNodeGetContent(path_node);
		if (xml_char_ptr == NULL || xml_char_ptr[0]=='\0') {
			if (xml_char_ptr != NULL)
				xmlFree(xml_char_ptr);
			throw EmptyPluginPathException(getConfigFile());
		}
		plugin_path = reinterpret_cast<char*>(xml_char_ptr);
		xmlFree(xml_char_ptr);
		
		// add the plug-in path (only warn if directory not found)
		try {
			plugin_path = ConfigManager::resolveRelativePath(plugin_path);
			PionPlugin::addPluginDirectory(plugin_path);
			m_plugin_paths.push_back(plugin_path);
		} catch (PionPlugin::DirectoryNotFoundException& e) {
			PION_LOG_WARN(m_logger, e.what());
		}
		
		// step to the next plug-in path
		path_node = path_node->next;
	}
	
	// get the VocabularyManager config file
	std::string config_file;
	if (! ConfigManager::getConfigOption(VOCABULARY_CONFIG_ELEMENT_NAME, config_file,
										 m_config_node_ptr->children))
		throw MissingVocabularyConfigException(getConfigFile());
	
	// open the VocabularyManager configuration
	m_vocab_mgr.setConfigFile(ConfigManager::resolveRelativePath(config_file));
	m_vocab_mgr.openConfigFile();
	
	// get the DataDirectory config parameter
	if (! ConfigManager::getConfigOption(DATA_DIRECTORY_ELEMENT_NAME, m_data_directory,
										 m_config_node_ptr->children))
		throw MissingDataDirectoryException(getConfigFile());
	m_data_directory = ConfigManager::resolveRelativePath(m_data_directory);

	// get the DebugMode config parameter
	std::string debug_str;
	if (ConfigManager::getConfigOption(DEBUG_MODE_ELEMENT_NAME, debug_str,
		m_config_node_ptr->children))
	{
		m_debug_mode = (debug_str == "true");
		if (m_debug_mode) {
			PION_LOG_WARN(m_logger, "Debugging mode enabled - do not use in production!");
		}
	} else {
		m_debug_mode = false;
	}

	// get the CodecFactory config file
	if (! ConfigManager::getConfigOption(CODEC_CONFIG_ELEMENT_NAME, config_file,
										 m_config_node_ptr->children))
		throw MissingCodecConfigException(getConfigFile());
	
	// open the CodecFactory configuration
	m_codec_factory.setConfigFile(ConfigManager::resolveRelativePath(config_file));
	m_codec_factory.setDataDirectory(m_data_directory);
	m_codec_factory.setDebugMode(m_debug_mode);
	m_codec_factory.openConfigFile();
	
	// get the ProtocolFactory config file
	if (! ConfigManager::getConfigOption(PROTOCOL_CONFIG_ELEMENT_NAME, config_file,
										 m_config_node_ptr->children))
		throw MissingProtocolConfigException(getConfigFile());
	
	// open the ProtocolFactory configuration
	m_protocol_factory.setConfigFile(ConfigManager::resolveRelativePath(config_file));
	m_protocol_factory.setDataDirectory(m_data_directory);
	m_protocol_factory.setDebugMode(m_debug_mode);
	m_protocol_factory.openConfigFile();

	// make sure that the directory exists
	if (! boost::filesystem::exists(m_data_directory) )
		throw DirectoryNotFoundException(m_data_directory);
	if (! boost::filesystem::is_directory(m_data_directory) )
		throw NotADirectoryException(m_data_directory);

	// get the DatabaseManager config file
	if (! ConfigManager::getConfigOption(DATABASE_CONFIG_ELEMENT_NAME, config_file,
										 m_config_node_ptr->children))
		throw MissingDatabaseConfigException(getConfigFile());
	
	// open the DatabaseManager configuration
	m_database_mgr.setConfigFile(ConfigManager::resolveRelativePath(config_file));
	m_database_mgr.setDataDirectory(m_data_directory);
	m_database_mgr.setDebugMode(m_debug_mode);
	m_database_mgr.openConfigFile();
	
	// get the ReactionEngine config file
	if (! ConfigManager::getConfigOption(REACTOR_CONFIG_ELEMENT_NAME, config_file,
										 m_config_node_ptr->children))
		throw MissingReactorConfigException(getConfigFile());
	
	// get the ReactionEngine (advanced/hidden) configuration parameters
	xmlNodePtr reaction_engine_node =
		ConfigManager::findConfigNodeByName(REACTION_ENGINE_ELEMENT_NAME, m_config_node_ptr->children);
	if (reaction_engine_node != NULL) {
		// get MaxThreads setting (if it is defined)
		std::string max_threads_str;
		if (ConfigManager::getConfigOption(MAX_THREADS_ELEMENT_NAME, max_threads_str,
										   reaction_engine_node->children))
		{
			const boost::uint32_t max_threads = boost::lexical_cast<boost::uint32_t>(max_threads_str);
			if (max_threads == 0) {
				PION_LOG_ERROR(m_logger, "Platform config has invalid MaxThreads setting for ReactionEngine (using default)");
			} else {
				PION_LOG_INFO(m_logger, "Setting ReactionEngine MaxThreads to " << max_threads);
				m_reaction_engine.setNumThreads(max_threads);
			}
		}

		// get MultithreadBranches setting (if it is defined)
		std::string mt_branches_str;
		if (ConfigManager::getConfigOption(MULTITHREAD_BRANCHES_ELEMENT_NAME,
										   mt_branches_str,
										   reaction_engine_node->children))
		{
			if (mt_branches_str == "true") {
				PION_LOG_INFO(m_logger, "Enabling multithreaded branches in ReactionEngine");
				m_reaction_engine.setMultithreadBranches(true);
			} else if (mt_branches_str == "false") {
				PION_LOG_INFO(m_logger, "Disabling multithreaded branches in ReactionEngine");
				m_reaction_engine.setMultithreadBranches(false);
			} else {
				PION_LOG_ERROR(m_logger, "Platform config has invalid MultithreadBranches setting for ReactionEngine (using default)");
			}
		}
	}
	
	// get the ServiceManager (advanced/hidden) configuration parameters
	xmlNodePtr service_manager_node =
		ConfigManager::findConfigNodeByName(SERVICE_MANAGER_ELEMENT_NAME, m_config_node_ptr->children);
	if (service_manager_node != NULL) {
		// get MaxThreads setting (if it is defined)
		std::string max_threads_str;
		if (ConfigManager::getConfigOption(MAX_THREADS_ELEMENT_NAME, max_threads_str,
										   service_manager_node->children))
		{
			const boost::uint32_t max_threads = boost::lexical_cast<boost::uint32_t>(max_threads_str);
			if (max_threads == 0) {
				PION_LOG_ERROR(m_logger, "Platform config has invalid MaxThreads setting for ServiceManager (using default)");
			} else {
				PION_LOG_INFO(m_logger, "Setting ServiceManager MaxThreads to " << max_threads);
				m_service_mgr.setNumThreads(max_threads);
			}
		}
	}
	
	// open the ReactionEngine configuration
	m_reaction_engine.setConfigFile(ConfigManager::resolveRelativePath(config_file));
	m_reaction_engine.setDataDirectory(m_data_directory);
	m_reaction_engine.setDebugMode(m_debug_mode);
	m_reaction_engine.openConfigFile();
	
	// get the ServiceManager config file
	if (! ConfigManager::getConfigOption(SERVICE_CONFIG_ELEMENT_NAME, config_file,
										 m_config_node_ptr->children))
		throw MissingServiceConfigException(getConfigFile());
	
	// open the ServiceManager configuration
	m_service_mgr.setConfigFile(ConfigManager::resolveRelativePath(config_file));
	m_service_mgr.setDataDirectory(m_data_directory);
	m_service_mgr.setDebugMode(m_debug_mode);
	m_service_mgr.openConfigFile();

	// get the UserManager config file
	if (! ConfigManager::getConfigOption(USER_CONFIG_ELEMENT_NAME, config_file,
										 m_config_node_ptr->children))
		throw MissingUserConfigException(getConfigFile());

	// open the UserManager configuration
	m_user_mgr_ptr->setConfigFile(ConfigManager::resolveRelativePath(config_file));
	m_user_mgr_ptr->setDataDirectory(m_data_directory);
	m_user_mgr_ptr->setDebugMode(m_debug_mode);
	m_user_mgr_ptr->openConfigFile();

	PION_LOG_INFO(m_logger, "Loaded platform configuration file: " << m_config_file);
}

void PlatformConfig::writeConfigXML(std::ostream& out) const
{
	ConfigManager::writeBeginPionConfigXML(out);
	
	out << "\t<" << VERSION_ELEMENT_NAME << '>' << PION_VERSION
		<< "</" << VERSION_ELEMENT_NAME << '>' << std::endl
		<< "\t<" << DEBUG_MODE_ELEMENT_NAME << '>' << (m_debug_mode ? "true" : "false")
		<< "</" << DEBUG_MODE_ELEMENT_NAME << '>' << std::endl
		<< "\t<" << PLATFORM_CONFIG_ELEMENT_NAME << '>' << getConfigFile()
		<< "</" << PLATFORM_CONFIG_ELEMENT_NAME << '>' << std::endl
		<< "\t<" << VOCABULARY_CONFIG_ELEMENT_NAME << '>' << m_vocab_mgr.getConfigFile()
		<< "</" << VOCABULARY_CONFIG_ELEMENT_NAME << '>' << std::endl
		<< "\t<" << CODEC_CONFIG_ELEMENT_NAME << '>' << m_codec_factory.getConfigFile()
		<< "</" << CODEC_CONFIG_ELEMENT_NAME << '>' << std::endl
		<< "\t<" << PROTOCOL_CONFIG_ELEMENT_NAME << '>' << m_protocol_factory.getConfigFile()
		<< "</" << PROTOCOL_CONFIG_ELEMENT_NAME << '>' << std::endl
		<< "\t<" << DATABASE_CONFIG_ELEMENT_NAME << '>' << m_database_mgr.getConfigFile()
		<< "</" << DATABASE_CONFIG_ELEMENT_NAME << '>' << std::endl
		<< "\t<" << REACTOR_CONFIG_ELEMENT_NAME << '>' << m_reaction_engine.getConfigFile()
		<< "</" << REACTOR_CONFIG_ELEMENT_NAME << '>' << std::endl
		<< "\t<" << SERVICE_CONFIG_ELEMENT_NAME << '>' << m_service_mgr.getConfigFile()
		<< "</" << SERVICE_CONFIG_ELEMENT_NAME << '>' << std::endl
		<< "\t<" << USER_CONFIG_ELEMENT_NAME << '>' << m_user_mgr_ptr->getConfigFile()
		<< "</" << USER_CONFIG_ELEMENT_NAME << '>' << std::endl
		<< "\t<" << LOG_CONFIG_ELEMENT_NAME << '>' << getLogConfigFile()
		<< "</" << LOG_CONFIG_ELEMENT_NAME << '>' << std::endl
		<< "\t<" << VOCABULARY_PATH_ELEMENT_NAME << '>' << m_vocab_mgr.getVocabularyPath()
		<< "</" << VOCABULARY_PATH_ELEMENT_NAME << '>' << std::endl
		<< "\t<" << DATA_DIRECTORY_ELEMENT_NAME << '>' << m_data_directory
		<< "</" << DATA_DIRECTORY_ELEMENT_NAME << '>' << std::endl;
	
	std::unique_lock<std::mutex> platform_lock(m_mutex);
	for (std::vector<std::string>::const_iterator path_it = m_plugin_paths.begin();
		 path_it != m_plugin_paths.end(); ++path_it)
	{
		out << "\t<" << PLUGIN_PATH_ELEMENT_NAME << '>' << *path_it
			<< "</" << PLUGIN_PATH_ELEMENT_NAME << '>' << std::endl;
	}
	platform_lock.unlock();
	
	ConfigManager::writeEndPionConfigXML(out);
}

	
}	// end namespace server
}	// end namespace pion

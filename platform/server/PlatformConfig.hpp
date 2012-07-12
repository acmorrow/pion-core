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

#ifndef __PION_PLATFORMCONFIG_HEADER__
#define __PION_PLATFORMCONFIG_HEADER__

#include <string>
#include <vector>
#include <libxml/tree.h>
#include <boost/cstdint.hpp>
#include <mutex>
#include <pion/PionConfig.hpp>
#include <pion/PionException.hpp>
#include <pion/platform/ConfigManager.hpp>
#include <pion/platform/VocabularyManager.hpp>
#include <pion/platform/ReactionEngine.hpp>
#include <pion/platform/CodecFactory.hpp>
#include <pion/platform/ProtocolFactory.hpp>
#include <pion/platform/DatabaseManager.hpp>
#include "ServiceManager.hpp"
#include "UserManager.hpp"


namespace pion {		// begin namespace pion
namespace server {		// begin namespace server (Pion Server)


///
/// PlatformConfig: manages system configuration for the Pion Platform
///
class PION_SERVER_API PlatformConfig :
	public pion::platform::ConfigManager
{
public:
	
	/// exception thrown if the config file does not contain a VocabularyConfig element
	class MissingVocabularyConfigException : public PionException {
	public:
		MissingVocabularyConfigException(const std::string& config_file)
			: PionException("Platform configuration file does not define a Vocabulary configuration file: ", config_file) {}
	};
	
	/// exception thrown if the config file does not contain a CodecConfig element
	class MissingCodecConfigException : public PionException {
	public:
		MissingCodecConfigException(const std::string& config_file)
			: PionException("Platform configuration file does not define a Codec configuration file: ", config_file) {}
	};
	
	/// exception thrown if the config file does not contain a ProtocolConfig element
	class MissingProtocolConfigException : public PionException {
	public:
		MissingProtocolConfigException(const std::string& config_file)
			: PionException("Platform configuration file does not define a Protocol configuration file: ", config_file) {}
	};
	
	/// exception thrown if the config file does not contain a DatabaseConfig element
	class MissingDatabaseConfigException : public PionException {
	public:
		MissingDatabaseConfigException(const std::string& config_file)
			: PionException("Platform configuration file does not define a Database configuration file: ", config_file) {}
	};
	
	/// exception thrown if the config file does not contain a ReactorConfig element
	class MissingReactorConfigException : public PionException {
	public:
		MissingReactorConfigException(const std::string& config_file)
			: PionException("Platform configuration file does not define a Reactor configuration file: ", config_file) {}
	};
	
	/// exception thrown if the config file does not contain a ServiceConfig element
	class MissingServiceConfigException : public PionException {
	public:
		MissingServiceConfigException(const std::string& config_file)
			: PionException("Platform configuration file does not define a Service configuration file: ", config_file) {}
	};
	
	/// exception thrown if the config file does not contain a UserConfig element
	class MissingUserConfigException : public PionException {
	public:
		MissingUserConfigException(const std::string& config_file)
			: PionException("Platform configuration file does not define a User configuration file: ", config_file) {}
	};
	
	/// exception thrown if the config file contains an empty PluginPath definition
	class EmptyPluginPathException : public PionException {
	public:
		EmptyPluginPathException(const std::string& config_file)
			: PionException("Platform configuration includes an empty plug-in path: ", config_file) {}
	};

	/// exception thrown if the config file does not contain a DataDirectory element
	class MissingDataDirectoryException : public PionException {
	public:
		MissingDataDirectoryException(const std::string& config_file)
			: PionException("Platform configuration file does not define a data directory: ", config_file) {}
	};
	
	/// exception thrown if the directory configured is not found
	class DirectoryNotFoundException : public PionException {
	public:
		DirectoryNotFoundException(const std::string& dir)
			: PionException("Platform configuration DataDirectory not found: ", dir) {}
	};
	
	/// exception thrown if the directory configuration option is not a directory
	class NotADirectoryException : public PionException {
	public:
		NotADirectoryException(const std::string& dir)
			: PionException("Platform configuration DataDirectory is not a directory: ", dir) {}
	};
	
	/// constructs a new PlatformConfig instance
	PlatformConfig(void);
	
	/// virtual destructor
	virtual ~PlatformConfig() {
		// make sure that the ReactionEngine stops before the ServiceManager
		// This will cleanly terminate any temporary Reactor connections first
		m_reaction_engine.stop();
		// stop threads and release service plugins in ServiceManager because
		// they use other plugins types; otherwise, code referenced by them
		// may be released first which can potentitally cause crashes
		m_service_mgr.shutdown();
		// release reactor plugins next for same reasons
		m_reaction_engine.shutdown();
	}
	
	/// opens an existing platform config file
	virtual void openConfigFile(void);
	
	/**
	 * writes the platform configuration to an output stream (as XML)
	 *
	 * @param out the ostream to write the configuration into
	 */
	virtual void writeConfigXML(std::ostream& out) const;

	/// returns a reference to the global VocabularyManager
	inline pion::platform::VocabularyManager& getVocabularyManager(void) { return m_vocab_mgr; }
	
	/// returns a reference to the global CodecFactory
	inline pion::platform::CodecFactory& getCodecFactory(void) { return m_codec_factory; }
	
	/// returns a reference to the global ProtocolFactory
	inline pion::platform::ProtocolFactory& getProtocolFactory(void) { return m_protocol_factory; }

	/// returns a reference to the global DatabaseManager
	inline pion::platform::DatabaseManager& getDatabaseManager(void) { return m_database_mgr; }

	/// returns a reference to the global ReactionEngine
	inline pion::platform::ReactionEngine& getReactionEngine(void) { return m_reaction_engine; }

	/// returns a const reference to the global ReactionEngine
	inline const pion::platform::ReactionEngine& getReactionEngine(void) const { return m_reaction_engine; }

	/// returns a reference to the global ServiceManager
	inline ServiceManager& getServiceManager(void) { return m_service_mgr; }

	/// returns a pointer to the global UserManager
	inline const UserManagerPtr getUserManagerPtr(void) const { return m_user_mgr_ptr; }

	/// returns the name of the logging configuration file
	inline const std::string& getLogConfigFile(void) const { return m_log_config_file; }

	/// returns the directory in which data files are stored
	inline const std::string& getDataDirectory(void) const { return m_data_directory; }
	
	/// returns true if pion is running in "debug mode"
	inline bool getDebugMode(void) const { return m_debug_mode; }

	
private:

	/// default name of the vocabulary config file
	static const std::string		DEFAULT_CONFIG_FILE;

	/// name of the version element for Pion XML config files
	static const std::string		VERSION_ELEMENT_NAME;

	/// name of the platform config element for Pion XML config files
	static const std::string		PLATFORM_CONFIG_ELEMENT_NAME;

	/// name of the vocabulary config element for Pion XML config files
	static const std::string		VOCABULARY_CONFIG_ELEMENT_NAME;
	
	/// name of the codec config element for Pion XML config files
	static const std::string		CODEC_CONFIG_ELEMENT_NAME;
	
	/// name of the protocol config element for Pion XML config files
	static const std::string		PROTOCOL_CONFIG_ELEMENT_NAME;
	
	/// name of the database config element for Pion XML config files
	static const std::string		DATABASE_CONFIG_ELEMENT_NAME;
	
	/// name of the reactor config element for Pion XML config files
	static const std::string		REACTOR_CONFIG_ELEMENT_NAME;
	
	/// name of the service config element for Pion XML config files
	static const std::string		SERVICE_CONFIG_ELEMENT_NAME;

	/// name of the user config element for Pion XML config files
	static const std::string		USER_CONFIG_ELEMENT_NAME;

	/// name of the log config file element for Pion XML config files
	static const std::string		LOG_CONFIG_ELEMENT_NAME;

	/// name of the vocabulary path element for Pion XML config files
	static const std::string		VOCABULARY_PATH_ELEMENT_NAME;
	
	/// name of the plug-in path element for Pion XML config files
	static const std::string		PLUGIN_PATH_ELEMENT_NAME;

	/// name of the data directory element for Pion XML config files
	static const std::string		DATA_DIRECTORY_ELEMENT_NAME;

	/// name of the debug mode element for Pion XML config files
	static const std::string		DEBUG_MODE_ELEMENT_NAME;

	/// name of the reaction engine element for Pion XML config files
	static const std::string		REACTION_ENGINE_ELEMENT_NAME;
	
	/// name of the service manager element for Pion XML config files
	static const std::string		SERVICE_MANAGER_ELEMENT_NAME;
	
	/// name of the max threads element for Pion XML config files
	static const std::string		MAX_THREADS_ELEMENT_NAME;

	/// name of the multithread branches element for Pion XML config files
	static const std::string		MULTITHREAD_BRANCHES_ELEMENT_NAME;

	
	/// global manager of Vocabularies
	pion::platform::VocabularyManager		m_vocab_mgr;
	
	/// global manager of Codecs
	pion::platform::CodecFactory			m_codec_factory;
	
	/// global manager of Protocols
	pion::platform::ProtocolFactory			m_protocol_factory;

	/// global manager of Databases
	pion::platform::DatabaseManager			m_database_mgr;
	
	/// global manager of Reactors
	pion::platform::ReactionEngine			m_reaction_engine;
	
	/// global manager of PlatformServices
	ServiceManager							m_service_mgr;

	/// global manager of Users
	UserManagerPtr							m_user_mgr_ptr;

	/// name of the logging configuration file
	std::string								m_log_config_file;
	
	/// directory in which database files are stored
	std::string								m_data_directory;
	
	/// collection of plugin paths that were successfully added
	std::vector<std::string>				m_plugin_paths;
	
	/// true if pion is running in "debug mode"
	bool									m_debug_mode;
	
	/// mutex to make class thread-safe
	mutable std::mutex					m_mutex;	
};


}	// end namespace server
}	// end namespace pion

#endif

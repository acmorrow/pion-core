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

#ifndef __PION_PLATFORMPLUGIN_HEADER__
#define __PION_PLATFORMPLUGIN_HEADER__

#include <string>
#include <libxml/tree.h>
#include <boost/noncopyable.hpp>
#include <pion/PionConfig.hpp>


namespace pion {		// begin namespace pion
namespace platform {	// begin namespace platform (Pion Platform Library)


// forward declarations
class Vocabulary;
class ConfigManager;	
class CodecFactory;
class DatabaseManager;
class ProtocolFactory;
class Codec;
class Database;
class Protocol;
typedef std::shared_ptr<Codec> CodecPtr;
typedef std::shared_ptr<Database> DatabasePtr;
typedef std::shared_ptr<Protocol> ProtocolPtr;


///
/// PlatformPlugin: interface class extended by all Pion Platform plug-ins
///
class PION_PLATFORM_API PlatformPlugin
	: private boost::noncopyable
{
public:

	/// constructs a new PlatformPlugin object
	PlatformPlugin(void) :
		m_config_mgr_ptr(NULL), m_codec_factory_ptr(NULL),
		m_database_mgr_ptr(NULL), m_protocol_factory_ptr(NULL)
	{}
	
	/// virtual destructor: this class is meant to be extended
	virtual ~PlatformPlugin() {}
	
	/**
	 * sets configuration parameters for this plug-in
	 *
	 * @param v the Vocabulary that this plug-in will use to describe Terms
	 * @param config_ptr pointer to a list of XML nodes containing plug-in
	 *                   configuration parameters
	 */
	virtual void setConfig(const Vocabulary& v, const xmlNodePtr config_ptr);

	/**
	 * this updates the Vocabulary information used by this plug-in;
	 * it should be called whenever the global Vocabulary is updated
	 *
	 * @param v the Vocabulary that this plug-in will use to describe Terms
	 */
	virtual void updateVocabulary(const Vocabulary& v);

	
	/// sets the unique identifier for this plug-in
	inline void setId(const std::string& plugin_id) { m_plugin_id = plugin_id; }
	
	/// returns the unique identifier for this plug-in
	inline const std::string& getId(void) const { return m_plugin_id; }
	
	/// sets the descriptive name for this plug-in
	inline void setName(const std::string& plugin_name) { m_plugin_name = plugin_name; }
	
	/// returns the descriptive name for this plug-in
	inline const std::string& getName(void) const { return m_plugin_name; }
	
	/// sets the descriptive comment for this plug-in
	inline void setComment(const std::string& plugin_comment) { m_plugin_comment = plugin_comment; }
	
	/// returns the descriptive comment for this plug-in
	inline const std::string& getComment(void) const { return m_plugin_comment; }

	/// sets the ConfigManager that will used by the plugin to access Codecs
	inline void setConfigManager(const ConfigManager& mgr) { m_config_mgr_ptr = & mgr; }
	
	/// sets the CodecFactory that will used by the plugin to access Codecs
	inline void setCodecFactory(CodecFactory& factory) { m_codec_factory_ptr = & factory; }
	
	/// sets the DatabaseManager that will used by the plugin to access Databases
	inline void setDatabaseManager(DatabaseManager& mgr) { m_database_mgr_ptr = & mgr; }
	
	/// sets the ProtocolFactory that will used by the plugin to access Protocols
	inline void setProtocolFactory(ProtocolFactory& factory) { m_protocol_factory_ptr = & factory; }

	/// returns the ConfigManager associated with this plugin
	inline const ConfigManager& getConfigManager(void) const {
		PION_ASSERT(m_config_mgr_ptr != NULL);
		return *m_config_mgr_ptr;
	}
	
	/**
	 * retrieves a new Codec instance
	 *
	 * @param plugin_ptr smart pointer which will be assigned to the new instance
	 * @param plugin_id unique identifier for the plugin to retrieve
	 *
	 * @return true if a matching plugin was found, otherwise false
	 */
	bool getCodecPlugin(CodecPtr& plugin_ptr, const std::string& plugin_id) const;
	
	/**
	 * retrieves a new Database instance
	 *
	 * @param plugin_ptr smart pointer which will be assigned to the new instance
	 * @param plugin_id unique identifier for the plugin to retrieve
	 *
	 * @return true if a matching plugin was found, otherwise false
	 */
	bool getDatabasePlugin(DatabasePtr& plugin_ptr, const std::string& plugin_id) const;

	/**
	 * retrieves a new Protocol instance
	 *
	 * @param plugin_ptr smart pointer which will be assigned to the new instance
	 * @param plugin_id unique identifier for the plugin to retrieve
	 *
	 * @return true if a matching plugin was found, otherwise false
	 */
	bool getProtocolPlugin(ProtocolPtr& plugin_ptr, const std::string& plugin_id) const;
	
	/// returns true if the unique identifier matches a known Codec plugin
	bool hasCodecPlugin(const std::string& plugin_id) const;

	/// returns true if the unique identifier matches a known Database plugin
	bool hasDatabasePlugin(const std::string& plugin_id) const;

	/// returns true if the unique identifier matches a known Protocol plugin
	bool hasProtocolPlugin(const std::string& plugin_id) const;


protected:
	
	/// protected copy function (use clone() instead)
	inline void copyPlugin(const PlatformPlugin& pp) {
		m_plugin_id = pp.m_plugin_id;
		m_plugin_name = pp.m_plugin_name;
		m_plugin_comment = pp.m_plugin_comment;
		m_config_mgr_ptr = pp.m_config_mgr_ptr;
		m_codec_factory_ptr = pp.m_codec_factory_ptr;
		m_database_mgr_ptr = pp.m_database_mgr_ptr;
		m_protocol_factory_ptr = pp.m_protocol_factory_ptr;
	}
	

private:
	
	/// name of the descriptive name element for Pion XML config files
	static const std::string		NAME_ELEMENT_NAME;
	
	/// name of the comment element for Pion XML config files
	static const std::string		COMMENT_ELEMENT_NAME;


	/// uniquely identifies this particular Plugin
	std::string						m_plugin_id;

	/// descriptive name for this Plugin
	std::string						m_plugin_name;

	/// descriptive comment for this Plugin
	std::string						m_plugin_comment;
	
	/// config manager object associated with this plugin
	const ConfigManager *			m_config_mgr_ptr;
	
	/// pointer to the CodecFactory, used by the plugin to access Codecs
	CodecFactory *					m_codec_factory_ptr;
	
	/// pointer to the DatabaseManager, used by the plugin to access Databases
	DatabaseManager *				m_database_mgr_ptr;

	/// pointer to the ProtocolFactory, used by the plugin to access NetworkProtocols
	ProtocolFactory *				m_protocol_factory_ptr;
};

	
}	// end namespace platform
}	// end namespace pion

#endif

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

#ifndef __PION_USERMANAGER_HEADER__
#define __PION_USERMANAGER_HEADER__

#include <functional>
#include <string>
#include <libxml/tree.h>
#include <boost/signal.hpp>
#include <pion/PionConfig.hpp>
#include <pion/PionException.hpp>
#include <pion/net/PionUser.hpp>
#include <pion/platform/ConfigManager.hpp>
#include "PlatformService.hpp"


namespace pion {		// begin namespace pion
namespace server {	// begin namespace server (Pion Server)


/**
 * UserManager : Manages a collection of platform Users
 */
class PION_SERVER_API UserManager :
	public pion::platform::ConfigManager,
	public pion::net::PionUserManager
{
public:

	/// exception thrown if Pion is built without support for OpenSSL
	class MissingOpenSSLException : public std::exception {
	public:
		virtual const char* what() const throw() {
			return "Missing OpenSSL library: User management is disabled!";
		}
	};
	
	/// exception thrown if the config file contains a User with an empty or missing identifier
	class MissingUserIdInConfigFileException : public PionException {
	public:
		MissingUserIdInConfigFileException(const std::string& config_file)
			: PionException("Users configuration file includes a User without a unique identifier: ", config_file) {}
	};

	/// exception thrown if a user_id parameter is found to be empty
	class EmptyUserIdException : public PionException {
	public:
		EmptyUserIdException()
			: PionException("The specified User identifier is empty.") {}
	};

	/// exception thrown if a request is made to add or update a User with a configuration with an empty or missing password
	class NoPasswordException : public PionException {
	public:
		NoPasswordException(const std::string& user_id)
			: PionException("The specified User configuration has an empty or missing password.  Specified User identifier: ", user_id) {}
	};

	/// exception thrown if a request is made to add a User when a User with the same ID has already been added
	class DuplicateUserException : public PionException {
	public:
		DuplicateUserException(const std::string& user_id)
			: PionException("A User already exists with the specified ID: ", user_id) {}
	};

	/// exception thrown if a User cannot be found
	class UserNotFoundException : public PionException {
	public:
		UserNotFoundException(const std::string& user_id)
			: PionException("No User found for identifier: ", user_id) {}
	};

	/// exception thrown if there is an error adding a User to the config file
	class AddUserConfigException : public PionException {
	public:
		AddUserConfigException(const std::string& config_file)
			: PionException("Unable to add a User to the configuration file: ", config_file) {}
	};
	
	/// exception thrown if there is an error updating a User in the config file
	class UpdateUserConfigException : public PionException {
	public:
		UpdateUserConfigException(const std::string& config_file)
			: PionException("Unable to update a User in the configuration file: ", config_file) {}
	};

	/// exception thrown if a User update failed
	class UserUpdateFailedException : public PionException {
	public:
		UserUpdateFailedException(const std::string& user_id)
			: PionException("Unable to update User with identifier: ", user_id) {}
	};

public:

	/// construct a new UserManager Instance
	UserManager();

	/// virtual destructor
	virtual ~UserManager() {}

	/// opens an existing User config file and loads the data it contains
	virtual void openConfigFile(void);

	/**
	 * writes the entire configuration tree to an output stream (as XML)
	 *
	 * @param out the ostream to write the configuration tree into
	 */
	virtual void writeConfigXML(std::ostream& out) const;

	/**
	 * writes the configuration data for a particular User (as XML)
	 *
	 * @param out the ostream to write the configuration tree into
	 * @param user_id unique identifier associated with the User
	 */
	bool writeConfigXML(std::ostream& out, const std::string& user_id) const;

	/**
	 * writes the Permission configuration data for a particular User (as XML)
	 *
	 * @param out the ostream to write the configuration tree into
	 * @param user_id unique identifier associated with the User
	 */
	bool writePermissionsXML(std::ostream& out, const std::string& user_id) const;

	/**
	 * uses a memory buffer to generate XML configuration data for a User
	 *
	 * @param user_id will get the unique identifier associated with the User
	 * @param buf pointer to a memory buffer containing configuration data
	 * @param len number of bytes available in the memory buffer
	 *
	 * @return xmlNodePtr XML configuration list for the User
	 */
	static xmlNodePtr createUserConfig(std::string& user_id, const char *buf, std::size_t len);

	/**
	 * adds a new managed User
	 *
	 * @param user_id unique identifier associated with the User
	 * @param config_ptr pointer to a list of XML nodes containing User
	 *                   configuration parameters 
	 *
	 * @return std::string the new User's unique identifier
	 */
	std::string addUser(const std::string& user_id, xmlNodePtr config_ptr);

	/**
	 * sets (updates) configuration parameters for a managed User
	 *
	 * @param user_id unique identifier associated with the User
	 * @param config_ptr pointer to a list of XML nodes containing User
	 *                   configuration parameters
	 */
	void setUserConfig(const std::string& user_id, xmlNodePtr config_ptr);

	/**
	 * removes a managed User
	 *
	 * @param user_id unique identifier associated with the User (username)
	 */
	virtual bool removeUser(const std::string& user_id);

	/**
	 * determines whether a User has permission to create a new configuration node
	 *
	 * @param user_from_request pointer to an existing User element node, as extracted from a request
	 * @param config_manager the plugin manager corresponding to the type of the configuration
	 * @param config_ptr pointer to the new configuration; if null, returns true only if the User has  
	 *                   permission for any configuration handled by the config manager
	 *
	 * @return true if the User has permission
	 */
	bool creationAllowed(
		const pion::net::PionUserPtr& user_from_request, 
		const pion::platform::ConfigManager& config_manager,
		const xmlNodePtr& config_ptr) const;

	/**
	 * determines whether a User has permission to update a configuration node
	 *
	 * @param user_from_request pointer to an existing User element node, as extracted from a request
	 * @param config_manager the plugin manager corresponding to the type of the configuration
	 * @param id unique identifier associated with an existing configuration node
	 * @param config_ptr pointer to the new configuration; if null, returns true only if the User has  
	 *                   permission for any configuration handled by the config manager
	 *
	 * @return true if the User has permission
	 */
	bool updateAllowed(
		const pion::net::PionUserPtr& user_from_request, 
		const pion::platform::ConfigManager& config_manager,
		const std::string& id,
		const xmlNodePtr& config_ptr) const;

	/**
	 * determines whether a User has permission to remove a configuration node or set of configuration nodes
	 *
	 * @param user_from_request pointer to an existing User element node, as extracted from a request
	 * @param config_manager the plugin manager corresponding to the type of the configuration
	 * @param id unique identifier associated with an existing configuration node or set of configuration nodes
	 *
	 * @return true if the User has permission
	 */
	bool removalAllowed(
		const pion::net::PionUserPtr& user_from_request, 
		const pion::platform::ConfigManager& config_manager,
		const std::string& id) const;

	/**
	 * determines whether a User has permission to use a plugin
	 *
	 * @param user_from_request pointer to an existing User element node, as extracted from a request
	 * @param config_manager the plugin manager corresponding to the plugin
	 * @param plugin_id unique identifier associated with an existing plugin
	 *
	 * @return true if the User has permission
	 */
	bool accessAllowed(
		const pion::net::PionUserPtr& user_from_request, 
		const pion::platform::ConfigManager& config_manager,
		const std::string& plugin_id) const;

	/**
	 * determines whether a User has permission to use a PlatformService
	 *
	 * @param user_from_request pointer to an existing User element node, as extracted from a request
	 * @param service the PlatformService
	 * @param id optional unique identifier specifying a subset of the PlatformService
	 *
	 * @return true if the User has permission
	 */
	bool accessAllowed(
		const pion::net::PionUserPtr& user_from_request, 
		const PlatformService& service,
		const std::string& id = "") const;


private:

	/**
	 * parses User configuration info from a XML config tree
	 * and uses it to update the User authentication manager
	 *
	 * @param user_id unique identifier associated with the User
	 * @param config_ptr pointer to a list of XML nodes containing User
	 *                   configuration parameters
	 * @param password_encrypted true if the password is encrypted; if not,
	 *                           the XML nodes will be updated with encrypted format
	 * @param new_user true if a new User is being created
	 *
	 * @return true if successful
	 */
	bool updateUserManager(const std::string& user_id, xmlNodePtr config_ptr,
		bool password_encrypted, bool new_user);

	/**
	 * sets configuration parameters for a User in the configuration file
	 *
	 * @param user_node_ptr pointer to the existing User element node
	 * @param config_ptr pointer to the new configuration parameters
	 *
	 * @return true if successful, false if there was an error
	 */
	bool setUserConfig(xmlNodePtr user_node_ptr, xmlNodePtr config_ptr);

	/**
	 * returns true if the User has administrator permission
	 *
	 * @param user_ptr pointer to PionUser record of a request
	 *
	 * @return true if the User has administrator permission
	 */
	bool isAdmin(const pion::net::PionUserPtr user_ptr) const;

	/**
	 * searches in the specified User's configuration for a Permission node whose "type" attribute has the specified value
	 *
	 * @param user_ptr pointer to PionUser record of a request
	 * @param permission_type the value that the "type" attribute should have
	 *
	 * @return xmlNodePtr pointer to an XML document node if found, otherwise NULL
	 */
	xmlNodePtr getPermissionNode(pion::net::PionUserPtr user_ptr, const std::string& permission_type) const;


private:

	/// default name of the User config file
	static const std::string			DEFAULT_CONFIG_FILE;

	/// name of the USER element for Pion XML config files
	static const std::string			USER_ELEMENT_NAME;

	/// name of the PASSWORD element for Pion XML config files
	static const std::string			PASSWORD_ELEMENT_NAME;

	/// name of the User Permission element for Pion XML config files
	static const std::string			USER_PERMISSION_ELEMENT_NAME;

	/// name of the permission type attribute for Pion XML config files
	static const std::string			PERMISSION_TYPE_ATTRIBUTE_NAME;

	/// type identifier for Administrator permission type
	static const std::string			ADMIN_PERMISSION_TYPE;

	/// mutex to make class thread-safe
	mutable boost::mutex				m_mutex;
};

/// data type for a UserManager pointer
typedef boost::shared_ptr<UserManager>	UserManagerPtr;


}	// end namespace server
}	// end namespace pion

#endif //__PION_USERMANAGER_HEADER__

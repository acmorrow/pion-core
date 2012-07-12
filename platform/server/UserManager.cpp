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

#include "UserManager.hpp"

namespace pion {		// begin namespace pion
namespace server {		// begin namespace server (Pion Server)


// static members of UserManager

const std::string			UserManager::DEFAULT_CONFIG_FILE = "users.xml";
const std::string			UserManager::USER_ELEMENT_NAME = "User";
const std::string			UserManager::PASSWORD_ELEMENT_NAME = "Password";
const std::string			UserManager::USER_PERMISSION_ELEMENT_NAME = "Permission";
const std::string			UserManager::PERMISSION_TYPE_ATTRIBUTE_NAME = "type";
const std::string			UserManager::ADMIN_PERMISSION_TYPE = "Admin";

// UserManager member functions

UserManager::UserManager(void)
	: ConfigManager(DEFAULT_CONFIG_FILE)
{
	setLogger(PION_GET_LOGGER("pion.server.UserManager"));
}

bool UserManager::updateUserManager(const std::string& user_id,
	xmlNodePtr config_ptr, bool password_encrypted, bool new_user)
{
	bool ret=false;
	
#ifdef PION_HAVE_SSL
	std::string password;

	// try to find the user's password in the XML config
	xmlNodePtr password_node = findConfigNodeByName(PASSWORD_ELEMENT_NAME, config_ptr);
	if (password_node == NULL) 
		throw NoPasswordException(user_id);	// not found

	// get password value
	xmlChar *xml_char_ptr = xmlNodeGetContent(password_node);
	if (xml_char_ptr == NULL)
		throw NoPasswordException(user_id);	// no password content
	password = reinterpret_cast<char*>(xml_char_ptr);
	xmlFree(xml_char_ptr);
	if (password.empty())
		throw NoPasswordException(user_id);	// no password content

	if (password_encrypted) {
		// password is already encrypted
		if (new_user) {
			// add new user
			ret = PionUserManager::addUserHash(user_id, password);
			if (!ret) 
				throw DuplicateUserException(user_id);
		} else {
			// update existing user
			ret = PionUserManager::updateUserHash(user_id, password);
		}
	} else {
		// password is not encrypted
		if (new_user) {
			// add new user
			ret = PionUserManager::addUser(user_id, password);
			if (!ret) 
				throw DuplicateUserException(user_id);
		} else {
			// update existing user
			
			// check if the password matches the existing encrypted value
			// this may happen if the user is being updated, but the password
			// is not modified (the client will send the existing encrypted password)
			if (password == PionUserManager::getUser(user_id)->getPassword()) {
				// it does match -> the password is not being changed!
				password_encrypted = true;
				// no need to update the user!
				//ret = PionUserManager::updateUserHash(user_id, password);
			} else {
				// it doesn't match -> assume that the password IS being changed
				ret = PionUserManager::updateUser(user_id, password);
			}
		}
		
		// update password in config tree so that it is saved in secure format
		// re-check flag since it might be changed
		if (! password_encrypted)	{
			password = PionUserManager::getUser(user_id)->getPassword();
			xmlNodeSetContent(password_node,  reinterpret_cast<const xmlChar*>(password.c_str()));
		}
	}
#endif

	return ret;
}

bool UserManager::setUserConfig(xmlNodePtr user_node_ptr, xmlNodePtr config_ptr)
{
	// create a copy of the user config parameters
	xmlNodePtr user_config_copy = xmlCopyNodeList(config_ptr);
	if (user_config_copy == NULL)
		return false;

	// nuke the existing user config
	xmlFreeNodeList(user_node_ptr->children);
	user_node_ptr->children = NULL;
	
	// add the new user config elements
	if (xmlAddChildList(user_node_ptr, user_config_copy) == NULL) {
		xmlFreeNodeList(user_config_copy);
		return false;
	}
	
	return true;
}

void UserManager::openConfigFile(void)
{
	std::lock_guard<std::mutex> users_lock(m_mutex);

	// just return if it's already open
	if (configIsOpen())
		return;

	// open the file and find the "config" root element
	ConfigManager::openConfigFile();

	// some strings that get re-used a bunch
	std::string user_id;
	
#ifndef PION_HAVE_SSL
	// keeps track of whether or not we logged a "missing openssl" error
	bool logged_error_msg = false;
#endif

	// step through user configurations
	xmlNodePtr user_node = m_config_node_ptr->children;
	while ( (user_node = ConfigManager::findConfigNodeByName(USER_ELEMENT_NAME, user_node)) != NULL)
	{
#ifdef PION_HAVE_SSL
		// get the unique identifier for the User (username)
		if (! getNodeId(user_node, user_id))
			throw MissingUserIdInConfigFileException(getConfigFile());

		// add user to authentication manager
		updateUserManager(user_id, user_node->children, true, true);
#else
		if (! logged_error_msg) {
			PION_LOG_ERROR(m_logger, "Missing OpenSSL library: user management is disabled!");
			logged_error_msg = true;
		}
#endif

		// step to the next user definition
		user_node = user_node->next;
	}

	PION_LOG_INFO(m_logger, "Loaded Users configuration file: " << m_config_file);
}

void UserManager::writeConfigXML(std::ostream& out) const 
{
	std::lock_guard<std::mutex> users_lock(m_mutex);
	ConfigManager::writeConfigXML(out, m_config_node_ptr, true);
}

bool UserManager::writeConfigXML(std::ostream& out,
									const std::string& user_id) const
{
	// find the plug-in element in the XML config document
	std::lock_guard<std::mutex> users_lock(m_mutex);
	xmlNodePtr user_node = findConfigNodeByAttr(USER_ELEMENT_NAME,
		ID_ATTRIBUTE_NAME,
		user_id,
		m_config_node_ptr->children);

	if (user_node == NULL)
		return false;	// not found

	// found it
	ConfigManager::writeBeginPionConfigXML(out);
	ConfigManager::writeConfigXML(out, user_node, false);
	ConfigManager::writeEndPionConfigXML(out);

	return true;
}

std::string UserManager::addUser(const std::string& user_id, xmlNodePtr config_ptr)
{
#ifdef PION_HAVE_SSL

	std::lock_guard<std::mutex> users_lock(m_mutex);

	// Sanity check
	if (user_id.empty())
		throw EmptyUserIdException();

	// process new user configuration
	if (updateUserManager(user_id, config_ptr, false, true)) {
		// add it to the XML scheme

		// create a new node for the User and add it to the XML config document
		xmlNodePtr new_node = xmlNewNode(NULL, reinterpret_cast<const xmlChar*>(USER_ELEMENT_NAME.c_str()));
		if (new_node == NULL)
			throw AddUserConfigException(getConfigFile());
		if ((new_node=xmlAddChild(m_config_node_ptr, new_node)) == NULL) {
			xmlFreeNode(new_node);
			throw AddUserConfigException(getConfigFile());
		}

		// set the id attribute for the User element
		if (xmlNewProp(new_node, reinterpret_cast<const xmlChar*>(ID_ATTRIBUTE_NAME.c_str()),
			reinterpret_cast<const xmlChar*>(user_id.c_str())) == NULL)
			throw AddUserConfigException(getConfigFile());

		// update the configuration parameters (if any)
		if (config_ptr != NULL) {
			if (! setUserConfig(new_node, config_ptr))
				throw AddUserConfigException(getConfigFile());
		}
	} else {
		throw UserUpdateFailedException(user_id);
	}

	// save the new XML config file
	saveConfigFile();
#else
	throw MissingOpenSSLException();
#endif
	
	return user_id;
}

bool UserManager::writePermissionsXML(std::ostream& out, const std::string& user_id) const
{
	// find the User node in users.xml
	std::lock_guard<std::mutex> users_lock(m_mutex);
	xmlNodePtr user_node = findConfigNodeByAttr(USER_ELEMENT_NAME,
		ID_ATTRIBUTE_NAME,
		user_id,
		m_config_node_ptr->children);

	if (user_node == NULL)
		return false;	// not found

	// found it

	ConfigManager::writeBeginPionConfigXML(out);
	out << '<' << USER_ELEMENT_NAME << ' ' << ID_ATTRIBUTE_NAME
		<< "=\"" << user_id << "\">" << std::endl;

	// find and output all <Permission> nodes for the user
	xmlNodePtr permission_node = user_node->children;
	while ((permission_node = ConfigManager::findConfigNodeByName(USER_PERMISSION_ELEMENT_NAME, permission_node)) != NULL) {
		ConfigManager::writeConfigXML(out, permission_node, false);
		permission_node = permission_node->next;
	}

	out << "</" << USER_ELEMENT_NAME << '>' << std::endl;
	ConfigManager::writeEndPionConfigXML(out);

	return true;
}

void UserManager::setUserConfig(const std::string& user_id, xmlNodePtr config_ptr)
{
	// Sanity check
	if (user_id.empty())
		throw EmptyUserIdException();

#ifdef PION_HAVE_SSL
	// Find existing user configuration
	std::lock_guard<std::mutex> users_lock(m_mutex);
	xmlNodePtr user_node = findConfigNodeByAttr(USER_ELEMENT_NAME,
		ID_ATTRIBUTE_NAME,
		user_id,
		m_config_node_ptr->children);
		
	if (user_node == NULL)
		throw UserNotFoundException(user_id);	// not found

	// update the user authentication configuration
	updateUserManager(user_id, config_ptr, false, false);

	// update the configuration in the XML tree
	if (! setUserConfig(user_node, config_ptr))
		throw UpdateUserConfigException(getConfigFile());
		
	// save the new XML config file
	saveConfigFile();
#else
	throw MissingOpenSSLException();
#endif
}

bool UserManager::removeUser(const std::string& user_id)
{
	bool ret = false;

#ifdef PION_HAVE_SSL
	std::lock_guard<std::mutex> users_lock(m_mutex);
	ret = PionUserManager::removeUser(user_id);
	if (ret) {
		removePluginConfig(USER_ELEMENT_NAME, user_id);
		// save the new XML config file
		saveConfigFile();
	}
#else
	throw MissingOpenSSLException();
#endif
	
	return ret;
}

bool UserManager::isAdmin(const pion::net::PionUserPtr user_ptr) const
{
	// Make sure that the User configuration file is open.
	if (! configIsOpen())
		throw ConfigNotOpenException(getConfigFile());

	// Find the configuration node for the User. 
	std::lock_guard<std::mutex> users_lock(m_mutex);
	xmlNodePtr user_node = findConfigNodeByAttr(USER_ELEMENT_NAME, ID_ATTRIBUTE_NAME, user_ptr->getUsername(), m_config_node_ptr->children);

	if (user_node == NULL)
		return false;	// User not found.

	// Look for <Permission type="Admin" /> inside User node. 
	xmlNodePtr perm_node = findConfigNodeByAttr(USER_PERMISSION_ELEMENT_NAME, PERMISSION_TYPE_ATTRIBUTE_NAME, 
												ADMIN_PERMISSION_TYPE, user_node->children);

	return (perm_node != NULL);
}

xmlNodePtr UserManager::getPermissionNode(pion::net::PionUserPtr user_ptr, const std::string& permission_type) const
{
	// Make sure that the User configuration file is open.
	if (! configIsOpen())
		throw ConfigNotOpenException(getConfigFile());

	// Find the configuration node for the User. 
	std::lock_guard<std::mutex> users_lock(m_mutex);
	xmlNodePtr user_node = findConfigNodeByAttr(USER_ELEMENT_NAME, ID_ATTRIBUTE_NAME, user_ptr->getUsername(), m_config_node_ptr->children);

	if (user_node == NULL)
		throw UserNotFoundException(user_ptr->getUsername());

	if (permission_type.empty())
		return NULL;

	// Return the specified Permission node (or NULL, if not found).
	return findConfigNodeByAttr(USER_PERMISSION_ELEMENT_NAME, PERMISSION_TYPE_ATTRIBUTE_NAME, permission_type, user_node->children);
}

xmlNodePtr UserManager::createUserConfig(std::string& user_id,
	const char *buf, std::size_t len) 
{
	// sanity check
	if (buf == NULL || len == 0)
		throw BadXMLBufferException();

	// parse request payload content as XML
	xmlNodePtr node_ptr = NULL;
	xmlDocPtr doc_ptr = xmlParseMemory(buf, len);
	if (doc_ptr == NULL)
		throw XMLBufferParsingException(buf);

	// find the ROOT element
	if ( (node_ptr = xmlDocGetRootElement(doc_ptr)) == NULL
		|| xmlStrcmp(node_ptr->name,
		reinterpret_cast<const xmlChar*>(ROOT_ELEMENT_NAME.c_str())) )
	{
		xmlFreeDoc(doc_ptr);
		// buf is missing the root "PionConfig" element 
		throw MissingRootElementException(buf);
	}
	// find the resource element
	node_ptr = findConfigNodeByName(USER_ELEMENT_NAME, node_ptr->children);
	if (node_ptr == NULL) {
		xmlFreeDoc(doc_ptr);
		throw MissingResourceElementException(USER_ELEMENT_NAME);
	}

	// get user_id (username) 
	getNodeId(node_ptr, user_id);

	// found the resource config -> make a copy of it
	node_ptr = xmlCopyNodeList(node_ptr->children);

	// free the temporary document
	xmlFreeDoc(doc_ptr);

	// return the copied configuration info
	return node_ptr;
}

bool UserManager::creationAllowed(
	const pion::net::PionUserPtr& user_from_request, 
	const pion::platform::ConfigManager& config_manager,
	const xmlNodePtr& config_ptr) const
{
	if (user_from_request) {
		if (isAdmin(user_from_request)) {
			// All actions are permitted for this User.
			return true;
		} else {
			xmlNodePtr permission_config_ptr = getPermissionNode(user_from_request, config_manager.getPermissionType());
			return config_manager.creationAllowed(permission_config_ptr, config_ptr);
		}
	} else {
		// No User was associated with the request, which means that no authentication was needed for the
		// request, so all actions are permitted.
		return true;
	}
}

bool UserManager::updateAllowed(
	const pion::net::PionUserPtr& user_from_request, 
	const pion::platform::ConfigManager& config_manager,
	const std::string& id,
	const xmlNodePtr& config_ptr) const
{
	if (user_from_request) {
		if (isAdmin(user_from_request)) {
			// All actions are permitted for this User.
			return true;
		} else {
			xmlNodePtr permission_config_ptr = getPermissionNode(user_from_request, config_manager.getPermissionType());
			return config_manager.updateAllowed(permission_config_ptr, id, config_ptr);
		}
	} else {
		// No User was associated with the request, which means that no authentication was needed for the
		// request, so all actions are permitted.
		return true;
	}
}

bool UserManager::removalAllowed(
	const pion::net::PionUserPtr& user_from_request, 
	const pion::platform::ConfigManager& config_manager,
	const std::string& id) const
{
	if (user_from_request) {
		if (isAdmin(user_from_request)) {
			// All actions are permitted for this User.
			return true;
		} else {
			xmlNodePtr permission_config_ptr = getPermissionNode(user_from_request, config_manager.getPermissionType());
			return config_manager.removalAllowed(permission_config_ptr, id);
		}
	} else {
		// No User was associated with the request, which means that no authentication was needed for the
		// request, so all actions are permitted.
		return true;
	}
}

bool UserManager::accessAllowed(
	const pion::net::PionUserPtr& user_from_request, 
	const pion::platform::ConfigManager& config_manager,
	const std::string& plugin_id) const
{
	if (user_from_request) {
		if (isAdmin(user_from_request)) {
			// All actions are permitted for this User.
			return true;
		} else {
			xmlNodePtr permission_config_ptr = getPermissionNode(user_from_request, config_manager.getPermissionType());
			return config_manager.accessAllowed(permission_config_ptr, plugin_id);
		}
	} else {
		// No User was associated with the request, which means that no authentication was needed for the
		// request, so all actions are permitted.
		return true;
	}
}

bool UserManager::accessAllowed(
	const pion::net::PionUserPtr& user_from_request, 
	const PlatformService& service,
	const std::string& id) const
{
	if (user_from_request) {
		if (isAdmin(user_from_request)) {
			// All actions are permitted for this User.
			return true;
		} else {
			xmlNodePtr permission_config_ptr = getPermissionNode(user_from_request, service.getPermissionType());
			return service.accessAllowed(permission_config_ptr, id);
		}
	} else {
		// No User was associated with the request, which means that no authentication was needed for the
		// request, so all actions are permitted.
		return true;
	}
}


}	// end namespace server
}	// end namespace pion


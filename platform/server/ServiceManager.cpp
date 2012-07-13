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

#include <pion/net/HTTPBasicAuth.hpp>
#include <pion/net/HTTPCookieAuth.hpp>
#include <pion/net/HTTPResponseWriter.hpp>
#include "PlatformConfig.hpp"
#include "ServiceManager.hpp"

using namespace pion::net;
using namespace pion::platform;


namespace pion {		// begin namespace pion
namespace server {		// begin namespace server (Pion Server)


// static members of ServiceManager
	
const std::uint32_t		ServiceManager::DEFAULT_NUM_THREADS = 4;
const std::string			ServiceManager::DEFAULT_CONFIG_FILE = "services.xml";
const std::string			ServiceManager::SERVER_ELEMENT_NAME = "Server";
const std::string			ServiceManager::WEB_SERVICE_ELEMENT_NAME = "WebService";
const std::string			ServiceManager::PLATFORM_SERVICE_ELEMENT_NAME = "PlatformService";
const std::string			ServiceManager::PORT_ELEMENT_NAME = "Port";
const std::string			ServiceManager::SSL_KEY_ELEMENT_NAME = "SSLKey";
const std::string			ServiceManager::REDIRECT_ELEMENT_NAME = "Redirect";
const std::string			ServiceManager::REDIRECT_SOURCE_ELEMENT_NAME = "Source";
const std::string			ServiceManager::REDIRECT_TARGET_ELEMENT_NAME = "Target";
const std::string			ServiceManager::WEB_SERVICE_RESOURCE_ELEMENT_NAME = "Resource";
const std::string			ServiceManager::WEB_SERVICE_SERVER_ELEMENT_NAME = "Server";
const std::string			ServiceManager::OPTION_ELEMENT_NAME = "Option";
const std::string			ServiceManager::AUTH_ELEMENT_NAME = "Auth";
const std::string			ServiceManager::AUTH_TYPE_ELEMENT_NAME = "AuthType";
const std::string			ServiceManager::AUTH_RESTRICT_ELEMENT_NAME = "Restrict";
const std::string			ServiceManager::AUTH_PERMIT_ELEMENT_NAME = "Permit";
const std::string			ServiceManager::AUTH_LOGIN_ELEMENT_NAME = "Login";
const std::string			ServiceManager::AUTH_LOGOUT_ELEMENT_NAME = "Logout";
const std::string			ServiceManager::AUTH_REDIRECT_ELEMENT_NAME = "AuthRedirect";
const std::string			ServiceManager::NAME_ATTRIBUTE_NAME = "name";
	
		
// ServiceManager member functions

ServiceManager::ServiceManager(const VocabularyManager& vocab_mgr, PlatformConfig& platform_config)
	: PluginConfig<PlatformService>(vocab_mgr, DEFAULT_CONFIG_FILE, PLATFORM_SERVICE_ELEMENT_NAME), m_platform_config(platform_config)
{
	setLogger(PION_GET_LOGGER("pion.server.ServiceManager"));
	m_scheduler.setLogger(PION_GET_LOGGER("pion.server.ServiceManager"));
	m_scheduler.setNumThreads(DEFAULT_NUM_THREADS);
	platform_config.getCodecFactory().registerForUpdates(std::bind(&ServiceManager::updateCodecs, this));
	platform_config.getDatabaseManager().registerForUpdates(std::bind(&ServiceManager::updateDatabases, this));
	platform_config.getReactionEngine().registerForUpdates(std::bind(&ServiceManager::updateReactors, this));
}

void ServiceManager::shutdown(void)
{
	std::lock_guard<std::mutex> services_lock(m_mutex);

	// stop servers first otherwise you might have lingering threads
	// that cause seg faults during shutdown
	for (ServerMap::iterator it=m_servers.begin(); it!=m_servers.end(); ++it)
		it->second->stop();

	// call the stop() method for each web service associated with this server
	try {
		m_plugins.run(std::bind(&PlatformService::stop, std::placeholders::_1));
		m_web_services.run(std::bind(&WebService::stop, std::placeholders::_1));
	} catch (std::exception& e) {
		// catch exceptions thrown by services since their exceptions may be free'd
		// from memory before they are caught
		PION_LOG_ERROR(m_logger, "Web Service exception caught during shutdown: " << e.what());
	}

	// stop scheduler after stopping servers
	m_scheduler.shutdown();

	// finally, it's safe to clear the HTTPServer/WebServer objects
	m_servers.clear();
	
	// release plugins used by the ServiceManager
	this->releasePlugins();
}

void ServiceManager::openConfigFile(void)
{
	std::lock_guard<std::mutex> services_lock(m_mutex);

	// just return if it's already open
	if (configIsOpen())
		return;
	
	// open the file and find the "config" root element
	ConfigManager::openConfigFile();

	// some strings that get re-used a bunch
	std::string server_id;
	std::string port_str;
	std::string ssl_key;
	std::string redirect_source;
	std::string redirect_target;
	std::string service_id;
	std::string plugin_type;
	std::string http_resource;

	// step through Server configurations
	xmlNodePtr server_node = m_config_node_ptr->children;
	while ( (server_node = ConfigManager::findConfigNodeByName(SERVER_ELEMENT_NAME, server_node)) != NULL)
	{
		// get the unique identifier for the Server
		if (! getNodeId(server_node, server_id))
			throw EmptyServerIdException(getConfigFile());

		// get the port number for the server
		if (! ConfigManager::getConfigOption(PORT_ELEMENT_NAME, port_str,
											 server_node->children))
			throw MissingPortException(server_id);
		
		// create the server and add it to our list
		HTTPServerPtr server_ptr(new HTTPServer(m_scheduler, boost::lexical_cast<unsigned int>(port_str)));
		m_servers[server_id] = server_ptr;

		// get the Authentication type configuration
		HTTPAuthPtr auth_ptr;
		std::string value;
		if (! ConfigManager::getConfigOption(AUTH_TYPE_ELEMENT_NAME, value, server_node->children)) {
			// default to cookie authentication
			value = "Cookie";
		}
		if (value == "Basic") {
			// use HTTP Basic authentication
			auth_ptr.reset(new HTTPBasicAuth(m_platform_config.getUserManagerPtr()));
		} else if (value == "Cookie") {
			// use session cookie authentication
			auth_ptr.reset(new HTTPCookieAuth(m_platform_config.getUserManagerPtr()));
			// parse parameters specific to cookie authentication
			if(ConfigManager::getConfigOption(AUTH_LOGIN_ELEMENT_NAME, value,server_node->children)){
				auth_ptr->setOption("login",value);
			}
			if(ConfigManager::getConfigOption(AUTH_LOGOUT_ELEMENT_NAME, value,server_node->children)){
				auth_ptr->setOption("logout",value);
			}
			if(ConfigManager::getConfigOption(AUTH_REDIRECT_ELEMENT_NAME, value,server_node->children)){
				auth_ptr->setOption("redirect",value);
			}
		} else {
			throw UnknownAuthTypeException(server_id);
		}

		// step through restricted URL definitions
		xmlNodePtr restrict_node=server_node->children;
		while ( (restrict_node = ConfigManager::findConfigNodeByName(AUTH_RESTRICT_ELEMENT_NAME, restrict_node)) != NULL)
		{
			// get the  value
			std::string restrict_value;
			xmlChar *xml_char_ptr = xmlNodeGetContent(restrict_node);
			if (xml_char_ptr != NULL) {
				restrict_value = reinterpret_cast<char*>(xml_char_ptr);
				xmlFree(xml_char_ptr);
				auth_ptr->addRestrict(restrict_value);
			}

			// step to the next definition
			restrict_node = restrict_node->next;
		}

		// step through permitted URL definitions
		xmlNodePtr permit_node=server_node->children;
		while ( (permit_node = ConfigManager::findConfigNodeByName(AUTH_PERMIT_ELEMENT_NAME, permit_node)) != NULL)
		{
			// get the  value
			std::string permit_value;
			xmlChar *xml_char_ptr = xmlNodeGetContent(permit_node);
			if (xml_char_ptr != NULL) {
				permit_value = reinterpret_cast<char*>(xml_char_ptr);
				xmlFree(xml_char_ptr);
				auth_ptr->addPermit(permit_value);
			}

			// step to the next definition
			permit_node = permit_node->next;
		}
		// set up authentication for given server
		server_ptr->setAuthentication(auth_ptr);

		// use ServiceManager for handling error responses
		server_ptr->setServerErrorHandler(std::bind(&ServiceManager::handleServerError, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

		// get the ssl key for the server (if defined)
		if (ConfigManager::getConfigOption(SSL_KEY_ELEMENT_NAME, ssl_key,
										   server_node->children))
		{
			#ifdef PION_HAVE_SSL
				// enable SSL for the server using the key provided
				const std::string path_to_key_file(ConfigManager::resolveRelativePath(ssl_key));
				try {
					server_ptr->setSSLKeyFile(path_to_key_file);
				} catch (std::exception&) {
					throw SSLKeyException(path_to_key_file);
				}
			#else
				PION_LOG_WARN(m_logger, "Ignoring SSL keyfile parameter (Pion was not built with SSL support)");
			#endif
		}

		// step through resource redirections
		xmlNodePtr redirect_node = server_node->children;
		while ((redirect_node = ConfigManager::findConfigNodeByName(REDIRECT_ELEMENT_NAME, redirect_node)) != NULL) {
			// get the source of the redirection 
			if (!getConfigOption(REDIRECT_SOURCE_ELEMENT_NAME, redirect_source, redirect_node->children))
				throw RedirectMissingSourceException(server_id);

			// get the target of the redirection 
			if (!getConfigOption(REDIRECT_TARGET_ELEMENT_NAME, redirect_target, redirect_node->children))
				throw RedirectMissingTargetException(server_id);

			server_ptr->addRedirect(redirect_source, redirect_target);

			redirect_node = redirect_node->next;
		}

		// step through nested PlatformService configurations
		xmlNodePtr service_node = server_node->children;
		while ( (service_node = ConfigManager::findConfigNodeByName(PLATFORM_SERVICE_ELEMENT_NAME, service_node)) != NULL)
		{
			// parse new plug-in definition
			if (! getNodeId(service_node, service_id))
				throw EmptyServiceIdException(server_id);
		
			// find plug-in type
			if (! getConfigOption(PLUGIN_ELEMENT_NAME, plugin_type, service_node->children))
				throw EmptyServicePluginTypeException(service_id);

			// catch exceptions thrown by services since their exceptions may be free'd
			// from memory before they are caught
			PlatformService *service_ptr;
			try {
				pion::platform::VocabularyPtr vocab_ptr(m_vocab_mgr.getVocabulary());
				service_ptr = m_plugins.load(service_id, plugin_type);
				service_ptr->setId(service_id);
				service_ptr->setConfigManager(*this);
				service_ptr->setPlatformConfig(m_platform_config);
				service_ptr->setServerId(server_id);
				service_ptr->setConfig(*vocab_ptr, service_node->children);
			} catch (std::exception& e) {
				throw WebServiceException(service_id, e.what());
			}
			
			// add the service to the HTTP server
			server_ptr->addResource(service_ptr->getResource(), std::ref(*service_ptr));
			
			// step to the next service definition
			service_node = service_node->next;
		}
		
		// step through nested WebService configurations
		service_node = server_node->children;
		while ( (service_node = ConfigManager::findConfigNodeByName(WEB_SERVICE_ELEMENT_NAME, service_node)) != NULL)
		{
			addWebService(service_node, server_id);

			// step to the next service definition
			service_node = service_node->next;
		}

		// step to the next server definition
		server_node = server_node->next;
	}

	// step through top level PlatformService configurations
	xmlNodePtr service_node = m_config_node_ptr->children;
	while ( (service_node = ConfigManager::findConfigNodeByName(PLATFORM_SERVICE_ELEMENT_NAME, service_node)) != NULL)
	{
		// find plug-in ID
		std::string new_plugin_id;
		if (! getNodeId(service_node, new_plugin_id))
			throw EmptyPluginIdException(getConfigFile());
		
		// find plug-in type
		std::string new_plugin_type;
		if (! getConfigOption(PLUGIN_ELEMENT_NAME, new_plugin_type, service_node->children))
			throw EmptyPluginElementException(new_plugin_id);

		addPluginNoLock(new_plugin_id, new_plugin_type, service_node->children);
		service_node = service_node->next;
	}

	// step through top level WebService configurations
	service_node = m_config_node_ptr->children;
	while ( (service_node = ConfigManager::findConfigNodeByName(WEB_SERVICE_ELEMENT_NAME, service_node)) != NULL)
	{
		// find the ID of the Server this WebService belongs to
		if (! getConfigOption(WEB_SERVICE_SERVER_ELEMENT_NAME, server_id, service_node->children))
			throw EmptyServiceServerIdException(getConfigFile());

		addWebService(service_node, server_id);

		service_node = service_node->next;
	}

	// call the start() method for each web service associated with this server
	try {
		m_plugins.run(std::bind(&WebService::start, std::placeholders::_1));
		m_web_services.run(std::bind(&WebService::start, std::placeholders::_1));
	} catch (std::exception& e) {
		// catch exceptions thrown by services since their exceptions may be free'd
		// from memory before they are caught
		throw WebServiceException("[Startup]", e.what());
	}
	
	// startup the web servers
	for (ServerMap::iterator it=m_servers.begin(); it!=m_servers.end(); ++it)
		it->second->start();

	PION_LOG_INFO(m_logger, "Loaded Service configuration file: " << m_config_file);
}

void ServiceManager::addWebService(xmlNodePtr service_node, const std::string& server_id)
{
	std::string service_id;
	std::string plugin_type;
	std::string http_resource;

	getWebServiceConfig(service_node, server_id, service_id, plugin_type, http_resource);
	
	// catch exceptions thrown by services since their exceptions may be free'd
	// from memory before they are caught
	WebService *service_ptr;
	try {
		// load the service
		service_ptr = m_web_services.load(service_id, plugin_type);
		service_ptr->setResource(http_resource);
	} catch (std::exception& e) {
		throw WebServiceException(service_id, e.what());
	}
	
	// step through WebService configuration options
	xmlNodePtr option_node = service_node->children;
	while ( (option_node = ConfigManager::findConfigNodeByName(OPTION_ELEMENT_NAME, option_node)) != NULL)
	{
		// get the option name
		xmlChar *xml_char_ptr = xmlGetProp(option_node, reinterpret_cast<const xmlChar*>(NAME_ATTRIBUTE_NAME.c_str()));
		if (xml_char_ptr == NULL || xml_char_ptr[0]=='\0') {
			if (xml_char_ptr != NULL)
				xmlFree(xml_char_ptr);
			throw EmptyOptionNameException(service_id);
		}
		const std::string option_name(reinterpret_cast<char*>(xml_char_ptr));
		xmlFree(xml_char_ptr);
		
		// get the option value
		std::string option_value;
		xml_char_ptr = xmlNodeGetContent(option_node);
		if (xml_char_ptr != NULL) {
			option_value = reinterpret_cast<char*>(xml_char_ptr);
			xmlFree(xml_char_ptr);
		}
		
		// catch exceptions thrown by services since their exceptions may be free'd
		// from memory before they are caught
		try {
			// set the configuration option
			service_ptr->setOption(option_name, option_value);
		} catch (std::exception& e) {
			throw WebServiceException(service_id, e.what());
		}
		
		// step to the next WebService configuration option
		option_node = option_node->next;
	}				

	// add the service to the HTTP server
	m_servers[server_id]->addResource(http_resource, std::ref(*service_ptr));
}

void ServiceManager::getWebServiceConfig(xmlNodePtr service_node, const std::string& server_id,
										 std::string& service_id, std::string& plugin_type,
										 std::string& http_resource)
{
	// get the unique identifier for the Service
	if (! getNodeId(service_node, service_id))
		throw EmptyServiceIdException(server_id);

	// find plug-in type
	if (! getConfigOption(PLUGIN_ELEMENT_NAME, plugin_type, service_node->children))
		throw EmptyServicePluginTypeException(service_id);
		
	// find the HTTP resource
	if (! getConfigOption(WEB_SERVICE_RESOURCE_ELEMENT_NAME, http_resource, service_node->children))
		throw EmptyServiceResourceException(service_id);
	
	// remove the trailing slash (if any) from the HTTP resource
	http_resource = HTTPServer::stripTrailingSlash(http_resource);
}

bool ServiceManager::writeServerXML(std::ostream& out,
									const std::string& server_id) const
{
	// find the Server element with the specified ID in the XML config document
	std::lock_guard<std::mutex> services_lock(m_mutex);
	xmlNodePtr server_node = findConfigNodeByAttr(SERVER_ELEMENT_NAME,
												  ID_ATTRIBUTE_NAME,
												  server_id,
												  m_config_node_ptr->children);
	if (server_node == NULL)
		return false;

	// found it
	ConfigManager::writeBeginPionConfigXML(out);
	ConfigManager::writeConfigXML(out, server_node, false);
	ConfigManager::writeEndPionConfigXML(out);

	return true;
}

void ServiceManager::writeServersXML(std::ostream& out) const
{
	ConfigManager::writeBeginPionConfigXML(out);

	// step through Server configurations
	xmlNodePtr server_node = m_config_node_ptr->children;
	while ( (server_node = ConfigManager::findConfigNodeByName(SERVER_ELEMENT_NAME, server_node)) != NULL)
	{
		ConfigManager::writeConfigXML(out, server_node, false);

		server_node = server_node->next;
	}

	ConfigManager::writeEndPionConfigXML(out);
}

void ServiceManager::updateCodecs(void)
{
	m_plugins.run(std::bind(&PlatformService::updateCodecs, std::placeholders::_1,
							  std::ref(m_platform_config)));
}

void ServiceManager::updateDatabases(void)
{
	m_plugins.run(std::bind(&PlatformService::updateDatabases, std::placeholders::_1,
							  std::ref(m_platform_config)));
}

void ServiceManager::updateReactors(void)
{
	m_plugins.run(std::bind(&PlatformService::updateReactors, std::placeholders::_1,
							  std::ref(m_platform_config)));
}
	
void ServiceManager::handleServerError(HTTPRequestPtr& http_request,
	TCPConnectionPtr& tcp_conn, const std::string& error_msg)
{
	HTTPResponseWriterPtr writer(HTTPResponseWriter::create(tcp_conn, *http_request,
															std::bind(&TCPConnection::finish, tcp_conn)));
	writer->getResponse().setStatusCode(HTTPTypes::RESPONSE_CODE_SERVER_ERROR);
	writer->getResponse().setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_SERVER_ERROR);
	writer->getResponse().setContentType(HTTPTypes::CONTENT_TYPE_TEXT);
	writer << error_msg;
	writer->send();
}

unsigned int ServiceManager::getPort(void) const
{
	unsigned int port_num = 0;
	std::lock_guard<std::mutex> services_lock(m_mutex);
	ServerMap::const_iterator it = m_servers.begin();
	if (it != m_servers.end())
		port_num = it->second->getPort();
	return port_num;
}

std::string ServiceManager::addPlatformService(const xmlNodePtr config_ptr)
{
	return PluginConfig<PlatformService>::addPlugin(config_ptr);
}

void ServiceManager::removePlatformService(const std::string& service_id) {
	// convert PluginNotFound exceptions into PlatformServiceNotFound exceptions
	try {
		PlatformService* service_ptr = m_plugins.get(service_id);
		if (service_ptr == NULL)
			throw PluginManager<PlatformService>::PluginNotFoundException(service_id);
		pion::net::HTTPServerPtr server_ptr = m_servers[service_ptr->getServerId()];
		server_ptr->removeResource(service_ptr->getResource());
		PluginConfig<PlatformService>::removePlugin(service_id);
	} catch (PluginManager<PlatformService>::PluginNotFoundException&) {
		throw PlatformServiceNotFoundException(service_id);
	}
}

	
}	// end namespace server
}	// end namespace pion

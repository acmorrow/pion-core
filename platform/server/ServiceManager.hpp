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

#ifndef __PION_SERVICEMANAGER_HEADER__
#define __PION_SERVICEMANAGER_HEADER__

#include <list>
#include <string>
#include <boost/thread/mutex.hpp>
#include <pion/PionConfig.hpp>
#include <pion/PionException.hpp>
#include <pion/PionScheduler.hpp>
#include <pion/PluginManager.hpp>
#include <pion/net/HTTPServer.hpp>
#include <pion/net/WebService.hpp>
#include <pion/platform/ConfigManager.hpp>
#include "PlatformService.hpp"
#include <pion/platform/PluginConfig.hpp>


namespace pion {		// begin namespace pion
namespace server {		// begin namespace server (Pion Server)
	
	
// forward declarations to avoid header dependencies
class PlatformConfig;
	

///
/// ServiceManager: manages configuration for platform services
///
class PION_SERVER_API ServiceManager :
	public pion::platform::PluginConfig<PlatformService>
{
public:
	
	/// exception thrown if the config file contains a Server with an empty or missing identifier
	class EmptyServerIdException : public PionException {
	public:
		EmptyServerIdException(const std::string& config_file)
			: PionException("Service configuration file includes a Server without a unique identifier: ", config_file) {}
	};

	/// exception thrown if the config file contains a Service with an empty or missing identifier
	class EmptyServiceIdException : public PionException {
	public:
		EmptyServiceIdException(const std::string& server_id)
			: PionException("Server configuration includes a Service without a unique identifier: ", server_id) {}
	};
	
	/// exception thrown if the config file contains a Service with an empty or missing plug-in type
	class EmptyServicePluginTypeException : public PionException {
	public:
		EmptyServicePluginTypeException(const std::string& service_id)
			: PionException("Service configuration does not define a plug-in type: ", service_id) {}
	};
	
	/// exception thrown if the config file contains a Service with an empty or missing HTTP resource
	class EmptyServiceResourceException : public PionException {
	public:
		EmptyServiceResourceException(const std::string& service_id)
			: PionException("Service configuration does not define a resource: ", service_id) {}
	};

	/// exception thrown if the config file contains a Service with an empty or missing Server identifier
	class EmptyServiceServerIdException : public PionException {
	public:
		EmptyServiceServerIdException(const std::string& config_file)
			: PionException("Service configuration file includes a Service without a Server identifier: ", config_file) {}
	};

	/// exception thrown if the config file contains a WebService option with an empty name
	class EmptyOptionNameException : public PionException {
	public:
		EmptyOptionNameException(const std::string& service_id)
			: PionException("Service configuration option does not define a name: ", service_id) {}
	};
	
	/// exception thrown if the config file contains a Server with a missing port number
	class MissingPortException : public PionException {
	public:
		MissingPortException(const std::string& server_id)
			: PionException("Server configuration does not define a port number: ", server_id) {}
	};

	/// exception thrown if the config file contains a unknown AuthType value
	class UnknownAuthTypeException : public PionException {
	public:
		UnknownAuthTypeException(const std::string& server_id)
			: PionException("Server configuration has unknown AuthType parameter: ", server_id) {}
	};

	/// exception thrown if there is an error loading an SSL key file
	class SSLKeyException : public PionException {
	public:
		SSLKeyException(const std::string& key_file)
			: PionException("Unable to load SSL key file: ", key_file) {}
	};

	/// exception thrown if the config file contains a Redirect element with no Source specified
	class RedirectMissingSourceException : public PionException {
	public:
		RedirectMissingSourceException(const std::string& server_id)
			: PionException("Service configuration Redirect element does not specify a Source: ", server_id) {}
	};

	/// exception thrown if the config file contains a Redirect element with no Target specified
	class RedirectMissingTargetException : public PionException {
	public:
		RedirectMissingTargetException(const std::string& server_id)
			: PionException("Service configuration Redirect element does not specify a Target: ", server_id) {}
	};

	/// exception used to propagate exceptions thrown by web services
	class WebServiceException : public PionException {
	public:
		WebServiceException(const std::string& service_id, const std::string& error_msg)
			: PionException(std::string("Service (") + service_id,
							std::string("): ") + error_msg)
		{}
	};

	/// exception thrown if a PlatformService cannot be found
	class PlatformServiceNotFoundException : public PionException {
	public:
		PlatformServiceNotFoundException(const std::string& service_id)
			: PionException("No platform service found for identifier: ", service_id) {}
	};

	/**
	 * constructs a new ServiceManager instance
	 *
	 * @param platform_config reference to the global platform configuration manager
	 */
	ServiceManager(const pion::platform::VocabularyManager& vocab_mgr, PlatformConfig& platform_config);
	
	/// virtual destructor
	virtual ~ServiceManager() { shutdown(); }
	
	/// shuts down the service manager -> stops all servers and threads and releases plugins
	void shutdown(void);
	
	/// opens an existing config file and loads the data it contains
	virtual void openConfigFile(void);

	/**
	 * writes the configurations for all servers to an output stream (as XML)
	 *
	 * @param out the ostream to write the configuration tree into
	 */
	virtual void writeServersXML(std::ostream& out) const;
	
	/**
	 * writes the configuration data for a particular server (as XML)
	 *
	 * @param out the ostream to write the configuration tree into
	 * @param server_id unique identifier associated with the HTTP server
	 */
	bool writeServerXML(std::ostream& out, const std::string& server_id) const;

	/**
	 * schedules work to be performed by one of the pooled threads
	 *
	 * @param work_func work function to be executed
	 */
	template<typename WorkFunction>
	inline void post(WorkFunction work_func) { m_scheduler.getIOService().post(work_func); }
	
	/// returns an async I/O service used to schedule work
	inline boost::asio::io_service& getIOService(void) { return m_scheduler.getIOService(); }
	
	/// returns the number of threads currently in use
	inline boost::uint32_t getNumThreads(void) const { return m_scheduler.getNumThreads(); }
	
	/// sets the number of threads used all of the HTTP servers & services
	inline void setNumThreads(const boost::uint32_t n) { m_scheduler.setNumThreads(n); }
		
	/// this notifies all the service plug-ins that the Codecs were updated
	void updateCodecs(void);

	/// this notifies all the service plug-ins that the Databases were updated
	void updateDatabases(void);

	/// this notifies all the service plug-ins that the Reactors were updated
	void updateReactors(void);
	
	/// returns port number the first server is listing to (for unit tests)
	unsigned int getPort(void) const;

	/**
	 * adds a new managed PlatformService
	 *
	 * @param config_ptr pointer to a list of XML nodes containing PlatformService
	 *                   configuration parameters (must include a Plugin type)
	 *
	 * @return std::string the new PlatformService's unique identifier
	 */
	std::string addPlatformService(const xmlNodePtr config_ptr);

	/**
	 * removes a managed PlatformService
	 *
	 * @param service_id unique identifier associated with the PlatformService
	 */
	void removePlatformService(const std::string& service_id);

	/**
	 * uses a memory buffer to generate XML configuration data for a PlatformService
	 *
	 * @param buf pointer to a memory buffer containing configuration data
	 * @param len number of bytes available in the memory buffer
	 *
	 * @return xmlNodePtr XML configuration list for the PlatformService
	 */
	static xmlNodePtr createPlatformServiceConfig(const char *buf, std::size_t len) {
		return ConfigManager::createResourceConfig(PLATFORM_SERVICE_ELEMENT_NAME, buf, len);
	}

protected:

	/**
	 * adds a new plug-in object (without locking or config file updates).  This
	 * function must be defined properly for any derived classes that wish to
	 * use openPluginConfig().
	 *
	 * @param plugin_id unique identifier associated with the plug-in
	 * @param plugin_name the name of the plug-in to load (searches
	 *                    plug-in directories and appends extensions)
	 * @param config_ptr pointer to a list of XML nodes containing plug-in
	 *                   configuration parameters
	 */
	virtual void addPluginNoLock(const std::string& plugin_id,
								 const std::string& plugin_name,
								 const xmlNodePtr config_ptr)
	{
		try {
			PlatformService *new_plugin_ptr = m_plugins.load(plugin_id, plugin_name);
			new_plugin_ptr->setId(plugin_id);
			new_plugin_ptr->setConfigManager(*this);
			new_plugin_ptr->setPlatformConfig(m_platform_config);
			if (config_ptr != NULL) {
				pion::platform::VocabularyPtr vocab_ptr(m_vocab_mgr.getVocabulary());
				new_plugin_ptr->setConfig(*vocab_ptr, config_ptr);
			}

			pion::net::HTTPServerPtr server_ptr = m_servers[new_plugin_ptr->getServerId()];
			server_ptr->addResource(new_plugin_ptr->getResource(), std::ref(*new_plugin_ptr));
		} catch (PionPlugin::PluginNotFoundException&) {
			throw;
		} catch (std::exception& e) {
			throw PluginException(e.what());
		}
	}

private:

	/**
	 * used to get basic config options for services
	 *
	 * @param service_node the XML tree node for the service
	 * @param server_id the unique identifier for the Server associated with the service
	 * @param service_id will be assigned to the service's unique identifier
	 * @param plugin_type will be assigned to the service's plug-in type
	 * @param http_resource will be assigned to the service's HTTP resource
	 */
	void getWebServiceConfig(xmlNodePtr service_node, const std::string& server_id,
							 std::string& service_id, std::string& plugin_type,
							 std::string& http_resource);
	
	/**
	 * used to send responses when a server error occurs (when an exception is caught)
	 *
	 * @param http_request the new HTTP request to handle
	 * @param tcp_conn the TCP connection that has the new request
	 * @param error_msg message that explains what went wrong
	 */
	static void handleServerError(pion::net::HTTPRequestPtr& http_request,
								  pion::net::TCPConnectionPtr& tcp_conn,
								  const std::string& error_msg);

	void addWebService(xmlNodePtr service_node, const std::string& server_id);

	
	/// data type for a map of HTTPServer pointers by id
	typedef std::map<std::string, pion::net::HTTPServerPtr>		ServerMap;
	
	/// data type for a collection of web services
	typedef PluginManager<pion::net::WebService>	WebServiceManager;
	
	/// data type for a collection of platform services
	typedef PluginManager<PlatformService>			PlatformServiceManager;
	
	
	/// default number of worker threads in the thread pool
	static const boost::uint32_t	DEFAULT_NUM_THREADS;
	
	/// default name of the vocabulary config file
	static const std::string		DEFAULT_CONFIG_FILE;

	/// name of the server (HTTPServer) element for Pion XML config files
	static const std::string		SERVER_ELEMENT_NAME;

	/// name of the service (WebService) element for Pion XML config files
	static const std::string		WEB_SERVICE_ELEMENT_NAME;
	
	/// name of the service (PlatformService) element for Pion XML config files
	static const std::string		PLATFORM_SERVICE_ELEMENT_NAME;
	
	/// name of the port number element for Pion XML config files
	static const std::string		PORT_ELEMENT_NAME;

	/// name of the ssl key element for Pion XML config files
	static const std::string		SSL_KEY_ELEMENT_NAME;

	/// name of the resource redirection element for Pion XML config files
	static const std::string		REDIRECT_ELEMENT_NAME;

	/// name of an element specifying the source of a redirect for Pion XML config files
	static const std::string		REDIRECT_SOURCE_ELEMENT_NAME;

	/// name of an element specifying the target of a redirect for Pion XML config files
	static const std::string		REDIRECT_TARGET_ELEMENT_NAME;

	/// name of the HTTP resource element of a WebService for Pion XML config files
	static const std::string		WEB_SERVICE_RESOURCE_ELEMENT_NAME;

	/// name of the element containing the Server a WebService belongs to for Pion XML config files
	static const std::string		WEB_SERVICE_SERVER_ELEMENT_NAME;

	/// name of the WebService option element for Pion XML config files
	static const std::string		OPTION_ELEMENT_NAME;
	
	/// name of the Authentication element for Pion XML config files
	static const std::string		AUTH_ELEMENT_NAME;
	
	/// name of the "Restricted-by-Authentication" element for Pion XML config files
	static const std::string		AUTH_RESTRICT_ELEMENT_NAME;

	/// name of the "Permit-by-Authentication" element for Pion XML config files
	static const std::string		AUTH_PERMIT_ELEMENT_NAME;

	/// name of the Authentication Type element for Pion XML config files
	static const std::string		AUTH_TYPE_ELEMENT_NAME;

	/// name of the Authentication Login URL element for Pion XML config files
	static const std::string		AUTH_LOGIN_ELEMENT_NAME;

	/// name of the Authentication Logout URL element for Pion XML config files
	static const std::string		AUTH_LOGOUT_ELEMENT_NAME;

	/// name of the Authentication Redirect URL element for Pion XML config files
	static const std::string		AUTH_REDIRECT_ELEMENT_NAME;

	/// name of the option name attribute for Pion XML config files
	static const std::string		NAME_ATTRIBUTE_NAME;

	/// reference to the Pion platform configuration manager
	PlatformConfig &				m_platform_config;
	
	/// used to manage a worker thread pool shared by all servers
	PionSingleServiceScheduler		m_scheduler;

	/// collection of HTTP servers
	ServerMap						m_servers;

	/// collection of regular web services used by the HTTP servers
	WebServiceManager				m_web_services;

	/// mutex to make class thread-safe
	mutable boost::mutex			m_mutex;
};


}	// end namespace server
}	// end namespace pion

#endif

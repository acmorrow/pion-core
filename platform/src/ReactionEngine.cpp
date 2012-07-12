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

#include <boost/asio.hpp>
#include <pion/platform/CodecFactory.hpp>
#include <pion/platform/ProtocolFactory.hpp>
#include <pion/platform/DatabaseManager.hpp>
#include <pion/platform/ReactionEngine.hpp>


namespace pion {		// begin namespace pion
namespace platform {	// begin namespace platform (Pion Platform Library)


// static members of ReactionEngine

const boost::uint32_t	ReactionEngine::DEFAULT_NUM_THREADS = 4;
const std::string		ReactionEngine::DEFAULT_CONFIG_FILE = "reactors.xml";
const std::string		ReactionEngine::CONNECTION_ELEMENT_NAME = "Connection";
const std::string		ReactionEngine::TYPE_ELEMENT_NAME = "Type";
const std::string		ReactionEngine::FROM_ELEMENT_NAME = "From";
const std::string		ReactionEngine::TO_ELEMENT_NAME = "To";
const std::string		ReactionEngine::TOTAL_OPS_ELEMENT_NAME = "TotalOps";
const std::string		ReactionEngine::EVENTS_QUEUED_ELEMENT_NAME = "EventsQueued";
const std::string		ReactionEngine::CONNECTION_TYPE_REACTOR = "reactor";
const std::string		ReactionEngine::CONNECTION_TYPE_INPUT = "input";
const std::string		ReactionEngine::CONNECTION_TYPE_OUTPUT = "output";
const std::string		ReactionEngine::REACTORS_PERMISSION_TYPE = "Reactors";
const std::string		ReactionEngine::UNRESTRICTED_ELEMENT_NAME = "Unrestricted";
const std::string		ReactionEngine::WORKSPACE_QUALIFIER_ELEMENT_NAME = "Workspace";


// ReactionEngine member functions
	
ReactionEngine::ReactionEngine(VocabularyManager& vocab_mgr,
							   CodecFactory& codec_factory,
							   ProtocolFactory& protocol_factory,
							   DatabaseManager& database_mgr)
	: PluginConfig<Reactor>(vocab_mgr, DEFAULT_CONFIG_FILE, Reactor::REACTOR_ELEMENT_NAME),
	m_codec_factory(codec_factory),
	m_protocol_factory(protocol_factory),
	m_database_mgr(database_mgr),
	m_is_running(false),
	m_multithread_branches(false)
{
	setLogger(PION_GET_LOGGER("pion.platform.ReactionEngine"));
	m_scheduler.setLogger(PION_GET_LOGGER("pion.platform.ReactionEngine"));
	m_scheduler.setNumThreads(DEFAULT_NUM_THREADS);
	m_codec_connection = m_codec_factory.registerForUpdates(std::bind(&ReactionEngine::updateCodecs, this));
	m_db_connection = m_database_mgr.registerForUpdates(std::bind(&ReactionEngine::updateDatabases, this));
	m_protocol_connection = m_protocol_factory.registerForUpdates(std::bind(&ReactionEngine::updateProtocols, this));
}

void ReactionEngine::openConfigFile(void)
{
	std::unique_lock<std::mutex> engine_lock(m_mutex);

	// just return if it's already open
	if (ConfigManager::configIsOpen())
		return;

	// open the plug-in config file and load plug-ins
	ConfigManager::openPluginConfig(m_plugin_element);
	
	// Step through and process Reactor connections.
	// This must be done last & independently to ensure that all Reactors have
	// been loaded; otherwise, you would encounter references to Reactors that
	// have not yet been loaded.
	xmlNodePtr connection_node = m_config_node_ptr->children;
	while ( (connection_node = ConfigManager::findConfigNodeByName(CONNECTION_ELEMENT_NAME, connection_node)) != NULL)
	{
		// parse new plug-in definition
		std::string connection_id;
		if (! ConfigManager::getNodeId(connection_node, connection_id))
			throw EmptyConnectionIdException(ConfigManager::getConfigFile());

		// make sure that it is a reactor connection type
		std::string connection_type;
		if (! ConfigManager::getConfigOption(TYPE_ELEMENT_NAME, connection_type, connection_node->children)
			|| connection_type != CONNECTION_TYPE_REACTOR)
		{
			throw BadConnectionTypeException(connection_id);
		}
		
		// get the ID for the Reactor where events come from
		std::string from_id;
		if (! ConfigManager::getConfigOption(FROM_ELEMENT_NAME, from_id, connection_node->children))
			throw EmptyFromException(connection_id);
		
		// get the ID for the Reactor where events go to
		std::string to_id;
		if (! ConfigManager::getConfigOption(TO_ELEMENT_NAME, to_id, connection_node->children))
			throw EmptyToException(connection_id);

		// add the connection w/o locking
		addConnectionNoLock(connection_id, from_id, to_id);

		// step to the next Reactor connection
		connection_node = connection_node->next;
	}
	
	PION_LOG_INFO(m_logger, "Loaded Reactor configuration file: " << ConfigManager::getConfigFile());
	
	// start the ReactionEngine
	engine_lock.unlock();
	start();
}
	
void ReactionEngine::clearReactorStats(const std::string& reactor_id)
{
	// convert "plugin not found" exceptions into "reactor not found"
	try {
		m_plugins.run(reactor_id, std::bind(&Reactor::clearStats, std::placeholders::_1));
	} catch (PluginManager<Reactor>::PluginNotFoundException& /* e */) {
		throw ReactorNotFoundException(reactor_id);
	}
	PION_LOG_DEBUG(m_logger, "Cleared reactor statistics: " << reactor_id);
}

void ReactionEngine::start(void)
{
	std::lock_guard<std::mutex> engine_lock(m_mutex);
	if (! m_is_running) {
		PION_LOG_INFO(m_logger, "Starting the ReactionEngine");

		// notify the thread scheduler that we need it now
		m_scheduler.addActiveUser();

		m_is_running = true;
	}
}

void ReactionEngine::stop(void)
{
	std::lock_guard<std::mutex> engine_lock(m_mutex);
	stopNoLock();
}

void ReactionEngine::shutdown(void)
{
	PION_LOG_DEBUG(m_logger, "shutting down");
	stop();
	PION_LOG_DEBUG(m_logger, "stopped; shutting down threads");
	m_scheduler.shutdown();
	PION_LOG_DEBUG(m_logger, "threads shutdown; clearing connections");
	m_temp_connections.clear();
	m_reactor_connections.clear();
	m_plugins.run(std::bind(&Reactor::clearConnections, std::placeholders::_1));
	PION_LOG_DEBUG(m_logger, "connections cleared; releasing plugins");
	this->releasePlugins();
	PION_LOG_DEBUG(m_logger, "shutdown complete");
}

void ReactionEngine::clearStats(void)
{
	m_plugins.run(std::bind(&Reactor::clearStats, std::placeholders::_1));
	PION_LOG_DEBUG(m_logger, "Cleared all reactor statistics");
}
	
void ReactionEngine::updateCodecs(void)
{
	m_plugins.run(std::bind(&Reactor::updateCodecs, std::placeholders::_1));
}

void ReactionEngine::updateDatabases(void)
{
	m_plugins.run(std::bind(&Reactor::updateDatabases, std::placeholders::_1));
}

void ReactionEngine::updateProtocols(void)
{
	m_plugins.run(std::bind(&Reactor::updateProtocols, std::placeholders::_1));
}

void ReactionEngine::restartReactorsThatShouldBeRunning(void)
{
	try {
		// step through each reactor config node
		xmlNodePtr reactor_node = m_config_node_ptr->children;
		while ( (reactor_node = findConfigNodeByName(Reactor::REACTOR_ELEMENT_NAME, reactor_node)) != NULL)
		{
			// get the reactor identifier
			std::string reactor_id;
			if (getNodeId(reactor_node, reactor_id)) {
				// attempt to restart the Reactor if necessary
				// TODO(acm): Why is this necessary?
				auto runfunc = std::bind(&Reactor::startOutRunning, std::placeholders::_1, reactor_node->children, true);
				m_plugins.run(reactor_id, [runfunc](Reactor* reactor) { runfunc(reactor); });
			}
	
			// look for more reactor config nodes
			reactor_node = reactor_node->next;
		}
	} catch (std::exception& e) {
		// log but don't propagate exceptions
		PION_LOG_ERROR(m_logger, e.what());
	}
}
	
void ReactionEngine::setReactorConfig(const std::string& reactor_id,
									  const xmlNodePtr config_ptr)
{
	// convert PluginNotFound exceptions into ReactorNotFound exceptions
	try {
		PluginConfig<Reactor>::setPluginConfig(reactor_id, config_ptr);
	} catch (PluginManager<Reactor>::PluginNotFoundException&) {
		throw ReactorNotFoundException(reactor_id);
	}
}

void ReactionEngine::setReactorLocation(const std::string& reactor_id,
										const xmlNodePtr config_ptr)
{
	// make sure that the plug-in configuration file is open
	if (! configIsOpen())
		throw ConfigNotOpenException(getConfigFile());

	// Find the Reactor node of the configuration with the specified ID.
	xmlNodePtr reactor_node = m_config_node_ptr->children;
	while ( (reactor_node = ConfigManager::findConfigNodeByName(Reactor::REACTOR_ELEMENT_NAME, reactor_node)) != NULL) {
		std::string node_id;
		getNodeId(reactor_node, node_id);
		if (node_id == reactor_id)
			break;
		reactor_node = reactor_node->next;
	}
	if (reactor_node == NULL)
		throw ReactorNotFoundException(reactor_id);

	// Update the X and Y coordinates using the values in config_ptr (if found).
	std::string x_coord;
	if (ConfigManager::getConfigOption(Reactor::X_COORDINATE_ELEMENT_NAME, x_coord, config_ptr))
		if (! ConfigManager::updateConfigOption(Reactor::X_COORDINATE_ELEMENT_NAME, x_coord, reactor_node))
			throw UpdateConfigOptionException(reactor_id);
	std::string y_coord;
	if (ConfigManager::getConfigOption(Reactor::Y_COORDINATE_ELEMENT_NAME, y_coord, config_ptr))
		if (! ConfigManager::updateConfigOption(Reactor::Y_COORDINATE_ELEMENT_NAME, y_coord, reactor_node))
			throw UpdateConfigOptionException(reactor_id);

	// Update any proxy coordinates found in config_ptr.
	const std::string prefix_1 = "Proxy_X_";
	const std::string prefix_2 = "Proxy_Y_";
	const xmlChar* xmlchar_prefix_1_ptr = reinterpret_cast<const xmlChar*>(prefix_1.c_str());
	const xmlChar* xmlchar_prefix_2_ptr = reinterpret_cast<const xmlChar*>(prefix_2.c_str());
	for (xmlNodePtr cur_node = config_ptr; cur_node != NULL; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			if (xmlStrncmp(cur_node->name, xmlchar_prefix_1_ptr, prefix_1.length()) == 0
				|| xmlStrncmp(cur_node->name, xmlchar_prefix_2_ptr, prefix_2.length()) == 0)
			{
				// The current element tag starts with "Proxy_X_" or "Proxy_Y_", so update reactor_node using its content.
				xmlChar* new_coord_ptr = xmlNodeGetContent(cur_node);
				if (new_coord_ptr != NULL) {
					if (! ConfigManager::updateConfigOption(reinterpret_cast<const char*>(cur_node->name),
															reinterpret_cast<const char*>(new_coord_ptr),
															reactor_node))
						throw UpdateConfigOptionException(reactor_id);
					xmlFree(new_coord_ptr);
				}
			}
		}
	}

	saveConfigFile();
}

std::string ReactionEngine::addReactor(const xmlNodePtr config_ptr)
{
	return PluginConfig<Reactor>::addPlugin(config_ptr);
}

void ReactionEngine::removeReactor(const std::string& reactor_id)
{
	std::unique_lock<std::mutex> engine_lock(m_mutex);

	Reactor *reactor_ptr = m_plugins.get(reactor_id);
	if (reactor_ptr == NULL)
		throw ReactorNotFoundException(reactor_id);

	// If the Reactor is still running, stop it.
	if (reactor_ptr->isRunning()) {
		stopReactor(reactor_id);
	}

	// disconnect any Reactor connections involving the Reactor being removed
	ReactorConnectionList::iterator reactor_i = m_reactor_connections.begin();
	while (reactor_i != m_reactor_connections.end()) {
		ReactorConnectionList::iterator current_i = reactor_i++;
		if (current_i->m_from_id == reactor_id || current_i->m_to_id == reactor_id) {
			// remove the connection
			removeConnectionNoLock(current_i->m_from_id, current_i->m_to_id);
			removeConnectionConfigNoLock(current_i->m_from_id, current_i->m_to_id);
			m_reactor_connections.erase(current_i);
		}
	}
	
	// disconnect any temporary output connections involving the Reactor being removed
	TempConnectionList::iterator connection_i = m_temp_connections.begin();
	while (connection_i != m_temp_connections.end()) {
		TempConnectionList::iterator current_i = connection_i++;
		if (current_i->m_reactor_id == reactor_id) {
			if (current_i->m_output_connection) {
				// remove the output connection from the Reactor
				removeConnectionNoLock(current_i->m_reactor_id, current_i->m_connection_id);
			}
			// send notification that the Reactor is being removed
			current_i->m_removed_handler();
			// remove the connection
			m_temp_connections.erase(current_i);
		}
	}

	// convert PluginNotFound exceptions into ReactorNotFound exceptions
	try {
		// remove the Reactor object
		engine_lock.unlock();
		PluginConfig<Reactor>::removePlugin(reactor_id);
	} catch (PluginManager<Reactor>::PluginNotFoundException&) {
		throw ReactorNotFoundException(reactor_id);
	}
}

Reactor *ReactionEngine::addTempConnectionIn(const std::string& reactor_id, 
											 const std::string& connection_id,
											 const std::string& connection_info,
											 std::function<void()> removed_handler)
{
	// make sure that the plug-in configuration file is open
	if (! configIsOpen())
		throw ConfigNotOpenException(getConfigFile());
	
	// get a pointer to the Reactor to return to the caller
	std::lock_guard<std::mutex> engine_lock(m_mutex);
	Reactor *reactor_ptr = m_plugins.get(reactor_id);
	if (reactor_ptr == NULL)
		throw ReactorNotFoundException(reactor_id);

	// keep track of the temporary connection
	m_temp_connections.push_back(TempConnection(false, reactor_id, connection_id,
												connection_info, removed_handler));
	
	PION_LOG_DEBUG(m_logger, "Added temporary Reactor input connection: "
				   << reactor_id << " <- " << connection_info);
	
	return reactor_ptr;
}

void ReactionEngine::addTempConnectionOut(const std::string& reactor_id, 
										  const std::string& connection_id,
										  const std::string& connection_info,
										  Reactor::EventHandler connection_handler)
{
	// make sure that the plug-in configuration file is open
	if (! configIsOpen())
		throw ConfigNotOpenException(getConfigFile());
	
	// connect the Reactor to the connection handler
	std::lock_guard<std::mutex> engine_lock(m_mutex);
	Reactor *reactor_ptr = m_plugins.get(reactor_id);
	if (reactor_ptr == NULL)
		throw ReactorNotFoundException(reactor_id);
	reactor_ptr->addConnection(connection_id, connection_handler);

	// if the Reactor is removed, send a null event to the connection
	EventPtr null_event_ptr;
	std::function<void()>	removed_handler(std::bind(connection_handler, null_event_ptr));
													
	// keep track of the temporary connection
	m_temp_connections.push_back(TempConnection(true, reactor_id, connection_id,
												connection_info, removed_handler));
	
	PION_LOG_DEBUG(m_logger, "Added temporary Reactor output connection: "
				   << reactor_id << " -> " << connection_info);
}
	
void ReactionEngine::removeTempConnection(const std::string& connection_id)
{ 
	// make sure that the plug-in configuration file is open
	if (! configIsOpen())
		throw ConfigNotOpenException(getConfigFile());

	// some variables to keep track of for later
	bool type_is_output = true;
	std::string reactor_id;
	std::string connection_info;

	// remove the connection from memory structures
	std::lock_guard<std::mutex> engine_lock(m_mutex);
	for (TempConnectionList::iterator i = m_temp_connections.begin();
		 i != m_temp_connections.end(); ++i)
	{
		if (i->m_connection_id == connection_id) {
			type_is_output = i->m_output_connection;
			reactor_id = i->m_reactor_id;
			connection_info = i->m_connection_info;
			m_temp_connections.erase(i);
			break;
		}
	}
	
	if (! reactor_id.empty()) {
		if (type_is_output) {
			// remove the output connection from the Reactor
			removeConnectionNoLock(reactor_id, connection_id);

			PION_LOG_DEBUG(m_logger, "Removed temporary Reactor output connection: "
						   << reactor_id << " -> " << connection_info);
		} else {
			PION_LOG_DEBUG(m_logger, "Removed temporary Reactor input connection: "
						   << reactor_id << " <- " << connection_info);
		}
	}
}

void ReactionEngine::startReactor(const std::string& reactor_id)
{
	// make sure that the plug-in configuration file is open
	if (! configIsOpen())
		throw ConfigNotOpenException(getConfigFile());

	// Find the Reactor node of the configuration with the specified ID.
	xmlNodePtr reactor_node = m_config_node_ptr->children;
	while ( (reactor_node = ConfigManager::findConfigNodeByName(Reactor::REACTOR_ELEMENT_NAME, reactor_node)) != NULL) {
		std::string node_id;
		getNodeId(reactor_node, node_id);
		if (node_id == reactor_id)
			break;
		reactor_node = reactor_node->next;
	}
	if (reactor_node == NULL)
		throw ReactorNotFoundException(reactor_id);

	// Start the reactor.
	m_plugins.run(reactor_id, std::bind(&Reactor::start, std::placeholders::_1));

	// Update the Reactor's run status and save the configuration file.
	if (! ConfigManager::updateConfigOption(Reactor::RUNNING_ELEMENT_NAME, "true", reactor_node))
		throw UpdateConfigOptionException(reactor_id);
	saveConfigFile();
}

void ReactionEngine::stopReactor(const std::string& reactor_id)
{
	// make sure that the plug-in configuration file is open
	if (! configIsOpen())
		throw ConfigNotOpenException(getConfigFile());

	// Find the Reactor node of the configuration with the specified ID.
	xmlNodePtr reactor_node = m_config_node_ptr->children;
	while ( (reactor_node = ConfigManager::findConfigNodeByName(Reactor::REACTOR_ELEMENT_NAME, reactor_node)) != NULL) {
		std::string node_id;
		getNodeId(reactor_node, node_id);
		if (node_id == reactor_id)
			break;
		reactor_node = reactor_node->next;
	}
	if (reactor_node == NULL)
		throw ReactorNotFoundException(reactor_id);

	// Stop the reactor.
	m_plugins.run(reactor_id, std::bind(&Reactor::stop, std::placeholders::_1));

	// Update the Reactor's run status and save the configuration file.
	if (! ConfigManager::updateConfigOption(Reactor::RUNNING_ELEMENT_NAME, "false", reactor_node))
		throw UpdateConfigOptionException(reactor_id);
	saveConfigFile();
}

std::string ReactionEngine::addReactorConnection(const std::string& from_id,
												 const std::string& to_id)
{
	// make sure that the plug-in configuration file is open
	if (! configIsOpen())
		throw ConfigNotOpenException(getConfigFile());
	
	// generate a unique identifier to represent the connection
	const std::string connection_id(ConfigManager::createUUID());
	
	// add the connection to memory structures
	std::lock_guard<std::mutex> engine_lock(m_mutex);
	addConnectionNoLock(connection_id, from_id, to_id);
	
	// add the connection to the config file
	
	// create a new node for the connection and add it to the XML config document
	xmlNodePtr connection_node = xmlNewNode(NULL, reinterpret_cast<const xmlChar*>(CONNECTION_ELEMENT_NAME.c_str()));
	if (connection_node == NULL)
		throw AddConnectionConfigException(getConnectionAsText(from_id, to_id));
	if ((connection_node=xmlAddChild(m_config_node_ptr, connection_node)) == NULL) {
		xmlFreeNode(connection_node);
		throw AddConnectionConfigException(getConnectionAsText(from_id, to_id));
	}

	// add the "id" attribute
	if (xmlNewProp(connection_node,
				   reinterpret_cast<const xmlChar*>(ID_ATTRIBUTE_NAME.c_str()),
				   reinterpret_cast<const xmlChar*>(connection_id.c_str())) == NULL)
		throw AddConnectionConfigException(getConnectionAsText(from_id, to_id));
	
	// add a "Type" child element to the connection
	if (xmlNewTextChild(connection_node, NULL,
						reinterpret_cast<const xmlChar*>(TYPE_ELEMENT_NAME.c_str()),
						reinterpret_cast<const xmlChar*>(CONNECTION_TYPE_REACTOR.c_str())) == NULL)
		throw AddConnectionConfigException(getConnectionAsText(from_id, to_id));
	
	// add a "From" child element to the connection
	if (xmlNewTextChild(connection_node, NULL,
						reinterpret_cast<const xmlChar*>(FROM_ELEMENT_NAME.c_str()),
						reinterpret_cast<const xmlChar*>(from_id.c_str())) == NULL)
		throw AddConnectionConfigException(getConnectionAsText(from_id, to_id));
	
	// add a "To" child element to the connection
	if (xmlNewTextChild(connection_node, NULL,
						reinterpret_cast<const xmlChar*>(TO_ELEMENT_NAME.c_str()),
						reinterpret_cast<const xmlChar*>(to_id.c_str())) == NULL)
		throw AddConnectionConfigException(getConnectionAsText(from_id, to_id));
	
	// save the new XML config file
	saveConfigFile();
	
	PION_LOG_DEBUG(m_logger, "Added reactor connection: " << from_id << " -> " << to_id);
	
	return connection_id;
}

std::string ReactionEngine::addReactorConnection(const xmlNodePtr config_ptr)
{
	// get the "From" value
	std::string from_id;
	if (! ConfigManager::getConfigOption(FROM_ELEMENT_NAME, from_id, config_ptr))
		throw BadConnectionConfigException();
	
	// get the "To" value
	std::string to_id;
	if (! ConfigManager::getConfigOption(TO_ELEMENT_NAME, to_id, config_ptr))
		throw BadConnectionConfigException();
	
	// call addReactorConnection() to do the real work
	return addReactorConnection(from_id, to_id);
}

void ReactionEngine::removeReactorConnection(const std::string& from_id,
											 const std::string& to_id)
{
	// make sure that the plug-in configuration file is open
	if (! configIsOpen())
		throw ConfigNotOpenException(getConfigFile());

	// remove the connection from memory structures
	std::lock_guard<std::mutex> engine_lock(m_mutex);
	removeConnectionNoLock(from_id, to_id);
	for (ReactorConnectionList::iterator i = m_reactor_connections.begin();
		 i != m_reactor_connections.end(); ++i)
	{
		if (i->m_from_id == from_id && i->m_to_id == to_id) {
			m_reactor_connections.erase(i);
			break;
		}
	}
	
	// remove the connection from the config file
	removeConnectionConfigNoLock(from_id, to_id);
	
	PION_LOG_DEBUG(m_logger, "Removed reactor connection: " << from_id << " -> " << to_id);
}
	
void ReactionEngine::removeReactorConnection(const std::string& connection_id)
{
	// make sure that the plug-in configuration file is open
	if (! configIsOpen())
		throw ConfigNotOpenException(getConfigFile());
	
	// find the connection in our memory structures
	std::lock_guard<std::mutex> engine_lock(m_mutex);
	ReactorConnectionList::iterator i = m_reactor_connections.begin();
	while (i != m_reactor_connections.end()) {
		if (i->m_connection_id == connection_id)
			break;
		++i;
	}
	if (i == m_reactor_connections.end())
		throw ConnectionNotFoundException(connection_id);
	
	// remove the connection between the reactors
	removeConnectionNoLock(i->m_from_id, i->m_to_id);
	
	// remove the connection from the config file
	removeConnectionConfigNoLock(i->m_from_id, i->m_to_id);
	
	PION_LOG_DEBUG(m_logger, "Removed reactor connection: "
				   << i->m_from_id << " -> " << i->m_to_id);

	// remove the connection from memory structures
	m_reactor_connections.erase(i);
}

void ReactionEngine::addConnectionNoLock(const std::string& connection_id,
										 const std::string& from_id,
										 const std::string& to_id)
{
	// find the source Reactor
	Reactor *from_ptr = m_plugins.get(from_id);
	if (from_ptr == NULL)
		throw ReactorNotFoundException(from_id);

	// find the destination Reactor
	Reactor *to_ptr = m_plugins.get(to_id);
	if (to_ptr == NULL)
		throw ReactorNotFoundException(to_id);
	
	// connect them together
	from_ptr->addConnection(*to_ptr);

	// keep track of all Reactor connections
	m_reactor_connections.push_back(ReactorConnection(connection_id, from_id, to_id));
}

void ReactionEngine::removeConnectionNoLock(const std::string& reactor_id,
											const std::string& connection_id)
{
	// find the source Reactor
	Reactor *from_ptr = m_plugins.get(reactor_id);
	if (from_ptr == NULL)
		throw ReactorNotFoundException(reactor_id);

	// remove the connection
	from_ptr->removeConnection(connection_id);
}

void ReactionEngine::removeConnectionConfigNoLock(const std::string& from_id,
												  const std::string& to_id)
{
	// step through each connection to find the right one
	xmlNodePtr connection_node = m_config_node_ptr->children;
	while ( (connection_node = ConfigManager::findConfigNodeByName(CONNECTION_ELEMENT_NAME, connection_node)) != NULL)
	{
		// get the "From" and "To" connection identifiers
		std::string node_from_id;
		std::string node_to_id;
		ConfigManager::getConfigOption(FROM_ELEMENT_NAME, node_from_id, connection_node->children);
		ConfigManager::getConfigOption(TO_ELEMENT_NAME, node_to_id, connection_node->children);
		
		// check if this is the connection we want to remove
		if (from_id == node_from_id && to_id == node_to_id) {
			// found it!
			break;
		}
		
		// step to the next Reactor connection
		connection_node = connection_node->next;
	}
	
	// throw exception if connection was not found
	if (connection_node == NULL)
		throw RemoveConnectionConfigException(getConnectionAsText(from_id, to_id));
	
	// remove the connection from the XML tree
	xmlUnlinkNode(connection_node);
	xmlFreeNode(connection_node);
	
	// save the new XML config file
	saveConfigFile();
}

std::string ReactionEngine::addWorkspace(const char* content_buf, std::size_t content_length)
{
	// make sure that the plug-in configuration file is open
	if (! configIsOpen())
		throw ConfigNotOpenException(getConfigFile());

	// generate a unique identifier to represent the Workspace
	const std::string workspace_id(ConfigManager::createUUID());

	std::lock_guard<std::mutex> engine_lock(m_mutex);

	// create a new node for the Workspace and add it to the XML config document
	xmlNodePtr workspace_node = xmlNewNode(NULL, reinterpret_cast<const xmlChar*>(Reactor::WORKSPACE_ELEMENT_NAME.c_str()));
	if (workspace_node == NULL)
		throw AddWorkspaceConfigException();
	if ((workspace_node = xmlAddChild(m_config_node_ptr, workspace_node)) == NULL) {
		xmlFreeNode(workspace_node);
		throw AddWorkspaceConfigException();
	}

	// add the "id" attribute
	if (xmlNewProp(workspace_node,
				   reinterpret_cast<const xmlChar*>(ID_ATTRIBUTE_NAME.c_str()),
				   reinterpret_cast<const xmlChar*>(workspace_id.c_str())) == NULL)
		throw AddWorkspaceConfigException();

	setWorkspaceConfig(workspace_node, content_buf, content_length);

	// save the new XML config file
	saveConfigFile();

	PION_LOG_DEBUG(m_logger, "Added Reactor Workspace: " << workspace_id);

	return workspace_id;
}

void ReactionEngine::removeReactorsFromWorkspace(const std::string& workspace_id)
{
	// make sure that the plug-in configuration file is open
	if (! configIsOpen())
		throw ConfigNotOpenException(getConfigFile());

	if (! hasWorkspace(workspace_id))
		throw WorkspaceNotFoundException(workspace_id);

	std::unique_lock<std::mutex> engine_lock(m_mutex);

	// Make a list of IDs of all the Reactors in the Workspace.
	std::vector<std::string> reactors_in_workspace;

	// Step through each Reactor config node.
	xmlNodePtr reactor_node = m_config_node_ptr->children;
	while ( (reactor_node = findConfigNodeByName(Reactor::REACTOR_ELEMENT_NAME, reactor_node)) != NULL) {
		std::string workspace_str;
		if (ConfigManager::getConfigOption(Reactor::WORKSPACE_ELEMENT_NAME, workspace_str, reactor_node->children)) {
			if (workspace_str == workspace_id) {
				std::string reactor_id;
				if (! getNodeId(reactor_node, reactor_id))
					throw EmptyPluginIdException(getConfigFile());
				reactors_in_workspace.push_back(reactor_id);
			}
		}

		// look for more reactor config nodes
		reactor_node = reactor_node->next;
	}

	// Remove all the Reactors that were found.
	engine_lock.unlock();
	std::for_each(reactors_in_workspace.begin(), reactors_in_workspace.end(), std::bind(&ReactionEngine::removeReactor, this, std::placeholders::_1));
}

void ReactionEngine::removeWorkspace(const std::string& workspace_id)
{
	// make sure that the plug-in configuration file is open
	if (! configIsOpen())
		throw ConfigNotOpenException(getConfigFile());

	// Find existing Workspace configuration.
	std::lock_guard<std::mutex> engine_lock(m_mutex);
	xmlNodePtr workspace_node = findConfigNodeByAttr(Reactor::WORKSPACE_ELEMENT_NAME,
													 ID_ATTRIBUTE_NAME,
													 workspace_id,
													 m_config_node_ptr->children);

	if (workspace_node == NULL)
		throw WorkspaceNotFoundException(workspace_id);

	// Determine whether the Workspace is empty by stepping through the Reactor config nodes and checking the Workspace IDs.
	bool empty = true;
	xmlNodePtr reactor_node = m_config_node_ptr->children;
	while ( (reactor_node = findConfigNodeByName(Reactor::REACTOR_ELEMENT_NAME, reactor_node)) != NULL) {
		std::string workspace_str;
		if (ConfigManager::getConfigOption(Reactor::WORKSPACE_ELEMENT_NAME, workspace_str, reactor_node->children)) {
			if (workspace_str == workspace_id) {
				empty = false;
				break;
			}
		}

		// look for more reactor config nodes
		reactor_node = reactor_node->next;
	}

	if (! empty)
		throw RemoveNonEmptyWorkspaceException(workspace_id);

	// remove the connection from the XML tree
	xmlUnlinkNode(workspace_node);
	xmlFreeNode(workspace_node);

	// save the new XML config file
	saveConfigFile();

	PION_LOG_DEBUG(m_logger, "Removed Reactor Workspace: " << workspace_id);
}

void ReactionEngine::setWorkspaceConfig(const std::string& workspace_id, const char* content_buf, std::size_t content_length)
{
	// Find existing Workspace configuration.
	std::lock_guard<std::mutex> engine_lock(m_mutex);
	xmlNodePtr workspace_node = findConfigNodeByAttr(Reactor::WORKSPACE_ELEMENT_NAME,
													 ID_ATTRIBUTE_NAME,
													 workspace_id,
													 m_config_node_ptr->children);

	if (workspace_node == NULL)
		throw WorkspaceNotFoundException(workspace_id);

	// update the configuration in the XML tree
	setWorkspaceConfig(workspace_node, content_buf, content_length);

	// save the new XML config file
	saveConfigFile();
}

void ReactionEngine::setWorkspaceConfig(xmlNodePtr workspace_node, const char* content_buf, std::size_t content_length)
{
	// extract the XML config info from the content buffer
	xmlNodePtr config_ptr = ConfigManager::createResourceConfig(Reactor::WORKSPACE_ELEMENT_NAME,
																content_buf,
																content_length);
	if (config_ptr == NULL)
		throw BadWorkspaceConfigException();

	// try to get the "Name" value, and if found, update the "Name" element in the Workspace node
	std::string name;
	if (ConfigManager::getConfigOption(ConfigManager::NAME_ELEMENT_NAME, name, config_ptr)) {
		if (! ConfigManager::updateConfigOption(ConfigManager::NAME_ELEMENT_NAME, name, workspace_node)) {
			xmlFreeNodeList(config_ptr);
			throw SetWorkspaceConfigException();
		}
	}

	// try to get the "Comment" value, and if found, update the "Comment" element in the Workspace node
	std::string comment;
	if (ConfigManager::getConfigOption(ConfigManager::COMMENT_ELEMENT_NAME, comment, config_ptr)) {
		if (! ConfigManager::updateConfigOption(ConfigManager::COMMENT_ELEMENT_NAME, comment, workspace_node)) {
			xmlFreeNodeList(config_ptr);
			throw SetWorkspaceConfigException();
		}
	}
	
	xmlFreeNodeList(config_ptr);
}

void ReactionEngine::stopNoLock(void)
{
	if (m_is_running) {
		PION_LOG_INFO(m_logger, "Stopping the ReactionEngine");

		// terminate all temporary connections
		for (TempConnectionList::iterator i = m_temp_connections.begin();
			 i != m_temp_connections.end(); ++i)
		{
			if (i->m_output_connection) {
				// remove the output connection from the Reactor
				removeConnectionNoLock(i->m_reactor_id, i->m_connection_id);
			}
			// send notification that the Reactor is being removed
			i->m_removed_handler();
		}
		m_temp_connections.clear();
		
		// stop all reactors
		PION_LOG_INFO(m_logger, "Stopping all reactors");
		m_plugins.run(std::bind(&Reactor::stop, std::placeholders::_1));

		// notify the thread scheduler that we no longer need it
		m_scheduler.removeActiveUser();

		// clear out the dummy node in the PionLockedQueue because otherwise
		// it still has a copy of the last node in the queue.  Since this queue
		// stores function objects referencing other code, and likely code which
		// is contained within a plugin of one kind or another, it's possible
		// that the plugin code would otherwise get released and then the
		// destructor on the function object would be called when the queue
		// destructs.  This seems to cause crashes, apparently due to bugs in
		// boost.function...
		m_scheduler.clear();

		m_is_running = false;
	}
}

void ReactionEngine::writeStatsXML(std::ostream& out, const std::string& only_id, const bool details)
{
	writeBeginPionStatsXML(out);

	const Reactor::QueryBranches branches;
	const Reactor::QueryParams qp;
	std::lock_guard<std::mutex> engine_lock(m_mutex);

	if (only_id.empty()) {
		// step through each reactor configured
		std::string reactor_id;
		Reactor *reactor_ptr;
		xmlNodePtr reactor_node = m_config_node_ptr->children;
		while ( (reactor_node = ConfigManager::findConfigNodeByName(Reactor::REACTOR_ELEMENT_NAME, reactor_node)) != NULL)
		{
			// get a pointer to the reactor
			if (getNodeId(reactor_node, reactor_id)
				&& (reactor_ptr = m_plugins.get(reactor_id)) != NULL)
			{
				// write reactor statistics
				if (details) {
					reactor_ptr->query(out, branches, qp);
				} else {
					reactor_ptr->writeStatsXML(out);
				}
			}
			// step to the next Reactor
			reactor_node = reactor_node->next;
		}

		// write total operations
		out << "\t<" << TOTAL_OPS_ELEMENT_NAME << '>' << getTotalOperations()
			<< "</" << TOTAL_OPS_ELEMENT_NAME << '>' << std::endl;

		// write events queued
		out << "\t<" << EVENTS_QUEUED_ELEMENT_NAME << '>' << getEventsQueued()
			<< "</" << EVENTS_QUEUED_ELEMENT_NAME << '>' << std::endl;
			
	} else {
		// get a pointer to the reactor
		Reactor *reactor_ptr = m_plugins.get(only_id);

		// write reactor statistics
		if (details) {
			reactor_ptr->query(out, branches, qp);
		} else {
			reactor_ptr->writeStatsXML(out);
		}
	}
	
	writeEndPionStatsXML(out);
}

void ReactionEngine::writeConnectionsXML(std::ostream& out,
										 const std::string& only_id) const
{
	ConfigManager::writeBeginPionConfigXML(out);
	
	bool found_one = false;
	std::lock_guard<std::mutex> engine_lock(m_mutex);

	// iterate through Reactor connections
	for (ReactorConnectionList::const_iterator reactor_i = m_reactor_connections.begin();
		 reactor_i != m_reactor_connections.end(); ++reactor_i)
	{
		if (only_id.empty() || reactor_i->m_connection_id == only_id
			|| reactor_i->m_from_id == only_id || reactor_i->m_to_id == only_id)
		{
			found_one = true;
			out << "\t<" << CONNECTION_ELEMENT_NAME << ' ' << ID_ATTRIBUTE_NAME
				<< "=\"" << reactor_i->m_connection_id << "\">" << std::endl
				<< "\t\t<" << TYPE_ELEMENT_NAME << '>' << CONNECTION_TYPE_REACTOR
				<< "</" << TYPE_ELEMENT_NAME << '>' << std::endl
				<< "\t\t<" << FROM_ELEMENT_NAME << '>' << reactor_i->m_from_id << "</"
				<< FROM_ELEMENT_NAME << '>' << std::endl
				<< "\t\t<" << TO_ELEMENT_NAME << '>' << reactor_i->m_to_id << "</"
				<< TO_ELEMENT_NAME << '>' << std::endl
				<< "\t</" << CONNECTION_ELEMENT_NAME << '>' << std::endl;
		}
	}

	// iterate through temporary connections
	for (TempConnectionList::const_iterator temp_i = m_temp_connections.begin();
		 temp_i != m_temp_connections.end(); ++temp_i)
	{
		if (only_id.empty() || temp_i->m_connection_id == only_id
			|| temp_i->m_reactor_id == only_id)
		{
			found_one = true;
			out << "\t<" << CONNECTION_ELEMENT_NAME << ' ' << ID_ATTRIBUTE_NAME
				<< "=\"" << temp_i->m_connection_id << "\">" << std::endl
				<< "\t\t<" << TYPE_ELEMENT_NAME << '>';
			if (temp_i->m_output_connection) {
				out << CONNECTION_TYPE_OUTPUT << "</" << TYPE_ELEMENT_NAME << '>' << std::endl
					<< "\t\t<" << FROM_ELEMENT_NAME << '>' << temp_i->m_reactor_id
					<< "</" << FROM_ELEMENT_NAME << '>' << std::endl
					<< "\t\t<" << TO_ELEMENT_NAME << '>' << temp_i->m_connection_info;
			} else {
				out << CONNECTION_TYPE_INPUT << "</" << TYPE_ELEMENT_NAME << '>' << std::endl
					<< "\t\t<" << FROM_ELEMENT_NAME << '>' << temp_i->m_connection_info
					<< "</" << FROM_ELEMENT_NAME << '>' << std::endl
					<< "\t\t<" << TO_ELEMENT_NAME << '>' << temp_i->m_reactor_id;
			}
			out << "</" << TO_ELEMENT_NAME << '>' << std::endl
				<< "\t</" << CONNECTION_ELEMENT_NAME << '>' << std::endl;
		}
	}

	if (! found_one && ! only_id.empty())
		throw ConnectionNotFoundException(only_id);
	
	ConfigManager::writeEndPionConfigXML(out);
}

bool ReactionEngine::writeWorkspaceXML(std::ostream& out,
									   const std::string& workspace_id) const
{
	// find the Workspace element with the specified ID in the XML config document
	std::lock_guard<std::mutex> engine_lock(m_mutex);
	xmlNodePtr workspace_node = findConfigNodeByAttr(Reactor::WORKSPACE_ELEMENT_NAME,
													 ID_ATTRIBUTE_NAME,
													 workspace_id,
													 m_config_node_ptr->children);
	if (workspace_node == NULL)
		return false;

	// found it
	ConfigManager::writeBeginPionConfigXML(out);
	ConfigManager::writeConfigXML(out, workspace_node, false);
	ConfigManager::writeEndPionConfigXML(out);

	return true;
}

void ReactionEngine::writeWorkspacesXML(std::ostream& out) const
{
	std::lock_guard<std::mutex> engine_lock(m_mutex);
	ConfigManager::writeBeginPionConfigXML(out);

	// step through Workspace configurations
	xmlNodePtr workspace_node = m_config_node_ptr->children;
	while ( (workspace_node = ConfigManager::findConfigNodeByName(Reactor::WORKSPACE_ELEMENT_NAME, workspace_node)) != NULL) {
		ConfigManager::writeConfigXML(out, workspace_node, false);
		workspace_node = workspace_node->next;
	}

	ConfigManager::writeEndPionConfigXML(out);
}

bool ReactionEngine::hasWorkspace(const std::string& workspace_id) const
{
	xmlNodePtr workspace_node = findConfigNodeByAttr(Reactor::WORKSPACE_ELEMENT_NAME,
													 ID_ATTRIBUTE_NAME,
													 workspace_id,
													 m_config_node_ptr->children);
	return workspace_node != NULL;
}

bool ReactionEngine::writeWorkspaceLimitedConfigXML(std::ostream& out, const std::string& workspace_id) const {
	// find the Workspace element with the specified ID in the XML config document
	std::lock_guard<std::mutex> engine_lock(m_mutex);
	xmlNodePtr workspace_node = findConfigNodeByAttr(Reactor::WORKSPACE_ELEMENT_NAME,
													 ID_ATTRIBUTE_NAME,
													 workspace_id,
													 m_config_node_ptr->children);
	if (workspace_node == NULL)
		return false;

	ConfigManager::writeBeginPionConfigXML(out);

	// step through each Reactor config node
	xmlNodePtr reactor_node = m_config_node_ptr->children;
	while ( (reactor_node = findConfigNodeByName(Reactor::REACTOR_ELEMENT_NAME, reactor_node)) != NULL) {
		std::string workspace_str;
		if (ConfigManager::getConfigOption(Reactor::WORKSPACE_ELEMENT_NAME, workspace_str, reactor_node->children)) {
			if (workspace_str == workspace_id)
				ConfigManager::writeConfigXML(out, reactor_node);
		}

		// look for more reactor config nodes
		reactor_node = reactor_node->next;
	}

	// Step through each Connection config node, and if it has either a source Reactor or sink Reactor
	// in the specified Workspace, output the configuration.
	xmlNodePtr connection_node = m_config_node_ptr->children;
	while ( (connection_node = findConfigNodeByName(CONNECTION_ELEMENT_NAME, connection_node)) != NULL) {
		std::string source_reactor_id;
		if (ConfigManager::getConfigOption(FROM_ELEMENT_NAME, source_reactor_id, connection_node->children)) {
			const Reactor* source_reactor_ptr = m_plugins.get(source_reactor_id);
			if (source_reactor_ptr == NULL)
				throw ReactorNotFoundException(source_reactor_id);

			if (source_reactor_ptr->getWorkspace() == workspace_id) {
				ConfigManager::writeConfigXML(out, connection_node);
			} else {
				std::string sink_reactor_id;
				if (ConfigManager::getConfigOption(TO_ELEMENT_NAME, sink_reactor_id, connection_node->children)) {
					const Reactor* sink_reactor_ptr = m_plugins.get(sink_reactor_id);
					if (sink_reactor_ptr == NULL)
						throw ReactorNotFoundException(sink_reactor_id);

					if (sink_reactor_ptr->getWorkspace() == workspace_id)
						ConfigManager::writeConfigXML(out, connection_node);
				}
			}
		}

		// look for more Connection config nodes
		connection_node = connection_node->next;
	}

	ConfigManager::writeEndPionConfigXML(out);

	return true;
}

bool ReactionEngine::creationAllowed(xmlNodePtr permission_config_ptr, xmlNodePtr config_ptr) const
{
	if (permission_config_ptr == NULL)
		return false;

	if (ConfigManager::findConfigNodeByContent(UNRESTRICTED_ELEMENT_NAME, "true", permission_config_ptr->children))
		return true;	// The User has full privileges for Reactors.

	if (config_ptr == NULL)
		return false;	// Since the User is restricted, the actual configuration is needed to determine permission.

	// Handle according to whether config_ptr is for a Reactor, Connection or Workspace.
	// Since we only have the children of the configuration node, we'll check for distinguishing child nodes.
	if (findConfigNodeByName(PLUGIN_ELEMENT_NAME, config_ptr) != NULL) {
		// config_ptr is for a Reactor

		// check if creating a protected reactor type
		std::string reactor_type;
		getConfigOption(PLUGIN_ELEMENT_NAME, reactor_type, config_ptr);
		if (reactor_type == "SnifferReactor")
			return false;	// admin or full privileges only can create these

		// otherwise, check workspace permissions
		std::string workspace_id;
		if (! ConfigManager::getConfigOption(Reactor::WORKSPACE_ELEMENT_NAME, workspace_id, config_ptr))
			throw Reactor::MissingWorkspaceException();
		if (ConfigManager::findConfigNodeByContent(WORKSPACE_QUALIFIER_ELEMENT_NAME, workspace_id, permission_config_ptr->children))
			return true;	// The User has permission to create Reactors in the specified Workspace.
			
	} else if (findConfigNodeByName(FROM_ELEMENT_NAME, config_ptr) != NULL) {
		// config_ptr is for a Connection

		std::string source_reactor_id;
		if (! ConfigManager::getConfigOption(FROM_ELEMENT_NAME, source_reactor_id, config_ptr))
			throw BadConnectionConfigException();
		std::string sink_reactor_id;
		if (! ConfigManager::getConfigOption(TO_ELEMENT_NAME, sink_reactor_id, config_ptr))
			throw BadConnectionConfigException();

		const Reactor* source_reactor_ptr = m_plugins.get(source_reactor_id);
		if (source_reactor_ptr == NULL)
			throw ReactorNotFoundException(source_reactor_id);
		const Reactor* sink_reactor_ptr = m_plugins.get(sink_reactor_id);
		if (sink_reactor_ptr == NULL)
			throw ReactorNotFoundException(sink_reactor_id);

		if (ConfigManager::findConfigNodeByContent(WORKSPACE_QUALIFIER_ELEMENT_NAME,
												   source_reactor_ptr->getWorkspace(),
												   permission_config_ptr->children) 
			&& ConfigManager::findConfigNodeByContent(WORKSPACE_QUALIFIER_ELEMENT_NAME,
													  sink_reactor_ptr->getWorkspace(),
													  permission_config_ptr->children)) {
			return true;	// The User has permission to modify both Reactors, so a Connection between them is allowed.
		}

	} else {
		// config_ptr is for a Workspace

		return false;	// Creating a new Workspace requires full privileges for Reactors.
	}

	return false;
}

bool ReactionEngine::updateAllowed(xmlNodePtr permission_config_ptr, const std::string& id, xmlNodePtr config_ptr) const
{
	if (permission_config_ptr == NULL)
		return false;

	if (ConfigManager::findConfigNodeByContent(UNRESTRICTED_ELEMENT_NAME, "true", permission_config_ptr->children))
		return true;	// The User has full privileges for Reactors.

	if (hasPlugin(id)) {
		// id is a Reactor ID, so we need to check the Workspace IDs of both the current and requested configurations.

		if (config_ptr == NULL)
			return false;	// Since the User is restricted, the actual configuration is needed to determine permission.

		const Reactor* reactor_ptr = m_plugins.get(id);
		if (reactor_ptr == NULL)
			throw ReactorNotFoundException(id);

		// Does the User have permission for the Workspace that the specified Reactor is currently in?
		if (ConfigManager::findConfigNodeByContent(ReactionEngine::WORKSPACE_QUALIFIER_ELEMENT_NAME, reactor_ptr->getWorkspace(), permission_config_ptr->children)) {
			// Does the User have permission for the Workspace specified in the new Reactor configuration?
			std::string workspace_id;
			if (! ConfigManager::getConfigOption(Reactor::WORKSPACE_ELEMENT_NAME, workspace_id, config_ptr))
				throw Reactor::MissingWorkspaceException();
			if (ConfigManager::findConfigNodeByContent(ReactionEngine::WORKSPACE_QUALIFIER_ELEMENT_NAME, workspace_id, permission_config_ptr->children))
				return true;
		}
	} else if (hasWorkspace(id)) {
		// id is a Workspace ID, so we just need to check whether the user has permission for this Workspace

		if (ConfigManager::findConfigNodeByContent(ReactionEngine::WORKSPACE_QUALIFIER_ELEMENT_NAME, id, permission_config_ptr->children))
			return true;
	}

	return false;
}

bool ReactionEngine::removalAllowed(xmlNodePtr permission_config_ptr, const std::string& id) const
{
	if (permission_config_ptr == NULL)
		return false;

	if (ConfigManager::findConfigNodeByContent(UNRESTRICTED_ELEMENT_NAME, "true", permission_config_ptr->children))
		return true;	// The User has full privileges for Reactors.

	// Handle according to whether id is for a Reactor, Connection or Workspace.
	if (hasPlugin(id)) {
		// id is for a Reactor.

		std::string workspace_id = m_plugins.get(id)->getWorkspace();
		if (ConfigManager::findConfigNodeByContent(WORKSPACE_QUALIFIER_ELEMENT_NAME, workspace_id, permission_config_ptr->children))
			return true;	// The User has permission for the Workspace of the specified Reactor.
	} else if (hasWorkspace(id)) {
		// id is for a Workspace.

		if (ConfigManager::findConfigNodeByContent(WORKSPACE_QUALIFIER_ELEMENT_NAME, id, permission_config_ptr->children))
			return true;	// The User has permission for the specified Workspace.
	} else {
		// Check if id is for a Connection.

		ReactorConnectionList::const_iterator i = m_reactor_connections.begin();
		while (i != m_reactor_connections.end()) {
			if (i->m_connection_id == id)
				break;
			++i;
		}
		if (i == m_reactor_connections.end())
			return false;	// The id is not for a Reactor, Connection or Workspace, so don't grant permission.

		// id is for a Connection.

		const Reactor* source_reactor_ptr = m_plugins.get(i->m_from_id);
		if (source_reactor_ptr == NULL)
			throw ReactorNotFoundException(i->m_from_id);
		const Reactor* sink_reactor_ptr = m_plugins.get(i->m_to_id);
		if (sink_reactor_ptr == NULL)
			throw ReactorNotFoundException(i->m_to_id);

		if (ConfigManager::findConfigNodeByContent(WORKSPACE_QUALIFIER_ELEMENT_NAME,
												   source_reactor_ptr->getWorkspace(),
												   permission_config_ptr->children) 
			&& ConfigManager::findConfigNodeByContent(WORKSPACE_QUALIFIER_ELEMENT_NAME,
													  sink_reactor_ptr->getWorkspace(),
													  permission_config_ptr->children)) {
			return true;	// The User has permission to modify both Reactors, so removing a Connection between them is allowed.
		}
	}

	return false;
}

bool ReactionEngine::accessAllowed(xmlNodePtr permission_config_ptr, const std::string& reactor_id) const
{
	if (permission_config_ptr == NULL)
		return false;

	if (reactor_id.empty())
		return true;

	if (ConfigManager::findConfigNodeByContent(UNRESTRICTED_ELEMENT_NAME, "true", permission_config_ptr->children))
		return true;	// The User has full privileges for Reactors.

	// Get the Workspace of the specified Reactor.
	const Reactor* reactor_ptr = m_plugins.get(reactor_id);
	if (reactor_ptr == NULL)
		throw ReactorNotFoundException(reactor_id);
	std::string workspace_id = reactor_ptr->getWorkspace();

	// Does the User have permission for the Workspace?
	if (ConfigManager::findConfigNodeByContent(WORKSPACE_QUALIFIER_ELEMENT_NAME, workspace_id, permission_config_ptr->children))
		return true;

	return false;
}


}	// end namespace platform
}	// end namespace pion

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

#ifndef __PION_REACTIONENGINE_HEADER__
#define __PION_REACTIONENGINE_HEADER__

#include <functional>
#include <string>
#include <libxml/tree.h>
#include <pion/PionConfig.hpp>
#include <pion/PionException.hpp>
#include <pion/platform/Event.hpp>
#include <pion/platform/Reactor.hpp>
#include <pion/platform/PluginConfig.hpp>
#include <pion/platform/ReactionScheduler.hpp>

namespace pion {		// begin namespace pion
namespace platform {	// begin namespace platform (Pion Platform Library)

// forward declarations
class VocabularyManager;
class CodecFactory;
class ProtocolFactory;
class DatabaseManager;
	
///
/// ReactionEngine: manages all of the registered Reactors and connections,
///                 and routes Events between Reactors
///
class PION_PLATFORM_API ReactionEngine :
	public PluginConfig<Reactor>
{
public:

	/// exception thrown if we are unable to find a Reactor with the same identifier
	class ReactorNotFoundException : public PionException {
	public:
		ReactorNotFoundException(const std::string& reactor_id)
			: PionException("No reactors found for identifier: ", reactor_id) {}
	};
	
	/// exception thrown if we are unable to find a connection with the same identifier
	class ConnectionNotFoundException : public PionException {
	public:
		ConnectionNotFoundException(const std::string& connection_id)
			: PionException("No connections found for identifier: ", connection_id) {}
	};

	/// exception thrown if we are unable to find a Workspace with the specified identifier
	class WorkspaceNotFoundException : public PionException {
	public:
		WorkspaceNotFoundException(const std::string& workspace_id)
			: PionException("No Workspace found for identifier: ", workspace_id) {}
	};

	/// exception thrown when an attempt is made to remove a Workspace with Reactors in it
	class RemoveNonEmptyWorkspaceException : public PionException {
	public:
		RemoveNonEmptyWorkspaceException(const std::string& workspace_id)
			: PionException("The Workspace specified for removal was not empty: ", workspace_id) {}
	};

	/// exception thrown if the config file contains a Reactor connection with a missing identifier
	class EmptyConnectionIdException : public PionException {
	public:
		EmptyConnectionIdException(const std::string& config_file)
			: PionException("Configuration file includes a connection with an empty identifier: ", config_file) {}
	};

	/// exception thrown if the config file includes a connection with a bad or missing type
	class BadConnectionTypeException : public PionException {
	public:
		BadConnectionTypeException(const std::string& connection_id)
			: PionException("Bad connection type in configuration file: ", connection_id) {}
	};
	
	/// exception thrown if the config file includes a connection with a missing From element
	class EmptyFromException : public PionException {
	public:
		EmptyFromException(const std::string& connection_id)
			: PionException("Reactor configuration has a connection with empty From element: ", connection_id) {}
	};
	
	/// exception thrown if the config file includes a connection with a missing To element
	class EmptyToException : public PionException {
	public:
		EmptyToException(const std::string& connection_id)
			: PionException("Reactor configuration has a connection with empty To element: ", connection_id) {}
	};
	
	/// exception thrown if there is an error adding a Connection element to the config file
	class AddConnectionConfigException : public PionException {
	public:
		AddConnectionConfigException(const std::string& connection)
			: PionException("Unable to add a Connection to the Reactor configuration file: ", connection) {}
	};
	
	/// exception thrown if there is an error removing a Connection from the config file
	class RemoveConnectionConfigException : public PionException {
	public:
		RemoveConnectionConfigException(const std::string& connection)
			: PionException("Unable to remove a Connection from the Reactor configuration file: ", connection) {}
	};

	/// exception thrown if the configuration info for a new connection is invalid
	class BadConnectionConfigException : public std::exception {
	public:
		virtual const char* what() const throw() {
			return "New Reactor connection configuration is invalid";
		}
	};

	/// exception thrown if the configuration info for a new Workspace is invalid
	class BadWorkspaceConfigException : public std::exception {
	public:
		virtual const char* what() const throw() {
			return "New Reactor Workspace configuration is invalid";
		}
	};

	/// exception thrown if there is an error adding a Workspace element to the config file
	class AddWorkspaceConfigException : public PionException {
	public:
		AddWorkspaceConfigException()
			: PionException("Unable to add a Workspace to the Reactor configuration file") {}
	};

	/// exception thrown if there is an error modifying a Workspace node
	class SetWorkspaceConfigException : public PionException {
	public:
		SetWorkspaceConfigException()
			: PionException("Error setting the configuration for a Workspace") {}
	};

	/// exception thrown if there is an error updating a configuration option
	class UpdateConfigOptionException : public PionException {
	public:
		UpdateConfigOptionException(const std::string& reactor_id)
			: PionException("updateConfigOption failed for Reactor with identifier ", reactor_id) {}
	};


	/**
	 * constructs a new ReactionEngine object
	 *
	 * @param vocab_mgr the global manager of Vocabularies
	 * @param codec_factory the global factory that manages Codecs
	 * @param database_mgr the global manager of Databases
	 */
	ReactionEngine(VocabularyManager& vocab_mgr,
				   CodecFactory& codec_factory,
				   ProtocolFactory& protocol_factory,
				   DatabaseManager& database_mgr);
	
	/// virtual destructor
	virtual ~ReactionEngine() { shutdown();  }
	
	/// opens an existing configuration file and loads the plug-ins it contains
	virtual void openConfigFile(void);

	/**
	 * clears the statistic counters for a Reactor
	 *
	 * @param reactor_id the identifier for the Reactor to be cleared
	 */
	void clearReactorStats(const std::string& reactor_id);

	/// starts all Event processing (does not start collection Reactors)
	void start(void);
	
	/// stops all Event processing (stops all Reactors and terminates all connections)
	void stop(void);
	
	/// shuts down the reaction engine -> stops all threads and releases plugins
	void shutdown(void);

	/// clears statistic counters for all Reactors
	void clearStats(void);

	/// this updates all of the Codecs used by Reactors
	void updateCodecs(void);

	/// this updates all of the Databases used by Reactors
	void updateDatabases(void);

	/// this updates all of the Protocols used by Reactors
	void updateProtocols(void);

	/// attempts to start all reactors that should be initialized in a "running" state and are not running
	void restartReactorsThatShouldBeRunning(void);
	
	/**
	 * starts Event processing for a collection Reactor
	 *
	 * @param reactor_id unique identifier associated with the Reactor
	 */
	void startReactor(const std::string& reactor_id);

	/**
	 * stops Event processing for a collection Reactor
	 *
	 * @param reactor_id unique identifier associated with the Reactor
	 */
	void stopReactor(const std::string& reactor_id);

	/**
	 * sets configuration parameters for a managed Reactor
	 *
	 * @param reactor_id unique identifier associated with the Reactor
	 * @param config_ptr pointer to a list of XML nodes containing Reactor
	 *                   configuration parameters
	 */
	void setReactorConfig(const std::string& reactor_id,
						  const xmlNodePtr config_ptr);
	
	/**
	 * sets configuration parameters specifying location in the UI for a managed Reactor
	 *
	 * @param reactor_id unique identifier associated with the Reactor
	 * @param config_ptr pointer to a list of XML nodes containing Reactor
	 *                   configuration parameters
	 */
	void setReactorLocation(const std::string& reactor_id,
							const xmlNodePtr config_ptr);

	/**
	 * adds a new managed Reactor
	 *
	 * @param config_ptr pointer to a list of XML nodes containing Reactor
	 *                   configuration parameters (must include a Plugin type)
	 *
	 * @return std::string the new Reactor's unique identifier
	 */
	std::string addReactor(const xmlNodePtr config_ptr);
	
	/**
	 * removes a managed Reactor
	 *
	 * @param reactor_id unique identifier associated with the Reactor
	 */
	void removeReactor(const std::string& reactor_id);
	
	/**
	 * registers a temporary connection that sends Events to a Reactor
	 * (not saved to config).  This should be used VERY carefully since the
	 * Reactor may be removed by another thread, invalidating the pointer that
	 * is returned to the caller.  Make sure that that the pointer is not used
	 * after the removed_handler has been called (this will be triggered before
	 * it is invalidated).
	 *
	 * @param reactor_id unique identifier associated with the Reactor events come from
	 * @param connection_id unique identifier associated with the output connection
	 * @param connection_info descriptive information for the temporary connection
	 * @param removed_handler function handler called if the Reactor is removed
	 *
	 * @return Reactor* pointer to the Reactor that Events should be sent into
	 */
	Reactor *addTempConnectionIn(const std::string& reactor_id, 
								 const std::string& connection_id,
								 const std::string& connection_info,
								 std::function<void()> removed_handler);
	
	/**
	 * temporarily connects an Event handler to the output of a Reactor
	 * (not saved to config).  Note that the connection_handler will be sent
	 * a null EventPtr object as a notification that the Reactor is being removed.
	 *
	 * @param reactor_id unique identifier associated with the Reactor events come from
	 * @param connection_id unique identifier associated with the output connection
	 * @param connection_info descriptive information for the temporary connection
	 * @param connection_handler function handler to which Events will be sent
	 */
	void addTempConnectionOut(const std::string& reactor_id, 
							  const std::string& connection_id,
							  const std::string& connection_info,
							  Reactor::EventHandler connection_handler);
	
	/**
	 * removes a temporary connection between Reactors (does not change config)
	 *
	 * @param connection_id unique identifier associated with the temporary connection
	 */
	void removeTempConnection(const std::string& connection_id);

	/**
	 * connects the output of one Reactor to the input of another Reactor
	 *
	 * @param from_id unique identifier associated with the Reactor events come from
	 * @param to_id unique identifier associated with the Reactor events go to
	 *
	 * @return std::string unique identifier associated with the new Reactor connection
	 */
	std::string addReactorConnection(const std::string& from_id, const std::string& to_id);
	
	/**
	 * connects the output of one Reactor to the input of another Reactor
	 *
	 * @param config_ptr pointer to a list of XML nodes including <From> and <To>
	 *
	 * @return std::string unique identifier associated with the new Reactor connection
	 */
	std::string addReactorConnection(const xmlNodePtr config_ptr);
	
	/**
	 * removes an existing connection between Reactors
	 *
	 * @param from_id unique identifier associated with the Reactor events come from
	 * @param to_id unique identifier associated with the Reactor events go to
	 */
	void removeReactorConnection(const std::string& from_id, const std::string& to_id);
	
	/**
	 * removes an existing connection between Reactors
	 *
	 * @param connection_id unique identifier associated with the Reactor connection
	 */
	void removeReactorConnection(const std::string& connection_id);

	/**
	 * adds a Reactor Workspace
	 *
	 * @param content_buf pointer to buffer containing XML config for the Workspace
	 * @param content_length size of the content buffer, in bytes
	 *
	 * @return std::string unique identifier associated with the new Reactor Workspace
	 */
	std::string addWorkspace(const char* content_buf, std::size_t content_length);

	/**
	 * removes an empty Reactor Workspace
	 *
	 * @param workspace_id unique identifier associated with the Workspace
	 */
	void removeWorkspace(const std::string& workspace_id);

	/**
	 * removes all the Reactors in a Workspace
	 *
	 * @param workspace_id unique identifier associated with the Workspace
	 */
	void removeReactorsFromWorkspace(const std::string& workspace_id);

	/**
	 * sets configuration parameters for a Workspace
	 *
	 * @param workspace_id unique identifier associated with the Workspace
	 * @param content_buf pointer to buffer containing XML config for the Workspace
	 * @param content_length size of the content buffer, in bytes
	 */
	void setWorkspaceConfig(const std::string& workspace_id, const char* content_buf, std::size_t content_length);

	/**
	 * writes Reactor statistics to an output stream (as XML)
	 *
	 * @param out the ostream to write the statistics into
	 * @param reactor_id include only the Reactor that matches this unique
	 *                   identifier, or include all Reactors if empty
	 * @param details if true, then display detailed stats using the reactor's query service
	 */
	void writeStatsXML(std::ostream& out, const std::string& reactor_id = "", const bool details = false);

	/**
	 * writes info for particular connections to an output stream (as XML)
	 *
	 * @param out the ostream to write the connection info into
	 * @param only_id include only connections where either the connection ID or the ID of one of its endpoints 
	 *                matches this unique identifier, or include all connections if empty
	 */
	void writeConnectionsXML(std::ostream& out, const std::string& only_id) const;

	/**
	 * writes connection info for all Reactors to an output stream (as XML)
	 *
	 * @param out the ostream to write the connection info into
	 */
	inline void writeConnectionsXML(std::ostream& out) const {
		std::string empty_only_id;
		writeConnectionsXML(out, empty_only_id);
	}

	/**
	 * writes info for a particular Reactor Workspace to an output stream (as XML)
	 *
	 * @param out the ostream to write the Workspace info into
	 * @param workspace_id the unique of the Workspace requested
	 *
	 * @return bool whether the Workspace was found
	 */
	bool writeWorkspaceXML(std::ostream& out, const std::string& workspace_id) const;

	/**
	 * writes Workspace info for all Reactor Workspaces to an output stream (as XML)
	 *
	 * @param out the ostream to write the Workspaces info into
	 */
	void writeWorkspacesXML(std::ostream& out) const;

	/**
	 * writes all info in the entire configuration that pertains to a particular Reactor Workspace to an output stream (as XML)
	 *
	 * @param out the ostream to write the Workspace info into
	 * @param workspace_id the unique of the Workspace requested
	 *
	 * @return bool whether the Workspace was found
	 */
	bool writeWorkspaceLimitedConfigXML(std::ostream& out, const std::string& workspace_id) const;

	/**
	 * checks to see if a Workspace with the specified ID exists
	 *
	 * @param workspace_id unique identifier associated with a Workspace
	 *
	 * @return bool whether the Workspace was found
	 */
	bool hasWorkspace(const std::string& workspace_id) const;

	/**
	 * determines whether a User has permission to create a Reactor, Connection or Workspace
	 *
	 * @param permission_config_ptr the Permission node of type "Reactors" from the User's configuration
	 * @param config_ptr pointer to the new configuration; if null, returns true only if the User has unrestricted Reactor permission
	 *
	 * @return true if the User has permission
	 */
	bool creationAllowed(xmlNodePtr permission_config_ptr, xmlNodePtr config_ptr) const;

	/**
	 * determines whether a User has permission to update a Reactor or Workspace
	 *
	 * @param permission_config_ptr the Permission node of type "Reactors" from the User's configuration
	 * @param id unique identifier associated with an existing Reactor or Workspace
	 * @param config_ptr pointer to the new configuration; if null, returns true only if any configuration would be allowed
	 *
	 * @return true if the User has permission
	 */
	bool updateAllowed(xmlNodePtr permission_config_ptr, const std::string& id, xmlNodePtr config_ptr) const;

	/**
	 * determines whether a User has permission to remove a Reactor, Connection or Workspace
	 *
	 * @param permission_config_ptr the Permission node of type "Reactors" from the User's configuration
	 * @param id unique identifier associated with an existing Reactor, Connection or Workspace
	 *
	 * @return true if the User has permission
	 */
	bool removalAllowed(xmlNodePtr permission_config_ptr, const std::string& id) const;

	/**
	 * determines whether a User has permission to use a Reactor
	 *
	 * @param permission_config_ptr the Permission node of type "Reactors" from the User's configuration
	 * @param reactor_id unique identifier associated with an existing Reactor
	 *
	 * @return true if the User has permission
	 */
	bool accessAllowed(xmlNodePtr permission_config_ptr, const std::string& reactor_id) const;

	/// returns the type attribute used for an XML Permission node pertaining to Reactors
	std::string getPermissionType(void) const { return REACTORS_PERMISSION_TYPE; }

	/**
	 * uses a memory buffer to generate XML configuration data for a Reactor
	 *
	 * @param buf pointer to a memory buffer containing configuration data
	 * @param len number of bytes available in the memory buffer
	 *
	 * @return xmlNodePtr XML configuration list for the Reactor
	 */
	static xmlNodePtr createReactorConfig(const char *buf, std::size_t len) {
		return ConfigManager::createResourceConfig(Reactor::REACTOR_ELEMENT_NAME, buf, len);
	}

	/**
	 * uses a memory buffer to generate XML configuration data for a Reactor connection
	 *
	 * @param buf pointer to a memory buffer containing configuration data
	 * @param len number of bytes available in the memory buffer
	 *
	 * @return xmlNodePtr XML configuration list for the Reactor connection
	 */
	static xmlNodePtr createConnectionConfig(const char *buf, std::size_t len) {
		return ConfigManager::createResourceConfig(CONNECTION_ELEMENT_NAME, buf, len);
	}

	/**
	 * schedules an Event to be processed by a Reactor
	 *
	 * @param reactor_id unique identifier associated with the Reactor
	 * @param e pointer to the Event that will be processed
	 */
	inline void send(const std::string& reactor_id, EventPtr& e) {
		Reactor *reactor_ptr = m_plugins.get(reactor_id);
		if (reactor_ptr == NULL)
			throw ReactorNotFoundException(reactor_id);
		m_scheduler.post(std::bind<void>(std::ref(*reactor_ptr), e));
	}

	/**
	 * subscribes an external observer to a Reactor's named signal
	 *
	 * @param reactor_id unique identifier associated with the Reactor
	 * @param signal_id unique identifier for the signal
	 * @param f callback function or slot to connect to the signal - signature must be
	 *          (const std::string& reactor_id, const std::string& signal_id, void*)
	 *
	 * @return boost::signals::connection object that represents the new slot connection
	 */
	template <typename F>
	inline boost::signals::connection subscribe(const std::string& reactor_id, const std::string& signal_id, F f) {
		Reactor *reactor_ptr = m_plugins.get(reactor_id);
		if (reactor_ptr == NULL)
			throw ReactorNotFoundException(reactor_id);
		return reactor_ptr->subscribe(signal_id, f);
	}

	/**
	 * use a Reactor to handle an HTTP query (from QueryService)
	 *
	 * @param reactor_id unique identifier associated with the Reactor
	 * @param out the ostream to write the statistics info into
	 * @param branches URI stem path branches for the HTTP request
	 * @param qp query parameters or pairs passed in the HTTP request
	 */
	inline void query(const std::string& reactor_id, std::ostream& out,
		const Reactor::QueryBranches& branches, const Reactor::QueryParams& qp)
	{
		if (branches.size() < 3) {
			// query request for /query/<reactor_id>
			writeStatsXML(out, reactor_id, true);
		} else {
			Reactor *reactor_ptr = m_plugins.get(reactor_id);
			if (reactor_ptr == NULL)
				throw ReactorNotFoundException(reactor_id);
			reactor_ptr->query(out, branches, qp);
		}
	}

	/**
	 * returns the total number operations performed by all managed Reactors
	 *
	 * @return boost::uint64_t number of operations performed
	 */
	inline boost::uint64_t getTotalOperations(void) const {
		return m_plugins.getStatistic(std::bind(&Reactor::getEventsIn, std::placeholders::_1));
	}
	
	/**
	 * returns the total number of Events received by a Reactor
	 *
	 * @param reactor_id unique identifier associated with the Reactor
	 * @return boost::uint64_t number of Events received
	 */
	inline boost::uint64_t getEventsIn(const std::string& reactor_id) const {
		return m_plugins.getStatistic(reactor_id, std::bind(&Reactor::getEventsIn, std::placeholders::_1));
	}
	
	/**
	 * returns the total number of Events delivered by a Reactor
	 *
	 * @param reactor_id unique identifier associated with the Reactor
	 * @return boost::uint64_t number of Events delivered
	 */
	inline boost::uint64_t getEventsOut(const std::string& reactor_id) const {
		return m_plugins.getStatistic(reactor_id, std::bind(&Reactor::getEventsOut, std::placeholders::_1));
	}
	
	/// returns the number of events queued in ReactionScheduler
	inline std::size_t getEventsQueued(void) const { return m_scheduler.getQueueSize(); }

	/**
	 * returns the running status of a Reactor
	 *
	 * @param reactor_id unique identifier associated with the Reactor
	 * @return bool true if the Reactor is running
	 */
	inline bool isRunning(const std::string& reactor_id) const {
		const Reactor *reactor_ptr = m_plugins.get(reactor_id);
		if (reactor_ptr == NULL)
			throw ReactorNotFoundException(reactor_id);
		return reactor_ptr->isRunning();
	}

	/**
	 * schedules work to be performed by one of the pooled threads
	 *
	 * @param work_func work function to be executed
	 */
	template<typename WorkFunction>
	inline void post(WorkFunction work_func) { m_scheduler.post(work_func); }
	
	/// returns the number of threads currently in use
	inline boost::uint32_t getNumThreads(void) const { return m_scheduler.getNumThreads(); }
	
	/// sets the number of threads used to route and process Events
	inline void setNumThreads(const boost::uint32_t n) { m_scheduler.setNumThreads(n); }
	
	/// returns the value of the "multithreaded branches" setting
	inline bool getMultithreadBranches(void) const { return m_multithread_branches; }
	
	/// sets the value of the "multithreaded branches" setting
	inline void setMultithreadBranches(bool b) { m_multithread_branches = b; }

	/// returns true if the ReactionEngine is running
	inline bool isRunning(void) const { return m_is_running; }	


private:
	
	/// data type used to keep track of temporary reactor connections (i.e. feeds)
	struct TempConnection {
		/**
		 * constructs a new temporary connection object
		 *
		 * @param output_connection true if the connection is for Events sent from
		 *                          the reactor, or false if Events sent to the Reactor
		 * @param reactor_id unique identifier associated with the Reactor events come from
		 * @param connection_id unique identifier associated with the output connection
		 * @param connection_info descriptive information for the temporary connection
		 * @param removed_handler function handler called if the Reactor is removed
		 */
		TempConnection(bool output_connection,
					   const std::string& reactor_id,
					   const std::string& connection_id,
					   const std::string& connection_info,
					   std::function<void()> removed_handler)
			: m_output_connection(output_connection), m_reactor_id(reactor_id),
			m_connection_id(connection_id), m_connection_info(connection_info),
			m_removed_handler(removed_handler)
		{}
		
		/// non-virtual destructor
		~TempConnection() {}
		
		const bool						m_output_connection;
		const std::string				m_reactor_id;
		const std::string				m_connection_id;
		const std::string				m_connection_info;
		std::function<void()>			m_removed_handler;
	};
	
	/// data type for a collection of temporary connection objects
	typedef std::list<TempConnection>	TempConnectionList;
	
	/// data type used to keep track of (internal) Reactor connections
	struct ReactorConnection {
		/**
		 * constructs a new ReactorConnection object
		 *
		 * @param connection_id unique identifier associated with the output connection
		 * @param from_id unique identifier associated with the Reactor events come from
		 * @param to_id unique identifier associated with the Reactor events go to
		 */
		ReactorConnection(const std::string& connection_id,
						  const std::string& from_id,
						  const std::string& to_id)
			: m_connection_id(connection_id), m_from_id(from_id), m_to_id(to_id)
		{}
		
		/// non-virtual destructor
		~ReactorConnection() {}
		
		const std::string				m_connection_id;
		const std::string				m_from_id;
		const std::string				m_to_id;
	};
	
	/// data type for a collection of temporary connection objects
	typedef std::list<ReactorConnection>	ReactorConnectionList;

	
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
			Reactor *reactor_ptr = m_plugins.load(plugin_id, plugin_name);
			reactor_ptr->setId(plugin_id);
			reactor_ptr->setScheduler(m_scheduler);
			reactor_ptr->setMultithreadBranches(m_multithread_branches);
			reactor_ptr->setCodecFactory(m_codec_factory);
			reactor_ptr->setProtocolFactory(m_protocol_factory);
			reactor_ptr->setDatabaseManager(m_database_mgr);
			reactor_ptr->setConfigManager(*this);
			if (config_ptr != NULL) {
				VocabularyPtr vocab_ptr(m_vocab_mgr.getVocabulary());
				reactor_ptr->setConfig(*vocab_ptr, config_ptr);
			}
			try {
				reactor_ptr->startOutRunning(config_ptr, true);
			} catch (std::exception& e) {
				// log but don't propagate exceptions from startOutRunning()
				PION_LOG_ERROR(m_logger, e.what());
			}
		} catch (PionPlugin::PluginNotFoundException&) {
			throw;
		} catch (std::exception& e) {
			PION_LOG_ERROR(m_logger, "plugin_id: " << plugin_id << " - " << e.what() << " - rethrowing");
			throw PluginException(e.what());
		}
	}

	/**
	 * simple helper function to display a connection in a friendly way
	 *
	 * @param from_id unique identifier associated with the Reactor events come from
	 * @param to_id unique identifier associated with the Reactor events go to
	 */
	static inline std::string getConnectionAsText(const std::string& from_id,
												  const std::string& to_id)
	{
		std::string result(from_id);
		result += " -> ";
		result += to_id;
		return result;
	}
	
	/**
	 * connects the output of one Reactor to the input of another Reactor (without locking)
	 *
	 * @param connection_id unique identifier associated with the Reactor connection
	 * @param from_id unique identifier associated with the Reactor events come from
	 * @param to_id unique identifier associated with the Reactor events go to
	 */
	void addConnectionNoLock(const std::string& connection_id,
							 const std::string& from_id,
							 const std::string& to_id);
	
	/**
	 * removes an existing Reactor connection (without locking)
	 *
	 * @param reactor_id unique identifier associated with the Reactor events come from
	 * @param connection_id unique identifier associated with the output connection
	 */
	void removeConnectionNoLock(const std::string& reactor_id,
								const std::string& connection_id);

	/**
	 * removes an existing connection between Reactors from the config file (without locking)
	 *
	 * @param from_id unique identifier associated with the Reactor events come from
	 * @param to_id unique identifier associated with the Reactor events go to
	 */
	void removeConnectionConfigNoLock(const std::string& from_id,
									  const std::string& to_id);
	
	/// stops all Event processing (without locking)
	void stopNoLock(void);

	/// sets configuration parameters for a Workspace
	void setWorkspaceConfig(xmlNodePtr workspace_node_ptr, const char* content_buf, std::size_t content_length);

	
	/// default number of worker threads in the thread pool
	static const boost::uint32_t	DEFAULT_NUM_THREADS;
	
	/// default name of the reactor config file
	static const std::string		DEFAULT_CONFIG_FILE;

	/// name of the connection element for Pion XML config files
	static const std::string		CONNECTION_ELEMENT_NAME;
	
	/// name of the connection type element for Pion XML config files
	static const std::string		TYPE_ELEMENT_NAME;
	
	/// name of the from connection element for Pion XML config files
	static const std::string		FROM_ELEMENT_NAME;
	
	/// name of the to connection element for Pion XML config files
	static const std::string		TO_ELEMENT_NAME;
	
	/// name of the total operations element for Pion XML statistics
	static const std::string		TOTAL_OPS_ELEMENT_NAME;
	
	/// name of the events queued element for Pion XML statistics
	static const std::string		EVENTS_QUEUED_ELEMENT_NAME;

	/// type identifier for internal reactor connections
	static const std::string		CONNECTION_TYPE_REACTOR;

	/// type identifier for temporary input connections
	static const std::string		CONNECTION_TYPE_INPUT;

	/// type identifier for temporary output connections
	static const std::string		CONNECTION_TYPE_OUTPUT;

	/// type identifier for Reactors permission type
	static const std::string		REACTORS_PERMISSION_TYPE;

	/// name of the Unrestricted element in Pion XML config file Permission nodes of type "Reactors"
	static const std::string		UNRESTRICTED_ELEMENT_NAME;

	/// name of the Workspace element in Pion XML config file Permission nodes of type "Reactors"
	static const std::string		WORKSPACE_QUALIFIER_ELEMENT_NAME;

	/// used to schedule the delivery of events to Reactors for processing
	ReactionScheduler				m_scheduler;

	/// references the global factory that manages Codecs
	CodecFactory &					m_codec_factory;

	/// references the global factory that manages Protocols
	ProtocolFactory &				m_protocol_factory;

	/// references the global manager of Databases
	DatabaseManager &				m_database_mgr;
	
	/// a list of the temporary Reactor connections being managed
	TempConnectionList				m_temp_connections;
	
	/// a list of the (permanent) Reactor connections being managed
	ReactorConnectionList			m_reactor_connections;

	/// connection to this object from the CodecFactory
	boost::signals::scoped_connection	m_codec_connection;
	
	/// connection to this object from the DatabaseManager
	boost::signals::scoped_connection	m_db_connection;

	/// connection to this object from the ProtocolFactory
	boost::signals::scoped_connection	m_protocol_connection;

	/// true if the reaction engine is running
	bool							m_is_running;

	/// if true, use multiple threads for Event delivery when a Reactor has
	/// more than one output connection (branches)
	bool							m_multithread_branches;
};


}	// end namespace platform
}	// end namespace pion

#endif

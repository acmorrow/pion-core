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

#ifndef __PION_REACTOR_HEADER__
#define __PION_REACTOR_HEADER__

#include <chrono>
#include <functional>
#include <iosfwd>
#include <list>
#include <string>
#include <thread>
#include <libxml/tree.h>
#include <boost/signal.hpp>
#include <pion/PionConfig.hpp>
#include <pion/PionException.hpp>
#include <pion/PionScheduler.hpp>
#include <pion/platform/Event.hpp>
#include <pion/platform/Vocabulary.hpp>
#include <pion/platform/PlatformPlugin.hpp>

namespace pion {		// begin namespace pion
namespace platform {	// begin namespace platform (Pion Platform Library)

///
/// Reactor: used to process Events, and to deliver the same or new Events to other Reactors
///
class PION_PLATFORM_API Reactor
	: public PlatformPlugin
{
public:

	/// name of the Reactor element for Pion XML config files
	static const std::string		REACTOR_ELEMENT_NAME;
	
	/// name of the run status element for Pion XML config files
	static const std::string		RUNNING_ELEMENT_NAME;

	/// name of the Workspace element for Pion XML config files
	static const std::string		WORKSPACE_ELEMENT_NAME;

	/// name of the X Coordinate element for Pion XML config files
	static const std::string		X_COORDINATE_ELEMENT_NAME;

	/// name of the Y Coordinate element for Pion XML config files
	static const std::string		Y_COORDINATE_ELEMENT_NAME;


	/// data type for a function that receives Events
	/// NOTE(acm): Can (should) this be by ref?
	typedef std::function<void(EventPtr)>	EventHandler;

	/// data type for a collection of uri path branches used for HTTP queries
	typedef std::vector<std::string>			QueryBranches;
	
	/// data type for a dictionary of strings (used for HTTP headers)
	typedef StringDictionary					QueryParams;
	
	/// data type used to describe the type of Reactor
	enum ReactorType {
		TYPE_COLLECTION, TYPE_PROCESSING, TYPE_STORAGE
	};
	
	
	/// exception thrown if you try to add a duplicate connection
	class AlreadyConnectedException : public PionException {
	public:
		AlreadyConnectedException(const std::string& reactor_id)
			: PionException("Reactor is already connected: ", reactor_id) {}
	};
	
	/// exception thrown if you try to remove a connection that does not exist
	class ConnectionNotFoundException : public PionException {
	public:
		ConnectionNotFoundException(const std::string& reactor_id)
			: PionException("Tried removing an unknown connection: ", reactor_id) {}
	};
	
	/// exception thrown if there is a problem obtaining a configuration lock
	class ConfigLockException : public PionException {
	public:
		ConfigLockException(const std::string& reactor_id)
			: PionException("Error obtaining Reactor configuration lock: ", reactor_id) {}
	};

	/// exception thrown if an unknown signal is referenced
	class UnknownSignalException : public PionException {
	public:
		UnknownSignalException(const std::string& reactor_id, const std::string& signal_id)
			: PionException("Unknown signal for Reactor " + reactor_id + ": ", signal_id) {}
	};

	/// exception thrown if a Reactor configuration does not have a Workspace element (or it is empty)
	class MissingWorkspaceException : public PionException {
	public:
		MissingWorkspaceException(const std::string& reactor_id)
			: PionException("Reactor configuration missing required Workspace parameter: ", reactor_id) {}
		MissingWorkspaceException(void)
			: PionException("Reactor configuration missing required Workspace parameter") {}
	};

	/// virtual destructor: this class is meant to be extended
	virtual ~Reactor() {}
	
	/// called by the ReactorEngine to start Event processing
	virtual void start(void) {
		ConfigWriteLock cfg_lock(*this);
		m_is_running = true;
	}
	
	/// called by the ReactorEngine to stop Event processing
	virtual void stop(void) {
		ConfigWriteLock cfg_lock(*this);
		m_is_running = false;
	}
	
	/// resets the Reactor to its initial state
	virtual void reset(void) { clearStats(); }
	
	/// clears statistic counters for the Reactor
	virtual void clearStats(void) {
		// atomic_count doesn't support assignment -- ugh!
		//m_events_in = m_events_out = boost::detail::atomic_count(0);
	}
	
	/**
	 * sets configuration parameters for this Reactor
	 *
	 * @param v the Vocabulary that this Reactor will use to describe Terms
	 * @param config_ptr pointer to a list of XML nodes containing Reactor
	 *                   configuration parameters
	 */
	virtual void setConfig(const Vocabulary& v, const xmlNodePtr config_ptr);

	/**
	 * this updates the Vocabulary information used by this Reactor;
	 * it should be called whenever the global Vocabulary is updated
	 *
	 * @param v the Vocabulary that this Reactor will use to describe Terms
	 */
	virtual void updateVocabulary(const Vocabulary& v);

	/**
	 * this updates the Codecs that are used by this Reactor; it should
	 * be called whenever any Codec's configuration is updated
	 */
	virtual void updateCodecs(void) {}
	
	/**
	 * this updates the Databases that are used by this Reactor; it should
	 * be called whenever any Database's configuration is updated
	 */
	virtual void updateDatabases(void) {}
	
	/**
	 * this updates the Protocols that are used by this Reactor; it should
	 * be called whenever any Protocol's configuration is updated
	 */
	virtual void updateProtocols(void) {}
	
	/**
	 * handle an HTTP query (from QueryService)
	 *
	 * @param out the ostream to write the statistics info into
	 * @param branches URI stem path branches for the HTTP request
	 * @param qp query parameters or pairs passed in the HTTP request
	 *
	 * @return std::string of XML response
	 */
	virtual void query(std::ostream& out, const QueryBranches& branches,
		const QueryParams& qp);
	
	/**
	 * subscribes an external observer to a named signal
	 *
	 * @param signal_id unique identifier for the signal
	 * @param f callback function or slot to connect to the signal - signature must be (const std::string&, const std::string&, void*)
	 *
	 * @return boost::signals::connection object that represents the new slot connection
	 */
	template <typename F>
	inline boost::signals::connection subscribe(const std::string& signal_id, F f) {
		ConfigWriteLock cfg_lock(*this);
		SignalMap::iterator it = m_signals.find(signal_id);
		if (it == m_signals.end())
			throw UnknownSignalException(getId(), signal_id);
		return it->second->connect(f);
	}

	/**
	 * public function to process a new Event.  checks to make sure the reactor
	 * is running, increments "events in" counter, and ensures configuration
	 * data is not being changed before calling the virtual process() function
	 *
	 * @param e pointer to the Event to process
	 */
	inline void operator()(const EventPtr& e) {
		if ( isRunning() ) {
			ConfigReadLock cfg_lock(*this);
			// re-check after locking
			if ( isRunning() ) {
				++m_events_in;
				try {
					process(e);
				} catch (std::exception& e) {
					PION_LOG_ERROR(m_logger, "reactor_id: " << getId() << " - " << e.what() << " - rethrowing");
					throw;
				}
			}
		}
	}

	/**
	 * returns true if the reactor should be initialized in a "running" state
	 *
	 * @param config_ptr pointer to a list of XML nodes containing Reactor
	 *                   configuration parameters
	 * @param exec_start if true, and the reactor is not running, and it should be initialized in
	 *                   a "running" state, then attempt to start the reactor
	 */
	bool startOutRunning(const xmlNodePtr config_ptr, bool exec_start);

	/**
	 * connects another Reactor to the output of this Reactor
	 *
	 * @param connection_id unique identifier associated with the output connection
	 * @param output_reactor the Reactor to which output Events will be sent
	 */
	void addConnection(Reactor& output_reactor);
	
	/**
	 * connects a handler to the output of this Reactor
	 *
	 * @param connection_id unique identifier associated with the output connection
	 * @param connection_handler function handler to which Events will be sent
	 */
	void addConnection(const std::string& connection_id, EventHandler connection_handler);
	
	/**
	 * removes an existing output connection
	 *
	 * @param connection_id unique identifier associated with the output connection
	 */
	void removeConnection(const std::string& connection_id);
	
	/// clears all of the reactor's output connections
	void clearConnections(void);

	/// write all XML statistics (including Reactor elements) for this Reactor to the output stream
	inline void writeStatsXML(std::ostream& out) const {
		writeBeginReactorXML(out);
		writeStatsOnlyXML(out);
		writeEndReactorXML(out);
	}

	/// sets the scheduler that will be used to deliver Events to other Reactors
	inline void setScheduler(PionScheduler& scheduler) { m_scheduler_ptr = & scheduler; }

	/// sets the value of the "multithreaded branches" setting
	inline void setMultithreadBranches(bool b) { m_multithread_branches = b; }

	/// returns the total number of Events received by this Reactor
	inline std::uint32_t getEventsIn(void) const { return m_events_in; }

	/// returns the total number of Events delivered by this Reactor
	inline std::uint32_t getEventsOut(void) const { return m_events_out; }

	/// returns true if the Reactor is running
	inline bool isRunning(void) const { return m_is_running; }
	
	/// returns the type of Reactor (collection, processing, storage)
	inline ReactorType getType(void) const { return m_type; }

	/// returns the ID of the Workspace the Reactor belongs to
	inline std::string getWorkspace(void) const { return m_workspace_id; }

	/// sets the logger to be used
	virtual inline void setLogger(PionLogger log_ptr) const { m_logger = log_ptr; }

	/// returns the logger currently in use
	inline PionLogger getLogger(void) const { return m_logger; }

protected:

	/// pointer to a named signal object - signature is (reactor_id, signal_name, void *)
	typedef boost::signal3<void, const std::string&, const std::string&, void*> SignalType;

	/// pointer to a named signal object - signature is (reactor_id, signal_name, void *)
	typedef std::shared_ptr<SignalType> SignalPtr;
	
	/// data type for a map of signal names to signal object pointers
	typedef std::unordered_map<std::string, SignalPtr> SignalMap;


	/// configuration reader lock -> multiple concurrent "readers" are allowed.
	/// helps guarantee configuration will not change until the lock is released
	class ConfigReadLock {
	public:
		ConfigReadLock(const Reactor& r)
			: m_reactor_ref(r)
		{
			std::uint16_t sleep_times = 0;
			do {
				while (m_reactor_ref.m_config_change_pending) {
					if (++sleep_times > 50)
						throw ConfigLockException(m_reactor_ref.getId());
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
				++m_reactor_ref.m_config_num_readers;
				if (m_reactor_ref.m_config_change_pending) {
					--m_reactor_ref.m_config_num_readers;
				} else {
					break;
				}
			} while (true);
		}
		~ConfigReadLock() {
			--m_reactor_ref.m_config_num_readers;
		}
	private:
		const Reactor& 	m_reactor_ref;
	};
	
	/// configuration write lock -> ReactionEngine guarantees that only one
	/// thread will attempt to modify Reactor configuration at a time.
	/// this helps ensure that there are no active "readers" until the lock is released.
	class ConfigWriteLock {
	public:
		ConfigWriteLock(Reactor& r)
			: m_reactor_ref(r), m_already_locked(r.m_config_change_pending)
		{
			if (! m_already_locked) {
				std::uint16_t sleep_times = 0;
				m_reactor_ref.m_config_change_pending = true;
				while (m_reactor_ref.m_config_num_readers > 0) {
					if (++sleep_times > 50) {
						m_reactor_ref.m_config_change_pending = false;
						throw ConfigLockException(m_reactor_ref.getId());
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
			}
		}
		~ConfigWriteLock() {
			if (! m_already_locked)
				m_reactor_ref.m_config_change_pending = false;
		}
	private:
		Reactor& 	m_reactor_ref;
		bool		m_already_locked;
	};
		
		
	/// constructs a new Reactor object
	Reactor(const ReactorType type)
		: m_is_running(false), m_logger(PION_GET_LOGGER("pion.Reactor")),
		m_type(type), m_scheduler_ptr(NULL), 
		m_events_in(0), m_events_out(0),
		m_config_change_pending(false), m_config_num_readers(0)
	{}

	/// returns the task scheduler used by the ReactionEngine
	inline PionScheduler& getScheduler(void) {
		PION_ASSERT(m_scheduler_ptr != NULL);
		return *m_scheduler_ptr;
	}
		
	/**
	 * processes a new Event.  Derived Reactors should call deliverEvent()
	 * to send Events to output connections
	 *
	 * @param e pointer to the Event to process
	 */
	virtual void process(const EventPtr& e) {
		deliverEvent(e);
	}

	/**
	 * delivers an Event to the output connections.  This is not thread-safe
	 * and should be called only while a ConfigReadLock is held by the thread
	 *
	 * @param e pointer to the Event to deliver
	 * @param return_immediately if true, all delivery will use other threads
	 */
	inline void deliverEvent(const EventPtr& e, bool return_immediately = false) {
		++m_events_out;
		if (! m_connections.empty()) {
			if (m_multithread_branches) {
				// iterate through each Reactor after the first one and send the Event
				// using the scheduler.  This way, the entire thread pool will be used
				// for processing pipelines
				ConnectionMap::iterator i = m_connections.begin();
				
				// skip Reactors that are not running to avoid queueing work
				// to another thread when it is unnecessary.  Otherwise, if the
				// first Reactor is not running, all work will get queued rather
				// than the "longest path" being executed by the current thread
				while (i != m_connections.end() && ! i->second.isRunning())
					++i;

				if (i != m_connections.end()) {
					// the first running reactor will be handled by the current thread
					ConnectionMap::iterator cur_thread_reactor = i;
					
					// queue all other branches to be handled by other threads
					while (++i != m_connections.end()) {
						if (i->second.isRunning())
							i->second.post(getScheduler(), e);
					}
				
					// send to the first Reactor using the same thread
					// this helps to reduce context switching by ensuring
					// that the longer processing chains remain unbroken
					if (return_immediately)
						cur_thread_reactor->second.post(getScheduler(), e);
					else
						cur_thread_reactor->second(e);
				}
			} else {
				// simple scheduling just iterates through connections and use the
				// same thread to carry the event through all the reaction chains
				for (ConnectionMap::iterator i = m_connections.begin();
					 i != m_connections.end(); ++i)
				{
					if (i->second.isRunning()) {
						if (return_immediately)
							i->second.post(getScheduler(), e);
						else
							i->second(e);
					}
				}
			}
		}
	}
	
	/**
	 * delivers a container of Events to the output connections.  This is not 
	 * thread-safe and should be called only while a ConfigReadLock is held by the thread
	 *
	 * @param events a container of Events to deliver
	 * @param return_immediately if true, all delivery will use other threads
	 */
	inline void deliverEvents(const EventContainer& events, bool return_immediately = false)
	{
		for (EventContainer::const_iterator event_it = events.begin();
				event_it != events.end(); ++event_it)
		{
			deliverEvent(*event_it, return_immediately);
		}
	}

	/**
	 * increments the incoming Events counter.  This is not thread-safe and
	 * should be called only when the Reactor's mutex is locked.
	 * processes a new Event.  Derived Reactors should call deliverEvent()
	 * to send Events to output connections
	 *
	 * @param e pointer to the Event to process
	 */
	inline void incrementEventsIn(void) { ++m_events_in; }

	/**
	 * publishes a new signal to which external observers may subscribe (should be called in constructor)
	 *
	 * @param signal_id unique identifier for the signal
	 */
	inline void publish(const std::string& signal_id) {
		SignalPtr signal_ptr(new SignalType);
		ConfigWriteLock cfg_lock(*this);
		m_signals.insert(std::make_pair(signal_id, signal_ptr));
	}

	/**
	 * emits a signal to all external observers which are subscribed
	 * *without locking* (must already have ConfigReadLock or ConfigWriteLock)
	 *
	 * @param signal_id unique identifier for the signal
	 * @param ptr points to an object that carries additional information about the signal
	 */
	inline void signalNoLock(const std::string& signal_id, void *ptr = NULL) {
		SignalMap::iterator it = m_signals.find(signal_id);
		if (it == m_signals.end())
			throw UnknownSignalException(getId(), signal_id);
		(*it->second)(getId(), signal_id, ptr);
	}

	/**
	 * emits a signal to all external observers which are subscribed
	 *
	 * @param signal_id unique identifier for the signal
	 * @param ptr points to an object that carries additional information about the signal
	 */
	inline void signal(const std::string& signal_id, void *ptr = NULL) {
		ConfigReadLock cfg_lock(*this);
		signalNoLock(signal_id, ptr);
	}

	/// write only XML statistics (excluding Reactor elements) for this Reactor to the output stream
	void writeStatsOnlyXML(std::ostream& out) const;
	
	/// write beginning XML element for this Reactor
	void writeBeginReactorXML(std::ostream& out) const;
	
	/// write ending XML element for this Reactor
	void writeEndReactorXML(std::ostream& out) const;		


	/// map of signals supported by this reactor
	SignalMap					m_signals;

	/// will be true if the Reactor is "running"
	volatile bool				m_is_running;

	/// primary logging interface used by this class
	mutable PionLogger			m_logger;

	
private:

	/// data type used to represent Reactor output connections
	class OutputConnection {
	public:
		/// default destructor
		~OutputConnection() {}
		
		/// constructs a new OutputConnection to a Reactor
		explicit OutputConnection(Reactor* reactor)
			: m_reactor_ptr(reactor), m_event_handler(std::ref(*reactor))
		{}

		/// constructs a new OutputConnection to an EventHandler
		explicit OutputConnection(EventHandler handler)
			: m_reactor_ptr(NULL), m_event_handler(handler)
		{}

		/// standard copy constructor
		OutputConnection(const OutputConnection& c)
			: m_reactor_ptr(c.m_reactor_ptr), m_event_handler(c.m_event_handler)
		{}

		/// returns true if the output connection is running
		inline bool isRunning(void) {
			return (m_reactor_ptr == NULL || m_reactor_ptr->isRunning());
		}
		
		/// sends an Event over the OutputConnection
		inline void operator()(const EventPtr& event_ptr) {
			m_event_handler(event_ptr);
		}
		
		/// schedules an Event to be sent over the OutputConnection
		inline void post(PionScheduler& scheduler, const EventPtr& event_ptr) {
			scheduler.post(std::bind<void>(m_event_handler, event_ptr));
		}

	private:
		
		/// points to a Reactor where events should be sent (if not NULL)
		Reactor *			m_reactor_ptr;
		
		/// function object that sends Events over the OutputConnection
		EventHandler		m_event_handler;
	};
	
	/// data type for a collection of connections to which Events may be sent
	typedef std::map<std::string, OutputConnection>	ConnectionMap;
	
	
	/// name of the events in element for Pion XML config files
	static const std::string		EVENTS_IN_ELEMENT_NAME;
	
	/// name of the events out element for Pion XML config files
	static const std::string		EVENTS_OUT_ELEMENT_NAME;

	/// name of the unique identifier attribute for Pion XML config files
	static const std::string		ID_ATTRIBUTE_NAME;


	/// the type of Reactor (collection, processing or storage)
	const ReactorType				m_type;
	
	/// used to schedule the delivery of events to Reactors for processing
	PionScheduler *			    	m_scheduler_ptr;
	
	/// a collection of connections to which Events may be sent
	ConnectionMap					m_connections;

	/// unique identifier for the workspace this reactor belongs to
	std::string						m_workspace_id;

	/// the total number of Events received by this Reactor
	/// note that this counter is likely to "wrap-around" and start back at zero
	boost::detail::atomic_count		m_events_in;

	/// the total number of Events delivered by this Reactor
	/// note that this counter is likely to "wrap-around" and start back at zero
	boost::detail::atomic_count		m_events_out;
	
	/// if true, use multiple threads for Event delivery when a Reactor has
	/// more than one output connection (CACHED VALUE FROM REACTIONENGINE)
	bool							m_multithread_branches;
	
	/// boolean flag used to signal pending configuration changes
	mutable volatile bool					m_config_change_pending;
	
	/// atomic counter used to track the number of configuration readers
	mutable boost::detail::atomic_count		m_config_num_readers;

	// allow configuration lock classes to modify private variables	
	friend class ConfigReadLock;
	friend class ConfigWriteLock;
};


//
// The following symbols must be defined for any reactor that you would
// like to be able to load dynamically using the ReactionEngine::load()
// function.  These are not required for any reactors that you only want to link
// directly into your programs.
//
// Make sure that you replace "REACTOR" with the name of your derived class.
// This name must also match the name of the object file (excluding the
// extension).  These symbols must be linked into your reactor's object file,
// not included in any headers that it may use (declarations are OK in headers
// but not the definitions).
//
// The "pion_create" function is used to create new instances of your reactor.
// The "pion_destroy" function is used to destroy instances of your reactor.
//
// extern "C" Reactor *pion_create_REACTOR(void) {
//		return new REACTOR;
// }
//
// extern "C" void pion_destroy_REACTOR(REACTOR *reactor_ptr) {
//		delete reactor_ptr;
// }
//


}	// end namespace platform
}	// end namespace pion

#endif

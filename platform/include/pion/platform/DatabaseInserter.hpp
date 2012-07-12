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

#ifndef __PION_DATABASINSERTER_HEADER__
#define __PION_DATABASINSERTER_HEADER__

#include <memory>
#include <vector>
#include <ctime>
#include <thread>
#include <condition_variable>
#include <pion/PionConfig.hpp>
#include <pion/PionException.hpp>
#include <pion/PionLogger.hpp>
#include <pion/platform/Event.hpp>
#include <pion/platform/RuleChain.hpp>
#include <pion/platform/Query.hpp>
#include <pion/platform/Database.hpp>
#include <pion/platform/PlatformPlugin.hpp>


namespace pion {		// begin namespace pion
namespace platform {	// begin namespace platform (Pion Platform Library)


///
/// DatabaseInserter: inserts Events into database transaction tables
///
class PION_PLATFORM_API DatabaseInserter
{
public:

	/// exception thrown if this class tries to retrieve a database plugin
	/// before it is initialized using setPlatformPlugin()
	class MissingPlatformPluginException : public std::exception {
	public:
		virtual const char* what() const throw() {
			return "DatabaseInserter is missing the PlatformPlugin";
		}
	};

	/// exception thrown if the DatabaseInserter configuration does not define a Database
	class EmptyDatabaseException : public std::exception {
	public:
		virtual const char* what() const throw() {
			return "DatabaseInserter configuration is missing a required Database parameter";
		}
	};

	/// exception thrown if the DatabaseInserter configuration does not define a Table
	class EmptyTableException : public std::exception {
	public:
		virtual const char* what() const throw() {
			return "DatabaseInserter configuration is missing a required Table parameter";
		}
	};

	/// exception thrown if there are no database field mappings in the configuration
	class NoFieldsException : public std::exception {
	public:
		virtual const char* what() const throw() {
			return "DatabaseInserter configuration must contain at least one field mapping";
		}
	};

	/// exception thrown if the DatabaseInserter configuration includes an empty field name
	class EmptyFieldException : public std::exception {
	public:
		virtual const char* what() const throw() {
			return "DatabaseInserter configuration includes an empty field name";
		}
	};

	/// exception thrown if the DatabaseInserter configuration does not define a term in a field mapping
	class EmptyTermException : public std::exception {
	public:
		virtual const char* what() const throw() {
			return "DatabaseInserter configuration is missing a term identifier";
		}
	};

	/// exception thrown if the DatabaseInserter configuration uses an unknown term in a field mapping
	class UnknownTermException : public PionException {
	public:
		UnknownTermException(const std::string& term_id)
			: PionException("DatabaseInserter configuration maps field to an unknown term: ", term_id) {}
	};

	/// exception thrown if the DatabaseInserter configuration has a field name with illegal characters
	class IllegalCharactersException: public PionException {
	public:
		IllegalCharactersException(const std::string& field_name)
			: PionException("DatabaseInserter configuration has a field name with illegal characters: ", field_name) {}
	};

	/// exception thrown if the DatabaseInserter configuration has a duplicate field name
	class DuplicateColumnName: public PionException {
	public:
		DuplicateColumnName(const std::string& field_name)
			: PionException("DatabaseInserter configuration has a duplicate field name: ", field_name) {}
	};

	/// exception thrown if the DatabaseInserter configuration has a field name with illegal characters
	class NoUniqueKeyFound: public PionException {
	public:
		NoUniqueKeyFound(void)
			: PionException("DatabaseInserter configuration has MaxAge, but there is no Unique indexed column") {}
	};

	/// exception thrown if the DatabaseInserter is configured to keep a key cache, with age, but age term is not defined
	class MissingEventTime: public PionException {
	public:
		MissingEventTime(const std::string& element )
			: PionException("DatabaseInserter configuration has MaxAge, but the age term is missing: " + element) {}
	};

	/// constructs a new DatabaseInserter object
	DatabaseInserter(void) :
		m_logger(PION_GET_LOGGER("pion.platform.DatabaseInserter")),
		m_platform_plugin_ptr(NULL),
		m_event_queue_ptr(new EventQueue), 
		m_queue_max(DEFAULT_QUEUE_SIZE), m_queue_timeout(DEFAULT_QUEUE_TIMEOUT),
		m_recovery_interval(DEFAULT_RECOVERY_INTERVAL),
		m_is_running(false), m_partition(0), m_wipe(false), m_max_age(0),
		m_last_time(0), m_next_connect(0), m_table_size(0)
	{}

	/// virtual destructor: this class may be extended
	virtual ~DatabaseInserter() { stop(); }

	/// sets the PlatformPlugin that will used by the plugin to access Databases
	inline void setPlatformPlugin(const PlatformPlugin& plugin) { m_platform_plugin_ptr = & plugin; }
	
	/**
	 * sets configuration parameters for this class
	 *
	 * @param v the Vocabulary that this class will use to describe Terms
	 * @param config_ptr pointer to a list of XML nodes containing class
	 *                   configuration parameters
	 */
	void setConfig(const Vocabulary& v, const xmlNodePtr config_ptr);

	/**
	 * this updates the Vocabulary information used by this class; it should
	 * be called whenever the global Vocabulary is updated
	 *
	 * @param v the Vocabulary that this class will use to describe Terms
	 */
	void updateVocabulary(const Vocabulary& v);

	/**
	 * this updates the Databases that are used by this class; it should
	 * be called whenever any Database's configuration is updated
	 */
	void updateDatabases(void);

	/**
	 * adds an event to the queue for database insertion
	 *
	 * @param e pointer to the Event to insert
	 */
	void insert(const EventPtr& e);

	/// starts the worker thread used to insert records.  NOTE: you must
	/// call setDatabaseManager() before starting the DatabaseInserter!
	void start(void);

	/// stops the worker thread used to insert records
	void stop(void);

	/// returns the total number of events queued
	std::size_t getEventsQueued(void) const;

	/// returns the total number of items in the key cache
	std::size_t getKeyCacheSize(void) const;

	/// returns the unique identifier for the database being used
	const std::string& getDatabaseId(void) const { return m_database_id; }

	/// returns the DatabasePtr (for queries)
	DatabasePtr getDatabasePtr(void) { return m_database_ptr; }

	/// returns the name of the database table that events are inserted into
	const std::string& getTableName(void) const { return m_table_name; }

	/// sets (overrides) the table name; must be called before start()
	bool setTableName(const std::string& name)
	{
		if (m_is_running) return false;
		m_table_name = name;
		return true;
	}

	/// set partition number -- must be called before start()
	bool setPartition(unsigned partition)
	{
		if (m_is_running) return false;
		m_partition = partition;
		return true;
	}

	/// Wipe out existing database before opening?
	bool setWipe(bool wipe)
	{
		if (m_is_running) return false;
		m_wipe = wipe;
		return true;
	}

	boost::uint32_t getRotate(void) const
	{
		return m_cache_size ? (m_cache_consumption * 100UL / m_cache_size) : 0;
	}

	boost::uint64_t getTableSize(void) const { return m_table_size; }

	/// returns a copy of the mapping of database fields to event terms
	Query::FieldMap getFieldMap(void) const { return m_field_map; }

	/// returns a copy of the index map
	Query::IndexMap getIndexMap(void) const { return m_index_map; }

	/// returns true if the worker thread is running
	inline bool isRunning(void) const { return m_is_running; }
	
	/// sets the logger to be used
	inline void setLogger(PionLogger log_ptr) const { m_logger = log_ptr; }

	/// returns the logger currently in use
	inline PionLogger getLogger(void) { return m_logger; }

	/// does table exist (table name, and partition must be already configured)
	inline bool tableExists(void)
	{
		if (!m_database_ptr)
			getDatabasePlugin(m_database_ptr, m_database_id);
		return m_database_ptr->tableExists(m_table_name, m_partition);
	}


private:

	/// data type for a collection of queued Events
	typedef std::vector<EventPtr>	EventQueue;


	/// return current timestamp (time_t format)
	static inline std::time_t now(void) { return ::time(NULL); }
        
	/**
	 * retrieves a new Database instance
	 *
	 * @param plugin_ptr smart pointer which will be assigned to the new instance
	 * @param plugin_id unique identifier for the plugin to retrieve
	 *
	 * @return true if a matching plugin was found, otherwise false
	 */
	bool getDatabasePlugin(DatabasePtr& plugin_ptr, const std::string& plugin_id) const;

	/// returns true if the unique identifier matches a known Database plugin
	bool hasDatabasePlugin(const std::string& plugin_id) const;

	/// (re)establishes a connection to the database
	void connect(void);

	/// attempts to reconnect the the database (true if successful)
	bool tryConnecting(void);
	
	/// returns true if the database connection is OK
	bool checkConnection(void);

	/// function used by the worker thread to store events to the database
	void insertEvents(void);

	/**
	 * function used by the worker thread to store events to the database
	 *
	 * @param insert_queue_ptr event queue ptr that will be swapped with available event queue
	 */
	void insertEvents(std::unique_ptr<EventQueue>& insert_queue_ptr);

	/**
	 * checks for new events queued for database storage
	 *
	 * @param insert_queue_ptr event queue ptr that will be swapped with available event queue
	 *
	 * @return bool true if there are new events available
	 */
	bool checkEventQueue(std::unique_ptr<EventQueue>& insert_queue_ptr);


	/// default maximum number of events that may be queued for insertion
	static const boost::uint32_t			DEFAULT_QUEUE_SIZE;

	/// default number of seconds before the queue is automatically flushed due to timeout
	static const boost::uint32_t			DEFAULT_QUEUE_TIMEOUT;

	/// default number of seconds in between database connection recovery attempts
	static const boost::uint32_t			DEFAULT_RECOVERY_INTERVAL;

	/// name of the database element for Pion XML config files
	static const std::string				DATABASE_ELEMENT_NAME;

	/// name of the table element for Pion XML config files
	static const std::string				TABLE_ELEMENT_NAME;

	/// name of the field element for Pion XML config files
	static const std::string				FIELD_ELEMENT_NAME;

	/// name of the queue size element for Pion XML config files
	static const std::string				QUEUE_SIZE_ELEMENT_NAME;

	/// name of the queue timeout element for Pion XML config files
	static const std::string				QUEUE_TIMEOUT_ELEMENT_NAME;

	/// name of the recovery interval element for Pion XML config files
	static const std::string				RECOVERY_INTERVAL_ELEMENT_NAME;

	/// name of the Term ID attribute for Pion XML config files
	static const std::string				TERM_ATTRIBUTE_NAME;

	/// legal character set for SQL92 column/table names
	static const char *						CHARSET_FOR_TABLES;

	/// Optional field, to index a column
	static const std::string				INDEX_ATTRIBUTE_NAME;

	/// Optional field, to override type
	static const std::string				SQL_ATTRIBUTE_NAME;

	/// Use the IgnoreInsert statement for inserting data
	static const std::string				IGNORE_INSERT_ELEMENT_NAME;

	/// Default for ignoring inserts
	static const std::string				DEFAULT_IGNORE;

	/// Max age for event
	static const std::string				MAX_KEY_AGE_ELEMENT_NAME;

	/// Term to use for determining event age
	static const std::string				EVENT_AGE_ELEMENT_NAME;

	/// Default max_age is 0, so it won't trigger when not necessary
	static const boost::uint32_t			DEFAULT_MAX_AGE;

	/// primary logging interface used by this class
	mutable PionLogger						m_logger;

	/// reference to the platform plugin used to create new DB connections
	const PlatformPlugin *					m_platform_plugin_ptr;

	/// unique identifier for the database that is used to store events
	std::string								m_database_id;

	/// name of the table into which events will be stored
	std::string								m_table_name;

	/// maps Term references to database field names
	Query::FieldMap							m_field_map;

	/// Corresponding (to FieldMap) index map
	Query::IndexMap							m_index_map;

	/// pointer to the database that is used to store events
	DatabasePtr								m_database_ptr;
	
	/// points to the query used to insert records into the database
	QueryPtr								m_insert_query_ptr;

	/// points to the query used to begin a transaction
	QueryPtr								m_begin_transaction_ptr;

	/// points to the query used to commit/end a transaction
	QueryPtr								m_commit_transaction_ptr;

	/// collection of events queued for storage to the database
	std::unique_ptr<EventQueue>			m_event_queue_ptr;

	/// maximum number of events that may be queued for insertion
	boost::uint32_t							m_queue_max;

	/// number of seconds before the queue is automatically flushed due to timeout
	boost::uint32_t							m_queue_timeout;

	/// number of seconds in between database connection recovery attempts
	boost::uint32_t							m_recovery_interval;

	/// used to protect the Event queue
	mutable std::mutex					m_queue_mutex;

	/// condition triggered to notify the worker thread to save events to the database
	std::condition_variable						m_wakeup_worker;

	/// condition triggered to notify all threads that the the queue was swapped
	std::condition_variable						m_swapped_queue;

	/// thread used to store events to the database
	std::unique_ptr<std::thread>		m_thread;

	/// ignore insert errors?
	bool									m_ignore_insert;
	
	/// true if the worker thread is running
	volatile bool							m_is_running;

	/// partition number (or 0 for no partition)
	unsigned								m_partition;

	/// wipe out previous content before opening?
	bool									m_wipe;

	/// a chain of Comparison rules used determine if event should be inserted
	pion::platform::RuleChain				m_rules;

	/// Hash map; incomplete, time bounded "unique key" vs. "age" -- for avoiding multiple inserts
	typedef std::unordered_map<pion::platform::Event::BlobType, boost::uint32_t> KeyHash;
	KeyHash									m_keys;

	/// The term ref, that has a unique index
	Vocabulary::TermRef						m_key_term_ref;

	/// MaxAge of keys in hash map (if configured)
	boost::uint32_t							m_max_age;

	/// Term ref to use for finding the timestamp from an event
	Vocabulary::TermRef						m_timestamp_term_ref;

	/// most recent timestamp used for key cache pruning
	boost::uint32_t							m_last_time;
	
	/// timestamp for next connection attempt (0 if connected)
	std::time_t								m_next_connect;

	/// Counter for index cache consumption
	boost::uint64_t							m_cache_consumption;

	/// Cache overhead (per item)
	boost::uint32_t							m_cache_overhead;

	/// Name of term to count stats on
	std::vector<Vocabulary::Term>			m_cache_terms;

	/// How many rows have been inserted
	boost::uint64_t							m_cache_rows;

	/// How big is the database cache
	boost::uint64_t							m_cache_size;

	/// How big is the database file
	boost::uint64_t							m_table_size;
};


}	// end namespace platform
}	// end namespace pion

#endif

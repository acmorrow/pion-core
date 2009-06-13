// ------------------------------------------------------------------------
// Pion is a development platform for building Reactors that process Events
// ------------------------------------------------------------------------
// Copyright (C) 2007-2009 Atomic Labs, Inc.  (http://www.atomiclabs.com)
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

#include <vector>
#include <boost/scoped_ptr.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/condition.hpp>
#include <pion/PionConfig.hpp>
#include <pion/PionException.hpp>
#include <pion/PionLogger.hpp>
#include <pion/platform/Event.hpp>
#include <pion/platform/Query.hpp>
#include <pion/platform/Database.hpp>
#include <pion/platform/DatabaseManager.hpp>


namespace pion {		// begin namespace pion
namespace platform {	// begin namespace platform (Pion Platform Library)


///
/// DatabaseInserter: inserts Events into database transaction tables
///
class PION_PLATFORM_API DatabaseInserter
{
public:

	/// exception thrown if this class tries to use DatabaseManager before it is
	/// initialized via setDatabaseManager()
	class MissingDatabaseManagerException : public std::exception {
	public:
		virtual const char* what() const throw() {
			return "DatabaseInserter is missing the DatabaseManager";
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


	/// constructs a new DatabaseInserter object
	DatabaseInserter(void) :
		m_logger(PION_GET_LOGGER("pion.platform.DatabaseInserter")),
		m_database_mgr_ptr(NULL),
		m_event_queue_ptr(new EventQueue), 
		m_queue_max(DEFAULT_QUEUE_SIZE), m_queue_timeout(DEFAULT_QUEUE_TIMEOUT),
		m_is_running(false)
	{}

	/// virtual destructor: this class may be extended
	virtual ~DatabaseInserter() { stop(); }

	/// sets the DatabaseManager that will used by the plugin to access Databases
	inline void setDatabaseManager(DatabaseManager& mgr) { m_database_mgr_ptr = & mgr; }
	
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

	/// returns the unique identifier for the database being used
	const std::string& getDatabaseId(void) const { return m_database_id; }

	/// returns the name of the database table that events are inserted into
	const std::string& getTableName(void) const { return m_table_name; }

	/// sets (overrides) the table name; must be called before start()
	bool setTableName(const std::string& name)
	{
		if (m_is_running) return false;
		m_table_name = name;
		return true;
	}

	/// returns a copy of the mapping of database fields to event terms
	Query::FieldMap getFieldMap(void) const { return m_field_map; }

	/// returns true if the worker thread is running
	inline bool isRunning(void) const { return m_is_running; }
	
	/// sets the logger to be used
	inline void setLogger(PionLogger log_ptr) { m_logger = log_ptr; }

	/// returns the logger currently in use
	inline PionLogger getLogger(void) { return m_logger; }


private:

	/// data type for a collection of queued Events
	typedef std::vector<EventPtr>	EventQueue;


	/// returns the DatabaseManager to use for accessing Databases
	DatabaseManager& getDatabaseManager(void);

	/// function used by the worker thread to store events to the database
	void insertEvents(void);

	/**
	 * checks for new events queued for database storage
	 *
	 * @param insert_queue_ptr event queue ptr that will be swapped with available event queue
	 *
	 * @return bool true if there are new events available
	 */
	bool checkEventQueue(boost::scoped_ptr<EventQueue>& insert_queue_ptr);


	/// default maximum number of events that may be queued for insertion
	static const boost::uint32_t			DEFAULT_QUEUE_SIZE;

	/// default number of seconds before the queue is automatically flushed due to timeout
	static const boost::uint32_t			DEFAULT_QUEUE_TIMEOUT;

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

	/// name of the Term ID attribute for Pion XML config files
	static const std::string				TERM_ATTRIBUTE_NAME;

	/// legal character set for SQL92 column/table names
	static const char *						CHARSET_FOR_TABLES;

	/// primary logging interface used by this class
	PionLogger								m_logger;

	/// reference to the database manager, used to create new DB connection
	DatabaseManager *						m_database_mgr_ptr;

	/// unique identifier for the database that is used to store events
	std::string								m_database_id;

	/// name of the table into which events will be stored
	std::string								m_table_name;

	/// maps Term references to database field names
	Query::FieldMap							m_field_map;

	/// pointer to the database that is used to store events
	DatabasePtr								m_database_ptr;

	/// collection of events queued for storage to the database
	boost::scoped_ptr<EventQueue>			m_event_queue_ptr;

	/// maximum number of events that may be queued for insertion
	boost::uint32_t							m_queue_max;

	/// number of seconds before the queue is automatically flushed due to timeout
	boost::uint32_t							m_queue_timeout;

	/// used to protect the Event queue
	mutable boost::mutex					m_queue_mutex;

	/// condition triggered to notify the worker thread to save events to the database
	boost::condition						m_wakeup_worker;

	/// condition triggered to notify all threads that the the queue was swapped
	boost::condition						m_swapped_queue;

	/// thread used to store events to the database
	boost::scoped_ptr<boost::thread>		m_thread;
	
	/// true if the worker thread is running
	volatile bool							m_is_running;
};


}	// end namespace platform
}	// end namespace pion

#endif
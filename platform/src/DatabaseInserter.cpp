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

#include <pion/platform/ConfigManager.hpp>
#include <pion/platform/DatabaseInserter.hpp>
#include <pion/platform/DatabaseManager.hpp>


namespace pion {		// begin namespace pion
namespace platform {	// begin namespace platform (Pion Platform Library)


// static members of DatabaseInserter

const boost::uint32_t		DatabaseInserter::DEFAULT_QUEUE_SIZE = 10000;
const boost::uint32_t		DatabaseInserter::DEFAULT_QUEUE_TIMEOUT = 5;
const boost::uint32_t		DatabaseInserter::DEFAULT_RECOVERY_INTERVAL = 5;
const std::string			DatabaseInserter::DEFAULT_IGNORE = "false";
const std::string			DatabaseInserter::DATABASE_ELEMENT_NAME = "Database";
const std::string			DatabaseInserter::TABLE_ELEMENT_NAME = "Table";
const std::string			DatabaseInserter::FIELD_ELEMENT_NAME = "Field";
const std::string			DatabaseInserter::QUEUE_SIZE_ELEMENT_NAME = "QueueSize";
const std::string			DatabaseInserter::QUEUE_TIMEOUT_ELEMENT_NAME = "QueueTimeout";
const std::string			DatabaseInserter::RECOVERY_INTERVAL_ELEMENT_NAME = "RecoveryInterval";
const std::string			DatabaseInserter::TERM_ATTRIBUTE_NAME = "term";
const std::string			DatabaseInserter::INDEX_ATTRIBUTE_NAME = "index";
const std::string			DatabaseInserter::SQL_ATTRIBUTE_NAME = "sql";
const char *				DatabaseInserter::CHARSET_FOR_TABLES = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
const std::string			DatabaseInserter::IGNORE_INSERT_ELEMENT_NAME = "IgnoreInsert";
const std::string			DatabaseInserter::MAX_KEY_AGE_ELEMENT_NAME = "KeyCacheMaxAge";
const boost::uint32_t		DatabaseInserter::DEFAULT_MAX_AGE = 0;
const std::string			DatabaseInserter::EVENT_AGE_ELEMENT_NAME = "KeyCacheAgeTerm";


// DatabaseInserter member functions

void DatabaseInserter::setConfig(const Vocabulary& v, const xmlNodePtr config_ptr)
{
	// stop for config changes, but cache running status
	// so that it can be restarted when finished
	const bool was_running = m_is_running;
	stop();

	boost::mutex::scoped_lock queue_lock(m_queue_mutex);

	// parse RuleChain configuration
	m_rules.setConfig(v, config_ptr);

	// get the maximum number of events that may be queued for insertion
	ConfigManager::getConfigOption(QUEUE_SIZE_ELEMENT_NAME, m_queue_max, DEFAULT_QUEUE_SIZE, config_ptr);

	// get the queue timeout parameter
	ConfigManager::getConfigOption(QUEUE_TIMEOUT_ELEMENT_NAME, m_queue_timeout, DEFAULT_QUEUE_TIMEOUT, config_ptr);

	// get the recovery interval parameter
	ConfigManager::getConfigOption(RECOVERY_INTERVAL_ELEMENT_NAME, m_recovery_interval, DEFAULT_RECOVERY_INTERVAL, config_ptr);

	// get the optional max_age parameter
	ConfigManager::getConfigOption(MAX_KEY_AGE_ELEMENT_NAME, m_max_age, DEFAULT_MAX_AGE, config_ptr);

	// get the optional max_keys parameter
	if (m_max_age) {
		std::string term_str;
		if (!ConfigManager::getConfigOption(EVENT_AGE_ELEMENT_NAME, term_str, config_ptr))
			throw MissingEventTime(EVENT_AGE_ELEMENT_NAME);
		m_timestamp_term_ref = v.findTerm(term_str);
		if (m_timestamp_term_ref == Vocabulary::UNDEFINED_TERM_REF)
			throw UnknownTermException(term_str);
	}

	// get the queue timeout parameter
	std::string ignore_str;
	m_ignore_insert = false;
	ConfigManager::getConfigOption(IGNORE_INSERT_ELEMENT_NAME, ignore_str, DEFAULT_IGNORE, config_ptr);
	if (ignore_str == "true")
		m_ignore_insert = true;

	// prepare the event queue
	m_event_queue_ptr->reserve(m_queue_max);

	// get the database to use
	if (! ConfigManager::getConfigOption(DATABASE_ELEMENT_NAME, m_database_id, config_ptr))
		throw EmptyDatabaseException();
	if (! hasDatabasePlugin(m_database_id))
		throw DatabaseManager::DatabaseNotFoundException(m_database_id);

	// get the name of the table to store events in
	if (! ConfigManager::getConfigOption(TABLE_ELEMENT_NAME, m_table_name, config_ptr))
		throw EmptyTableException();

	// next, map the database fields to Terms
	m_field_map.clear();
	m_index_map.clear();
	m_cache_consumption = 0;
	m_cache_rows = 0;
	m_cache_terms.clear();
	xmlNodePtr field_node = config_ptr;
	m_key_term_ref = Vocabulary::UNDEFINED_TERM_REF;
	while ( (field_node = ConfigManager::findConfigNodeByName(FIELD_ELEMENT_NAME, field_node)) != NULL)
	{
		// parse new field mapping

		// start with the name of the field (element content)
		xmlChar *xml_char_ptr = xmlNodeGetContent(field_node);
		if (xml_char_ptr == NULL || xml_char_ptr[0]=='\0') {
			if (xml_char_ptr != NULL)
				xmlFree(xml_char_ptr);
			throw EmptyFieldException();
		}
		const std::string field_name(reinterpret_cast<char*>(xml_char_ptr));
		xmlFree(xml_char_ptr);

		if (strspn(field_name.c_str(), CHARSET_FOR_TABLES) != field_name.length())
			throw IllegalCharactersException(field_name);

		for (unsigned i = 0; i < m_field_map.size(); i++)
			if (m_field_map[i].first == field_name)
				throw DuplicateColumnName(field_name);

		// next get the Term we want to map to
		const std::string term_id = ConfigManager::getAttribute(TERM_ATTRIBUTE_NAME, field_node);
		if (term_id.empty())
			throw EmptyTermException();

		const std::string index_str = ConfigManager::getAttribute(INDEX_ATTRIBUTE_NAME, field_node);

		const std::string sql_str = ConfigManager::getAttribute(SQL_ATTRIBUTE_NAME, field_node);

		// make sure that the Term is valid
		const Vocabulary::TermRef term_ref = v.findTerm(term_id);
		if (term_ref == Vocabulary::UNDEFINED_TERM_REF)
			throw UnknownTermException(term_id);

		// add the field mapping
		m_field_map.push_back(std::make_pair(field_name, v[term_ref]));
		m_index_map.push_back(index_str);

		// Try to find the unique key term, if max_keys is defined
		if (m_max_age && index_str == "unique")
			m_key_term_ref = term_ref;

		if (!index_str.empty() && index_str != "false")
			m_cache_terms.push_back(v[term_ref]);

		// step to the next field mapping
		field_node = field_node->next;
	}
	if (m_field_map.empty())
		throw NoFieldsException();

	if (m_max_age && m_key_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw NoUniqueKeyFound();

	// if config changed while running, then restart
	queue_lock.unlock();
	if (was_running)
		start();
}

void DatabaseInserter::updateVocabulary(const Vocabulary& v)
{
	boost::mutex::scoped_lock queue_lock(m_queue_mutex);
	if (m_database_ptr)
		m_database_ptr->updateVocabulary(v);

	// update Term mappings (note: references never change for a given id)
	// TaO090319: field_map no longer contains termref, can't be updated without
	//			stop/start of database, i.e. reconstruct the whole field_map
/*
	for (Query::FieldMap::iterator i = m_field_map.begin(); i != m_field_map.end(); ++i)
		i->second.second = v[i->first];
*/
}

void DatabaseInserter::updateDatabases(void)
{
	// just check to see if the database was deleted (if so, stop now!)
	if (! hasDatabasePlugin(m_database_id)) {
		stop();
	}
}

void DatabaseInserter::start(void)
{
	boost::mutex::scoped_lock queue_lock(m_queue_mutex);
	if (! m_is_running) {
		m_is_running = true;

		try {
			// open a new database connection
			connect();
		} catch (...) {
			// failed to initialize properly -> reset and update running state to false
			m_insert_query_ptr.reset();
			m_begin_transaction_ptr.reset();
			m_commit_transaction_ptr.reset();
			m_database_ptr.reset();
			m_is_running = false;
			throw;
		}
	
		// spawn a new thread that will be used to save events to the database
		PION_LOG_DEBUG(m_logger, "Starting worker thread: " << m_database_id);
		void(DatabaseInserter::* insertEvents)(void) = &DatabaseInserter::insertEvents;
		m_thread.reset(new boost::thread(std::bind(insertEvents, this)));

		// wait for the worker thread to startup
		m_swapped_queue.wait(queue_lock);
	}
}

void DatabaseInserter::stop(void)
{
	boost::mutex::scoped_lock queue_lock(m_queue_mutex);
	if (m_is_running) {
		// set flag to notify worker thread to shutdown
		m_is_running = false;
		PION_LOG_DEBUG(m_logger, "Stopping worker thread: " << m_database_id);
		m_wakeup_worker.notify_one();
		queue_lock.unlock();

		// wait for worker thread to shutdown
		m_thread->join();
		m_table_size = m_database_ptr->getCache(Database::DB_FILE_SIZE);

		// close the database connection (ensure that data is flushed)
		boost::mutex::scoped_lock queue_lock_two(m_queue_mutex);
		m_insert_query_ptr.reset();
		m_begin_transaction_ptr.reset();
		m_commit_transaction_ptr.reset();
		m_database_ptr.reset();
		
		// clear the event queue
		m_event_queue_ptr->clear();
		
		// clear the key cache
		m_keys.clear();
	}
}

std::size_t DatabaseInserter::getEventsQueued(void) const
{
	boost::mutex::scoped_lock queue_lock(m_queue_mutex);
	return m_event_queue_ptr->size();
}

std::size_t DatabaseInserter::getKeyCacheSize(void) const
{
	boost::mutex::scoped_lock queue_lock(m_queue_mutex);
	return m_keys.size();
}

void DatabaseInserter::insert(const EventPtr& e)
{
	// if not running... there might not be filter rules...
	if (! m_is_running)
		return;

	// check filter rules first
	if ( m_rules(e) ) {

		boost::mutex::scoped_lock queue_lock(m_queue_mutex);

		// make sure worker thread is running
		if (! m_is_running)
			return;

		// do we have collision avoidance?
		if (m_max_age) {
			// look for key within the event
			const Event::ParameterValue *param_ptr = e->getPointer(m_key_term_ref);

			// If key is not found, then "null key" and no insert
			if (param_ptr == NULL) {
				// log warning if we're not ignoring insert errors
				if (! m_ignore_insert)
					PION_LOG_WARN(m_logger, "Event missing required table key: " << m_database_id);
				return;
			}

			// Lookup key in memory cache
			KeyHash::iterator ki = m_keys.find(boost::get<const Event::BlobType&>(*param_ptr));
			
			// Update timestamp used for pruning
			boost::uint32_t event_timestamp;
			if (e->getUInt(m_timestamp_term_ref, event_timestamp) && event_timestamp > m_last_time) {
				m_last_time = event_timestamp;
			}

			// Do we have this key already?
			if (ki != m_keys.end()) {
				ki->second = m_last_time;				// Update age
				return;									// Bail out
			}

			// Add key & age
			m_keys[boost::get<const Event::BlobType&>(*param_ptr)] = m_last_time;

			// Pruning will be done in insert thread
		}

		for (unsigned i = 0; i < m_cache_terms.size(); i++) {
			boost::uint32_t size = 0;
			switch (m_cache_terms[i].term_type) {
				case Vocabulary::TYPE_NULL:
				case Vocabulary::TYPE_OBJECT:
					break;
				case Vocabulary::TYPE_INT8:
				case Vocabulary::TYPE_UINT8:
					size = 1;
					break;
				case Vocabulary::TYPE_INT16:
				case Vocabulary::TYPE_UINT16:
					size = 2;
					break;
				case Vocabulary::TYPE_INT32:
				case Vocabulary::TYPE_UINT32:
					size = 4;
					break;
				case Vocabulary::TYPE_INT64:
				case Vocabulary::TYPE_UINT64:
					size = 8;
					break;
				case Vocabulary::TYPE_FLOAT:
				case Vocabulary::TYPE_DOUBLE:
				case Vocabulary::TYPE_LONG_DOUBLE:
					size = 8;							// 8-byte IEEE float
					break;
				case Vocabulary::TYPE_SHORT_STRING:
				case Vocabulary::TYPE_STRING:
				case Vocabulary::TYPE_LONG_STRING:
				case Vocabulary::TYPE_CHAR:
				case Vocabulary::TYPE_BLOB:
				case Vocabulary::TYPE_ZBLOB:			// It won't be compressed in the index
					try {
						// Measure the size
						size = (boost::get<const Event::BlobType&>(*e->getPointer(m_cache_terms[i].term_ref))).size();
					} catch (...) {
						size = 0;						// Default to zero when not found
					}
					break;
				case Vocabulary::TYPE_DATE_TIME:
					size = 4+1+2+1+2 +1+ 2+1+2+1+2;		// 2009-09-29T11:43:00
					break;
				case Vocabulary::TYPE_DATE:
					size = 4+1+2+1+2;					// 2009-09-29
					break;
				case Vocabulary::TYPE_TIME:
					size = 2+1+2+1+2;					// 11:43:00
					break;
			}
			m_cache_consumption += size + m_cache_overhead;
		}
		m_cache_rows++;

		// signal the worker thread if the queue is full (wait for swap)
		while (m_event_queue_ptr->size() >= m_queue_max) {

			// wakeup the worker thread to swap queues & insert events
			m_wakeup_worker.notify_one();

			// wait until the worker thread has swapped the queues
			m_swapped_queue.wait(queue_lock);
		
			// exit immediately if no longer running
			if (! m_is_running)
				return;
		}

		// add the event to the insert queue
		m_event_queue_ptr->push_back(e);
	}
}

void DatabaseInserter::connect(void)
{
	// open a new database connection
	PION_LOG_DEBUG(m_logger, "Connecting to database: " << m_database_id);
	getDatabasePlugin(m_database_ptr, m_database_id);

	if (m_wipe && m_database_ptr->tableExists(m_table_name, m_partition)) {
		m_database_ptr->dropTable(m_table_name, m_partition);
		PION_LOG_DEBUG(m_logger, "Wiping partition: " << m_table_name << "/" << m_partition << " on thread: " << m_database_id);
	}

	// open up the database if it isn't already open
	// TODO: This only works with SQLite (wiping before opening)
	m_database_ptr->open(m_partition);
	PION_ASSERT(m_database_ptr->is_open());

	// create the database table if it does not yet exist
	m_database_ptr->createTable(m_field_map, m_table_name, m_index_map, m_partition);
	m_table_size = m_database_ptr->getCache(Database::DB_FILE_SIZE);

	// prepare the query used to insert new events into the table
	m_insert_query_ptr = m_ignore_insert ? m_database_ptr->prepareInsertIgnoreQuery(m_field_map, m_table_name) : m_database_ptr->prepareInsertQuery(m_field_map, m_table_name);
	PION_ASSERT(m_insert_query_ptr);

	// prepare the query used to begin new transactions
	m_begin_transaction_ptr = m_database_ptr->getBeginTransactionQuery();
	PION_ASSERT(m_begin_transaction_ptr);

	// prepare the query used to commit transactions
	m_commit_transaction_ptr = m_database_ptr->getCommitTransactionQuery();
	PION_ASSERT(m_commit_transaction_ptr);
		
	m_cache_overhead = m_database_ptr->getCache(Database::CACHE_INDEX_ROW_OVERHEAD);
	m_cache_size = m_database_ptr->getCache(Database::CACHE_PAGE_CACHE_SIZE);

	PION_LOG_DEBUG(m_logger, "Database connected (cache overhead: " << m_cache_overhead << ", cache size: " << m_cache_size / 1024UL << "k)");
}

bool DatabaseInserter::tryConnecting()
{
	try {
		connect();
		m_next_connect = 0;
		return true;
	} catch (std::exception& e) {
		m_next_connect = now() + m_recovery_interval;
		PION_LOG_ERROR(m_logger, "Lost database connection; retry in " << m_recovery_interval << " seconds: " << e.what());
	} catch (...) {
		m_next_connect = now() + m_recovery_interval;
		PION_LOG_ERROR(m_logger, "Lost database connection; retry in " << m_recovery_interval << " seconds: unknown exception");
	}
	return false;
}

bool DatabaseInserter::checkConnection()
{
	// check if we need to try to reconnect
	bool connection_ok = true;
	if (m_next_connect==0) {
		if (!m_database_ptr->is_open()) {
			// we just lost the connection -> try reconnect immediately
			connection_ok = tryConnecting();
			if (connection_ok) {
				PION_LOG_WARN(m_logger, "Lost connection but recovered successfully");
			}
		}
	} else if (now() >= m_next_connect) {
		// time to try reconnecting again
		connection_ok = tryConnecting();
		if (connection_ok) {
			PION_LOG_WARN(m_logger, "Successfully reconnected to database");
		}
	} else {
		// not yet time to reconnect
		connection_ok = false;
	}
	return connection_ok;
}

void DatabaseInserter::insertEvents(void)
{
	PION_LOG_DEBUG(m_logger, "Worker thread is running: " << m_database_id);

	try {

		// sanity checks (should all be handled now by start())
		PION_ASSERT(m_database_ptr);
		PION_ASSERT(m_database_ptr->is_open());
		PION_ASSERT(m_insert_query_ptr);
		PION_ASSERT(m_begin_transaction_ptr);
		PION_ASSERT(m_commit_transaction_ptr);

		// queue of events pending insertion into the database
		std::unique_ptr<EventQueue>	insert_queue_ptr(new EventQueue);
		insert_queue_ptr->reserve(m_queue_max);

		// notify all threads that we have started up
		{
			// lock first to ensure start() thread is waiting when signal is sent
			boost::mutex::scoped_lock queue_lock(m_queue_mutex);
			m_swapped_queue.notify_all();
		}

		while (m_is_running) {
		
			// check if new events are available in the "insert_queue"
			if (checkEventQueue(insert_queue_ptr)) {

				PION_LOG_DEBUG(m_logger, "Worker thread woke with " << insert_queue_ptr->size() << " events available: " << m_database_id);
	
				try {

					// attempt to insert events into the database
					insertEvents(insert_queue_ptr);

				} catch (Database::DatabaseBusyException& e) {
				
					// data was busy and could not recover after timeout
					PION_LOG_ERROR(m_logger, "Dropping " << insert_queue_ptr->size() << " events because database was busy: " << m_database_id);
					insert_queue_ptr->clear();	// drop events in insert queue
					m_keys.clear();				// erase key cache since it may be inaccurate

				} catch (...) {
					
					// check if insert failed because we lost database connection
					bool able_to_recover = false;
					if (!m_database_ptr->is_open()) {
						// it did -> try to reconnect and insert again
						if (checkConnection()) {
							try {
								insertEvents(insert_queue_ptr);
								able_to_recover = true;
							} catch (...) {}
						}
					}
					if (!able_to_recover) {
						// insert failure occured -> just log and drop versus stopping altogether
						PION_LOG_ERROR(m_logger, "Dropping " << insert_queue_ptr->size() << " events due to insert exception");
						insert_queue_ptr->clear();	// drop events in insert queue
						m_keys.clear();				// erase key cache since it may be inaccurate
					}
				}

			} else {
				PION_LOG_DEBUG(m_logger, "Worker thread woke with no new events: " << m_database_id);
			}
		}
		
	} catch (std::exception& e) {
		PION_LOG_FATAL(m_logger, e.what());
		m_is_running = false;
		m_swapped_queue.notify_all();
	} catch (...) {
		PION_LOG_FATAL(m_logger, "Worker thread caught unknown exception");
		m_is_running = false;
		m_swapped_queue.notify_all();
	}

	PION_LOG_DEBUG(m_logger, "Worker thread is exiting: " << m_database_id);
}

void DatabaseInserter::insertEvents(std::unique_ptr<EventQueue>& insert_queue_ptr)
{
	// begin a new transaction
	m_begin_transaction_ptr->run();
	m_begin_transaction_ptr->reset();
	
	// step through the event queue, inserting each event individually
	for (unsigned int n = 0; n < insert_queue_ptr->size(); ++n) {
		// bind the event to the insert query
		m_insert_query_ptr->bindEvent(m_field_map, *((*insert_queue_ptr)[n]), false);
		// execute the query to insert the record
		m_insert_query_ptr->run();
		m_insert_query_ptr->reset();
	}
	
	// end & commit the transaction
	m_commit_transaction_ptr->run();
	m_commit_transaction_ptr->reset();
	
	// done flushing the queue
	PION_LOG_DEBUG(m_logger, "Worker thread wrote " << insert_queue_ptr->size() << " events: " << m_database_id);
	insert_queue_ptr->clear();
	
	// Pruning needed?
	if (m_max_age) {
		boost::uint32_t size_before, size_after;
		{
			boost::mutex::scoped_lock queue_lock(m_queue_mutex);
			boost::uint32_t min_age = m_last_time - m_max_age;
			size_before = m_keys.size();
			KeyHash::iterator cur_it;
			KeyHash::iterator i = m_keys.begin();
			while (i != m_keys.end()) {
				cur_it = i;
				++i;
				if (cur_it->second < min_age)
					m_keys.erase(cur_it);
			}
			size_after = m_keys.size();
		}
		PION_LOG_DEBUG(m_logger, "Worker thread pruned " << (size_before - size_after) << " keys from cache, " << size_after << " left");
	}
	
	m_table_size = m_database_ptr->getCache(Database::DB_FILE_SIZE);	
}

bool DatabaseInserter::checkEventQueue(std::unique_ptr<EventQueue>& insert_queue_ptr)
{
	// acquire ownership of queue
	boost::mutex::scoped_lock queue_lock(m_queue_mutex);
	
	// skip waiting if the queue is already full (missed wakeup signal)
	if (m_event_queue_ptr->size() < m_queue_max) {

		// wait until the queue is full or the timeout expires
		m_wakeup_worker.timed_wait(queue_lock,
			boost::get_system_time() + boost::posix_time::time_duration(0, 0, m_queue_timeout, 0) );
	
		// check for early & spurious wake-ups
		if (m_event_queue_ptr->size() == 0)
			return false;
	}

	if (!checkConnection()) {
		// still waiting for reconnect
		if (m_event_queue_ptr->size() >= m_queue_max) {
			// queue is full -> drop events
			PION_LOG_ERROR(m_logger, "Dropping " << m_event_queue_ptr->size() << " events: not ready to reconnect");
			m_event_queue_ptr->clear();	// drop events in insert queue
			m_keys.clear();				// erase key cache since it may be inaccurate
			// notify threads that the event queue has been cleared
			m_swapped_queue.notify_all();
		}	// else not yet full -> just wait a little longer
		return false;
	}

	// swap the event queues
	insert_queue_ptr.swap(m_event_queue_ptr);

	// notify threads that the event queue has been swapped
	m_swapped_queue.notify_all();

	return true;
}

bool DatabaseInserter::getDatabasePlugin(DatabasePtr& plugin_ptr, const std::string& plugin_id) const
{
	if (!m_platform_plugin_ptr)
		throw MissingPlatformPluginException();
	return m_platform_plugin_ptr->getDatabasePlugin(plugin_ptr, plugin_id);
}

bool DatabaseInserter::hasDatabasePlugin(const std::string& plugin_id) const
{
	return (m_platform_plugin_ptr && m_platform_plugin_ptr->hasDatabasePlugin(plugin_id));
}

}	// end namespace platform
}	// end namespace pion


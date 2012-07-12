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

#ifndef __PION_DATABASE_HEADER__
#define __PION_DATABASE_HEADER__

#include <map>
#include <string>
#include <boost/regex.hpp>
#include <pion/PionConfig.hpp>
#include <pion/platform/Event.hpp>
#include <pion/platform/Vocabulary.hpp>
#include <pion/platform/PlatformPlugin.hpp>
#include <pion/platform/Query.hpp>
#include <pion/PionLogger.hpp>


namespace pion {		// begin namespace pion
namespace platform {	// begin namespace platform (Pion Platform Library)


///
/// Database: abstract class for storing and retrieving Events
///
class PION_PLATFORM_API Database
	: public PlatformPlugin
{
public:

	/// name of the database engines config file
	static const std::string				DBENGINES_FILE;

	/// element names used in database engines config file
	static const std::string				DBENGINES_ROOT_ELEMENT_NAME;
	static const std::string				TEMPLATE_ELEMENT_NAME;

	/// name of XML element for the database engine type
	static const std::string				ENGINE_ELEMENT_NAME;

	/// exception thrown if the database is busy & the query should be tried again later
	class DatabaseBusyException : public std::exception {
	public:
		virtual const char* what() const throw() {
			return "Database is busy.  Try again later";
		}
	};

	/// exception thrown if a Query cannot be found
	class QueryNotFoundException : public PionException {
	public:
		QueryNotFoundException(const std::string& query_id)
			: PionException("Unable find database query identified by: ", query_id) {}
	};

	/// exception thrown if there is an error opening a Database
	class OpenDatabaseException : public PionException {
	public:
		OpenDatabaseException(const std::string& db_name)
			: PionException("Unable to open database: ", db_name) {}
	};

	/// exception for missing configuration element
	class DatabaseConfigMissing: public PionException {
	public:
		DatabaseConfigMissing(const std::string& database_id)
			: PionException("Database template element missing: ", database_id) {}
	};

	class DatabaseClientException : public PionException {
	public:
		DatabaseClientException(const std::string& database_id)
			: PionException("Database client not defined: ", database_id) {}
	};

	/// Isolation level must be: ReadUncommitted (default), ReadCommitted, RepeatableRead, Serializable
	class InvalidIsolationLevel : public PionException {
	public:
		InvalidIsolationLevel(const std::string& database_id)
			: PionException("Invalid Isolation level defined: ", database_id) {}
	};

	/// Bad type pair in Vocabulary-to-SQL map
	class BadTypePair : public PionException {
	public:
		BadTypePair(const std::string& bad_pair)
			: PionException("Bad SQL Type pair: ", bad_pair) {}
	};

	/// Every database engine must have Vocabulary-to-SQL map
	class MissingTypeMap : public PionException {
	public:
		MissingTypeMap(const std::string& database_id)
			: PionException("SQL Type map missing: ", database_id) {}
	};

	class MissingTemplateException : public PionException {
	public:
		MissingTemplateException(const std::string& database_id)
			: PionException("Database template file not found: ", database_id) {}
	};

	class MissingRootElementException : public PionException {
	public:
		MissingRootElementException(const std::string& database_id)
			: PionException("Database template file missing root element: ", database_id) {}
	};

	class ReadConfigException: public PionException {
	public:
		ReadConfigException(const std::string& database_id)
			: PionException("Database template file not readable: ", database_id) {}
	};

	/// constructs a new Database object
	Database(const std::string& logger_name)
		: m_isolation_level(IL_LevelUnknown), m_logger(PION_GET_LOGGER(logger_name))
	{}

	/// virtual destructor: this class is meant to be extended
	virtual ~Database() {}

	/**
	 * retrieves a pre-compiled query for the database
	 *
	 * @param query_id string used to uniquely identify the type of query
	 * @return QueryPtr pointer to the compiled query
	 */
	inline QueryPtr getQuery(QueryID query_id) const {
		QueryMap::const_iterator i = m_query_map.find(query_id);
		if (i == m_query_map.end())
			throw QueryNotFoundException(query_id);
		return i->second;
	}

	/**
	 * clones the Database, returning a pointer to the cloned copy
	 *
	 * @return DatabasePtr pointer to the cloned copy of the Database
	 */
	virtual std::shared_ptr<Database> clone(void) const = 0;

	/**
	 * opens the database connection
	 *
	 * @param partition partition number, if non-zero
	 */
	virtual void open(unsigned partition = 0) = 0;

	/// closes the database connection
	virtual void close(void) = 0;

	/// Cache related queries
	enum CACHEPARAM {
		CACHE_INDEX_ROW_OVERHEAD, 
		CACHE_PAGE_CACHE_SIZE, 
		CACHE_PAGE_UTILIZATION,
		DB_FILE_SIZE
	};

	/// Get various cache related parameters
	virtual boost::uint64_t getCache(CACHEPARAM what) = 0;

	/// returns true if the database connection is open
	virtual bool is_open(void) const = 0;

	/**
	 * runs a simple query, ignoring any results returned
	 *
	 * @param sql_query SQL query to execute
	 * @param suppress Regex to describe what errors/exceptions to suppress
	 */
	virtual void runQuery(const std::string& sql_query, const boost::regex& suppress) = 0;

	/// Same as runQuery, but no errors are suppressed
	inline void runQuery(const std::string& sql_query)
	{
		static boost::regex no_suppress;
		runQuery(sql_query, no_suppress);
	}

	/**
	 * adds a compiled SQL query to the database
	 *
	 * @param query_id string used to uniquely identify the type of query
	 * @param sql_query SQL query to compile and cache for later use
	 */
	virtual QueryPtr addQuery(QueryID query_id, const std::string& sql_query) = 0;

	/**
	 * creates a database table for output, if it does not already exist
	 *
	 * @param field_map mapping of Vocabulary Terms to Database fields
	 * @param table_name name of the table to create (modified to reflect partition)
	 * @param index_map table of indexes, matches field_map
	 */
	virtual void createTable(const Query::FieldMap& field_map,
							std::string& table_name,
							const Query::IndexMap& index_map,
							unsigned partition = 0) = 0;

	/**
	 * drops table, fastest way
	 *
	 * @param table_name name of the table to drop
	 * @param partition optional partition number
	 */
	virtual void dropTable(std::string& table_name, unsigned partition = 0) = 0;

	/**
	 * table exists?
	 *
	 * @param table_name name of the table to drop
	 * @param partition optional partition number
	 */
	virtual bool tableExists(std::string& table_name, unsigned partition = 0) = 0;

	/**
	 * prepares the query that is used to insert events
	 *
	 * @param field_map mapping of Vocabulary Terms to Database fields
	 * @param table_name name of the table to insert events into
	 *
	 * @return QueryPtr smart pointer to the new query for inserting events
	 */
	virtual QueryPtr prepareInsertQuery(const Query::FieldMap& field_map,
										const std::string& table_name) = 0;

	virtual QueryPtr prepareInsertIgnoreQuery(const Query::FieldMap& field_map,
										const std::string& table_name) = 0;

	/**
	 * prepares a generic query
	 *
	 * @param query the SQL for the query
	 * @param suppress regex containing pattern of suppressable errors
	 *
	 * @return QueryPtr smart pointer to the new query
	 */
	virtual pion::platform::QueryPtr prepareFullQuery(const std::string& query, const boost::regex& suppress) = 0;

	/// same as prepareFullQuery, but no errors are suppressed
	inline pion::platform::QueryPtr prepareFullQuery(const std::string& query)
	{
		static boost::regex no_suppress;
		return prepareFullQuery(query, no_suppress);
	}

	/**
	 * returns the query that is used to begin new transactions
	 *
	 * @return QueryPtr smart pointer to the "begin transaction" query
	 */
	virtual QueryPtr getBeginTransactionQuery(void) = 0;

	/**
	 * returns the query that is used to end and commit transactions
	 *
	 * @return QueryPtr smart pointer to the "commit transaction" query
	 */
	virtual QueryPtr getCommitTransactionQuery(void) = 0;

	/**
	 * sets configuration parameters for this Database
	 *
	 * @param v the Vocabulary that this Database will use to describe Terms
	 * @param config_ptr pointer to a list of XML nodes containing Database
	 *                   configuration parameters
	 */
	virtual void setConfig(const Vocabulary& v, const xmlNodePtr config_ptr);

	/**
	 * generic simple, non-escaped, string-replace function; replace every instance of search with substitute
	 *
	 * @param src string to modify
	 * @param search string to find (and substitute)
	 * @param substitute string to put in place of search in src
	 */
	void stringReplace(std::string& src, const char* search, const std::string& substitute);

	/**
	 * replace TABLE/FIELD/COLUMNS/QUESTIONS/PARAMS with appropriate values in pseudo-SQL
	 *
	 * @param query string to modify, contains the original pseudo-SQL
	 * @param field_map fieldMap to use for substitutions of COLUMNS/PARAMS
	 * @param table_name name of table to use to substitute TABLE
	 * @param columns_override is the optional columns list override (for custom indexes)
	 * @return std::string returns modified query
	 */
	std::string& stringSubstitutes(std::string& query, const pion::platform::Query::FieldMap& field_map,
									const std::string& table_name, const std::string& columns_override = "");

	/// Database Isolation Levels
	enum IsolationLevel_t {
		IL_LevelUnknown = -1,
		IL_ReadUncommitted,
		IL_ReadCommitted,
		IL_RepeatableRead,
		IL_Serializable
	};

protected:

	/// protected copy function (use clone() instead)
	inline void copyDatabase(const Database& d) {
		PlatformPlugin::copyPlugin(d);
		// clone all the top level defined member variables
		m_database_engine = d.m_database_engine;
		m_database_client = d.m_database_client;
		m_begin_insert = d.m_begin_insert;
		m_commit_insert = d.m_commit_insert;
		m_create_log = d.m_create_log;
		m_create_log_attr = d.m_create_log_attr;
		m_insert_log = d.m_insert_log;
		m_insert_log_attr = d.m_insert_log_attr;
		m_create_stat = d.m_create_stat;
		m_create_stat_attr = d.m_create_stat_attr;
		m_update_stat = d.m_update_stat;
		m_update_stat_attr = d.m_update_stat_attr;
		m_select_stat = d.m_select_stat;
		m_select_stat_attr = d.m_select_stat_attr;
		m_drop_index = d.m_drop_index;
		m_drop_index_attr = d.m_drop_index_attr;
		m_create_index_normal = d.m_create_index_normal;
		m_create_index_normal_attr = d.m_create_index_normal_attr;
		m_create_index_unique = d.m_create_index_unique;
		m_create_index_unique_attr = d.m_create_index_unique_attr;
		m_create_index_custom = d.m_create_index_custom;
		m_create_index_custom_attr = d.m_create_index_custom_attr;
		m_isolation_level = d.m_isolation_level;
		// Query_map will be overridden
//		m_query_map = d.m_query_map;
		m_sql_affinity = d.m_sql_affinity;
		m_pre_sql = d.m_pre_sql;
		m_pre_sql_attr = d.m_pre_sql_attr;
		m_options = d.m_options;
		m_options_values = d.m_options_values;
		m_insert_ignore = d.m_insert_ignore;
		m_drop_table = d.m_drop_table;
		m_drop_table_attr = d.m_drop_table_attr;
	}


	/// data type that maps query identifiers to pointers of compiled queries
	typedef std::map<QueryID, QueryPtr>		QueryMap;

	/// unique identifier used to represent the "insert event" query
	static const std::string				INSERT_QUERY_ID;
	static const std::string				INSERT_IGNORE_QUERY_ID;


	/// unique identifier used to represent the "begin transaction" query
	static const std::string				BEGIN_QUERY_ID;

	/// unique identifier used to represent the "commit transaction" query
	static const std::string				COMMIT_QUERY_ID;

	static const std::string				MAP_ELEMENT_NAME;
	static const std::string				PAIR_ELEMENT_NAME;

	static const std::string				CLIENT_ELEMENT_NAME;
	static const std::string				BEGIN_ELEMENT_NAME;
	static const std::string				COMMIT_ELEMENT_NAME;
	static const std::string				CREATE_LOG_ELEMENT_NAME;
	static const std::string				INSERT_LOG_ELEMENT_NAME;
	static const std::string				ISOLATION_ELEMENT_NAME;
	static const std::string				PRESQL_ELEMENT_NAME;
	static const std::string				OPTION_ELEMENT_NAME;

	static const std::string				CREATE_STAT_ELEMENT_NAME;
	static const std::string				UPDATE_STAT_ELEMENT_NAME;
	static const std::string				SELECT_STAT_ELEMENT_NAME;

	static const std::string				DROP_INDEX_ELEMENT_NAME;
	static const std::string				CREATE_INDEX_NORMAL_ELEMENT_NAME;
	static const std::string				CREATE_INDEX_UNIQUE_ELEMENT_NAME;
	static const std::string				CREATE_INDEX_CUSTOM_ELEMENT_NAME;

	static const std::string				IGNORE_ATTRIBUTE_NAME;
	static const std::string				OPTION_ATTRIBUTE_NAME;

	static const std::string				INSERT_IGNORE_ELEMENT_NAME;
	static const std::string				DROP_TABLE_ELEMENT_NAME;


	/**
	 * readConfig must be called by a Database implementation as soon as it has called setConfig
	 *
	 * @param config_ptr Pointer to XML configuration (passed in as config_ptr)
	 * @param engine_str String identifying the database/engine that is being initialized
	 */
	void readConfig(const xmlNodePtr config_ptr, std::string engine_str);

	/**
	 * readConfigDetails is called by readConfig, once it locates the SQL details for an engine
	 *
	 * @param config_ptr XML pointer to where SQL details are found
	 */
	void readConfigDetails(const xmlNodePtr config_ptr);

	/**
	 * gets the configuration for the specified database engine
	 *
	 * @param database_engine the name of the database engine
	 * @param config_detail_ptr XML configuration for the database engine
	 */
	xmlDocPtr getDatabaseEngineConfig(const std::string& database_engine, xmlNodePtr& config_detail_ptr);


	/// Database engine name, e.g. MySQL-MyISAM
	/// This is used to find a matching set of SQL from configuation file
	std::string								m_database_engine;

	/// name of database client, e.g. MySQL
	std::string								m_database_client;

	/// begin & commit insert strings
	std::string								m_begin_insert;
	std::string								m_commit_insert;

	/// Database create/insert statements, and their optional ignore attributes
	std::string								m_create_log;
	boost::regex							m_create_log_attr;

	std::string								m_insert_log;
	boost::regex							m_insert_log_attr;

	std::string								m_create_stat;
	boost::regex							m_create_stat_attr;

	std::string								m_update_stat;
	boost::regex							m_update_stat_attr;

	std::string								m_select_stat;
	boost::regex							m_select_stat_attr;

	std::string								m_drop_index;
	boost::regex							m_drop_index_attr;

	std::string								m_create_index_normal;
	boost::regex							m_create_index_normal_attr;

	std::string								m_create_index_unique;
	boost::regex							m_create_index_unique_attr;

	std::string								m_create_index_custom;
	boost::regex							m_create_index_custom_attr;

	std::string								m_insert_ignore;
	boost::regex							m_insert_ignore_attr;

	std::string								m_drop_table;
	boost::regex							m_drop_table_attr;

	/// Isolation level, default is SA_ReadUncommitted (for speed/ease)
	IsolationLevel_t						m_isolation_level;

	/// used to keep track of all the database's pre-compiled queries
	QueryMap								m_query_map;

	/// Affinity map; index is term_type, value should be compatible SQL type
	std::vector<std::string>				m_sql_affinity;

	/// PreSQL statements, to be executed before table operations
	std::vector<std::string>				m_pre_sql;

	/// Optional "ignore" attributes for PreSQL statements
	std::vector<boost::regex>				m_pre_sql_attr;

	/// SQLAPI Options to be executed before connecting
	std::vector<std::string>				m_options;

	/// SQLAPI Options values to be executed before connecting
	std::vector<std::string>				m_options_values;

	/// primary logging interface used by this class
	PionLogger								m_logger;
};


/// data type used for Database smart pointers
typedef std::shared_ptr<Database>			DatabasePtr;


//
// The following symbols must be defined for any Databases that you would
// like to be able to load dynamically using the DatabaseManager::load()
// function.  These are not required for any Databases that you only want
// to link directly into your programs.
//
// Make sure that you replace "DATABASE" with the name of your derived class.
// This name must also match the name of the object file (excluding the
// extension).  These symbols must be linked into your Database's object
// file, not included in any headers that it may use (declarations are OK in
// headers but not the definitions).
//
// The "pion_create" function is used to create new instances of your Database.
// The "pion_destroy" function is used to destroy instances of your Database.
//
// extern "C" PION_PLUGIN_API Database *pion_create_DATABASE(void) {
//		return new DATABASE;
// }
//
// extern "C" PION_PLUGIN_API void pion_destroy_DATABASE(DATABASE *database_ptr) {
//		delete database_ptr;
// }
//


}	// end namespace platform
}	// end namespace pion

#endif

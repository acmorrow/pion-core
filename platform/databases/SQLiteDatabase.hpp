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

#ifndef __PION_SQLITEDATABASE_HEADER__
#define __PION_SQLITEDATABASE_HEADER__

#include <boost/filesystem/path.hpp>
#include <pion/PionConfig.hpp>
#include <pion/PionException.hpp>
#include <pion/platform/Database.hpp>
#include "../../sqlite/pion-sqlite.h"

#if SQLITE_VERSION_NUMBER != 3007005
#error Incorrect version of Pion-SQLite
#endif

namespace pion {		// begin namespace pion
namespace plugins {		// begin namespace plugins


///
/// SQLiteDatabase: class for storing and retrieving Events using a SQLite database
///
class SQLiteDatabase :
	public pion::platform::Database
{
	PionLogger									m_logger;

public:

	/// exception thrown if the SQLiteDatabase configuration does not define a Filename
	class EmptyFilenameException : public PionException {
	public:
		EmptyFilenameException(const std::string& database_id)
			: PionException("SQLiteDatabase configuration is missing a required Filename parameter: ", database_id) {}
	};

	/// exception thrown if the the SQLite API returns an unexpected result
	class SQLiteAPIException : public PionException {
	public:
		SQLiteAPIException(const std::string& sqlite_error_msg)
			: PionException("SQLiteDatabase API error: ", sqlite_error_msg) {}
	};

	/// exception thrown if the the SQLite API returns an unexpected result
	class DBStillOpen : public PionException {
	public:
		DBStillOpen(const std::string& database_id)
			: PionException("SQLiteDatabase DROP on table which is still open: ", database_id) {}
	};


	/**
	 * constructs a new SQLiteDatabase object
	 */
	SQLiteDatabase(void)
		: pion::platform::Database("pion.SQLiteDatabase"), m_logger(PION_GET_LOGGER("pion.SQLiteDatabase")),
			m_sqlite_db(NULL), m_error_ptr(NULL), m_cache_size(0), m_partition(0)
	{
	}

	/// virtual destructor: this class is meant to be extended
	virtual ~SQLiteDatabase()
	{
		m_query_map.clear();
		close();
	}

	/**
	 * clones the Database, returning a pointer to the cloned copy
	 *
	 * @return DatabasePtr pointer to the cloned copy of the Database
	 */
	virtual pion::platform::DatabasePtr clone(void) const;

	/**
	 * opens the database connection
	 *
	 * @param create_backup if true, create a backup of the old database before opening
	 */
	virtual void open(unsigned partition = 0);

	/// closes the database connection
	virtual void close(void);

	/// Produce database name, based on table name, and partition#
	std::string dbPartition(std::string name, unsigned partition)
	{
		if (partition) {
			char buff[10];
			sprintf(buff, "_%03u.db", partition);
			std::string::size_type i = 0;
			if ((i = name.find(".db", i)) != std::string::npos)
				name.replace(i, strlen(".db"), buff);
			else
				name += buff;
		}
		return name;
	}

	/// get various database related configuration parameters
	virtual boost::uint64_t getCache(CACHEPARAM what)
	{
		switch (what) {
			case CACHE_INDEX_ROW_OVERHEAD:
				return 9;	// SQLite has a 9 byte rowid
			case CACHE_PAGE_CACHE_SIZE:
				return m_cache_size;
			case CACHE_PAGE_UTILIZATION:
				return 85;	// average efficiency for index
			case DB_FILE_SIZE:
				{
					boost::filesystem::path f(dbPartition(m_database_name, m_partition).c_str());
  					if (!boost::filesystem::exists(f) || !boost::filesystem::is_regular(f))
						return 0;
					return boost::filesystem::file_size(f);
				}
		}
		return 0;	// Just to make compiler happy...
	}

	/// returns true if the database connection is open
	virtual bool is_open(void) const { return m_sqlite_db != NULL; }

	/**
	 * runs a simple query, ignoring any results returned
	 *
	 * @param sql_query SQL query to execute
	 * @param suppress Regex to describe what errors/exceptions to suppress
	 */
	virtual void runQuery(const std::string& sql_query, const boost::regex& suppress);

	/**
	 * adds a compiled SQL query to the database
	 *
	 * @param query_id string used to uniquely identify the type of query
	 * @param sql_query SQL query to compile and cache for later use
	 */
	virtual pion::platform::QueryPtr addQuery(pion::platform::QueryID query_id,
											  const std::string& sql_query);

	/**
	 * creates a database table for output, if it does not already exist
	 *
	 * @param field_map mapping of Vocabulary Terms to Database fields
	 * @param table_name name of the table to create
	 * @param index_map table of indexes, matches field_map
	 * @param partition optional partition number (default 0 = no partition)
	 */
	virtual void createTable(const pion::platform::Query::FieldMap& field_map,
							std::string& table_name,
							const pion::platform::Query::IndexMap& index_map,
							unsigned partition);

	/**
	 * drops table, fastest way
	 *
	 * @param table_name name of the table to drop
	 * @param partition optional partition number
	 */
	virtual void dropTable(std::string& table_name, unsigned partition = 0);

	/**
	 * table exists?
	 *
	 * @param table_name name of the table to drop
	 * @param partition optional partition number
	 */
	virtual bool tableExists(std::string& table_name, unsigned partition = 0);

	/**
	 * prepares the query that is used to insert events
	 *
	 * @param field_map mapping of Vocabulary Terms to Database fields
	 * @param table_name name of the table to insert events into
	 *
	 * @return QueryPtr smart pointer to the new query for inserting events
	 */
	virtual pion::platform::QueryPtr prepareInsertQuery(const pion::platform::Query::FieldMap& field_map,
														const std::string& table_name);

	virtual pion::platform::QueryPtr prepareInsertIgnoreQuery(const pion::platform::Query::FieldMap& field_map,
														const std::string& table_name);

/**
	 * prepares a generic query
	 *
	 * @param query the SQL for the query
	 * @param suppress regex containing pattern of suppressable errors
	 *
	 * @return QueryPtr smart pointer to the new query
	 */
	virtual pion::platform::QueryPtr prepareFullQuery(const std::string& query, const boost::regex& suppress);

	/**
	 * returns the query that is used to begin new transactions
	 *
	 * @return QueryPtr smart pointer to the "begin transaction" query
	 */
	virtual pion::platform::QueryPtr getBeginTransactionQuery(void);

	/**
	 * returns the query that is used to end and commit transactions
	 *
	 * @return QueryPtr smart pointer to the "commit transaction" query
	 */
	virtual pion::platform::QueryPtr getCommitTransactionQuery(void);

	/**
	 * sets configuration parameters for this Database
	 *
	 * @param v the Vocabulary that this Database will use to describe Terms
	 * @param config_ptr pointer to a list of XML nodes containing Database
	 *                   configuration parameters
	 */
	virtual void setConfig(const pion::platform::Vocabulary& v, const xmlNodePtr config_ptr);

	/**
	 * throws an API exception using the most recent log message set for a database
	 *
	 * @param db_ptr pointer to a SQLite database to use
	 */
	static inline void throwAPIException(sqlite3 *db_ptr) {
		PION_ASSERT(db_ptr != NULL);
		std::string error_msg(sqlite3_errmsg(db_ptr));
		throw SQLiteAPIException(error_msg);
	}


protected:

	/// returns a string containing the SQLite API error message
	inline std::string getSQLiteError(void);


	///
	/// SQLiteQuery: a compiled query used by SQLite databases
	///
	class SQLiteQuery
		: public pion::platform::Query
	{
	public:

		/// virtual destructor releases the prepared statement
		virtual ~SQLiteQuery() { sqlite3_finalize(m_sqlite_stmt); }

		/**
		 * constructs a new SQLiteQuery object
		 *
		 * @param database reference to the Database used to perform the query
		 * @param sqlite_db the SQLite database that this query belongs to
		 */
		SQLiteQuery(const std::string& sql_query, sqlite3 *db_ptr);

		/**
		 * binds a NULL value to a query parameter
		 *
		 * @param param the query parameter number to which the value will be bound (starting with 0)
		 */
		virtual void bindNull(unsigned int param) {
			if (sqlite3_bind_null(m_sqlite_stmt, param+1) != SQLITE_OK)
				SQLiteDatabase::throwAPIException(m_sqlite_db);
		}

		/**
		 * binds a std::string value to a query parameter
		 *
		 * @param param the query parameter number to which the value will be bound (starting with 0)
		 * @param value the value to bind to the query parameter
		 * @param copy_value if true, the string will be copied into a temporary buffer
		 */
		virtual void bindString(unsigned int param, const std::string& value, bool copy_value = true) {
			if (sqlite3_bind_text(m_sqlite_stmt, param+1, value.c_str(),
								  value.size(), (copy_value ? SQLITE_TRANSIENT : SQLITE_STATIC)) != SQLITE_OK)
				SQLiteDatabase::throwAPIException(m_sqlite_db);
		}

		/**
		 * binds a std::string value to a query parameter (Blob)
		 *
		 * @param param the query parameter number to which the value will be bound (starting with 0)
		 * @param value the value to bind to the query parameter
		 * @param copy_value if true, the string will be copied into a temporary buffer
		 */
		virtual void bindBlob(unsigned int param, const char *value, size_t size, bool copy_value = true) {
			if (sqlite3_bind_blob(m_sqlite_stmt, param+1, value,
								  size, (copy_value ? SQLITE_TRANSIENT : SQLITE_STATIC)) != SQLITE_OK)
				SQLiteDatabase::throwAPIException(m_sqlite_db);
		}

		/**
		 * binds a string (const char *) value to a query parameter
		 *
		 * @param param the query parameter number to which the value will be bound (starting with 0)
		 * @param value the value to bind to the query parameter
		 * @param copy_value if true, the string will be copied into a temporary buffer
		 */
		virtual void bindString(unsigned int param, const char *value, bool copy_value = true) {
			if (sqlite3_bind_text(m_sqlite_stmt, param+1, value,
								  -1, (copy_value ? SQLITE_TRANSIENT : SQLITE_STATIC)) != SQLITE_OK)
				SQLiteDatabase::throwAPIException(m_sqlite_db);
		}

		/**
		 * binds an integer value to a query parameter
		 *
		 * @param param the query parameter number to which the value will be bound (starting with 0)
		 * @param value the value to bind to the query parameter
		 */
		virtual void bindInt(unsigned int param, const boost::int32_t value) {
			if (sqlite3_bind_int(m_sqlite_stmt, param+1, value) != SQLITE_OK)
				SQLiteDatabase::throwAPIException(m_sqlite_db);
		}

		/**
		 * binds an unsigned integer value to a query parameter
		 *
		 * @param param the query parameter number to which the value will be bound (starting with 0)
		 * @param value the value to bind to the query parameter
		 */
		virtual void bindUInt(unsigned int param, const boost::uint32_t value) {
			if (sqlite3_bind_int(m_sqlite_stmt, param+1, value) != SQLITE_OK)
				SQLiteDatabase::throwAPIException(m_sqlite_db);
		}

		/**
		 * binds a big integer value to a query parameter
		 *
		 * @param param the query parameter number to which the value will be bound (starting with 0)
		 * @param value the value to bind to the query parameter
		 */
		virtual void bindBigInt(unsigned int param, const boost::int64_t value) {
			if (sqlite3_bind_int64(m_sqlite_stmt, param+1, value) != SQLITE_OK)
				SQLiteDatabase::throwAPIException(m_sqlite_db);
		}

		/**
		 * binds an unsigned big integer value to a query parameter
		 *
		 * @param param the query parameter number to which the value will be bound (starting with 0)
		 * @param value the value to bind to the query parameter
		 */
		virtual void bindUBigInt(unsigned int param, const boost::uint64_t value) {
			if (sqlite3_bind_int64(m_sqlite_stmt, param+1, value) != SQLITE_OK)
				SQLiteDatabase::throwAPIException(m_sqlite_db);
		}

		/**
		 * binds a floating point number value to a query parameter
		 *
		 * @param param the query parameter number to which the value will be bound (starting with 0)
		 * @param value the value to bind to the query parameter
		 */
		virtual void bindFloat(unsigned int param, const float value) {
			if (sqlite3_bind_double(m_sqlite_stmt, param+1, value) != SQLITE_OK)
				SQLiteDatabase::throwAPIException(m_sqlite_db);
		}

		/**
		 * binds a double floating point number value to a query parameter
		 *
		 * @param param the query parameter number to which the value will be bound (starting with 0)
		 * @param value the value to bind to the query parameter
		 */
		virtual void bindDouble(unsigned int param, const double value) {
			if (sqlite3_bind_double(m_sqlite_stmt, param+1, value) != SQLITE_OK)
				SQLiteDatabase::throwAPIException(m_sqlite_db);
		}

		/**
		 * binds a long double floating point number value to a query parameter
		 *
		 * @param param the query parameter number to which the value will be bound (starting with 0)
		 * @param value the value to bind to the query parameter
		 */
		virtual void bindLongDouble(unsigned int param, const long double value) {
			if (sqlite3_bind_double(m_sqlite_stmt, param+1, value) != SQLITE_OK)
				SQLiteDatabase::throwAPIException(m_sqlite_db);
		}

		/**
		 * binds a date_time value to a query parameter (use bindString() instead)
		 *
		 * @param param the query parameter number to which the value will be bound (starting with 0)
		 * @param value the value to bind to the query parameter
		 */
		virtual void bindDateTime(unsigned int param, const PionDateTime& value) {
			// store it as an iso extended string
			char *as_string = getDateTimeString(boost::get<const PionDateTime&>(value));
			if (sqlite3_bind_text(m_sqlite_stmt, param+1, as_string,
								  strlen(as_string), SQLITE_TRANSIENT) != SQLITE_OK)
				SQLiteDatabase::throwAPIException(m_sqlite_db);
		}

		/**
		 * binds a date_time value to a query parameter (use bindString() instead)
		 *
		 * @param param the query parameter number to which the value will be bound (starting with 0)
		 * @param value the value to bind to the query parameter
		 */
		virtual void bindDate(unsigned int param, const PionDateTime& value) {
			// store it as an iso extended string
			char *as_string = getDateString(boost::get<const PionDateTime&>(value));
			if (sqlite3_bind_text(m_sqlite_stmt, param+1, as_string,
								  strlen(as_string), SQLITE_TRANSIENT) != SQLITE_OK)
				SQLiteDatabase::throwAPIException(m_sqlite_db);
		}

		/**
		 * binds a date_time value to a query parameter (use bindString() instead)
		 *
		 * @param param the query parameter number to which the value will be bound (starting with 0)
		 * @param value the value to bind to the query parameter
		 */
		virtual void bindTime(unsigned int param, const PionDateTime& value) {
			// store it as an iso extended string
			char *as_string = getTimeString(boost::get<const PionDateTime&>(value));
			if (sqlite3_bind_text(m_sqlite_stmt, param+1, as_string,
								  strlen(as_string), SQLITE_TRANSIENT) != SQLITE_OK)
				SQLiteDatabase::throwAPIException(m_sqlite_db);
		}

		/// Fetch a string from a column
		virtual void fetchString(unsigned int param, std::string& value) {
			const char *ptr = (const char *)sqlite3_column_text(m_sqlite_stmt, param);
			if (ptr)
				value = ptr;
			else
				value = "";
		}

		/// Fetch a Blob from a column
		virtual void fetchBlob(unsigned int param, std::string& value) {
			const char *ptr = (const char *)sqlite3_column_blob(m_sqlite_stmt, param);
			int size = sqlite3_column_bytes(m_sqlite_stmt, param);
			if (ptr && size > 0)
				value = std::string(ptr, size);
			else
				value = "";
		}

		/// Fetch an int from a column
		virtual boost::int32_t fetchInt(unsigned int param) {
			return sqlite3_column_int(m_sqlite_stmt, param);
		}

		/// Fetch an unsigned int from a column
		virtual boost::uint32_t fetchUInt(unsigned int param) {
			return sqlite3_column_int(m_sqlite_stmt, param);
		}

		/// Fetch a long int from a column
		virtual boost::int64_t fetchBigInt(unsigned int param) {
			return sqlite3_column_int64(m_sqlite_stmt, param);
		}

		/// Fetch an unsigned long int from a column
		virtual boost::uint64_t fetchUBigInt(unsigned int param) {
			return sqlite3_column_int64(m_sqlite_stmt, param);
		}

		/// Fetch a float from a column
		virtual float fetchFloat(unsigned int param) {
			return sqlite3_column_double(m_sqlite_stmt, param);
		}

		/// Fetch a double from a column
		virtual double fetchDouble(unsigned int param) {
			return sqlite3_column_double(m_sqlite_stmt, param);
		}

		/// Fetch a long double from a column
		virtual long double fetchLongDouble(unsigned int param) {
			return sqlite3_column_double(m_sqlite_stmt, param);
		}

		/// Fetch a date from a column
		virtual void fetchDateTime(unsigned int param, PionDateTime& val) {
			// %Y-%m-%dT%H:%M:%S%F
			const char *ptr = (const char *)sqlite3_column_text(m_sqlite_stmt, param);
			if (ptr) {
//				PionTimeFacet time_facet("%Y-%m-%dT%H:%M:%S%F");
				PionTimeFacet time_facet("%Y-%m-%d %H:%M:%S");
				val = time_facet.fromString(ptr);
			}
//			val = boost::posix_time::time_from_string(reinterpret_cast<const char *>(sqlite3_column_text(m_sqlite_stmt, param)));
//			val = f.fromString(reinterpret_cast<const char *>(sqlite3_column_text(m_sqlite_stmt, param)));
		}

		/// Fetch a time from a column
		virtual void fetchDate(unsigned int param, PionDateTime& val) {
			// %Y-%m-%dT%H:%M:%S%F
			const char *ptr = (const char *)sqlite3_column_text(m_sqlite_stmt, param);
			if (ptr) {
				PionTimeFacet time_facet("%Y-%m-%d");
				val = time_facet.fromString(ptr);
			}
		}

		/// Fetch a time from a column
		virtual void fetchTime(unsigned int param, PionDateTime& val) {
			// %Y-%m-%dT%H:%M:%S%F
			const char *ptr = (const char *)sqlite3_column_text(m_sqlite_stmt, param);
			if (ptr) {
				PionTimeFacet time_facet("%H:%M:%S");
				val = time_facet.fromString(ptr);
			}
		}

		/**
		 * runs the compiled query
		 *
		 * @return true if there is a result row available
		 */
		virtual bool run(void);

		/**
		 * runs a generic query
	 	*
	 	* @param ins a FieldMap of input fields
	 	* @param src input event, used for ins FieldMap
	 	* @param outs a FieldMap of output fields
	 	* @param dest output event, used for outs FieldMap
		* @param suppress regex containing pattern of suppressable errors
	 	*
	 	* @return bool if output event was modified
	 	*/
		virtual bool runFullQuery(const pion::platform::Query::FieldMap& ins, const pion::platform::EventPtr& src,
									const pion::platform::Query::FieldMap& outs, pion::platform::EventPtr& dest,
									unsigned int limit, const boost::regex& suppress);

		virtual bool runFullGetMore(const pion::platform::Query::FieldMap& outs, pion::platform::EventPtr& dest,
									unsigned int limit);

		/// resets the compiled query so that it can be run again
		virtual void reset(void) { sqlite3_reset(m_sqlite_stmt); }

		/// After a query, fetch a row into an event
		bool fetchRow(const FieldMap& field_map, pion::platform::EventPtr e);


	private:

		/// points to the SQLite database that this query belongs to
		sqlite3 *			m_sqlite_db;

		/// points to a SQLite prepared statement used for this Query
		sqlite3_stmt *		m_sqlite_stmt;
	};


private:

	/// extension added to the name of backup files
	static const std::string		BACKUP_FILE_EXTENSION;

	/// name of the Filename element for Pion XML config files
	static const std::string		FILENAME_ELEMENT_NAME;


	/// filename of the SQLite database
	std::string						m_database_name;

	/// SQLite v3 database handle
	sqlite3	*						m_sqlite_db;

	/// points the an error message returned from a SQLite API call
	char *							m_error_ptr;

	/// calculated page cache size
	boost::uint64_t					m_cache_size;

	/// local copy of partition#
	unsigned						m_partition;
};


// inline member functions for SQLiteDatabase

inline std::string SQLiteDatabase::getSQLiteError(void)
{
	std::string error_str;
	if (m_error_ptr != NULL) {
		error_str = m_error_ptr;
		sqlite3_free(m_error_ptr);
		m_error_ptr = NULL;
	}
	return error_str;
}


}	// end namespace plugins
}	// end namespace pion

#endif

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

#ifdef _MSC_VER
// This could be any valid .lib file; its only purpose is to prevent the compiler  
// from trying to link to boost_zlib-*.lib (e.g. boost_zip-vc80-mt-1_37.dll).  
// LogInputReactor only uses zlib indirectly, through boost_iostreams-*.dll.
#define BOOST_ZLIB_BINARY "zdll.lib"

// See above comment.
#define BOOST_BZIP2_BINARY "bzip2.lib"
#endif


#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <pion/PionScheduler.hpp>
#include <pion/platform/ConfigManager.hpp>
#include <pion/platform/CodecFactory.hpp>
#include <pion/platform/ReactionEngine.hpp>
#include "LogInputReactor.hpp"

using namespace pion::platform;
namespace bfs = boost::filesystem;


namespace pion {		// begin namespace pion
namespace plugins {		// begin namespace plugins


// static members of LogInputReactor
	
const std::uint32_t		LogInputReactor::DEFAULT_FREQUENCY = 1;
const std::string			LogInputReactor::CODEC_ELEMENT_NAME = "Codec";
const std::string			LogInputReactor::DIRECTORY_ELEMENT_NAME = "Directory";
const std::string			LogInputReactor::FILENAME_ELEMENT_NAME = "Filename";
const std::string			LogInputReactor::JUST_ONE_ELEMENT_NAME = "JustOne";
const std::string			LogInputReactor::TAIL_F_ELEMENT_NAME = "TailF";
const std::string			LogInputReactor::FREQUENCY_ELEMENT_NAME = "Frequency";
const std::string			LogInputReactor::CURRENT_LOG_ELEMENT_NAME = "CurrentLog";
const std::string			LogInputReactor::CONSUMED_LOG_ELEMENT_NAME = "ConsumedLog";


// LogInputReactor member functions

void LogInputReactor::setConfig(const Vocabulary& v, const xmlNodePtr config_ptr)
{
	// first set config options for the Reactor base class
	ConfigWriteLock cfg_lock(*this);
	Reactor::setConfig(v, config_ptr);
	
	// get the Codec that the Reactor should use
	if (! ConfigManager::getConfigOption(CODEC_ELEMENT_NAME, m_codec_id, config_ptr))
		throw EmptyCodecException(getId());
	
	// get the directory where the Reactor will look for new log files
	if (! ConfigManager::getConfigOption(DIRECTORY_ELEMENT_NAME, m_log_directory, config_ptr))
		throw EmptyDirectoryException(getId());
	
	// resolve paths relative to the ReactionEngine's config file location
	m_log_directory = getConfigManager().resolveRelativeDataPath(m_log_directory);

	// make sure that the directory exists
	if (! boost::filesystem::exists(m_log_directory) )
		throw DirectoryNotFoundException(m_log_directory);
	if (! boost::filesystem::is_directory(m_log_directory) )
		throw NotADirectoryException(m_log_directory);
	
	// get the filename regex to use for finding log files
	std::string filename_str;
	if (! ConfigManager::getConfigOption(FILENAME_ELEMENT_NAME, filename_str, config_ptr))
		throw EmptyFilenameException(getId());
	m_log_regex = filename_str;
	
	// check if the the Reactor should only read the first Event & duplicate it (for testing)
	m_just_one = false;
	std::string just_one_option;
	if (ConfigManager::getConfigOption(JUST_ONE_ELEMENT_NAME, just_one_option,
									   config_ptr))
	{
		if (just_one_option == "true")
			m_just_one = true;
	}

	// check if "tail -f" option was requested
	m_tail_f = false;
	std::string tail_f_option;
	if (ConfigManager::getConfigOption(TAIL_F_ELEMENT_NAME, tail_f_option,
									   config_ptr))
	{
		if (tail_f_option == "true")
			m_tail_f = true;
	}

	// get the frequency to check for new logs (if defined)
	std::string frequency_str;
	if (ConfigManager::getConfigOption(FREQUENCY_ELEMENT_NAME, frequency_str, config_ptr)) {
		const std::uint32_t frequency_value = boost::lexical_cast<std::uint32_t>(frequency_str);
		if (frequency_value <= 0)
			throw BadFrequencyException(getId());
		m_frequency = frequency_value;
	} else {
		m_frequency = DEFAULT_FREQUENCY;
	}

	// assign names for the cache files
	m_history_cache_filename = getId() + ".cache";
	m_history_cache_filename = getConfigManager().resolveRelativePath(m_history_cache_filename);
	m_current_log_file_cache_filename = getId() + "-cur.cache";
	m_current_log_file_cache_filename = getConfigManager().resolveRelativePath(m_current_log_file_cache_filename);
}
	
void LogInputReactor::query(std::ostream& out, const QueryBranches& branches,
	const QueryParams& qp)
{
	writeBeginReactorXML(out);
	writeStatsOnlyXML(out);
	
	std::unique_lock<std::mutex> logs_consumed_lock(m_logs_consumed_mutex);

	out << '<' << CURRENT_LOG_ELEMENT_NAME << '>' << m_log_file
	    << "</" << CURRENT_LOG_ELEMENT_NAME << '>' << std::endl;
	    
	for (LogFileCollection::const_iterator i = m_logs_consumed.begin();
		i != m_logs_consumed.end(); ++i)
	{
		out << '<' << CONSUMED_LOG_ELEMENT_NAME << '>' << *i
			<< "</" << CONSUMED_LOG_ELEMENT_NAME << '>' << std::endl;
	}

	logs_consumed_lock.unlock();
	
	writeEndReactorXML(out);
}

void LogInputReactor::start(void)
{
	std::lock_guard<std::mutex> worker_lock(m_worker_mutex);
	if (! m_is_running) {
		std::lock_guard<std::mutex> logs_consumed_lock(m_logs_consumed_mutex);

		// Process history cache (list of log files that have already been consumed) if present.
		if (bfs::exists(m_history_cache_filename)) {
			std::ifstream history_cache(m_history_cache_filename.c_str());
			if (! history_cache)
				throw PionException("Unable to open history cache file for reading.");
			m_logs_consumed.clear();
			std::string already_consumed_file;
			while (history_cache >> already_consumed_file) {
				m_logs_consumed.insert(already_consumed_file);
			}
		}

		// Read the log file cache file to get the number of events previously read for all log files
		// that were open for reading the last time this Reactor was stopped.
		if (bfs::exists(m_current_log_file_cache_filename)) {
			std::ifstream current_log_file_cache(m_current_log_file_cache_filename.c_str());
			if (! current_log_file_cache)
				throw PionException("Unable to open current log file cache file for reading.");
			getline(current_log_file_cache, m_log_file, '|');
			while (! current_log_file_cache.eof()) {
				current_log_file_cache >> m_num_events_read_previously[m_log_file];
				getline(current_log_file_cache, m_log_file, '|');
			}
			current_log_file_cache.close();
			bfs::remove(m_current_log_file_cache_filename);
		}

		m_is_running = true;
		m_worker_is_active = true;
		getScheduler().getIOService().post(std::bind(&LogInputReactor::checkForLogFiles, this));
	}
}
	
void LogInputReactor::stop(void)
{
	std::unique_lock<std::mutex> worker_lock(m_worker_mutex);
	if (m_is_running) {
		// set flag to notify reader thread to shutdown
		PION_LOG_DEBUG(m_logger, "Stopping worker thread: " << getId());
		m_is_running = false;
		m_timer_ptr.reset();

		// wait until the worker thread has finished
		while (m_worker_is_active) {
			m_worker_stopped.wait(worker_lock);
		}
		PION_LOG_DEBUG(m_logger, "Worker thread has finished: " << getId());

		// Write filename and number of events read for all open streams to a cache file.
		if (m_open_streams.empty()) {
			boost::filesystem::remove(m_current_log_file_cache_filename);
		} else {
			PION_LOG_DEBUG(m_logger, "Updating log cache file: " << getId());
			std::ofstream current_log_file_cache(m_current_log_file_cache_filename.c_str());
			if (! current_log_file_cache)
				throw PionException("Unable to open current log file cache file for writing.");
			for (StreamMap::iterator it = m_open_streams.begin(); it != m_open_streams.end(); ++it) {
				current_log_file_cache << it->first << "|" << *it->second.second << std::endl;
			}
		}

		// Close all the open streams.
		for (StreamMap::iterator it = m_open_streams.begin(); it != m_open_streams.end(); ++it) {
			std::shared_ptr<boost::iostreams::filtering_istream> log_stream = it->second.first;
			// Remove and close all Filters and Devices.
			while (! log_stream->empty()) log_stream->pop();
		}
		m_open_streams.clear();
	}
}

void LogInputReactor::scheduleLogFileCheck(std::uint32_t seconds)
{
	std::unique_lock<std::mutex> worker_lock(m_worker_mutex);
	if (! m_is_running) {
	    // stop() was called after last isRunning() check in checkForLogFiles()
	    worker_lock.unlock();
		finishWorkerThread();
	} else if (seconds == 0) {
		getScheduler().getIOService().post(std::bind(&LogInputReactor::checkForLogFiles, this));
	} else {
		if (! m_timer_ptr)
			m_timer_ptr.reset(new boost::asio::deadline_timer(getScheduler().getIOService()));
		m_timer_ptr->expires_from_now(boost::posix_time::seconds(seconds));
		m_timer_ptr->async_wait(std::bind(&LogInputReactor::checkForLogFiles, this));
	}
}

void LogInputReactor::checkForLogFiles(void)
{
	// make sure that the reactor is still running
	if (! isRunning() ) {
		finishWorkerThread();
		return;
	}

	PION_LOG_DEBUG(m_logger, "Checking for new log files in " << m_log_directory);

	ConfigReadLock cfg_lock(*this);

	try {
		// get the current logs located in the log directory
		LogFileCollection current_logs;
		getLogFilesInLogDirectory(current_logs);
	
		std::lock_guard<std::mutex> logs_consumed_lock(m_logs_consumed_mutex);

		// remove logs from the consumed collection that are no longer there
		LogFileCollection::iterator temp_itr;
		LogFileCollection::iterator log_itr = m_logs_consumed.begin();
		while (log_itr != m_logs_consumed.end()) {
			temp_itr = log_itr++;
			if (current_logs.find(*temp_itr) == current_logs.end())
				m_logs_consumed.erase(temp_itr);
		}

		// TODO: Is this useful?  I wrote it, then realized I couldn't test it
		// because I couldn't delete log files while they were open.
		/*
		// remove logs from the open log stream collection that are no longer there
		StreamMap::iterator temp_itr_2;
		StreamMap::iterator log_itr_2 = m_open_streams.begin();
		while (log_itr_2 != m_open_streams.end()) {
			temp_itr_2 = log_itr_2++;
			if (! bfs::exists(temp_itr_2->first))
				m_open_streams.erase(temp_itr_2);
		}
		*/

		// Update the history cache, which should always contain the same list as m_logs_consumed.
		if (m_logs_consumed.empty()) {
			bfs::remove(m_history_cache_filename);
		} else {
			std::ofstream history_cache(m_history_cache_filename.c_str());
			if (! history_cache)
				throw PionException("Unable to open history cache file for writing.");
			for (log_itr = m_logs_consumed.begin(); log_itr != m_logs_consumed.end(); ++log_itr) {
				history_cache << *log_itr << std::endl;
			}
		}

		// check for an existing log that has not yet been consumed
		for (log_itr = current_logs.begin(); log_itr != current_logs.end(); ++log_itr) {
			if (m_logs_consumed.find(*log_itr) == m_logs_consumed.end())
				break;
		}

		if (log_itr == current_logs.end()) {
			// no new logs to consume

			if (m_tail_f) {
				// Check the open streams to see if any have new records.
				StreamMap::iterator it;
				for (it = m_open_streams.begin(); it != m_open_streams.end(); ++it) {
					it->second.first->clear();
					if (it->second.first->peek() != EOF)
						break;
				}
				if (it == m_open_streams.end()) {
					// No open streams with new records found, so sleep until it is time to check again.
					PION_LOG_DEBUG(m_logger, "No new or incremented logs (sleeping for " << m_frequency
								   << " seconds): " << m_log_directory);
					scheduleLogFileCheck(m_frequency);
				} else {
					m_log_file = it->first;
					PION_LOG_DEBUG(m_logger, "Found an open log file with new records to consume: " << m_log_file);
					m_current_stream_data = it->second;
					scheduleReadFromLog();
				}
			} else {
				// sleep until it is time to check again
				PION_LOG_DEBUG(m_logger, "No new logs (sleeping for " << m_frequency
							   << " seconds): " << m_log_directory);
				scheduleLogFileCheck(m_frequency);
			}
		} else {
			// found a new log to consume
			
			// re-calculate the full path to the file
			bfs::path full_path(m_log_directory);
			full_path /= *log_itr;
# if defined(BOOST_FILESYSTEM_VERSION) && BOOST_FILESYSTEM_VERSION >= 3
            m_log_file = full_path.string();
#else
            m_log_file = full_path.file_string();
#endif 
			PION_LOG_DEBUG(m_logger, "Found a new log file to consume: " << m_log_file);
			m_current_stream_data = StreamData(
				std::shared_ptr<boost::iostreams::filtering_istream>(new boost::iostreams::filtering_istream), 
				std::shared_ptr<std::uint64_t>(new std::uint64_t(0)));
			m_open_streams[m_log_file] = m_current_stream_data;
			scheduleReadFromLog();
		}
	} catch (std::exception& e) {
		PION_LOG_ERROR(m_logger, e.what());
		finishWorkerThread();
		m_is_running = false;
	}
}

void LogInputReactor::readFromLog(void)
{
	// make sure that the reactor is still running
	if (! m_is_running) {
		finishWorkerThread();
		return;
	}

	std::shared_ptr<boost::iostreams::filtering_istream> log_stream = m_current_stream_data.first;
	try {
		// open up the log file for reading (if not open already)
		std::uint64_t num_events_to_skip = 0;
		bool compressed = false;
		bool already_open = log_stream->is_complete();  // is_complete() returns true if a Device, in this case a file, is attached to the stream.
		if (! already_open) {

			// Insert a decompression filter if the file suffix indicates one is needed.
			if (boost::algorithm::iequals(bfs::extension(bfs::path(m_log_file)), ".gz")) {
				log_stream->push(boost::iostreams::gzip_decompressor());
				compressed = true;
			}
			if (boost::algorithm::iequals(bfs::extension(bfs::path(m_log_file)), ".bz2")) {
				log_stream->push(boost::iostreams::bzip2_decompressor());
				compressed = true;
			}

			// Open and attach a file.
			log_stream->push(boost::iostreams::file_source(m_log_file.c_str(), std::ios::in | std::ios::binary));
			if (! log_stream->is_complete())
				throw OpenLogException(m_log_file);

			if (log_stream->peek() == EOF && ! m_tail_f) {
				std::string log_file_tmp = m_log_file;
				recordLogFileAsDone();

				// TODO: Should this really throw an exception, or would a warning be good enough?
				throw EmptyLogException(log_file_tmp);
			}

			// If there were any previously read Events, we need to skip over them, because the log file has been reopened since they were read.
			std::map<std::string, std::uint64_t>::iterator it = m_num_events_read_previously.find(m_log_file);
			if (it != m_num_events_read_previously.end()) {
				num_events_to_skip = it->second;
				PION_LOG_DEBUG(m_logger, "Resuming log file parsing by skipping " << num_events_to_skip << " events");
				m_num_events_read_previously.erase(it);
			}

		}

		// Get a new Codec for reading the current log file.
		CodecPtr codec_ptr;
		{
			ConfigReadLock cfg_lock(*this);
			getCodecPlugin(codec_ptr, m_codec_id);
			PION_ASSERT(codec_ptr);
		}

		const Event::EventType event_type(codec_ptr->getEventType());
		EventFactory event_factory;
		EventPtr event_ptr;
		do {
			// get a new event from the EventFactory
			event_factory.create(event_ptr, event_type);

			// read an Event from the log file (convert into an Event using the Codec)
			if (! isRunning()) break;
			bool event_read = codec_ptr->read(*log_stream, *event_ptr);
			if (! event_read && ! log_stream->eof())
				throw ReadEventException(m_log_file);
			if (event_read) {
				// Increment the count of events for this file.
				++(*m_current_stream_data.second);

				// Ignore Events that were previously read.
				if (num_events_to_skip) {
					num_events_to_skip--;
					continue;
				}
			}

			// check if only the first Event should be read
			if (m_just_one) {
				PION_LOG_DEBUG(m_logger, "JustOne: generating lots of event copies for testing");

				// Remove and close all Filters and Devices.
				while (! log_stream->empty()) log_stream->pop();

				// just duplicate the event repeatedly until the Reactor is stopped
				EventPtr original_event_ptr(event_ptr);
				while (isRunning()) {
					// duplicate the original event
					event_factory.create(event_ptr, event_type);
					*event_ptr += *original_event_ptr;
					// deliver the Event to connected Reactors
					deliverEventFromLog(event_ptr);
				}
				break;
			}

			// check for end of file
			if (log_stream->eof()) {
				if (m_tail_f && ! compressed) {
					log_stream->clear();
					PION_LOG_DEBUG(m_logger, "Finished consuming currently available records in log file: " << m_log_file);
				} else {
					// all done with this log file
					PION_LOG_DEBUG(m_logger, "Finished consuming log file: " << m_log_file);

					// Remove and close all Filters and Devices.
					while (! log_stream->empty()) log_stream->pop();

					StreamMap::iterator it = m_open_streams.find(m_log_file);
					if (it != m_open_streams.end())	// sanity check
						m_open_streams.erase(it);

					recordLogFileAsDone();
				}

				// deliver the Event to connected Reactors
				if (event_read)
					deliverEventFromLog(event_ptr);

				break;
			}

			// deliver the Event to connected Reactors
			if (event_read)
				deliverEventFromLog(event_ptr);

		} while (isRunning()); 

	} catch (std::exception& e) {
		PION_LOG_ERROR(m_logger, e.what());

		// Remove and close all Filters and Devices.
		while (! log_stream->empty()) log_stream->pop();

		recordLogFileAsDone();
	}	

	// check for more logs
	// note: if not running, the new thread should just return (almost) immediately
	scheduleLogFileCheck(0);
}

void LogInputReactor::getLogFilesInLogDirectory(LogFileCollection& files)
{
	bfs::path dir_path(m_log_directory);
	for (bfs::directory_iterator itr(dir_path); itr!=bfs::directory_iterator(); ++itr) {
		if (bfs::is_regular(itr->status())) {
# if defined(BOOST_FILESYSTEM_VERSION) && BOOST_FILESYSTEM_VERSION >= 3
            const std::string filename(itr->path().filename().string());
#else
            const std::string filename(itr->path().leaf());
#endif 
			if (boost::regex_search(filename, m_log_regex)) {
# if defined(BOOST_FILESYSTEM_VERSION) && BOOST_FILESYSTEM_VERSION >= 3
                if (! m_tail_f || (m_open_streams.find(itr->path().string()) == m_open_streams.end()))
#else
                if (! m_tail_f || (m_open_streams.find(itr->path().file_string()) == m_open_streams.end()))
#endif 
					files.insert(filename);
			}
		}
	}
}

void LogInputReactor::recordLogFileAsDone() {
	// No need to record anything if "JustOne" is enabled.
	if (m_just_one) return;

	// Add the current log file to the list of consumed files and the history cache.
	bfs::path log_file_path(m_log_file);
	std::lock_guard<std::mutex> logs_consumed_lock(m_logs_consumed_mutex);
	m_log_file.clear();
# if defined(BOOST_FILESYSTEM_VERSION) && BOOST_FILESYSTEM_VERSION >= 3
    m_logs_consumed.insert(log_file_path.filename().string());
#else
    m_logs_consumed.insert(log_file_path.leaf());
#endif 
	std::ofstream history_cache(m_history_cache_filename.c_str(), std::ios::out | std::ios::app);
	if (! history_cache)
		throw PionException("Unable to open history cache file for writing.");
	{
		// signal finished processing log
# if defined(BOOST_FILESYSTEM_VERSION) && BOOST_FILESYSTEM_VERSION >= 3
        std::string log_leaf = log_file_path.filename().string();
#else
        std::string log_leaf = log_file_path.leaf();
#endif 
		signal("FinishedLog", (void*) &log_leaf);
	}
# if defined(BOOST_FILESYSTEM_VERSION) && BOOST_FILESYSTEM_VERSION >= 3
    history_cache << log_file_path.filename().string() << std::endl;
#else
    history_cache << log_file_path.leaf() << std::endl;
#endif 
	
}
	
}	// end namespace plugins
}	// end namespace pion


/// creates new LogInputReactor objects
extern "C" PION_PLUGIN_API pion::platform::Reactor *pion_create_LogInputReactor(void) {
	return new pion::plugins::LogInputReactor();
}

/// destroys LogInputReactor objects
extern "C" PION_PLUGIN_API void pion_destroy_LogInputReactor(pion::plugins::LogInputReactor *reactor_ptr) {
	delete reactor_ptr;
}


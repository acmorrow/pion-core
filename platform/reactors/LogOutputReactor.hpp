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

#ifndef __PION_LOGOUTPUTREACTOR_HEADER__
#define __PION_LOGOUTPUTREACTOR_HEADER__

#include <string>
#include <fstream>
#include <pion/PionConfig.hpp>
#include <pion/PionLogger.hpp>
#include <pion/PionException.hpp>
#include <pion/platform/Codec.hpp>
#include <pion/platform/Reactor.hpp>


namespace pion {		// begin namespace pion
namespace plugins {		// begin namespace plugins


///
/// LogOutputReactor: creates log files that are populated with events
///
class LogOutputReactor :
	public pion::platform::Reactor
{
public:

	/// exception thrown if the Reactor configuration does not define a Codec
	class EmptyCodecException : public PionException {
	public:
		EmptyCodecException(const std::string& reactor_id)
			: PionException("LogOutputReactor configuration is missing a required Codec parameter: ", reactor_id) {}
	};
	
	/// exception thrown if the Reactor configuration does not define a Filename
	class EmptyFilenameException : public PionException {
	public:
		EmptyFilenameException(const std::string& reactor_id)
			: PionException("LogOutputReactor configuration is missing a required Filename parameter: ", reactor_id) {}
	};
	
	/// exception thrown if the Reactor is unable to open a log file for writing
	class OpenLogException : public PionException {
	public:
		OpenLogException(const std::string& log_filename)
			: PionException("Unable to open log file for writing: ", log_filename) {}
	};
	
	/// exception thrown if the Reactor is unable to write an Event to the log file
	class WriteToLogException : public PionException {
	public:
		WriteToLogException(const std::string& log_filename)
			: PionException("Unable to write Event to log file: ", log_filename) {}
	};

	/// exception thrown if the Reactor is unable to rotate the log file
	class LogRotationException : public PionException {
	public:
		LogRotationException(const std::string& log_filename)
			: PionException("Unable to rotate log file: ", log_filename) {}
	};


	/// constructs a new LogOutputReactor object
	LogOutputReactor(void)
		: Reactor(TYPE_STORAGE)
	{
		setLogger(PION_GET_LOGGER("pion.LogOutputReactor"));
	}
	
	/// virtual destructor: this class is meant to be extended
	virtual ~LogOutputReactor() { stop(); }
	
	/**
	 * sets configuration parameters for this Reactor
	 *
	 * @param v the Vocabulary that this Reactor will use to describe Terms
	 * @param config_ptr pointer to a list of XML nodes containing Reactor
	 *                   configuration parameters
	 */
	virtual void setConfig(const pion::platform::Vocabulary& v, const xmlNodePtr config_ptr);
	
	/**
	 * this updates the Vocabulary information used by this Reactor; it should
	 * be called whenever the global Vocabulary is updated
	 *
	 * @param v the Vocabulary that this Reactor will use to describe Terms
	 */
	virtual void updateVocabulary(const pion::platform::Vocabulary& v);
	
	/**
	 * this updates the Codecs that are used by this Reactor; it should
	 * be called whenever any Codec's configuration is updated
	 */
	virtual void updateCodecs(void);
	
	/**
	 * processes an Event by writing it to a log file
	 *
	 * @param e pointer to the Event to process
	 */
	virtual void process(const pion::platform::EventPtr& e);
	
	/**
	 * handle an HTTP query (from QueryService)
	 *
	 * @param out the ostream to write the statistics info into
	 * @param branches URI stem path branches for the HTTP request
	 * @param qp query parameters or pairs passed in the HTTP request
	 */
	virtual void query(std::ostream& out, const QueryBranches& branches,
		const QueryParams& qp);
	
	/// called by the ReactorEngine to start Event processing
	virtual void start(void);
	
	/// called by the ReactorEngine to stop Event processing
	virtual void stop(void);

private:

	/// Opens the log file for writing.  Caller is responsible for locking.
	void openLogFileNoLock(void);

	/// Finishes and closes the log file.  Caller is responsible for locking.
	void closeLogFileNoLock(void);

	/// name of the Codec element for Pion XML config files
	static const std::string			CODEC_ELEMENT_NAME;
	
	/// name of the Filename element for Pion XML config files
	static const std::string			FILENAME_ELEMENT_NAME;


	/// unique identifier of the Codec that is used for writing Events
	std::string							m_codec_id;
	
	/// pointer to the Codec that is used for writing Events
	pion::platform::CodecPtr			m_codec_ptr;
	
	/// name of the log file to write Events into
	std::string							m_log_filename;
	
	/// output stream for the log that the Reactor is currently writing
	std::ofstream						m_log_stream;

	/// used to ensure only log writer at a time
	std::mutex						m_log_writer_mutex;
};


}	// end namespace plugins
}	// end namespace pion

#endif

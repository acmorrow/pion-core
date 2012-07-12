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

#ifndef __PION_SCRIPTREACTOR_HEADER__
#define __PION_SCRIPTREACTOR_HEADER__

#ifdef _MSC_VER
	#include <windows.h>
	#include <string.h>
#else
	#include <errno.h>
	#include <unistd.h>
	#include <sys/wait.h>
#endif
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <pion/PionConfig.hpp>
#include <pion/PionLogger.hpp>
#include <pion/PionException.hpp>
#include <pion/PionScheduler.hpp>
#include <pion/platform/Codec.hpp>
#include <pion/platform/Reactor.hpp>

#ifdef _MSC_VER
	#define FILE_DESC_TYPE			HANDLE
	#define INVALID_DESCRIPTOR		INVALID_HANDLE_VALUE
	#define CLOSE_DESCRIPTOR(x)		CloseHandle(x)
	#define PROCESS_INFO_TYPE		LPPROCESS_INFORMATION
	#define INVALID_PROCESS			NULL
#else
	#define FILE_DESC_TYPE			int
	#define INVALID_DESCRIPTOR		-1
	#define CLOSE_DESCRIPTOR(x)		::close(x)
	#define PROCESS_INFO_TYPE		pid_t
	#define INVALID_PROCESS			-1
#endif


namespace pion {		// begin namespace pion
namespace plugins {		// begin namespace plugins


///
/// ScriptReactor: interfaces with an external shell script or program
///
class ScriptReactor :
	public pion::platform::Reactor
{
public:	

	/// exception thrown if the Reactor configuration does not define an Input Codec
	class EmptyInputCodecException : public PionException {
	public:
		EmptyInputCodecException(const std::string& reactor_id)
			: PionException("ScriptReactor configuration is missing a required InputCodec parameter: ", reactor_id) {}
	};

	/// exception thrown if the Reactor configuration does not define an Output Codec
	class EmptyOutputCodecException : public PionException {
	public:
		EmptyOutputCodecException(const std::string& reactor_id)
			: PionException("ScriptReactor configuration is missing a required OutputCodec parameter: ", reactor_id) {}
	};

	/// exception thrown if the Reactor configuration does not define a Command
	class EmptyCommandException : public PionException {
	public:
		EmptyCommandException(const std::string& reactor_id)
			: PionException("ScriptReactor configuration is missing a required Command parameter: ", reactor_id) {}
	};

	/// exception thrown if there is a problem parsing arguments out of the command string
	class CommandParsingException : public PionException {
	public:
		CommandParsingException(const std::string& command)
			: PionException("ScriptReactor was unable to parse arguments out of command string: ", command) {}
	};

	/// exception thrown if opening a pipe to the shell command fails
	class OpenPipeException : public PionException {
	public:
		OpenPipeException(const std::string& command)
			: PionException("ScriptReactor failed to open pipe: ", command) {}
	};

	/// exception thrown if the Reactor has trouble reading events from the pipe
	class ReadFromPipeException : public PionException {
	public:
		ReadFromPipeException(const std::string& reactor_id)
			: PionException("ScriptReactor failed reading an event from pipe: ", reactor_id) {}
	};

	/// exception thrown if the Reactor has trouble writing events to the pipe
	class WriteToPipeException : public PionException {
	public:
		WriteToPipeException(const std::string& reactor_id)
			: PionException("ScriptReactor failed writing an event to pipe: ", reactor_id) {}
	};


	/// constructs a new ScriptReactor object
	ScriptReactor(void)
		: Reactor(TYPE_PROCESSING),
		m_input_pipe(INVALID_DESCRIPTOR), m_output_pipe(INVALID_DESCRIPTOR), m_child(INVALID_PROCESS)
	{
		setLogger(PION_GET_LOGGER("pion.ScriptReactor"));
	}

	/// virtual destructor: this class is meant to be extended
	virtual ~ScriptReactor() { stop(); }

	/// called by the ReactorEngine to start Event processing
	virtual void start(void);

	/// called by the ReactorEngine to stop Event processing
	virtual void stop(void) { stopIfRunning(); }

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
	 * processes an Event by delivering it to the shell script or program
	 *
	 * @param e pointer to the Event to process
	 */
	virtual void process(const pion::platform::EventPtr& e);


private:

	/// stops the reactor and returns true if it was running
	bool stopIfRunning(void);

	/// thread function to read events from the script command
	void readEvents(void);

	/// opens a pipe to the script command
	void openPipe(void);

	/// closes the pipe to the script command	
	void closePipe(void);

	/// parses out individual arguments from a command string
	void parseArguments(void);


#ifdef BOOST_IOSTREAMS_WINDOWS
	struct winpipe_handle_source : private boost::iostreams::file_descriptor {
		typedef char char_type;
		struct category : boost::iostreams::source_tag, boost::iostreams::closable_tag { };
		std::streamsize read(char_type* s, std::streamsize n) {
			DWORD result = 0, error = 0;
			if (!::ReadFile(handle(), s, n, &result, NULL) && (error = GetLastError()) != ERROR_BROKEN_PIPE)
				throw boost::iostreams::detail::bad_read();
			return error == ERROR_BROKEN_PIPE ? -1 : static_cast<std::streamsize>(result);
		}
		using boost::iostreams::file_descriptor::close;
		using boost::iostreams::file_descriptor::handle;
		template <typename FLAG_TYPE>
		winpipe_handle_source(HANDLE h, FLAG_TYPE f) : boost::iostreams::file_descriptor(h, f) { }
		winpipe_handle_source(const winpipe_handle_source& w) :
			boost::iostreams::file_descriptor(static_cast<const boost::iostreams::file_descriptor&>(w)) { }
	};

	struct winpipe_handle_sink : private boost::iostreams::file_descriptor {
		typedef char char_type;
		struct category : boost::iostreams::sink_tag, boost::iostreams::closable_tag { };
		std::streamsize write(const char_type* s, std::streamsize n) {
			DWORD ignore = 0;
			if (!::WriteFile(handle(), s, n, &ignore, NULL))
				throw boost::iostreams::detail::bad_write();
			return n;
		}
		using boost::iostreams::file_descriptor::close;
		using boost::iostreams::file_descriptor::handle;
		template <typename FLAG_TYPE>
		winpipe_handle_sink(HANDLE h, FLAG_TYPE f) : boost::iostreams::file_descriptor(h, f) { }
		winpipe_handle_sink(const winpipe_handle_sink& w) :
			boost::iostreams::file_descriptor(static_cast<const boost::iostreams::file_descriptor&>(w)) { }
	};

	/// data types for iostreams streambufs that use Windows pipe file-handles
	typedef boost::iostreams::stream_buffer<winpipe_handle_source>	IStreamBuffer;
	typedef boost::iostreams::stream_buffer<winpipe_handle_sink>	OStreamBuffer;
#else
	/// data types for iostreams streambufs that use C-style file descriptors
	typedef boost::iostreams::stream_buffer<boost::iostreams::file_descriptor_source>	IStreamBuffer;
	typedef boost::iostreams::stream_buffer<boost::iostreams::file_descriptor_sink>		OStreamBuffer;
#endif


	/// name of the InputCodec element for Pion XML config files
	static const std::string			INPUT_CODEC_ELEMENT_NAME;

	/// name of the OutputCodec element for Pion XML config files
	static const std::string			OUTPUT_CODEC_ELEMENT_NAME;

	/// name of the Command element for Pion XML config files
	static const std::string			COMMAND_ELEMENT_NAME;


	/// pointer to the Codec that is used for writing Events to the script
	pion::platform::CodecPtr			m_input_codec_ptr;

	/// pointer to the Codec that is used for reading Events from the script
	pion::platform::CodecPtr			m_output_codec_ptr;

	/// unique identifier of the Codec that is used for writing Events
	std::string							m_input_codec_id;

	/// unique identifier of the Codec that is used for reading Events
	std::string							m_output_codec_id;

	/// name of the shell script or program to execute (includes all arguments)
	std::string							m_command;

	/// vector of command arguments parsed out (the first is the executable or script)
	std::vector<std::string>			m_args;

	/// pipe used to write Events to the shell script or program
	FILE_DESC_TYPE						m_input_pipe;

	/// pipe used to read Events from the shell script or program
	FILE_DESC_TYPE						m_output_pipe;

	/// process id for the shell script or program
	PROCESS_INFO_TYPE					m_child;

	/// pointer to a C++ stream buffer used to write Events to the pipe
	std::unique_ptr<OStreamBuffer>	m_input_streambuf_ptr;

	/// pointer to a C++ stream buffer used to read Events from the pipe
	std::unique_ptr<IStreamBuffer>	m_output_streambuf_ptr;

	/// pointer to a C++ iostream used to write Events to the pipe
	std::unique_ptr<std::ostream>		m_input_stream_ptr;

	/// pointer to a C++ iostream used to read Events from the pipe
	std::unique_ptr<std::istream>		m_output_stream_ptr;

	/// thread used to read events generated by the script
	std::unique_ptr<std::thread>	m_thread_ptr;

	/// used to ensure only event may be written at a time
	std::mutex						m_write_mutex;
};


}	// end namespace plugins
}	// end namespace pion

#endif

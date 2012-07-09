// ------------------------------------------------------------------
// pion-platform: a C++ framework for building lightweight HTTP interfaces
// ------------------------------------------------------------------
// Copyright (C) 2007 Atomic Labs, Inc.  (http://www.atomiclabs.com)
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file COPYING or copy at http://www.boost.org/LICENSE_1_0.txt
//

#ifndef _MSC_VER
	#include <fcntl.h>
	#include <unistd.h>
	#include <sys/stat.h>
#endif

#ifdef _MSC_VER
	#include <windows.h>
	#include <direct.h>
#endif

// mlockall support on LINUX
#ifndef _MSC_VER
	#include <sys/mman.h>
	#include <string.h>
#endif

#include <pion/PionConfig.hpp>
#include <pion/PionProcess.hpp>

#ifdef PION_HAVE_SSL
	#include <openssl/ssl.h>
//#ifdef _MSC_VER
//	#include <openssl/applink.c>
//#endif
#endif

#ifndef SYSCONFDIR
	#define SYSCONFDIR "/usr/local/etc"
#endif

#include <iostream>
#include <boost/filesystem/operations.hpp>
#include "PlatformConfig.hpp"

#ifdef PION_STATIC_LINKING
	#include <pion/PionPlugin.hpp>
	PION_DECLARE_PLUGIN(FileService)
	PION_DECLARE_PLUGIN(HTTPProtocol)
	PION_DECLARE_PLUGIN(SnifferReactor)
#endif

using namespace std;
using namespace pion;
using namespace pion::net;
using namespace pion::platform;
using namespace pion::server;

// some forward declarations of functions used
void daemonize_server(void);
void argument_error(void);
int run(bool run_as_daemon, bool lock_memory, const std::string& platform_config_file);

int parse_args(int argc, char *argv[], bool& run_as_daemon, bool& lock_memory, std::string& platform_config_file)
{
	// get platform config file
	run_as_daemon = false;
	lock_memory = false;

#ifdef _MSC_VER
	platform_config_file = "config\\platform.xml";
#else
	platform_config_file = "/etc/pion/platform.xml";
	if (! boost::filesystem::exists(platform_config_file) )
		platform_config_file = std::string(SYSCONFDIR) + "/pion/platform.xml";
#endif

	for (int argnum=1; argnum < argc; ++argnum) {
		if (argv[argnum][0] == '-') {
			if (argv[argnum][1] == 'D') {
				run_as_daemon = true;
			} else if (argv[argnum][1] == 'M') {
				lock_memory = true;
			} else if (argv[argnum][1] == 'c' && argv[argnum][2] == '\0' && argnum+1 < argc) {
				platform_config_file = boost::filesystem::system_complete(argv[++argnum]).normalize().string();
			} else if (strncmp(argv[argnum], "--version", 9) == 0 || strncmp(argv[argnum], "-version", 8) == 0) {
				std::cout << "pion version " << PION_VERSION << std::endl;
				return 1;
			} else {
				argument_error();
				return 1;
			}
		} else {
			argument_error();
			return 1;
		}
	}

	return 0;
}

/// Windows Service stuff
#ifdef _MSC_VER
#define SVCNAME TEXT("Pion")

/// store command line parameters to pass between main() and SvcMain
bool					g_lock_memory = false;
std::string				g_platform_config_file;

/// Windows Service status and handle
SERVICE_STATUS          gSvcStatus; 
SERVICE_STATUS_HANDLE   gSvcStatusHandle;

/// reports serive status to the Service Control Manager (uses gSvcStatusHandle)
void report_service_status( DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint );

/// serivce control handler - we only support Stop request
void WINAPI service_control_handler( DWORD dwCtrl )
{
	// Handle the requested control code. 
	switch(dwCtrl) {  
	case SERVICE_CONTROL_STOP: 
	case SERVICE_CONTROL_SHUTDOWN:
		report_service_status(SERVICE_STOP_PENDING, NO_ERROR, 0);
		// Signal the Pion to shutdown.
		PionProcess::shutdown();
		return;

	case SERVICE_CONTROL_INTERROGATE: 
		// Fall through to send current status.
		break; 

	default: 
		break;
	} 

   report_service_status(gSvcStatus.dwCurrentState, NO_ERROR, 0);
}

void report_service_status( DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint )
{
    static DWORD dwCheckPoint = 1;

    // Fill in the SERVICE_STATUS structure.
    gSvcStatus.dwCurrentState = dwCurrentState;
    gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
    gSvcStatus.dwWaitHint = dwWaitHint;

    if (dwCurrentState == SERVICE_START_PENDING)
        gSvcStatus.dwControlsAccepted = 0;
    else 
		gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

    if ( (dwCurrentState == SERVICE_RUNNING) || (dwCurrentState == SERVICE_STOPPED) )
        gSvcStatus.dwCheckPoint = 0;
    else 
		gSvcStatus.dwCheckPoint = dwCheckPoint++;

    // Report the status of the service to the SCM.
    SetServiceStatus( gSvcStatusHandle, &gSvcStatus );
}

void service_report_event(LPTSTR szFunction) 
{
	//TODO
}

void WINAPI SvcMain( DWORD dwArgc, LPTSTR *lpszArgv )
{
    // Register the handler function for the service
    gSvcStatusHandle = RegisterServiceCtrlHandler( SVCNAME, service_control_handler );

    if( !gSvcStatusHandle ){ 
		PION_ASSERT( gSvcStatusHandle );
        service_report_event(TEXT("RegisterServiceCtrlHandler")); 
        return; 
    } 

    // These SERVICE_STATUS members remain as set here
    gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS; 
    gSvcStatus.dwServiceSpecificExitCode = 0;    

    // Report initial status to the SCM
    report_service_status( SERVICE_START_PENDING, NO_ERROR, 3000 );

	bool run_as_daemon = false;
	bool lock_memory = false;
	
	run( true, g_lock_memory, g_platform_config_file );

	report_service_status( SERVICE_STOPPED, NO_ERROR, 0 );
}
#endif

/// main control function
int main (int argc, char *argv[])
{
	bool run_as_daemon = false;
	bool lock_memory = false;

#ifdef _MSC_VER
	std::string platform_config_file("config\\platform.xml");
#else
	std::string platform_config_file("/etc/pion/platform.xml");
#endif

	if(parse_args(argc, argv, run_as_daemon, lock_memory, platform_config_file) != 0)
		return 1;

#ifdef _MSC_VER
	if(run_as_daemon) {
		// running as Windows Service
		// initialize the global variables to be used by SrvMain
		g_platform_config_file = platform_config_file;
		g_lock_memory = lock_memory;

		SERVICE_TABLE_ENTRY DispatchTable[] = 
		{ 
			{ SVCNAME, (LPSERVICE_MAIN_FUNCTION) SvcMain }, 
			{ NULL, NULL } 
		}; 
		// this call returns when the service has stopped. 
		// the process should simply terminate when the call returns.
		if (!StartServiceCtrlDispatcher( DispatchTable )) { 
			service_report_event(("StartServiceCtrlDispatcher")); 
		}
		return 0;
	}
#endif
	return run(run_as_daemon, lock_memory, platform_config_file);
}

int run (bool run_as_daemon, bool lock_memory, const std::string& platform_config_file)
{
	// initialize log system (use simple configuration)
	PionLogger pion_log(PION_GET_LOGGER("pion"));

	// This level might be overridden if there is a LogConfig file specified in the platform 
	// configuration (and if using a logging library that supports logging configuration.)
	PION_LOG_SETLEVEL_INFO(pion_log);
	PION_LOG_CONFIG_BASIC;

	// get logger for main() process
	PionLogger pion_main_log(PION_GET_LOGGER("pion.main"));

	/// become a daemon if the -D option is given
	if (run_as_daemon)
		daemonize_server();
	
	// setup signal handler
	PionProcess::initialize();
		
#ifdef PION_HAVE_SSL
	// initialize the OpenSSL library
	CRYPTO_malloc_init();
	SSL_library_init();
#endif

#ifndef _MSC_VER
	if (lock_memory)
		if (mlockall(MCL_CURRENT | MCL_FUTURE))
			PION_LOG_FATAL(pion_main_log, "Failed to lock memory: " << strerror(errno));
#endif

	// PlatformConfig destructor can throw exceptions, make sure we handle them
	try	{
		PlatformConfig platform_cfg;
		try {
			// load the platform configuration
			platform_cfg.setConfigFile(platform_config_file);
			platform_cfg.openConfigFile();
			
			PION_LOG_INFO(pion_main_log, "Pion has started successfully (v" << PION_VERSION << ')');
#ifdef _MSC_VER
			if(run_as_daemon)
				report_service_status( SERVICE_RUNNING, NO_ERROR, 0 );
#endif
			// wait for shutdown
			PionProcess::wait_for_shutdown();
		} catch (std::exception& e) {
			PION_LOG_FATAL(pion_main_log, e.what());
		}

		PION_LOG_INFO(pion_main_log, "Pion is shutting down");
	} catch (std::exception& e) {
		PION_LOG_FATAL(pion_main_log, e.what());
	}
	return 0;
}

#ifdef _MSC_VER
/// run server as Windows service
void daemonize_server(void)
{
    gSvcStatusHandle = RegisterServiceCtrlHandler( SVCNAME, service_control_handler );
    if( !gSvcStatusHandle )
    { 
        service_report_event(TEXT("RegisterServiceCtrlHandler")); 
        return; 
    }

    // These SERVICE_STATUS members remain as set here
    gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS; 
    gSvcStatus.dwServiceSpecificExitCode = 0;    

    // Report initial status to the SCM
    report_service_status( SERVICE_START_PENDING, NO_ERROR, 20000 );

	// Windows tends to start the service in C:\windows\system32 folder
	// We need to change it to Pion's installation dir for Pion to work
	char exe_path[MAX_PATH];
	exe_path[0]=0;
	if( GetModuleFileName(NULL, exe_path, sizeof(exe_path)/sizeof(exe_path[0])) ) {
		char exe_dir[MAX_PATH], drive[MAX_PATH], filename[MAX_PATH];
		exe_dir[0]=0; drive[0] = 0; filename[0]=0;
		_splitpath( exe_path, drive, exe_dir, filename, NULL);
		SetCurrentDirectory( exe_dir );
	}
}
#else
/// run server as a daemon (Unix)
void daemonize_server(void)
{
	PionProcess::daemonize();
}
#endif


/// displays an error message if the arguments are invalid
void argument_error(void)
{
	std::cerr << "usage:   pion [-c PLATFORM_XML_FILE] [-D] [-M]" << std::endl;
}

/* main.c  -  Foundation library  -  Public Domain  -  2013 Mattias Jansson / Rampant Pixels
 * 
 * This library provides a cross-platform foundation library in C11 providing basic support data types and
 * functions to write applications and games in a platform-independent fashion. The latest source code is
 * always available at
 * 
 * https://github.com/rampantpixels/foundation_lib
 * 
 * This library is put in the public domain; you can redistribute it and/or modify it without any restrictions.
 *
 */

#include <foundation/foundation.h>
#include <foundation/main.h>


#if FOUNDATION_PLATFORM_WINDOWS

#  include <foundation/windows.h>


BOOL __stdcall _main_console_handler( DWORD control_type )
{
	const char* control_name = "UNKNOWN";
	bool post_terminate = false;
	bool handled = true;

	switch( control_type )
	{
		case CTRL_C_EVENT:         control_name = "CTRL_C"; post_terminate = true; break;
		case CTRL_BREAK_EVENT:     control_name = "CTRL_BREAK"; break;
		case CTRL_CLOSE_EVENT:     control_name = "CTRL_CLOSE"; post_terminate = true; break;
		case CTRL_LOGOFF_EVENT:    control_name = "CTRL_LOGOFF"; post_terminate = !config_bool( HASH_APPLICATION, HASH_DAEMON ); break;
		case CTRL_SHUTDOWN_EVENT:  control_name = "CTRL_SHUTDOWN"; post_terminate = true; break;
		default:                   handled = false; break;
	}
	log_infof( "Caught console control: %s (%d)", control_name, control_type );
	if( post_terminate )
	{
		unsigned int level = 0, flags = 0;

		system_post_event( FOUNDATIONEVENT_TERMINATE );
		
		GetProcessShutdownParameters( &level, &flags );
		SetProcessShutdownParameters( level, SHUTDOWN_NORETRY );

		thread_sleep( 1000 );
	}
	return handled;
}


/*! Win32 UI application entry point, credits to Microsoft for ignoring yet another standard... */
#  if FOUNDATION_COMPILER_MSVC
FOUNDATION_API int APIENTRY WinMain( HINSTANCE, HINSTANCE, LPSTR, int );
#  endif

int APIENTRY WinMain( HINSTANCE instance, HINSTANCE previnst, LPSTR cline, int cmd_show )
{
	int ret = -1;

	if( main_initialize() < 0 )
		return -1;

	SetConsoleCtrlHandler( _main_console_handler, TRUE );

	thread_set_main();

#if BUILD_DEBUG
	ret = main_run( 0 );
#else
	{
		char* name = 0;
		const application_t* app = environment_application();
		{
			const char* aname = app->short_name;
			name = string_clone( aname ? aname : "unknown" );
			name = string_append( name, "-" );
			name = string_append( name, version_to_string_static( app->version ) );
		}

		if( app->dump_callback )
			crash_guard_set( app->dump_callback, name );

		ret = crash_guard( main_run, 0, app->dump_callback, name );

		string_deallocate( name );
	}
#endif

	main_shutdown();

	return ret;
}


#elif FOUNDATION_PLATFORM_ANDROID

#include <foundation/android.h>

#elif FOUNDATION_PLATFORM_POSIX

#include <signal.h>
#include <unistd.h>
#include <stdio.h>

#if FOUNDATION_PLATFORM_MACOSX
extern int NSApplicationMain( int argc, const char *argv[] );
#elif FOUNDATION_PLATFORM_IOS
extern int UIApplicationMain ( int argc, char *argv[], void *principalClassName, void *delegateClassName );
#endif

void sighandler( int sig )
{
	const char* signame = "UNKNOWN";

	switch( sig )
	{
		case SIGKILL: signame = "SIGKILL"; break;
		case SIGTERM: signame = "SIGTERM"; break;
		case SIGQUIT: signame = "SIGQUIT"; break;
		case SIGINT:  signame = "SIGINT"; break;
		default: break;
	}
	log_infof( "Caught signal: %s (%d)", signame, sig );
	system_post_event( FOUNDATIONEVENT_TERMINATE );
	//process_exit( -1 );
}

#endif


/*! Normal entry point for all platforms */

#if FOUNDATION_PLATFORM_ANDROID
void android_main( struct android_app* app )
#else
int main( int argc, char **argv )
#endif
{
	int ret = -1;

#if FOUNDATION_PLATFORM_ANDROID
	if( android_initialize( app ) < 0 )
		return;
	if( main_initialize() < 0 )
		return;
#else
	if( main_initialize() < 0 )
		return ret;
#endif

#if FOUNDATION_PLATFORM_POSIX && !FOUNDATION_PLATFORM_ANDROID
	
	//Set signal handlers
	signal( SIGKILL, sighandler );
	signal( SIGTERM, sighandler );
	signal( SIGQUIT, sighandler );
	signal( SIGINT,  sighandler );

	//Ignore sigpipe
	{
		struct sigaction action;
		memset( &action, 0, sizeof( action ) );
		action.sa_handler = SIG_IGN;
		sigaction( SIGPIPE, &action, 0 );
	}

#endif

#if FOUNDATION_PLATFORM_WINDOWS

	SetConsoleCtrlHandler( _main_console_handler, TRUE );

#endif

	thread_set_main();

#if FOUNDATION_PLATFORM_MACOSX || FOUNDATION_PLATFORM_IOS
	if( !app_config_bool( _HASH_APPLICATION, _HASH_BSDUTILITY ) )
	{
#if FOUNDATION_PLATFORM_MACOSX

		//Fire up new thread to continue engine, then run Cocoa event loop in main thread
		_app_start_main_ns_thread( argc, argv );	
		ret = NSApplicationMain( argc, (const char**)argv );

#elif FOUNDATION_PLATFORM_IOS

		ret = UIApplicationMain( argc, (char**)argv, 0, 0 );

#endif
	}
	else
	{
		ret = app_main( 0 );
	}

#else // !( FOUNDATION_PLATFORM_MACOSX || FOUNDATION_PLATFORM_IOS )

#if BUILD_DEBUG
	ret = main_run( 0 );
#else
	{
		char* name = 0;
		const application_t* app = environment_application();
		{
			const char* aname = app->short_name;
			name = string_clone( aname ? aname : "unknown" );
			name = string_append( name, "-" );
			name = string_append( name, version_to_string_static( app->version ) );
		}

		if( app->dump_callback )
			crash_guard_set( app->dump_callback, name );

		ret = crash_guard( main_run, 0, app->dump_callback, name );

		string_deallocate( name );
	}
#endif

#endif

	main_shutdown();

#if FOUNDATION_PLATFORM_MACOSX || FOUNDATION_PLATFORM_IOS
	if( _bundle )
		CFRelease( _bundle );
#endif

#if FOUNDATION_PLATFORM_ANDROID

	android_shutdown();
	
#else

	return ret;

#endif
}


#if FOUNDATION_PLATFORM_MACOSX || FOUNDATION_PLATFORM_IOS

void _app_did_finish_launching( void )
{
#if FOUNDATION_PLATFORM_IOS
	_app_main_pre_loop();
#endif
}


void _app_did_become_active( void )
{
#if FOUNDATION_PLATFORM_IOS
	app_reset_frame_time();
#endif
}


void _app_will_resign_active( void )
{
}


void _app_will_terminate( void )
{	
	system_event_post( COREEVENT_TERMINATE );
	
	app_pump_events();

	_app_main_post_loop();
	
	core_terminate_services();
	
#if FOUNDATION_PLATFORM_MACOSX || FOUNDATION_PLATFORM_IOS
	if( _bundle )
		CFRelease( _bundle );
#endif
	
	app_exit();
	_app_post_exit();
	
	array_deallocate( _invoke_argv ); //Not cloned strings, so just free array itself
	
	core_shutdown();
}

#endif

#if FOUNDATION_PLATFORM_IOS

bool (* app_open_url)( const char* url, const char* source_app, void* nsurl ) = 0;

bool _app_open_url( const char* url, const char* source_app, void* nsurl )
{
	return app_open_url ? app_open_url( url, source_app, nsurl ) : false;
}

#endif


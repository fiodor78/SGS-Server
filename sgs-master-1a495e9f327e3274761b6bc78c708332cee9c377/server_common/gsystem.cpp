/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"

#ifdef LINUX
#include "google/coredumper.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#endif

static int max_core_count = 3;
static int core_nr = 1;

// Idea sygnalu 'close_gently' jest taka, ze podpiete do niego funkcje
// maja duzo czasu na to, zeby 'posprzatac' przed zamknieciem (np. czekaja
// na zakonczenie trwajacych rozdan w pokerze), a dopiero po wszystkim ma
// zostac wywolany sygnal 'close_slow'.

void SigQuit(int)
{
	//Ctrl 
	if (global_signals->close_gently.num_slots() > 0)
	{
		global_signals->close_gently();
	}
	else
	{
		global_signals->close_slow();
	}
}
void SigInt(int)
{
	//Ctrl-C
	if (global_signals->close_gently.num_slots() > 0)
	{
		global_signals->close_gently();
	}
	else
	{
		global_signals->close_slow();
	}
}
void SigTerm(int)
{	
	//kill -9
	global_signals->close_fast();
}

void SigTStp(int)
{
	//Ctrl-Z
	WriteCore();
}

void WriteCore(bool force_coredump)
{
#ifdef LINUX
	if (force_coredump || max_core_count > 0)
	{
		if (!force_coredump)
		{
			max_core_count--;
		}
		char buf[256];
		sprintf(buf,"core.%s.%d",game_name.c_str(),core_nr++);
		WriteCoreDump(buf);
	}
#endif
}

void SigUser1(int)
{
	WriteCore();
}

/***************************************************************************/
GSystem::GSystem()
{
}
/***************************************************************************/
void GSystem::Init()
{
#ifdef LINUX
	openlog(game_name.c_str(),LOG_NDELAY|LOG_CONS|LOG_PID,LOG_LOCAL0);
#endif

	assert(global_signals!=NULL);

#ifdef LINUX
	::signal(SIGQUIT,SigQuit);
	::signal(SIGINT,SigInt);
	::signal(SIGTERM,SigTerm);
	::signal(SIGTSTP,SigTStp);
	::signal(SIGUSR1,SigUser1);
#endif
};
/***************************************************************************/
GSystem::~GSystem()
{
#ifdef LINUX
	closelog();
#endif
};
/***************************************************************************/
#ifdef LINUX
void GSystem::LogInfo(const char * msg,...)
{
	char buffer[MAX_PRINT_SIZE];
	va_list args;
	va_start(args,msg);
	int cb = vsprintf(buffer, msg, args);
	syslog(LOG_INFO,"%s",buffer);
	va_end(args);
}
/***************************************************************************/
void GSystem::LogError(const char * msg,...)
{
	char buffer[MAX_PRINT_SIZE];
	va_list args;
	va_start(args,msg);
	int cb = vsprintf(buffer, msg, args);
	syslog(LOG_ERR,"%s",buffer);
	va_end(args);
}
#endif
/***************************************************************************/




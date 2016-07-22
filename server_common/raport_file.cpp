/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"
DWORD32 GetAddrFromName(const char * name);

#pragma warning (disable: 4996)

//#define FL
/////////////////////////////////////////////////////////////////////////////
GRaportManager::GRaportManager():time_period(GSECOND_1),flush_period(GMINUTE_1)
{
	raport_file=NULL;
	lines=0;
	modified=false;
	initialized=false;
	raport_callback=NULL;
	overflow=-1;
	overflow_lines=0;
	add_time_signature=false;
	add_line_time_signature=true;
	raport_mode=RAP_DAY;
	memset(&yesterday,0,sizeof(tm));
}
/////////////////////////////////////////////////////////////////////////////
GRaportManager::~GRaportManager()
{
	Destroy();
}
/////////////////////////////////////////////////////////////////////////////
void GRaportManager::Init(const char * raport_path_p, const char * game_name_p)
{
	raport_path=raport_path_p;
	raport_file=NULL;
	lines=0;
	game_name=game_name_p;
	modified=false;
	file_name[0]=0;
	wait_for_data=false;
	add_time_signature=false;
	initialized=true;
}
/////////////////////////////////////////////////////////////////////////////
void GRaportManager::Destroy()
{
	if (raport_file)
	{
		fflush(raport_file);
		fclose(raport_file);

		raport_file=NULL;
		if (modified==false)
		{
			unlink(file_name);
		}
		else
		{
			switch(raport_mode)
			{
			case RAP_15MIN:
			case RAP_5MIN:
				{
					char buf[256];
					strcpy(buf, file_name);
					char * ptr=strstr(buf,".log");
					if(ptr!=NULL)
					{
						STRUPR(ptr);
						rename(file_name, buf);
					}
				}
				break;
			}
		}
	}
}
/////////////////////////////////////////////////////////////////////////////
void GRaportManager::UpdateTime(GClock & clock)
{
	if(!time_period(clock)) return;

	if(flush_period(clock))
	{
		if(raport_file) 
		{
			fflush(raport_file);
#ifdef LINUX
			struct stat buf;
			int ret= stat( file_name, &buf );
#else
			struct __stat64 buf;
			int ret= _stat64( file_name, &buf );
#endif
			if(ret==-1){fclose(raport_file);raport_file=NULL;wait_for_data=true;}
		}
		overflow_lines=0;
	}
	UpdateName(clock);
}
/////////////////////////////////////////////////////////////////////////////
void GRaportManager::UpdateName(GClock & )
{
	boost::mutex::scoped_lock lock(sem_raport);

	struct tm *today;
	time_t ltime;
	time( &ltime );
	today = gmtime( &ltime );

	strftime(date, 128, "%H:%M:%S", today);

	if (raport_mode == RAP_15MIN)
		today->tm_min -= (today->tm_min % 15);
	if (raport_mode == RAP_5MIN)
		today->tm_min -= (today->tm_min % 5);
	
	bool create_new_rap=false;
	switch(raport_mode)
	{
	case RAP_DAY:
		if (yesterday.tm_mday!=today->tm_mday) create_new_rap=true;
		break;
	case RAP_HOUR:
		if (yesterday.tm_hour!=today->tm_hour) create_new_rap=true;
		break;
	case RAP_15MIN:
	case RAP_5MIN:
		if (yesterday.tm_min!=today->tm_min) create_new_rap=true;
		break;
	}
	
	memcpy(&yesterday,today,sizeof(tm));
	
	if (create_new_rap || (raport_file==NULL && wait_for_data==false && create_new_rap==false))
	{
		char time[256];
		if(raport_mode==RAP_DAY) strftime(time,256,"[%Y_%m_%d].log",today);
		else
		if(raport_mode==RAP_HOUR) strftime(time,256,"[%Y_%m_%d %H_00].log",today);
		else 
			strftime(time,256,"[%Y_%m_%d %H_%M].log",today);

		if (raport_file)
		{
			fflush(raport_file);
			fclose(raport_file);

			raport_file=NULL;
			if (modified==false)
			{
				unlink(file_name);
			}
			else
			{
				char buf[256];
				strcpy(buf, file_name);
				char * ptr=strstr(buf,".log");
				if(ptr!=NULL)
				{
					STRUPR(ptr);
					rename(file_name, buf);
				}
			}

			modified=false;
		}
		char buf[512];
		sprintf(buf,"%s%s%s",raport_path.c_str(),game_name.c_str(),time);
		strncpy(file_name,buf,256);
		wait_for_data=true;
	}
}
/////////////////////////////////////////////////////////////////////////////
bool GRaportManager::TestRaportFile()
{
	if (raport_file==NULL) 
	{
		if(wait_for_data)
		{
			wait_for_data=false;
#ifdef LINUX
			struct stat buf;
			int ret= stat( file_name, &buf );
#else
			struct __stat64 buf;
			int ret= _stat64( file_name, &buf );
#endif
			raport_file=fopen(file_name,"ab");
			lines=1;
			if (raport_file==NULL) return false;

			if(ret==-1)
			{
				unsigned char utf_marker[3]={0xEF,0xBB,0xBF};
				fwrite(utf_marker,1,3,raport_file);
#ifdef FL
				fflush(raport_file);
#endif
			}
		}
		if (raport_file==NULL) return false;
	}
	return true;
}
/////////////////////////////////////////////////////////////////////////////
bool GRaportManager::TestOverflow()
{
	if(overflow==-1) return false;

	if(raport_file!=NULL) 
	if(overflow_lines==overflow)
	{
		max_len=4096;
		SNPRINTF(buffer_format,max_len,"Raport overflow. %d lines generated in one minute. Limit is %d lines\r\n", overflow_lines,overflow);
		max_len=strlen(buffer_format);
		fwrite(buffer_format,max_len,1,raport_file);
#ifdef FL
		fflush(raport_file);
#endif
		lines++;
	}
	if(overflow_lines>=overflow) return true;
	overflow_lines++;
	return false;
}
/////////////////////////////////////////////////////////////////////////////
void GRaportManager::Flush()
{
	if (raport_file)
	{
		fflush(raport_file);
	};
};
/////////////////////////////////////////////////////////////////////////////
void GRaportManager::AddTimeSignature()
{
	if(!add_time_signature) return;
	add_time_signature=false;
	max_len=128;
	char buffer_time[128];
	SNPRINTF(buffer_time,max_len,"[TIMESIGNATURE:%s][LINESIGNATURE:%d]\r\n",date, lines);
	max_len=strlen(buffer_time);
	fwrite(buffer_time,max_len,1,raport_file);
#ifdef FL
fflush(raport_file);
#endif
	lines++;
	modified=true;
	if(raport_callback!=NULL)
	{
		(*raport_callback)("games_normal",buffer_time);
	}
}
/////////////////////////////////////////////////////////////////////////////
void GRaportManager::Raport(const GRaportInterface * raport_interface)
{
	boost::mutex::scoped_lock lock(sem_raport);
	if (TestOverflow()) return;
	if (TestRaportFile())
	{
		AddTimeSignature();
		if(raport_interface->final_buffers_size)
		{
			fwrite(raport_interface->final_buffers,raport_interface->final_buffers_size,1,raport_file);
			if(raport_callback!=NULL) (*raport_callback)("games_normal",raport_interface->final_buffers);
			lines+=raport_interface->lines;
		}

#ifdef FL
		fflush(raport_file);
#endif
		modified=true;
	}
}
/////////////////////////////////////////////////////////////////////////////
void GRaportManager::Raport(const char * format, ...)
{
	boost::mutex::scoped_lock lock(sem_raport);
	va_list args;
	va_start(args, format) ;
	max_len = 4096 - 32;								// Zostawiamy 32 bajty miejsca na date i znak konca linii.
	VSNPRINTF(buffer_arg,max_len,format, args) ;
	max_len=4096;
	if(add_line_time_signature)
		SNPRINTF(buffer_format,max_len,"[%s] %s\r\n", date, buffer_arg);
	else
		SNPRINTF(buffer_format,max_len,"%s\r\n", buffer_arg);
	max_len=strlen(buffer_format);
	AddBuffer(buffer_format);
}
/////////////////////////////////////////////////////////////////////////////
void GRaportManager::Raport(string str)
{
	boost::mutex::scoped_lock lock(sem_raport);
	max_len=4096;
	if(add_line_time_signature)
		SNPRINTF(buffer_format,max_len,"[%s] %s\r\n", date, str.c_str());
	else
		SNPRINTF(buffer_format,max_len,"%s\r\n", str.c_str());
	max_len=strlen(buffer_format);
	AddBuffer(buffer_format);
}
/////////////////////////////////////////////////////////////////////////////
void GRaportManager::RaportGame(DWORD64 game,const char * format, ...)
{
	boost::mutex::scoped_lock lock(sem_raport);
	va_list args;
	va_start(args, format) ;
	max_len = 4096 - 32;								// Zostawiamy 32 bajty miejsca na date i znak konca linii.
	VSNPRINTF(buffer_arg,max_len,format, args) ;
	max_len=4096;
#ifdef LINUX
	SNPRINTF(buffer_format,max_len,"[GAME %10lld]:[%s] %s\r\n", game,date, buffer_arg);
#else
	SNPRINTF(buffer_format,max_len,"[GAME %10I64d]:[%s] %s\r\n", game,date, buffer_arg);
#endif
	max_len=strlen(buffer_format);
	AddBuffer(buffer_format);
}
/////////////////////////////////////////////////////////////////////////////
void GRaportManager::RaportLine()
{
	boost::mutex::scoped_lock lock(sem_raport);
	AddBuffer("----------------------------------------------------------------------------------------------------\r\n");
}
/////////////////////////////////////////////////////////////////////////////
void GRaportManager::AddBuffer(const char * buffer)
{
	if (TestOverflow()) return;
	if (TestRaportFile())
	{
		AddTimeSignature();
		int len=strlen(buffer);
		fwrite(buffer,len,1,raport_file);
		if(raport_callback!=NULL) (*raport_callback)("games_normal",buffer);
		lines++;

#ifdef FL
		fflush(raport_file);
#endif
		modified=true;
	}
}
/////////////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////////////
GRaportInterface::GRaportInterface(GRaportManager * raport_manager_)
{
	mgr=raport_manager_;
	Init();
}
/////////////////////////////////////////////////////////////////////////////
GRaportInterface::~GRaportInterface()
{
	//Destroy();
}
/////////////////////////////////////////////////////////////////////////////
void GRaportInterface::Init()
{
	final_buffers_size=0;
	lines=0;

	struct tm *today;
	time_t ltime;
	time( &ltime );
	today = gmtime( &ltime );

	strftime(date, 128, "%H:%M:%S", today);
	verbose_mode = false;
}
/////////////////////////////////////////////////////////////////////////////
void GRaportInterface::Destroy()
{
	Flush();
}
/////////////////////////////////////////////////////////////////////////////
void GRaportInterface::UpdateTime(GClock & )
{
	Flush();
	struct tm * today;
	time_t ltime;
	time( &ltime );
#ifndef LINUX
	today = gmtime( &ltime );
#else
	struct tm today_local;
	today = &today_local;
	if (!gmtime_r(&ltime, today))
	{
		return;
	}
#endif

	strftime(date, 128, "%H:%M:%S", today);
}
/////////////////////////////////////////////////////////////////////////////
void GRaportInterface::Raport(const char * format, ...)
{
	va_list args;
	va_start(args, format) ;
	max_len = 4096 - 32;								// Zostawiamy 32 bajty miejsca na date i znak konca linii.
	VSNPRINTF(buffer_arg,max_len,format, args) ;
	max_len=4096;
	SNPRINTF(buffer_format,max_len,"[%s] %s\r\n", date, buffer_arg);
	max_len=strlen(buffer_format);
	AddBuffer(buffer_format);
}
/////////////////////////////////////////////////////////////////////////////
void GRaportInterface::RaportVerbose(const char * format, ...)
{
	if (!verbose_mode)
	{
		return;
	}
	va_list args;
	va_start(args, format) ;
	max_len = 4096 - 32;								// Zostawiamy 32 bajty miejsca na date i znak konca linii.
	VSNPRINTF(buffer_arg,max_len,format, args) ;
	max_len=4096;
	SNPRINTF(buffer_format,max_len,"[%s] %s\r\n", date, buffer_arg);
	max_len=strlen(buffer_format);
	AddBuffer(buffer_format);
}
/////////////////////////////////////////////////////////////////////////////
void GRaportInterface::Raport(string str)
{
	max_len=4096;
	SNPRINTF(buffer_format,max_len,"[%s] %s\r\n", date, str.c_str());
	max_len=strlen(buffer_format);
	AddBuffer(buffer_format);
}
/////////////////////////////////////////////////////////////////////////////
void GRaportInterface::RaportGame(DWORD64 game,const char * format, ...)
{
	va_list args;
	va_start(args, format) ;
	max_len = 4096 - 32;								// Zostawiamy 32 bajty miejsca na date i znak konca linii.
	VSNPRINTF(buffer_arg,max_len,format, args) ;
	max_len=4096;
#ifdef LINUX
	SNPRINTF(buffer_format,max_len,"[GAME %10lld]:[%s] %s\r\n", game,date, buffer_arg);
#else
	SNPRINTF(buffer_format,max_len,"[GAME %10I64d]:[%s] %s\r\n", game,date, buffer_arg);
#endif
	max_len=strlen(buffer_format);
	AddBuffer(buffer_format);
}
/////////////////////////////////////////////////////////////////////////////
void GRaportInterface::RaportLine()
{
	AddBuffer("----------------------------------------------------------------------------------------------------\r\n");
}
/////////////////////////////////////////////////////////////////////////////
void GRaportInterface::AddBuffer(const char * buffer)
{
	size_t len = strlen(buffer);
	size_t added = 0;
	while (added < len)
	{
		size_t to_add = min((INT32)(len - added), (INT32)(K64 - 1));
		if (final_buffers_size + to_add + 1 >= K64)
		{
			Flush();
		}
		strncpy(&final_buffers[final_buffers_size], buffer + added, to_add + 1);
		final_buffers_size += to_add;
		final_buffers[final_buffers_size] = 0;
		added += to_add;
	}
}
/////////////////////////////////////////////////////////////////////////////
void GRaportInterface::Flush()
{

	if(final_buffers_size==0) return;
	if(mgr) mgr->Raport(this);
	final_buffers_size=0;
	lines=0;
}
/////////////////////////////////////////////////////////////////////////////








GSimpleRaportManager::GSimpleRaportManager():time_period(GSECOND_1),flush_period(GMINUTE_1)
{
	raport_file=NULL;
	lines=0;
	modified=false;
	initialized=false;
	overflow=-1;
	overflow_lines=0;
	add_time_signature=false;
	add_line_time_signature=true;
	raport_mode=RAP_DAY;
	memset(&yesterday,0,sizeof(tm));
}
/////////////////////////////////////////////////////////////////////////////
GSimpleRaportManager::~GSimpleRaportManager()
{
	Destroy();
}
/////////////////////////////////////////////////////////////////////////////
void GSimpleRaportManager::Init(const char * raport_path_p, const char * base_name_p)
{
	raport_path=raport_path_p;
	raport_file=NULL;
	lines=0;
	base_name=base_name_p;
	modified=false;
	file_name[0]=0;
	wait_for_data=false;
	add_time_signature=false;
	initialized=true;
}
/////////////////////////////////////////////////////////////////////////////
void GSimpleRaportManager::Destroy()
{
	if (raport_file)
	{
		fflush(raport_file);
		fclose(raport_file);

		raport_file=NULL;
		if (modified==false)
		{
			unlink(file_name);
		}
		else
		{
			switch(raport_mode)
			{
			case RAP_15MIN:
			case RAP_5MIN:
				{
					char buf[256];
					strcpy(buf, file_name);
					char * ptr=strstr(buf,".log");
					if(ptr!=NULL)
					{
						STRUPR(ptr);
						rename(file_name, buf);
					}
				}
				break;
			}
		}
	}
}
/////////////////////////////////////////////////////////////////////////////
void GSimpleRaportManager::UpdateTime(GClock & clock)
{
	if(!time_period(clock)) return;

	if(flush_period(clock))
	{
		if(raport_file) 
		{
			fflush(raport_file);
#ifdef LINUX
			struct stat buf;
			int ret= stat( file_name, &buf );
#else
			struct __stat64 buf;
			int ret= _stat64( file_name, &buf );
#endif
			if(ret==-1){fclose(raport_file);raport_file=NULL;wait_for_data=true;}
		}
		overflow_lines=0;
	}
	UpdateName(clock);
}
/////////////////////////////////////////////////////////////////////////////
void GSimpleRaportManager::UpdateName(GClock & )
{
	struct tm *today;
	time_t ltime;
	time( &ltime );
	today = gmtime( &ltime );

	strftime(date, 128, "%H:%M:%S", today);

	if (raport_mode == RAP_15MIN)
		today->tm_min -= (today->tm_min % 15);
	if (raport_mode == RAP_5MIN)
		today->tm_min -= (today->tm_min % 5);
	
	bool create_new_rap=false;
	switch(raport_mode)
	{
	case RAP_DAY:
		if (yesterday.tm_mday!=today->tm_mday) create_new_rap=true;
		break;
	case RAP_HOUR:
		if (yesterday.tm_hour!=today->tm_hour) create_new_rap=true;
		break;
	case RAP_15MIN:
	case RAP_5MIN:
		if (yesterday.tm_min!=today->tm_min) create_new_rap=true;
		break;
	}
	
	memcpy(&yesterday,today,sizeof(tm));
	
	if (create_new_rap || (raport_file==NULL && wait_for_data==false && create_new_rap==false))
	{
		char time[256];
		if(raport_mode==RAP_DAY) strftime(time,256,"[%Y_%m_%d].log",today);
		else
		if(raport_mode==RAP_HOUR) strftime(time,256,"[%Y_%m_%d %H_00].log",today);
		else 
			strftime(time,256,"[%Y_%m_%d %H_%M].log",today);

		if (raport_file)
		{
			fflush(raport_file);
			fclose(raport_file);

			raport_file=NULL;
			if (modified==false)
			{
				unlink(file_name);
			}
			else
			{
				char buf[256];
				strcpy(buf, file_name);
				char * ptr=strstr(buf,".log");
				if(ptr!=NULL)
				{
					STRUPR(ptr);
					rename(file_name, buf);
				}
			}

			modified=false;
		}
		char buf[512];
		sprintf(buf,"%s%s%s",raport_path.c_str(),base_name.c_str(),time);
		strncpy(file_name,buf,256);
		wait_for_data=true;
	}
}
/////////////////////////////////////////////////////////////////////////////
bool GSimpleRaportManager::TestRaportFile()
{
	if (raport_file==NULL) 
	{
		if(wait_for_data)
		{
			wait_for_data=false;
#ifdef LINUX
			struct stat buf;
			int ret= stat( file_name, &buf );
#else
			struct __stat64 buf;
			int ret= _stat64( file_name, &buf );
#endif
			raport_file=fopen(file_name,"ab");
			lines=1;
			if (raport_file==NULL) return false;

			if(ret==-1)
			{
				unsigned char utf_marker[3]={0xEF,0xBB,0xBF};
				fwrite(utf_marker,1,3,raport_file);
#ifdef FL
				fflush(raport_file);
#endif
			}
		}
		if (raport_file==NULL) return false;
	}
	return true;
}
/////////////////////////////////////////////////////////////////////////////
bool GSimpleRaportManager::TestOverflow()
{
	if(overflow==-1) return false;

	if(raport_file!=NULL) 
	if(overflow_lines==overflow)
	{
		const int max_len=4096;
		char buffer_format[max_len];
		SNPRINTF(buffer_format,max_len,"Raport overflow. %d lines generated in one minute. Limit is %d lines\r\n", overflow_lines,overflow);
		int len=strlen(buffer_format);
		fwrite(buffer_format,len,1,raport_file);
#ifdef FL
		fflush(raport_file);
#endif
		lines++;
	}
	if(overflow_lines>=overflow) return true;
	overflow_lines++;
	return false;
}
/////////////////////////////////////////////////////////////////////////////
void GSimpleRaportManager::Flush()
{
	if (raport_file)
	{
		fflush(raport_file);
	};
};
/////////////////////////////////////////////////////////////////////////////
void GSimpleRaportManager::AddTimeSignature()
{
	if(!add_time_signature) return;
	add_time_signature=false;
	const int max_len=128;
	char buffer_time[max_len];
	SNPRINTF(buffer_time,max_len,"[TIMESIGNATURE:%s][LINESIGNATURE:%d]\r\n",date, lines);
	int len=strlen(buffer_time);
	fwrite(buffer_time,len,1,raport_file);
#ifdef FL
fflush(raport_file);
#endif
	lines++;
	modified=true;
}
/////////////////////////////////////////////////////////////////////////////
void GSimpleRaportManager::Raport(const char *raport_str)
{
	if (TestOverflow()) return;
	if (TestRaportFile())
	{
//		AddTimeSignature();
		fprintf(raport_file,"[%s] ",date);
		fwrite(raport_str,strlen(raport_str),1,raport_file);
		const char *line_end="\r\n";
		fwrite(line_end,strlen(line_end),1,raport_file);
		lines+=1;
#ifdef FL
		fflush(raport_file);
#endif
		modified=true;
	}
}
/////////////////////////////////////////////////////////////////////////////

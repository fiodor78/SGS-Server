/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"
#include "headers_game.h"
#include "gmain_server.h"

extern GServerBase * global_server;

#ifdef USE_SPREAD
mailbox			SP_Mbox;
char			SP_PG[MAX_GROUP_NAME];
#endif

/***************************************************************************/
GMainServer::GMainServer():raport_interface(&raport_manager_global)
{
}
/***************************************************************************/
bool GMainServer::Init()
{
	RAPORT("Initializing basic services");
	if(!InitSystem()) 
	{
		PRINT("Signals error\n");
		return false;
	}
	if(!InitRaport()) 
	{
		PRINT("Raport error\n");
		return false;
	}
#ifdef USE_SPREAD
	if(!InitSpread()) 
	{
		PRINT("Spread error\n");
		return false;
	}
#endif
	InitRandom();
	if(!InitSocket())
	{
		PRINT("Socket initialization error\n");
		return false;
	}

	return true;
}
/***************************************************************************/
bool GMainServer::InitSocket()
{
#ifndef LINUX
	WSADATA wsaData;
	WORD versionRequested = MAKEWORD( 2, 0 );
	memset(&wsaData,0,sizeof(wsaData));
	if(WSAStartup( versionRequested, &wsaData ))
	{
		return false;
	}
	RAPORT("Socket library initialized");
#endif
	return true;
}
/***************************************************************************/
bool GMainServer::InitSystem()
{
#ifdef LINUX
	mallopt(M_MMAP_MAX,10000);
	mallopt(M_MMAP_THRESHOLD,32*1024);
#endif

	global_signals=&gsignals;
	global_system=&gsystem;
	global_statistics=&gstatistics;
	global_sqlmanager=&gsqlmanager;
	gsignals.Init();
	gsystem.Init();
	gstatistics.Init();
	gsqlmanager.Init();

	return true;
}
/***************************************************************************/
bool GMainServer::InitRaport()
{
	GClock clock;
	raport_manager_global.Init(global_serverconfig->log_path.c_str(),game_name.c_str());
	raport_manager_global.SetOverflow(global_serverconfig->misc.raport_overflow);
	raport_manager_global.UpdateName(clock);

	PRINT("Raport Initialized, File %s\n",global_serverconfig->log_path.c_str());
	return true;
}
/***************************************************************************/
void GMainServer::InitRandom()
{
	rng_alg.seed((DWORD32)GetTime());
	int rnd_buffer[2048];
	int a;
	for (a=0;a<2048;a++) rnd_buffer[a]=rng_alg();
	RAND_seed(rnd_buffer, sizeof(rnd_buffer));
}
/***************************************************************************/
#ifdef USE_SPREAD
bool SpreadCallback(const char * group,const char * msg)
{
	int len=(int)strlen(msg);
	int ret=SP_multicast( SP_Mbox, SAFE_MESS,group,1, len,(char*) msg); 
	if(ret<0) 
	{
		SP_error(ret);
		return false;
	}
	return true;
}
/***************************************************************************/
bool GMainServer::InitSpread()
{
	if(global_serverconfig->spread.use==false)
	{
		PRINT("Spread Disabled\n");
		return true;
	}
	int ret = SP_connect( global_serverconfig->spread.server.c_str(),"connection", 0, 1, &SP_Mbox, SP_PG);
	if( ret != ACCEPT_SESSION ) 
	{
		SP_error( ret );
		return false;
	}
	ret=SP_join(SP_Mbox,"games"); 
	if(ret<0) 
	{
		SP_error(ret);
		PRINT("Spread Initialization Error\n");
		return false;
	};
	raport_manager_global.SetRaportCallback(SpreadCallback);
	PRINT("Spread Library Initialized, Server %s\n",global_serverconfig->spread.server.c_str());
	return true;
}
#endif
/***************************************************************************/
bool GMainServer::Destroy()
{
#ifdef USE_SPREAD
	DestroySpread();
#endif
	DestroySocket();
	DestroySystem();
	raport_interface.Destroy();
	return true;
}
/***************************************************************************/
bool GMainServer::DestroySocket()
{
	RAPORT("Socket Library Destruction");
#ifndef LINUX
	WSAGetLastError();
	WSACleanup( );
#endif
	return true;
}
/***************************************************************************/
#ifdef USE_SPREAD
bool GMainServer::DestroySpread()
{
	RAPORT("Spread Library Destruction");
	return true;
}
#endif
/***************************************************************************/
bool GMainServer::DestroySystem()
{
	gstatistics.Destroy();
	gsignals.Destroy();
	gsqlmanager.Destroy();
	return true;
}
/***************************************************************************/
bool GMainServer::Action()
{
	GSERVER_INIT gserver;
	global_server=&gserver;

	raport_interface.Flush();
	if(gserver.Init())
	{
		gserver.RaportRestartSQL("start");
		gserver.Action();
		gserver.Destroy();
		gserver.RaportRestartSQL("stop");
		return true;
	}
	return false;
}
/***************************************************************************/

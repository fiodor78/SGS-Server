/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#pragma warning(disable: 4511)

/***************************************************************************/
class GServerManager: public GServerBaseConsole
{
public:
	GServerManager();
	~GServerManager(){Destroy();};
	bool			Init();
	// Funkcja wolana w momencie rozpoczecia faktycznego dzialania przez serwis typu 'single_instance'.
	virtual void	WakeUpService() {};
	virtual bool	InitSQL();
	void			Destroy();

	bool			RegisterLogic(GSocket *);
	void			UnregisterLogic(GSocket *,EUSocketMode, bool);
	bool 			PreProcessInternalMessage(GSocket * socket,GNetMsg & msg,int message);
	bool 			PreProcessTargetMessage(GSocket * socket,GNetMsg & msg,int message,GNetTarget & target);
	virtual bool	ProcessInternalMessage(GSocket *,GNetMsg &,int){return false;};
	virtual bool	ProcessTargetMessage(GSocket *,GNetMsg &,int,GNetTarget &){return false;};
	virtual GSocket *	SocketNew(EServiceTypes){GSocket * s=new GSocketInternalServer(raport_interface_local);return s;};
	void			ConfigureClock();
	void			PreConnect(GSocket * socket,INT32 p1,INT32 p2,INT32 p3,INT32 p4,INT32 p5){Connect(socket,p1,p2,p3,p4,p5);};
	void			PreDisconnect(GSocket * socket,INT32 p1,INT32 p2,INT32 p3,INT32 p4,INT32 p5){Disconnect(socket,p1,p2,p3,p4,p5);};
	virtual void	Connect(GSocket * ,INT32 ,INT32 ,INT32 ,INT32 ,INT32 ){};
	virtual void	Disconnect(GSocket * ,INT32 ,INT32 ,INT32 ,INT32 ,INT32 ){};
	void			UpdateHeartBeat(bool zero_heartbeat = false);
	void			CheckForOtherInstances(bool server_init);

	GRaportInterface	raport_interface_local;
	GRaportManager		raport_manager_local;

	INT64				last_heartbeat;					// -1 - serwis jeszcze nie podniesiony, 0 - serwis zgaszony
	bool				single_instance_service;

protected:
	boost::recursive_mutex	mtx_sql;
	MYSQL					mysql;
	bool					sql_config_initialized;
};
/***************************************************************************/
struct eq_INT32
{
	bool operator()(const INT32 s1, const INT32 s2) const
	{
		return (s1==s2);
	}
};
struct hash_INT32
{
	size_t operator()(const INT32 x) const { return x;} ;
};

typedef hash_multimap<INT32, INT32>		TFriendsList;     // playerId -> playerId
	
#include "gaccessmanager.h"	
#include "gcachemanager.h"	
#include "gslowsqlmanager.h"	
#include "gflashpolicymanager.h"
#include "gglobalinfomanager.h"
#include "gnodebalancermanager.h"
#include "gleaderboardmanager.h"
#include "gcenzormanager.h"
#include "gglobalmessagemanager.h"
/***************************************************************************/
class GServerStats: public GServer
{
public:
	GServerStats():GServer() {	console.SetServer(this);};
	bool			Init();
	void			Destroy();
	void			InitServices();
	bool			InitThreads();
	bool			InitSQL();
	virtual GSocket *	SocketNew(EServiceTypes){GSocket * s=new GSocketInternalServer(raport_interface);return s;};
	void			ParseConsole(GSocketConsole *socket){console.ParseConsole(socket);};
	void			ConfigureClock();
	void			ServicesWatchdog();

	typedef map<string, pair<GServerManager *, boost::thread *> >	TManagersMap;
	TManagersMap	managers;

private:
	GConsole		console;
};
/***************************************************************************/
#undef DECLARE
#pragma warning(default: 4511)
/***************************************************************************/
#undef MySQLQuery
#define MySQLQuery(m, q) this->MySQL_Query_Raport(m, q, __FILE__, __LINE__)
/***************************************************************************/

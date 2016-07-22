/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#pragma warning(disable: 4996)
/***************************************************************************/
class GConsoleAccess: public GConsoleBasic
{
public:
	GConsoleAccess():GConsoleBasic(){InitConsole();};
	//! inicjalizuje konsole
	void InitConsole()
	{
#define GCONSOLE(command, function,description)		console_commands.insert(make_pair(string(#command).erase(0,8),boost::bind(&GConsoleAccess::function,this,_1,_2)));
#include "tbl/gconsole_access.tbl"
#undef GCONSOLE
#define GCONSOLE(command, function,description)		console_descriptions.push_back(make_pair(string(#command).erase(0,8),description));
#include "tbl/gconsole_access.tbl"
#undef GCONSOLE
	}
#undef GCONSOLE
#define GCONSOLE(command, function,description)		void function(GSocketConsole *,char * buffer);
#include "tbl/gconsole_access.tbl"
#undef GCONSOLE
	GRaportInterface &	GetRaportInterface();
};
/***************************************************************************/
struct SAccessRegistrationTask
{
	SAccessRegistrationTask()
	{
		socketid = -1;
		expiration_time = 0;
	}
	SAccessRegistrationTask(const SAccessRegistrationTask & t)
	{
		operator=(t);
	}

	INT32				socketid;
	GNetTarget			target;
	GTicketPlayer		t_player;
	DWORD64				expiration_time;

	void operator=(const SAccessRegistrationTask & t)
	{
		socketid = t.socketid;
		target = t.target;
		t_player = t.t_player;
		expiration_time = t.expiration_time;
	}
};
/***************************************************************************/


extern boost::hash<std::string> hash_str;

class GAccessManager: public GServerManager
{
	friend class GConsoleAccess;
public:
	GAccessManager();
	~GAccessManager(){Destroy();};
	virtual void	WakeUpService();
	void			InitInternalClients();
	void			InitServices()
	{
		GServerManager::InitServices();
		service_manager.services.push_back(SSockService(ESTAcceptAccess,ESTAcceptAccessConsole));
	}
	bool			RegisterPlayer(SPlayerInfo &);
	bool			UnregisterPlayer(SPlayerInfo &);
	void			UnregisterAllInstances(GSocket * socket);
	bool 			ProcessInternalMessage(GSocket *,GNetMsg &,int);
	bool 			ProcessTargetMessage(GSocket *,GNetMsg &,int,GNetTarget &);
	void			ProcessConnection(GNetMsg & msg);
	void			ParseConsole(GSocketConsole *socket){console.ParseConsole(socket);};

private:
	void			GetSessionsFromSQL();

	bool			RegisterLogic(GSocket *);
	void			UnregisterLogic(GSocket *,EUSocketMode,bool);
	void			ConfigureClock();
	void			UpdateSession();
	void			ClearFailedLoginBans();
	bool			TestRegisterPlayer(GSocket *socket,GNetMsg & msg,int message,GNetTarget & target);
	void			UpdateSocialFriends(INT32 playerId, GNetMsg & msg);
	void			ClearExpiredSocialFriends();
	void			ProcessPendingRegistrationTasks(INT32 playerId);
	void			ProcessExpiredRegistrationTasks();

	bool			TestRegisterID_SID(GNetMsg & msg,GNetTarget & target,GTicketConnection & t_access, GTicketPlayer & t_player);
private:
	/////////////////////////////

	struct SSession
	{
		SSession()
		{
			SID[0]=0;
			ID=0;
			last_connection = 0;
			expiration=0;
		};
		INT32						ID;
		DWORD64						last_connection;
		DWORD64					 	expiration;
		char						SID[MAX_SID];
		void operator = (SSession & s)
		{
			ID=s.ID;
			last_connection = s.last_connection;
			expiration=s.expiration;
			strncpy(SID,s.SID,MAX_SID);
		}
	};
	struct eq_int
	{
		bool operator()(const INT32 s1, const INT32 s2) const
		{
			return (s1==s2);
		}
	};

	struct hash_int
	{ 
		size_t operator()(const INT32 x) const { return x;} ;
	};
	typedef dense_hash_map<const INT32,SSession,hash_int,eq_int>	TSessionMap;

	TSessionMap						sessions;
	/////////////////////////////

	struct SSessionRev
	{
		SSessionRev(){buf[0]=0;};
		SSessionRev(const char * str){strncpy(buf,str,MAX_SID);};
		char buf[MAX_SID];
		void operator=(SSessionRev & s)
		{
			strncpy(buf,s.buf,MAX_SID);
		}
	};
	struct eq_str
	{
		bool operator()(const SSessionRev & s1, const SSessionRev & s2) const
		{
			return (strncmp(s1.buf,s2.buf,MAX_SID)==0);
		}
	};
	struct hash_str_
	{ 
		size_t operator()(const SSessionRev & x) const { return hash_str(x.buf);} ;
	};
	typedef dense_hash_map<const SSessionRev,INT32,hash_str_,eq_str>	TSessionMapRev;

	TSessionMapRev					sessions_rev;
	/////////////////////////////

	void			AddSessionFromSQL(INT32,SSession);
	/////////////////////////////

	struct SStats
	{
		SStats()
		{
			registered=unregistered=0;
		}
		INT64						registered;
		INT64						unregistered;
	}stats;

	typedef hash_multimap<INT32, SPlayerInfo> TInstanceMap;
	TInstanceMap					instances;

	void            NotifyGlobalInfoManagerAboutInstance(INT32 message, SPlayerInfo & s_player);

private:

	GConsoleAccess					console;

	typedef hash_map<DWORD32, INT32>	TFailedLoginCountMap;
	TFailedLoginCountMap				failed_login_count;

	// Rzeczy zwiazane z pobieraniem listy przyjaciol z serwisu cache.
	typedef hash_map<INT32, pair<DWORD64, DWORD64> >		TSocialFriendsTimes;	// playerId -> <list_creation_time, expiration_time>

	TFriendsList					social_friends_map;
	TSocialFriendsTimes				social_friends_map_timestamp;

	GTicketInternalClient *			i_cache;

	vector<SAccessRegistrationTask>	tasks_registration;
};
#pragma warning(default: 4996)


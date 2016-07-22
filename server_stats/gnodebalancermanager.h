/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

class GConsoleNodeBalancer: public GConsoleBasic
{
public:
	GConsoleNodeBalancer():GConsoleBasic(){InitConsole();};
	//! inicjalizuje konsole
	void InitConsole()
	{
#define GCONSOLE(command, function,description)		console_commands.insert(make_pair(string(#command).erase(0,8),boost::bind(&GConsoleNodeBalancer::function,this,_1,_2)));
#include "tbl/gconsole_nodebalancer.tbl"
#undef GCONSOLE
#define GCONSOLE(command, function,description)		console_descriptions.push_back(make_pair(string(#command).erase(0,8),description));
#include "tbl/gconsole_nodebalancer.tbl"
#undef GCONSOLE
	}
#undef GCONSOLE
#define GCONSOLE(command, function,description)		void function(GSocketConsole *,char * buffer);
#include "tbl/gconsole_nodebalancer.tbl"
#undef GCONSOLE
	GRaportInterface &	GetRaportInterface();
};
/***************************************************************************/
struct SGameGroup
{
	SGameGroup()
	{
		Zero();
	}
	void Zero()
	{
		global_group_id = 0;
		host = 0;
		port = 0;
		group_id = 0;
		type = EGT_NONE;
		status = EEGS_STOPPED;
		last_beat = 0;
		host_str = "";
		port_str = "";
		host_console = "";
		port_console = "";
		host_external = "";
		ports_external = "";
	}
	SGameGroup(const SGameGroup & r)
	{
		operator=(r);
	}

	SGameGroup & operator=(const SGameGroup & r)
	{
		global_group_id = r.global_group_id;
		host = r.host;
		port = r.port;
		group_id = r.group_id;
		type = r.type;
		status = r.status;
		last_beat = r.last_beat;
		host_str = r.host_str;
		port_str = r.port_str;
		host_console = r.host_console;
		port_console = r.port_console;
		host_external = r.host_external;
		ports_external = r.ports_external;
		return *this;
	}

	DWORD64					global_group_id;					// global_group_id = [host, port, group_id]
	DWORD32					host;
	USHORT16				port;
	INT32					group_id;
	EGroupType				type;

	EEffectiveGroupStatus	status;
	DWORD64					last_beat;

	string					host_str;							// 'host' jako string
	string					port_str;							// 'port' jako string
	string					host_console;
	string					port_console;
	string					host_external;
	string					ports_external;
};
/***************************************************************************/
#ifndef LINUX
typedef hash_map<DWORD64, SGameGroup> TGameGroups;
#else
struct eq_DWORD64
{
	bool operator()(const DWORD64 s1, const DWORD64 s2) const
	{
		return (s1==s2);
	}
};
struct hash_DWORD64
{
	size_t operator()(const DWORD64 x) const { return x;} ;
};
typedef hash_map<DWORD64, SGameGroup, hash_DWORD64, eq_DWORD64>  TGameGroups;
#endif
/***************************************************************************/
class GNodeBalancerManager: public GServerManager
{
	friend class GConsoleNodeBalancer;
public:
	GNodeBalancerManager();
	virtual bool	Init();
	void			InitServices()
	{
		GServerManager::InitServices();
		service_manager.services.push_back(SSockService(ESTAcceptNodeBalancer,ESTAcceptNodeBalancerConsole));
	}
	void			ConfigureClock();
	bool 			ProcessInternalMessage(GSocket *,GNetMsg &,int);
	bool 			ProcessTargetMessage(GSocket *,GNetMsg &,int,GNetTarget &);
	void			ParseConsole(GSocketConsole *socket){console.ParseConsole(socket);};

private:
	void			ProcessGroupStatusUpdate(GSocket * socket, GNetMsg & msg);
	bool			ReleaseLocation(DWORD64 global_group_id, const char * location_ID = "");
	void			ClearReleasedLocations();

	// gnodebalancermanager_getgroups.cpp
	bool			ReadGroupsInfoFromSQL(TGameGroups & game_groups);
	void			GetDeprecatedGroups(TGameGroups & game_groups, set<DWORD64> & deprecated_group_ids);
	void			SetGroupsInfoReloadDeadline(DWORD64 deadline_seconds);
	void			GetGroupsInfoFromSQL();
	void			RaportCurrentStatus();

	// gnodebalancermanager_assigngroup.cpp
	DWORD64			PickReadyGroup(EGroupType group_type);
	bool			GetLocationAddress(string & location_ID, EServiceTypes connection_type, string & host, string & ports, INT32 & group_id);

private:
	GConsoleNodeBalancer	console;

	DWORD64					next_groups_info_reload_deadline;
	TGameGroups				game_groups;
	vector<DWORD64>			game_groups_ready[EGT_LAST];			// global_group_id grup danego typu w stanie ready
	set<DWORD64>			potentially_dead_game_groups;			// global_group_id grup do sprawdzenia, czy nie staly sie martwe i nie trzeba zwolnic ich lokacji
};
/***************************************************************************/

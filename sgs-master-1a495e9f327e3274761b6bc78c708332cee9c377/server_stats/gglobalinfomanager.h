/***************************************************************************
***************************************************************************
(c) 1999-2006 Ganymede Technologies                 All Rights Reserved
Krakow, Poland                                  www.ganymede.eu
***************************************************************************
***************************************************************************/

// Struktura SListenerInstance jednoznacznie identyfikuje instancje gracza podpietego do serwisu globalinfo
// poprzez pare <socketId, RID>.

struct SListenerInstance
{
	SListenerInstance()
	{
		socketId = 0;
		RID = 0;
	}
	SListenerInstance(INT32 socketId, INT32 RID)
	{
		this->socketId = socketId;
		this->RID = RID;
	}
	SListenerInstance(const SListenerInstance & r)
	{
		operator=(r);
	}

	SListenerInstance & operator=(const SListenerInstance & r)
	{
		socketId = r.socketId;
		RID = r.RID;
		return *this;
	}

	bool operator==(const SListenerInstance & r)
	{
		return (socketId == r.socketId && RID == r.RID);
	}
	
	INT32					socketId;
	INT32					RID;
};
/***************************************************************************/
enum ELeaderboardRequestType
{
	ELRT_None,
	ELRT_get,
	ELRT_get_standings,
	ELRT_get_batch_info,
	ELRT_get_user_position,
	ELRT_set,
};
/***************************************************************************/
struct SLeaderboardRequest
{
	SLeaderboardRequest()
	{
		type = ELRT_None;
		lb_key = "";
		lb_subkey = 0;
		standings_index = 0;
		max_results = 0;
		socketId = 0;
	}
	SLeaderboardRequest(const SLeaderboardRequest & r)
	{
		operator=(r);
	}

	SLeaderboardRequest & operator=(const SLeaderboardRequest & r)
	{
		type = r.type;
		target = r.target;
		socketId = r.socketId;
		lb_key = r.lb_key;
		lb_subkey = r.lb_subkey;
		standings_index = r.standings_index;
		max_results = r.max_results;
		users_id = r.users_id;
		scores = r.scores;
		return *this;
	}

	ELeaderboardRequestType	type;
	GNetTarget				target;
	INT32					socketId;

	string					lb_key;
	DWORD64					lb_subkey;
	INT32					standings_index;
	INT32					max_results;
	vector<INT32>			users_id;
	vector<INT32>			scores;						// uzywane przy IGMITIC_LEADERBOARD_SET
};
/***************************************************************************/
struct SPlayerListener
{
	SPlayerListener()
	{
		last_update_sent = 0;
		have_social_friends_data = false;
		expiration = 0;
	}
	SPlayerListener(const SPlayerListener & r)
	{
		operator=(r);
	}

	SPlayerListener & operator=(const SPlayerListener & r)
	{
		last_update_sent = r.last_update_sent;
		listeners = r.listeners;

		have_social_friends_data = r.have_social_friends_data;
		pending_online_social_friends_and_buddies = r.pending_online_social_friends_and_buddies;

		pending_leaderboard_requests = r.pending_leaderboard_requests;

		expiration = r.expiration;
		return *this;
	}

	DWORD64					last_update_sent;						// kiedy ostatnio wyslalismy do gracza update informacji o jego friendach i turniejach

	vector<SListenerInstance>	listeners;							// aktualnie podpiete instancje gracza

	bool						have_social_friends_data;								// czy dostalismy juz informacje o przyjaciolach gracza z sajtow socjalnych
	vector<pair<INT32, SPlayerInstance> >	pending_online_social_friends_and_buddies;	// instancje przyjaciol z sajtow socjalnych i buddies, ktore pojawily sie lub zniknely od czasu ostatniego updejtu
																						// .first = IGMI_REGISTER_INSTANCE | IGMI_UNREGISTER_INSTANCE

	vector<SLeaderboardRequest>	pending_leaderboard_requests;		// nie wyslane jeszcze zadania do serwisu leaderboard

	DWORD64					expiration;								// kiedy mozemy usunac z hashy informacje o przyjaciolach i turniejach gracza
};
/***************************************************************************/
#ifndef LINUX
typedef hash_map<INT64, SPlayerInstance>	TInstanceMap;
#else
struct eq_INT64
{
	bool operator()(const INT64 s1, const INT64 s2) const
	{
		return (s1==s2);
	}
};
struct hash_INT64
{
	size_t operator()(const INT64 x) const { return x;} ;
};
typedef hash_map<INT64, SPlayerInstance, hash_INT64, eq_INT64>	TInstanceMap;
#endif
typedef hash_multimap<INT32, INT64>			TPlayerInstanceMap;

typedef dense_hash_map<const INT32, SPlayerListener, hash_INT32, eq_INT32>		TListenerInstanceMap;

/***************************************************************************/

class GConsoleGlobalInfo: public GConsoleBasic
{
public:
	GConsoleGlobalInfo():GConsoleBasic(){InitConsole();};
	//! inicjalizuje konsole
	void InitConsole()
	{
#define GCONSOLE(command, function,description)		console_commands.insert(make_pair(string(#command).erase(0,8),boost::bind(&GConsoleGlobalInfo::function,this,_1,_2)));
#include "tbl/gconsole_globalinfo.tbl"
#undef GCONSOLE
#define GCONSOLE(command, function,description)		console_descriptions.push_back(make_pair(string(#command).erase(0,8),description));
#include "tbl/gconsole_globalinfo.tbl"
#undef GCONSOLE
	}
#undef GCONSOLE
#define GCONSOLE(command, function,description)		void function(GSocketConsole *,char * buffer);
#include "tbl/gconsole_globalinfo.tbl"
#undef GCONSOLE
	GRaportInterface &	GetRaportInterface();
};
/////////////////////////////
/***************************************************************************/

class GGlobalInfoManager: public GServerManager
{
	friend class GConsoleGlobalInfo;
public:
	GGlobalInfoManager();
	void			InitInternalClients();
	void			InitServices()
	{
		GServerManager::InitServices();
		service_manager.services.push_back(SSockService(ESTAcceptGlobalInfo,ESTAcceptGlobalInfoConsole));
	}
	bool			RegisterLogic(GSocket *);
	void			UnregisterLogic(GSocket *, EUSocketMode, bool);

	void			ConfigureClock();

	bool 			ProcessInternalMessage(GSocket *,GNetMsg &,int);
	bool 			ProcessTargetMessage(GSocket *,GNetMsg &,int,GNetTarget &);
	void			ProcessConnection(GNetMsg & msg);
	void			ParseConsole(GSocketConsole *socket){console.ParseConsole(socket);};

private:
	void			ClearAllInstances();
	void			ClearExpiredData();
	void			GetFriendsLastConnectionTime(INT32 playerId, const TFriendsList& friends_list);

	void			RegisterListener(GSocket * socket, SPlayerInfo & s_player);
	void			UnregisterListener(GSocket * socket, SPlayerInfo & s_player);
	void			UnregisterAllListeners(GSocket * socket);

	void			FillOnlineOfflineFriends(GNetMsg& msg, INT32 playerId, INT32& count, set<INT32>& online_friends, set<INT32>& offline_friends, const TFriendsList& friends_list);
	void			WriteOnlineFriends(INT32 playerId, GSocket * socket = NULL, INT32 RID = -1);
	void			WritePendingUpdate(INT32 playerId, SPlayerListener & s_player_listener);
	void			WritePendingUpdates();

	void			UpdateSocialFriends(INT32 playerId, GNetMsg & msg);
	void			UpdateOnlineFriend(INT32 message, SPlayerInstance & s_instance, const TFriendsList& friends_list);

	void			RegisterBuddies(const SRegisterBuddies& register_buddies);
	void			EraseFromFriendsLists(INT32 user_id, TFriendsList& friends_map, TFriendsList& friends_map_rev);

	void			ProcessPendingLeaderboardRequests(INT32 user_id = -1);


private:
	TInstanceMap			instances;
	TPlayerInstanceMap		player_instances;
	INT64					instance_id_seq;

	map<INT32, pair<DWORD64, DWORD64> >		game_maintenances;

	TListenerInstanceMap	listeners;

	TFriendsList				buddies_map;
	TFriendsList				buddies_map_rev;
	TFriendsList				social_friends_map;
	TFriendsList				social_friends_map_rev;
	hash_map<INT32, DWORD64>	social_friends_map_timestamp;	// czas utworzenia listy przyjaciol socjalnych danego gracza (wg serwisu cache)

	hash_map<INT32, DWORD64>	player_last_connection;	// czas ostatniego podlaczenia gracza do gry

public:
	GTicketInternalClient *			i_access;
	GTicketInternalClient *			i_cache;
	GTicketInternalClient *			i_leaderboard;

	GConsoleGlobalInfo		console;
};

/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

class GConsoleCache: public GConsoleBasic
{
public:
	GConsoleCache():GConsoleBasic(){InitConsole();};
	//! inicjalizuje konsole
	void InitConsole()
	{
#define GCONSOLE(command, function,description)		console_commands.insert(make_pair(string(#command).erase(0,8),boost::bind(&GConsoleCache::function,this,_1,_2)));
#include "tbl/gconsole_cache.tbl"
#undef GCONSOLE
#define GCONSOLE(command, function,description)		console_descriptions.push_back(make_pair(string(#command).erase(0,8),description));
#include "tbl/gconsole_cache.tbl"
#undef GCONSOLE
	}
#undef GCONSOLE
#define GCONSOLE(command, function,description)		void function(GSocketConsole *,char * buffer);
#include "tbl/gconsole_cache.tbl"
#undef GCONSOLE
	GRaportInterface &	GetRaportInterface();
};
/***************************************************************************/
class GCacheManager: public GServerManager
{
	friend class GConsoleCache;
public:
	GCacheManager();
	void			InitServices()
	{
		GServerManager::InitServices();
		service_manager.services.push_back(SSockService(ESTAcceptCache,ESTAcceptCacheConsole));
	}
	bool 			ProcessInternalMessage(GSocket *,GNetMsg &,int);
	bool 			ProcessTargetMessage(GSocket *,GNetMsg &,int,GNetTarget &);
	void			ParseConsole(GSocketConsole *socket){console.ParseConsole(socket);};
private:
	void			GetSocialFriendsResources(GNetMsg & msg, GNetTarget & target);
	void			GetSocialFriendsResourcesFromSQL(INT32 user_id);
	void			SendSocialFriendsResources(GNetMsg & msg, GNetTarget & target, DWORD64 updated);

	void			ReloadSocialFriends(INT32 user_id);
	void			EraseSocialFriends(INT32 user_id);

private:

	GConsoleCache		console;

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
	typedef pair<DWORD64,DWORD64> times;
	typedef hash_multimap<INT32,times> TFriendsDate;
	typedef hash_multimap<INT32,INT32> TFriendsList;
	typedef set<INT32>										TPlayerSocialAppFriendsList;
	typedef hash_map<INT32, TPlayerSocialAppFriendsList>	TSocialFriendsList;

	TFriendsDate			social_friends_date;
	TSocialFriendsList		social_friends_list;
};

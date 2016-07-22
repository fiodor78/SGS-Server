/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

typedef hash_map<INT32, pair<vector<INT32>, DWORD64> >		TPlayerScoresMap;			// user_id -> <scores, expiration_time>
typedef map<string, TPlayerScoresMap>						TLeaderboardsMap;			// "key_subkey" -> TPlayerScoresMap
/***************************************************************************/
class GConsoleLeaderBoard: public GConsoleBasic
{
public:
	GConsoleLeaderBoard():GConsoleBasic(){InitConsole();};
	//! inicjalizuje konsole
	void InitConsole()
	{
#define GCONSOLE(command, function,description)		console_commands.insert(make_pair(string(#command).erase(0,8),boost::bind(&GConsoleLeaderBoard::function,this,_1,_2)));
#include "tbl/gconsole_leaderboard.tbl"
#undef GCONSOLE
#define GCONSOLE(command, function,description)		console_descriptions.push_back(make_pair(string(#command).erase(0,8),description));
#include "tbl/gconsole_leaderboard.tbl"
#undef GCONSOLE
	}
#undef GCONSOLE
#define GCONSOLE(command, function,description)		void function(GSocketConsole *,char * buffer);
#include "tbl/gconsole_leaderboard.tbl"
#undef GCONSOLE
	GRaportInterface &	GetRaportInterface();
};
/***************************************************************************/
class GLeaderBoardManager: public GServerManager
{
	friend class GConsoleLeaderBoard;
public:
	GLeaderBoardManager();
	void			InitServices()
	{
		GServerManager::InitServices();
		service_manager.services.push_back(SSockService(ESTAcceptLeaderBoard,ESTAcceptLeaderBoardConsole));
	}
	void			ConfigureClock();

	bool 			ProcessInternalMessage(GSocket *,GNetMsg &,int);
	bool 			ProcessTargetMessage(GSocket *,GNetMsg &,int,GNetTarget &);
	void			ParseConsole(GSocketConsole *socket){console.ParseConsole(socket);};

private:
	void			LeaderBoardGet(string & lb_key, DWORD64 lb_subkey, vector<INT32> & users_id, vector<pair<INT32, INT32> > & result);
	void			LeaderBoardGetStandings(string & lb_key, DWORD64 lb_subkey, vector<INT32> & users_id, INT32 standings_index, vector<pair<INT32, INT32> > & result);
	void			LeaderBoardGetUserPosition(string & lb_key, DWORD64 lb_subkey, vector<INT32> & users_id, vector<INT32> & result);
	void			LeaderBoardGetGlobal(string & lb_key, DWORD64 lb_subkey, vector<pair<INT32, INT32> > & result);
	void			LeaderBoardGetStandingsGlobal(string & lb_key, DWORD64 lb_subkey, INT32 standings_index, INT32 max_results, vector<pair<INT32, INT32> > & result);
	void			LeaderBoardSet(string & lb_key, DWORD64 lb_subkey, INT32 user_id, vector<INT32> & scores);
	
	vector<INT32> *	GetLeaderBoardData(string & lb_key, DWORD64 lb_subkey, INT32 user_id);
	vector<INT32> *	GetLeaderBoardDataFromCache(string & lb_key, DWORD64 lb_subkey, INT32 user_id);
	void			PutLeaderBoardDataToCache(string & lb_key, DWORD64 lb_subkey, INT32 user_id, vector<INT32> & lb_data);
	void			ClearExpiredData();

private:
	GConsoleLeaderBoard		console;

	TLeaderboardsMap		lb_cache;
};
/***************************************************************************/

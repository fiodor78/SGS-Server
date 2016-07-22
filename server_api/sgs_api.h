/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#ifndef SGS_API_VERSION

#define SGS_API_VERSION		112

class ILogicUpCalls
{
public:
	/*
		wolane z poziomu logiki
		Funkcje sa public tylko po to, zeby dalo sie je wolac z zewnetrznych funkcji.
	 */
	virtual void			Log(const char * str) = 0;
	virtual void			SendMessage(int user_id, int gamestate_ID, const std::string & message, const std::string & params) = 0;
	virtual void			SendBroadcastMessage(int exclude_ID, int gamestate_ID, const std::string & message, const std::string & params) = 0;
	virtual void			SaveSnapshot(int gamestate_ID) = 0;
	virtual void			LoadSnapshot(int snapshot_ID, int gamestate_ID) = 0;
	virtual void			ReportScriptError(int user_id, int gamestate_ID, const string & description) = 0;
	virtual void			RequestLeaderboard(int user_id, const string & leaderboard_key, DWORD64 leaderboard_subkey) = 0;
	virtual void			RequestLeaderboardUserPosition(int user_id, const string & leaderboard_key, DWORD64 leaderboard_subkey) = 0;
	virtual void			RequestLeaderboardStandings(int user_id, const string & leaderboard_key, DWORD64 leaderboard_subkey, INT32 standings_index, INT32 max_results) = 0;
	virtual void			UpdateLeaderboard(int user_id, const string & leaderboard_key, DWORD64 leaderboard_subkey, vector<INT32> & scores) = 0;
	virtual void			RequestLeaderboardBatchInfo(int user_id, const string & leaderboard_key, DWORD64 leaderboard_subkey, INT32 standings_index, vector<INT32> & users_id) = 0;
	virtual void			SendMessageToGamestate(int gamestate_ID, const std::string & command, const std::string & params) = 0;
	virtual void			SubmitAnalyticsEvent(const char * Game_ID, DWORD64 Player_ID, const char * event_name_with_category, const std::map<string, string> & options) = 0;
	virtual void			RegisterBuddies(int user_id, const std::set<int> & buddies_ids) = 0;
	
	virtual void			DisconnectPlayer(int user_id, const char * location_ID, const char * reason) = 0;
	virtual void			SendMessageFromLocation(int user_id, const char * location_ID, const std::string & command, const std::string & params) = 0;
	virtual void			SendBroadcastMessageFromLocation(int exclude_ID, const char * location_ID, const std::string & command, const std::string &params) = 0;
	virtual void			SendMessageToLocation(const char * location_ID, const std::string & command, const std::string & params) = 0;
	virtual void			GetGloballyUniqueLocationSuffix(string & unique_location_suffix) = 0;
	virtual void			CreateLocation(const char * location_ID, const char * new_location_ID, const char * new_location_data) = 0;
	virtual void			ReleaseLocation(const char * location_ID, bool destroy_location) = 0;
	virtual void			FlushLocation(const char * location_ID) = 0;
	virtual void			ReplaceVulgarWords(const char * location_ID, const std::string & text, const std::string & params) = 0;
	virtual INT32			GetLocationsIdsLike(const char * location_ID, const std::string & pattern) = 0;
	virtual void			SendGlobalMessage(const std::string & message, const std::string & params) = 0;

	
	virtual INT32			PerformHTTPRequest(const char * location_ID, const char * url, const char * request_method = "GET", const std::map<string, string> * params = NULL, const char * secret_key = NULL) = 0;
	virtual INT32			PerformHTTPRequest(const char * location_ID, const char * url, const char * request_method, const string & payload, const string & content_type, const char * secret_key = NULL) = 0;
	virtual void			PerformFireAndForgetHTTPRequest(const char * url, const char * request_method = "GET", const std::map<string, string> * params = NULL, const char * secret_key = NULL) = 0;
	virtual void			PerformFireAndForgetHTTPRequest(const char * url, const char * request_method, const string & payload, const string & content_type, const char * secret_key = NULL) = 0;

	virtual INT32			GetGamestateIDFromLocationID(const char * location_ID) = 0;
	virtual std::string 	GetLocationIDFromGamestateID(INT32 gamestate_ID) = 0;

	virtual ~ILogicUpCalls() {}
};

/***************************************************************************/

class ILogicEngineBase
{
public:
	virtual bool			on_init_global_config(DWORD64 timestamp, const string & global_config) = 0;
	virtual void			on_cleanup() = 0;
	virtual void			on_print_statistics(strstream & s) {};

	virtual void			on_clock_tick(DWORD64 clock_msec) {};

	// Obsluga lokacji

	virtual bool			on_put_location(DWORD64 timestamp, const char * location_ID, const char * location_data) { return false; };
	virtual bool			on_get_location(const char * location_ID, string & location_data, bool server_closing, bool logic_reload) { return false; };
	virtual void			on_erase_location(const char * location_ID) {};

	virtual void			handle_user_location_message(DWORD64 timestamp, INT32 user_id, const char * location_ID,
												const std::string & message, const std::string & params) {};
	virtual void			handle_system_location_message(DWORD64 timestamp, const char *location_ID,
												const std::string & message, const std::string & params, std::string & output) {};
	virtual void			handle_location_message(DWORD64 timestamp, const char * location_ID,
												const std::string & message, const std::string & params) {};
	virtual void			handle_system_global_message(DWORD64 timestamp, const std::string & message,
												const std::string & params, std::string & output) {};
	virtual void			handle_global_message(DWORD64 timestamp, const std::string & message,
												const std::string & params) {};

	virtual void			on_connect_player_to_location(INT32 user_id, const char * location_ID) {};
	virtual void			on_disconnect_player_from_location(INT32 user_id, const char * location_ID) {};

	virtual void			on_create_location(const char * location_ID, const char * new_location_ID, const char * new_location_data, bool creation_success) {};
	virtual void			on_flush_location(const char * location_ID, const char * location_data, bool flush_success) {};
	virtual void			on_get_locations_ids_like(const char * location_ID, INT32 request_id, const std::set<std::string> & locations_ids) {};

	virtual void			on_initiate_gentle_close() {};

	virtual void			on_http_response(const char * location_ID, INT32 http_request_id, INT32 http_response_code, const std::string & http_response) {};
	virtual void			on_replace_vulgar_words(DWORD64 timestamp, const char * location_ID, const std::string & text, const std::string & params) {};

	virtual ~ILogicEngineBase() {}
};

/***************************************************************************/

class ILogicEngine : public ILogicEngineBase
{
public:
	virtual bool			on_put_gamestate(DWORD64 timestamp, INT32 gamestate_ID, const string & gamestate) = 0;
	virtual void			on_get_gamestate(INT32 gamestate_ID, string & gamestate, bool server_closing, bool logic_reload) = 0;
	virtual void			on_erase_gamestate(INT32 gamestate_ID) = 0;

	/*
		Obsluga polecen logiki.
	*/
	virtual void			handle_player_message(DWORD64 timestamp, INT32 user_id, INT32 gamestate_ID,
												const std::string & message, const std::string & params) {};
	virtual void			handle_friend_message(DWORD64 timestamp, INT32 user_id, INT32 gamestate_ID,
												const std::string & message, const std::string & params) {};
	virtual void			handle_stranger_message(DWORD64 timestamp, INT32 user_id, INT32 gamestate_ID,
												const std::string & message, const std::string & params) {};
	virtual void			handle_system_message(DWORD64 timestamp, INT32 gamestate_ID,
												const std::string & message, const std::string & params, std::string & output) {};
	virtual void			handle_gamestate_message(DWORD64 timestamp, INT32 gamestate_ID,
												const std::string & message, const std::string & params) {};

	virtual bool			on_connect_player(INT32 user_id, INT32 gamestate_ID) { return false; };
	virtual void			on_disconnect_player(INT32 user_id, INT32 gamestate_ID) {};
};

/***************************************************************************/
#endif

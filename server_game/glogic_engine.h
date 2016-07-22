/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#pragma warning(disable: 4511)
#pragma warning(disable: 4355)
#include <shared_ptr.h>
extern "C" {
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"
}

INT32	gamestate_ID_from_location_ID(const char * location_ID);
string	location_ID_from_gamestate_ID(INT32 gamestate_ID);

/***************************************************************************/
enum ELocationDBTaskDirection
{
	ELDBTD_None,
	ELDBTD_Read,
	ELDBTD_Read_Locations_Ids,
	ELDBTD_Write,
	ELDBTD_Insert,														// Wymuszenie INSERT INTO (i fail jesli wiersz juz istnieje)
	ELDBTD_Delete,
};
/***************************************************************************/
enum ELocationDBTaskStatus
{
	ELDBTS_None,
	ELDBTS_OK,
	ELDBTS_ExecutionDeadline,											// przekroczony zostal 'execution_deadline' - nie bylo nawet proby operacji read/write
	ELDBTS_WriteDeadlineReached,										// przekroczony zostal 'write_locations_deadline' - nie bylo nawet proby operacji write
	ELDBTS_ReadFailed,
	ELDBTS_WriteFailed,
	ELDBTS_WriteFailedLogicFailure,										// blad podczas wykonania funkcji GetLocationFromScript()
};
/***************************************************************************/
struct SLocationDBTask
{
	string						location_ID;
	INT32						group_id;
	ELocationDBTaskDirection	direction;
	DWORD64						execution_deadline;						// do kiedy zezwalamy na wrzucenie taska do watku wykonujacego go
	INT32						taskid;
	bool						has_priority;
	bool						task_executed;							// czy podjeta zostala choc jedna proba wykonania taska
	string						location_data;
};
/***************************************************************************/
struct SLocationDBTaskResult
{
	string						location_ID;
	ELocationDBTaskDirection	direction;
	INT32						taskid;
	ELocationDBTaskStatus		status;
	string						location_data;
};
/***************************************************************************/
struct SLocationDBTasks
{
	SLocationDBTasks()
	{
		Zero();
	}
	void Zero()
	{
		tasks_read.clear();
		read_execution_deadline = 0;
		priority_read_execution_deadline = 0;
		read_task_running_taskid = 0;
		last_read_operation_success = 0;
		last_read_operation_fail = 0;
		last_read_fails_count = 0;

		tasks_write.clear();
		write_execution_deadline = 0;
		priority_write_execution_deadline = 0;
		write_task_running_taskid = 0;
		last_write_operation_success = 0;
		last_write_operation_fail = 0;
		last_write_fails_count = 0;

		random_interval_change = 0;
	}

	// tasks read
	vector<SLocationDBTask>		tasks_read;
	DWORD64						read_execution_deadline;				// min. z execution_deadline tasks_read
	DWORD64						priority_read_execution_deadline;		// min. z execution_deadline tasks_read, dla taskow z has_priority = true
	INT32						read_task_running_taskid;

	DWORD64						last_read_operation_success;
	DWORD64						last_read_operation_fail;
	INT32						last_read_fails_count;

	// tasks write
	vector<SLocationDBTask>		tasks_write;
	DWORD64						write_execution_deadline;				// min. z execution_deadline tasks_write
	DWORD64						priority_write_execution_deadline;		// min. z execution_deadline tasks_write, dla taskow z has_priority = true
	INT32						write_task_running_taskid;

	DWORD64						last_write_operation_success;
	DWORD64						last_write_operation_fail;
	INT32						last_write_fails_count;

	INT32						random_interval_change;					// procentowo o ile modyfikujemy odstepy miedzy kolejnymi udanymi/nieudanymi operacjami,
																		// zeby unikac peakow zapytan wykonywanych w jednym momencie
};
typedef map<string, SLocationDBTasks>				TLocationDBTasksMap;		// location_ID -> tasks
typedef map<INT32, vector<SLocationDBTaskResult> >	TLocationDBTasksResultsMap;	// group_id -> task results
/***************************************************************************/
enum ELocationTriggerAction
{
	ELTA_None,
	ELTA_Read_ProcessSystemMessage,
	ELTA_Read_ProcessLogicMessage,
	ELTA_Read_ProcessLocationMessage,
	ELTA_Read_ConnectToLocation,
	ELTA_Read_DisconnectFromLocation,
	ELTA_Read_LocationsIdsLikeFromDB,
	ELTA_Write_ProcessSystemMessage,
	ELTA_Write_WriteChangedGameStateToDatabase,
	ELTA_Write_GroupShutdown,
	ELTA_Write_EraseOldGameState,
	ELTA_Write_CreateLocation,
	ELTA_Write_FlushLocation,
	ELTA_Write_DeleteLocation,
};
/***************************************************************************/
struct SLocationDBTaskTrigger
{
	SLocationDBTaskTrigger()
	{
		Zero();
	}
	void Zero()
	{
		action = ELTA_None;
		location_ID = "";
		command = "";
		params = "";
		flush = false;
		socketid = 0;
		user_id = 0;
		call_ctx = 0;
	}

	ELocationTriggerAction		action;
	string						location_ID;
	string						command;
	string						params;
	bool						flush;
	INT32						socketid;
	INT32						user_id;
	SMetricStats::SCallContext	call_ctx;
};
typedef hash_map<INT32, SLocationDBTaskTrigger>	TLocationDBTaskTriggersMap;	// taskid -> trigger
/***************************************************************************/
struct SLocationStatus
{
	SLocationStatus()
	{
		last_access = 0;
		last_write = 0;
		dirty = false;
		location_error_waiting_for_release = false;
		location_released_by_logic = false;
		locationdb_write_tasks.clear();
		last_confirmed_location_MD5 = "";
		location_ready_to_gently_close = false;
	}

	// Ponizsze pola (last_access, last_write, dirty) maja znaczenie jedynie dla lokacji odpowiadajacych gamestatom.

	//last access uzywamy do okreslenia ktore gamestate-y mozna wyrzucic bo nic ich nie uzywa (po przekroczeniu caching period)
	DWORD64					last_access;
	//uzywany do okreslenia kiedy ostatnio zapisalismy dany gamestate do bazy, inicjowany w chwili ustawienia dirty na true
	//(zawsze zapisujemy z opoznieniem)
	DWORD64					last_write;
	bool					dirty;
	
	bool					location_error_waiting_for_release;		// jak tylko wykonaja sie wszystkie locationdb_write_tasks wywolamy ReleaseLocation()
	bool					location_released_by_logic;				// modul logiki wykonal funkcje ReleaseLocation()
	set<INT32>				locationdb_write_tasks;					// taskid
	string					last_confirmed_location_MD5;			// suma kontrolna z ostatnio odczytanej/zapisanej zawartosci lokacji
	bool					location_ready_to_gently_close;			// zapisana do bazy lokacja czeka na release z powodu zamkniecia serwera
};
/***************************************************************************/
//action that is temporarily stored in commit log, and in case of error flushed to database
struct SDebugAction
{
	INT64			timestamp;
	INT32			source_id;
	INT32			access;
	std::string		message_id;
	std::string		params;

	SDebugAction()
	{
	}

	SDebugAction(const SDebugAction &da)
	{
		operator=(da);
	}

	SDebugAction& operator=(const SDebugAction &da)
	{
		timestamp=da.timestamp;
		source_id=da.source_id;
		access=da.access;
		message_id=da.message_id;
		params=da.params;
		return *this;
	}
};
/***************************************************************************/
//set of action + location_data that these actions are based on
struct SDebugContext
{
	std::string					location_data;
	std::vector<SDebugAction>	actions;

	SDebugContext()
	{
	}

	SDebugContext(const SDebugContext &dc)
	{
		operator=(dc);
	}

	SDebugContext& operator=(const SDebugContext &dc)
	{
		location_data = dc.location_data;
		actions=dc.actions;
		return *this;
	}

};
/***************************************************************************/
class GLogicEngine : public ILogicUpCalls
{
public:
	GLogicEngine();
	void					Init(GGroup *, GSimpleRaportManager *);
	void					Destroy();
	virtual void			InitLogicLibrary() = 0;
	virtual void			DestroyLogicLibrary() = 0;

	void					WriteChangedGameStatesToDatabase();
	void					WriteChangedLocationsToDatabaseGroupShutdown();
	void					EraseOldLocations(bool force = false);
	INT32					GetLocationsCount() { return status_map.size(); };
	bool					IsLocationLoaded(const char * location_ID);
	INT32					GetLocationsDBTriggersCount() { return locationdb_triggers.size(); };

	bool					MoveRandomLocationTo(GLogicEngine * target_engine);

	void					ExecuteLocationDBTriggerRead(INT32 taskid, ELocationDBTaskStatus status, const string & location_data);
	void					ExecuteLocationDBTriggerWrite(INT32 taskid, ELocationDBTaskStatus status, const string & location_data);

	/*
		Obsluga komend konsoli i innych komunikatow pochodzacych z zewnatrz.
	*/
	void					InitLogic();
	void					DestroyLogic();
	bool					ProcessSystemMessage(strstream & s, const char * location_ID, const std::string & command, const std::string & params, bool flush, INT32 socketid);
	void					ProcessLocationMessage(const char * location_ID, const std::string & command, const std::string & params);
	void					ProcessGlobalMessage(const std::string & command, const std::string & params);
	void					ProcessSystemGlobalMessage(strstream & s, const std::string & command, const std::string & params);
	void					PrintStatistics(strstream & s);
	void					ClearStatistics(strstream & s);

	virtual void			HandleClockTick(DWORD64 clock_msec) {};

	virtual void			HandlePrintStatistics(strstream & s) {};
	virtual bool			HandleSystemMessage(strstream & s, const char * location_ID, const std::string & command, const std::string & params) { return true; };
	virtual bool			HandleLocationMessage(const char * location_ID, const std::string & command, const std::string & params) { return true; };
	virtual bool			HandleGlobalMessage(const std::string & command, const std::string & params) { return true; };
	virtual bool			HandleSystemGlobalMessage(strstream & s, const std::string & command, const std::string & params) { return true; };

	virtual void			HandleHTTPResponse(const char * location_ID, INT32 http_request_id, INT32 http_response_code, const std::string & http_response) {};

	/*
		Obsluga komunikatow od gracza: bridge do logiki wolany z c++
	 */
	void					ProcessLogicMessage(GPlayerGame * player, const std::string & command, const std::string & params);

	void					ProcessConnectToLocation(INT32 user_id, const char * location_ID);
	void					ProcessDisconnectFromLocation(INT32 user_id, const char * location_ID);
	void					ProcessGetLocationsIdsLike(const char * location_ID, INT32 request_id, const std::string& location_ids);

	virtual bool			HandlePlayerAdd(INT32 user_id, const char * location_ID) { return false; };
	virtual void			HandlePlayerDelete(INT32 user_id, const char * location_ID) {};
	virtual bool			HandleLogicMessage(INT32 user_id, const char * location_ID, INT32 access, DWORD64 msg_time, const std::string & command, const std::string & params) { return true; };
	virtual void			HandleReplaceVulgarWords(const char * location_ID, const std::string & text, const std::string & params) {};
	virtual void			HandleGetLocationsIdsLike(const char * location_ID, INT32 request_id, const std::set<std::string> & locations_ids){};

	GSimpleRaportManager *	raport_manager;

public:
	/*
		wolane z poziomu logiki
		Funkcje sa public tylko po to, zeby dalo sie je wolac z zewnetrznych funkcji.
	 */
	void					Log(const char *str);
	void					SendMessage(int user_id,int gamestate_ID,const std::string &message,const std::string &params);
	void					SendBroadcastMessage(int exclude_ID,int gamestate_ID,const std::string &message,const std::string &params);
	void					SaveSnapshot(int gamestate_ID);
	void					LoadSnapshot(int snapshot_ID,int gamestate_ID);
	void					ReportScriptError(int user_id,int gamestate_ID,const string &description);
	void					RequestLeaderboard(int user_id, const string & leaderboard_key, DWORD64 leaderboard_subkey);
	void					RequestLeaderboardUserPosition(int user_id, const string & leaderboard_key, DWORD64 leaderboard_subkey);
	void					RequestLeaderboardStandings(int user_id, const string & leaderboard_key, DWORD64 leaderboard_subkey, INT32 standings_index, INT32 max_results);
	void					RequestLeaderboardBatchInfo(int user_id, const string & leaderboard_key, DWORD64 leaderboard_subkey, INT32 standings_index, vector<INT32> & users_id);
	void					UpdateLeaderboard(int user_id, const string & leaderboard_key, DWORD64 leaderboard_subkey, vector<INT32> & scores);
	void					SendMessageToGamestate(int gamestate_ID, const std::string & command, const std::string & params);
	void					SubmitAnalyticsEvent(const char * Game_ID, DWORD64 Player_ID, const char * event_name_with_category, const std::map<string, string> & options);
	void					RegisterBuddies(int user_id, const std::set<int> & buddies_ids);

	void					SendMessageFromLocation(int user_id, const char * location_ID, const std::string &message, const std::string &params);
	void					SendBroadcastMessageFromLocation(int exclude_ID, const char * location_ID, const std::string &message, const std::string &params);
	void					SaveLocationSnapshot(const char * location_ID);
	void					LoadLocationSnapshot(int snapshot_ID, const char * location_ID);
	void					SendMessageToLocation(const char * location_ID, const std::string & command, const std::string & params);
	void					ReportScriptErrorLocation(int user_id, const char * location_ID, const string & description);

	void					DisconnectPlayer(int user_id, const char * location_ID, const char * reason);
	void					GetGloballyUniqueLocationSuffix(string & unique_location_suffix);
	void					CreateLocation(const char * location_ID, const char * new_location_ID, const char * new_location_data);
	void					ReleaseLocation(const char * location_ID, bool destroy_location);
	void					FlushLocation(const char * location_ID);
	void					ReplaceVulgarWords(const char* location_ID, const std::string & text, const std::string & params);
	INT32					GetLocationsIdsLike(const char * location_ID, const std::string & pattern);
	void					SendGlobalMessage(const std::string &message, const std::string &params);
	
	INT32					PerformHTTPRequest(const char * location_ID, const char * url, const char * request_method = "GET", const std::map<string, string> * params = NULL, const char * secret_key = NULL);
	INT32					PerformHTTPRequest(const char * location_ID, const char * url, const char * request_method, const string & payload, const string & content_type, const char * secret_key = NULL);
	void					PerformFireAndForgetHTTPRequest(const char * url, const char * request_method = "GET", const std::map<string, string> * params = NULL, const char * secret_key = NULL);
	void					PerformFireAndForgetHTTPRequest(const char * url, const char * request_method, const string & payload, const string & content_type, const char * secret_key = NULL);

	INT32					GetGamestateIDFromLocationID(const char * location_ID);
	std::string				GetLocationIDFromGamestateID(INT32 gamestate_ID);

protected:
	DWORD64					CurrentSTime();
	GGroup *				group;

public:
	ELogicEngineStatus		status;

private:
	unsigned int						write_delay;
	unsigned int						cache_period;
	bool								gentle_close_initiated;

	std::map<string, SLocationStatus>	status_map;						// lokacje wczytane do pamieci
	std::map<string, SDebugContext>		debug_data;
	DWORD64								last_error_report;				//report rate limiter

private:
	TLocationDBTaskTriggersMap			locationdb_triggers;

	void					GetConfig(std::string & out_str);

	INT32					AddLocationDBTaskRead(ELocationTriggerAction action, const char * location_ID, const string & command, const string & params, bool flush = false, INT32 socketid = -1, INT32 user_id = -1);
	void					AddLocationDBTaskWrite(ELocationTriggerAction action, const char * location_ID, const string & params = "", INT32 socketid = -1, const string & extra_data = "");
	void					DisconnectPlayers(const char * location_ID, const char * reason = "");

	bool					InitScript(const std::string & config_str);
	bool					PutLocationToScript(const char * location_ID, const std::string & location_data);
	bool					GetLocationFromScript(const char * location_ID, std::string & out, bool server_closing, bool logic_reload);
	void					EraseLocation(const char * location_ID);

	virtual bool			HandleInitScript(const std::string & config_str) { return true; };
	virtual bool			HandlePutLocationToScript(const char * location_ID, const std::string & location_data) { return true; };
	virtual bool			HandleGetLocationFromScript(const char * location_ID, std::string & out, bool server_closing, bool logic_reload) { return true; };
	virtual void			HandleEraseLocation(const char * location_ID) {};
	virtual void			HandleCreateLocationNotification(const char * location_ID, const char * new_location_ID, const char * new_location_data, bool creation_success) {};
	virtual void			HandleFlushLocationNotification(const char * location_ID, const char * location_data, bool flush_success) {};
	virtual void			HandleInitiateGentleClose() {};
};
/***************************************************************************/
class GLogicEngineLUA : public GLogicEngine
{
public:
	GLogicEngineLUA();
	void					InitLogicLibrary();
	void					DestroyLogicLibrary();

	/*
		Obsluga komend konsoli i innych komunikatow pochodzacych z zewnatrz.
	*/
	void					HandlePrintStatistics(strstream & s);
	bool					HandleSystemMessage(strstream & s, const char * location_ID, const std::string & command, const std::string & params);
	bool					HandleLocationMessage(const char * location_ID, const std::string & command, const std::string & params);

	/*
		Obsluga komunikatow od gracza: bridge do lua wolany z c++
	 */
	bool					HandleLogicMessage(INT32 user_id, const char * location_ID, INT32 access, DWORD64 msg_time, const std::string & command, const std::string & params);

protected:
	bool					HandleInitScript(const std::string & config_str);
	bool					HandlePutLocationToScript(const char * location_ID, const std::string & location_data);
	bool					HandleGetLocationFromScript(const char * location_ID, std::string & out, bool server_closing, bool logic_reload);
	void					HandleEraseLocation(const char * location_ID);

private:
	lua_State				*L;
	unsigned int			max_script_operations;
};
/***************************************************************************/
class GLogicEngineCPP : public GLogicEngine
{
public:
	GLogicEngineCPP();
	void					InitLogicLibrary();
	void					DestroyLogicLibrary();

	/*
		Obsluga komend konsoli i innych komunikatow pochodzacych z zewnatrz.
	*/
	void					HandleClockTick(DWORD64 clock_msec);

	void					HandlePrintStatistics(strstream & s);
	bool					HandleSystemMessage(strstream & s, const char * location_ID, const std::string & command, const std::string & params);
	bool					HandleLocationMessage(const char * location_ID, const std::string & command, const std::string & params);
	bool					HandleGlobalMessage(const std::string & command, const std::string & params);
	bool					HandleSystemGlobalMessage(strstream & s, const std::string & command, const std::string & params);


	void					HandleHTTPResponse(const char * location_ID, INT32 http_request_id, INT32 http_response_code, const std::string & http_response);

	/*
		Obsluga komunikatow od gracza.
	 */
	bool					HandlePlayerAdd(INT32 user_id, const char * location_ID);
	void					HandlePlayerDelete(INT32 user_id, const char * location_ID);
	bool					HandleLogicMessage(INT32 user_id, const char * location_ID, INT32 access, DWORD64 msg_time, const std::string & command, const std::string & params);

private:
	bool					HandleInitScript(const std::string & config_str);
	bool					HandlePutLocationToScript(const char * location_ID, const std::string & location_data);
	bool					HandleGetLocationFromScript(const char * location_ID, std::string & out, bool server_closing, bool logic_reload);
	void					HandleEraseLocation(const char * location_ID);

	void					HandleCreateLocationNotification(const char * location_ID, const char * new_location_ID, const char * new_location_data, bool creation_success);
	void					HandleFlushLocationNotification(const char * location_ID, const char * location_data, bool flush_success);
	void					HandleInitiateGentleClose();
	void					HandleReplaceVulgarWords(const char * location_ID, const std::string & text, const std::string & params);
	void					HandleGetLocationsIdsLike(const char * location_ID, INT32 request_id, const std::set<std::string> & locations_ids);

	void					LogException(const char * msg, ...);

private:
	string					logic_library_copy_path;
	void *					logic_library_handle;
	class ILogicEngine *	(* create_logic)(class ILogicUpCalls * upcalls);
	void					(* destroy_logic)(class ILogicEngine * logic);
	class ILogicEngine *	logic_base;
};
/***************************************************************************/

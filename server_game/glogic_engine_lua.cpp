/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"
#include "headers_game.h"
#include "utils_conversion.h"
#include "lua/ldebug.h"
#include "zlib.h"

#ifdef LINUX
    #ifndef __x86_64__
	    #define __cdecl __attribute__((cdecl))
	#else
		#define __cdecl
	#endif
	#define _putenv putenv
#endif

extern "C"
{
LUALIB_API int (luaopen_cjson) (lua_State *L);
LUALIB_API int (luaopen_getsize) (lua_State *L);
};

/*
 *  Lua proxy
 */

static int __cdecl lua_error_handler(lua_State *L)
{
	lua_getfield(L, LUA_GLOBALSINDEX, "debug");
	if(!lua_istable(L,-1))
	{
		lua_pop(L,1);
		return 1;
	}
	lua_getfield(L, -1, "traceback");
	if(!lua_isfunction(L,-1))
	{
		lua_pop(L,2);
		return 1;
	}
	lua_pushvalue(L,1);
	lua_pushinteger(L,2);
	lua_call(L,2,1);
	return 1;
}

static void __cdecl lua_hook(lua_State *L, lua_Debug *ar)
{
	if(ar->event == LUA_HOOKCOUNT)
	{
		lua_pushstring(L, "Max script operations exceeded");
		lua_error(L);
	}
}

static int __cdecl lua_proxy_ganymede_log(lua_State *L)
{
	const char *str=luaL_checkstring(L,-1);
	if(!str)
	{
		return 0;
	}
	lua_getfield(L,LUA_REGISTRYINDEX,"_SELF");
	GLogicEngineLUA **g=(GLogicEngineLUA**)lua_touserdata(L,-1);
	lua_pop(L,1);
	(*g)->Log(str);
	return 0;
}

static int __cdecl lua_proxy_ganymede_send_message(lua_State *L)
{
	int user_id=luaL_checkint(L,-4);
	int gamestate_ID=luaL_checkint(L,-3);
	string message=luaL_checkstring(L,-2);
	string params=luaL_checkstring(L,-1);
	lua_getfield(L,LUA_REGISTRYINDEX,"_SELF");
	GLogicEngineLUA **g=(GLogicEngineLUA**)lua_touserdata(L,-1);
	lua_pop(L,1);
	(*g)->SendMessage(user_id,gamestate_ID,message,params);
	return 0;
}

static int __cdecl lua_proxy_ganymede_send_broadcast_message(lua_State *L)
{
	int exclude_ID=luaL_checkint(L,-4);
	int gamestate_ID=luaL_checkint(L,-3);
	string message=luaL_checkstring(L,-2);
	string params=luaL_checkstring(L,-1);
	lua_getfield(L,LUA_REGISTRYINDEX,"_SELF");
	GLogicEngineLUA **g=(GLogicEngineLUA**)lua_touserdata(L,-1);
	lua_pop(L,1);
	(*g)->SendBroadcastMessage(exclude_ID,gamestate_ID,message,params);
	return 0;
}

static int __cdecl lua_proxy_ganymede_report_error(lua_State *L)
{
	int user_id=luaL_checkint(L,-3);
	int gamestate_ID=luaL_checkint(L,-2);
	string desc=luaL_checkstring(L,-1);
	lua_getfield(L,LUA_REGISTRYINDEX,"_SELF");
	GLogicEngineLUA **g=(GLogicEngineLUA**)lua_touserdata(L,-1);
	lua_pop(L,1);
	(*g)->ReportScriptError(user_id,gamestate_ID,desc);
	return 0;
}

static int __cdecl lua_proxy_request_leaderboard(lua_State * L)
{
	int user_id = luaL_checkint(L, -3);
	string leaderboard_key = luaL_checkstring(L, -2);
	DWORD64 leaderboard_subkey = (DWORD64)luaL_checklong(L, -1);
	lua_getfield(L, LUA_REGISTRYINDEX, "_SELF");
	GLogicEngineLUA ** g = (GLogicEngineLUA**)lua_touserdata(L, -1);
	lua_pop(L, 1);
	(*g)->RequestLeaderboard(user_id, leaderboard_key, leaderboard_subkey);
	return 0;
}

static int __cdecl lua_proxy_request_leaderboard_standings(lua_State * L)
{
	int user_id = luaL_checkint(L, -5);
	string leaderboard_key = luaL_checkstring(L, -4);
	DWORD64 leaderboard_subkey = (DWORD64)luaL_checklong(L, -3);
	int standings_index = luaL_checkint(L, -2);
	int max_results = luaL_checkint(L, -1);
	lua_getfield(L, LUA_REGISTRYINDEX, "_SELF");
	GLogicEngineLUA ** g = (GLogicEngineLUA**)lua_touserdata(L, -1);
	lua_pop(L, 1);
	(*g)->RequestLeaderboardStandings(user_id, leaderboard_key, leaderboard_subkey, standings_index, max_results);
	return 0;
}

static int __cdecl lua_proxy_update_leaderboard(lua_State * L)
{
	int user_id = luaL_checkint(L, -4);
	string leaderboard_key = luaL_checkstring(L, -3);
	DWORD64 leaderboard_subkey = (DWORD64)luaL_checklong(L, -2);

	luaL_checktype(L, -1, LUA_TTABLE);
	int a, scores_size = luaL_getn(L, -1);
	vector<INT32> scores;
	scores.resize(scores_size);

	for (a = 0; a < scores_size; a++)
	{
		lua_pushinteger(L, a + 1);
		lua_gettable(L, -2);
		scores[a] = lua_isnumber(L, -1) ? lua_tointeger(L, -1) : 0;
		lua_pop(L, 1);
	}

	lua_getfield(L, LUA_REGISTRYINDEX, "_SELF");
	GLogicEngineLUA ** g = (GLogicEngineLUA**)lua_touserdata(L, -1);
	lua_pop(L, 1);
	(*g)->UpdateLeaderboard(user_id, leaderboard_key, leaderboard_subkey, scores);
	return 0;
}

static int __cdecl lua_proxy_save_snapshot(lua_State *L)
{
	int gamestate_ID=luaL_checkint(L,-1);
	lua_getfield(L,LUA_REGISTRYINDEX,"_SELF");
	GLogicEngineLUA **g=(GLogicEngineLUA**)lua_touserdata(L,-1);
	lua_pop(L,1);
	std::string config_str;
	(*g)->SaveSnapshot(gamestate_ID);
	return 0;
}

static int __cdecl lua_proxy_load_snapshot(lua_State *L)
{
	int snapshot_ID=luaL_checkint(L,-2);
	int gamestate_ID=luaL_checkint(L,-1);
	lua_getfield(L,LUA_REGISTRYINDEX,"_SELF");
	GLogicEngineLUA **g=(GLogicEngineLUA**)lua_touserdata(L,-1);
	lua_pop(L,1);
	std::string config_str;
	(*g)->LoadSnapshot(snapshot_ID,gamestate_ID);
	return 0;
}

static int __cdecl lua_proxy_ganymede_send_message_to_gamestate(lua_State *L)
{
	int gamestate_ID=luaL_checkint(L,-3);
	string message=luaL_checkstring(L,-2);
	string params=luaL_checkstring(L,-1);
	lua_getfield(L,LUA_REGISTRYINDEX,"_SELF");
	GLogicEngineLUA **g=(GLogicEngineLUA**)lua_touserdata(L,-1);
	lua_pop(L,1);
	(*g)->SendMessageToGamestate(gamestate_ID,message,params);
	return 0;
}

static int __cdecl lua_proxy_get_high_resolution_clock(lua_State *L)
{
	double result = 0.0f;

#ifndef LINUX
	LARGE_INTEGER li, freq;
	if (QueryPerformanceFrequency(&freq))
	{
		QueryPerformanceCounter(&li);
		result = (double)(li.QuadPart * 1000.0) / double(freq.QuadPart);
	}
#else
	struct timespec ts;
	if (clock_gettime(CLOCK_REALTIME, &ts) != -1)
	{
		result = (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
	}
#endif
	lua_pushnumber(L, result);
	return 1;
}

static const struct luaL_Reg ganymede_lib[]={
	{"log",lua_proxy_ganymede_log},
	{"send_message",lua_proxy_ganymede_send_message},
	{"send_broadcast_message",lua_proxy_ganymede_send_broadcast_message},
	{"save_snapshot",lua_proxy_save_snapshot},
	{"load_snapshot",lua_proxy_load_snapshot},
	{"report_error",lua_proxy_ganymede_report_error},
	{"request_leaderboard", lua_proxy_request_leaderboard },
	{"request_leaderboard_standings", lua_proxy_request_leaderboard_standings },
	{"update_leaderboard", lua_proxy_update_leaderboard },
	{"send_message_to_gamestate",lua_proxy_ganymede_send_message_to_gamestate},
	{"get_high_resolution_clock",lua_proxy_get_high_resolution_clock},
	{NULL,NULL}
};

static int count_limited_script_call(lua_State *L, int nargs, int nresults, int errfunc) {
	resethookcount(L);
	return lua_pcall(L,nargs,nresults,errfunc);
}

/*
 *  GLogicEngineLUA
 */
/***************************************************************************/
GLogicEngineLUA::GLogicEngineLUA() :
GLogicEngine(),
L(NULL),
max_script_operations(DEFAULT_MAX_SCRIPT_OPERATIONS)
{
}
/***************************************************************************/
void GLogicEngineLUA::InitLogicLibrary()
{
	max_script_operations=global_serverconfig->scripting_config.Lua.max_script_operations;

	if(L)
	{
		return;
	}
	std::string main_dir=global_serverconfig->scripting_config.Lua.script_path;
	std::string lua_env="LUA_PATH=";
	lua_env+=main_dir;
	lua_env+="?.lua;;";
	_putenv(const_cast<char*>(lua_env.c_str()));
	L=luaL_newstate();
	luaL_openlibs(L);

	lua_pushcfunction(L, luaopen_cjson);
	lua_pushstring(L, "cjson");
	lua_call(L, 1, 0);

	lua_pushcfunction(L, luaopen_getsize);
	lua_pushstring(L, "getsize");
	lua_call(L, 1, 0);

	luaL_register(L,"gserver",ganymede_lib);
	GLogicEngineLUA **userdata=(GLogicEngineLUA**)lua_newuserdata(L,sizeof(GLogicEngineLUA*));
	lua_setfield(L,LUA_REGISTRYINDEX,"_SELF");
	*userdata=this;
	std::string main_script=main_dir;
	main_script+="main.lua";
	if(luaL_loadfile(L,main_script.c_str()))
	{
		string err_str=str(boost::format("Error loading file: %1%") % lua_tostring(L,-1));
		Log(err_str.c_str());
		lua_pop(L,1);
		return;
	}
	lua_sethook(L,lua_hook,LUA_MASKCOUNT,max_script_operations);
	if(count_limited_script_call(L,0,0,0))
	{
		string err_str=str(boost::format("Error running file: %1%") % lua_tostring(L,-1));
		Log(err_str.c_str());
		lua_pop(L,1);
		group->metrics.CommitEvent("logic_lua_script_call_errors");
		return;
	}
}
/***************************************************************************/
void GLogicEngineLUA::DestroyLogicLibrary()
{
	lua_close(L);
	L=NULL;
}
/***************************************************************************/
bool GLogicEngineLUA::HandleInitScript(const std::string &config_str)
{
	lua_pushcfunction(L,lua_error_handler);
	lua_getglobal(L,"gscript_init");
	if(config_str.size())
	{
		lua_pushstring(L,config_str.c_str());
	}
	else
	{
		lua_pushnil(L);
	}
	if(count_limited_script_call(L,1,0,-3))
	{
		string err_str=str(boost::format("Error calling gscript_init: %1%") % lua_tostring(L,-1));
		Log(err_str.c_str());
		lua_pop(L,1);
		group->metrics.CommitEvent("logic_lua_script_call_errors");
		return false;
	}
	return true;
}
/***************************************************************************/
bool GLogicEngineLUA::HandlePutLocationToScript(const char * location_ID, const std::string& location_data)
{
	INT32 gamestate_ID = gamestate_ID_from_location_ID(location_ID);
	if (gamestate_ID == 0)
	{
		return true;
	}

	//push data to Lua VM
	lua_pushcfunction(L,lua_error_handler);
	lua_getglobal(L,"gscript_put_gamestate");
	lua_pushnumber(L,(lua_Number)CurrentSTime());
	lua_pushinteger(L,gamestate_ID);
	lua_pushstring(L,location_data.c_str());
	if(count_limited_script_call(L,3,0,-5))
	{
		string err_str=str(boost::format("Error calling gscript_put_gamestate: %1%") % lua_tostring(L,-1));
		Log(err_str.c_str());
		lua_pop(L,1);
		group->metrics.CommitEvent("logic_lua_script_call_errors");
		return false;
	}
	return true;
}
/***************************************************************************/
bool GLogicEngineLUA::HandleGetLocationFromScript(const char * location_ID, std::string & out, bool server_closing, bool logic_reload)
{
	INT32 gamestate_ID = gamestate_ID_from_location_ID(location_ID);
	if (gamestate_ID == 0)
	{
		return true;
	}

	//get data from Lua VM
	lua_pushcfunction(L,lua_error_handler);
	lua_getglobal(L,"gscript_get_gamestate");
	lua_pushinteger(L,gamestate_ID);
	lua_pushinteger(L, (int)server_closing);
	lua_pushinteger(L, (int)logic_reload);
	if(count_limited_script_call(L,3,1,-5))
	{
		string err_str=str(boost::format("Error calling gscript_get_gamestate: %1%") % lua_tostring(L,-1));
		Log(err_str.c_str());
		lua_pop(L,1);
		group->metrics.CommitEvent("logic_lua_script_call_errors");
		return false;
	}
	if(!lua_isstring(L,-1))
	{
		Log("Error in script: gscript_get_gamestate returned non-string value");
		return false;
	}
	out=lua_tostring(L,-1);
	lua_pop(L,1);
	return true;
}
/***************************************************************************/
void GLogicEngineLUA::HandleEraseLocation(const char * location_ID)
{
	INT32 gamestate_ID = gamestate_ID_from_location_ID(location_ID);
	if (gamestate_ID == 0)
	{
		return;
	}

	if(!L)
	{
		return;
	}
	//Lua call only, other stuff done by caller
	lua_pushcfunction(L,lua_error_handler);
	lua_getglobal(L,"gscript_erase_gamestate");
	lua_pushinteger(L,gamestate_ID);
	if(count_limited_script_call(L,1,0,-3))
	{
		string err_str=str(boost::format("Error calling gscript_erase_gamestate: %1%") % lua_tostring(L,-1));
		Log(err_str.c_str());
		lua_pop(L,1);
		group->metrics.CommitEvent("logic_lua_script_call_errors");
	}
}
/***************************************************************************/
bool GLogicEngineLUA::HandleLogicMessage(INT32 user_id, const char * location_ID, INT32 access, DWORD64 msg_time, const std::string & command, const std::string & params)
{
	INT32 gamestate_ID = gamestate_ID_from_location_ID(location_ID);
	if (gamestate_ID == 0)
	{
		return true;
	}

	lua_pushcfunction(L,lua_error_handler);
	if (access == EGS_PLAYER)
	{
		lua_getglobal(L,"gscript_handle_player_message");
	}
	else if (access == EGS_FRIEND)
	{
		lua_getglobal(L,"gscript_handle_friend_message");
	}
	else
	{
		lua_getglobal(L,"gscript_handle_stranger_message");
	}
	lua_pushnumber(L,(lua_Number)msg_time);
	lua_pushinteger(L,user_id);
	lua_pushinteger(L,gamestate_ID);
	lua_pushstring(L,command.c_str());
	lua_pushstring(L,params.c_str());
	if(count_limited_script_call(L,5,0,-7))
	{
		string err_str=str(boost::format("Error calling gscript_handle_X_message: %1%") % lua_tostring(L,-1));
		Log(err_str.c_str());
		lua_pop(L,1);
		group->metrics.CommitEvent("logic_lua_script_call_errors");
		return false;
	}
	return true;
}
/***************************************************************************/
bool GLogicEngineLUA::HandleSystemMessage(strstream & s, const char * location_ID, const std::string & command, const std::string & params)
{
	INT32 gamestate_ID = gamestate_ID_from_location_ID(location_ID);
	if (gamestate_ID == 0)
	{
		return true;
	}
	
	//call handler in Lua VM
	lua_pushcfunction(L,lua_error_handler);
	lua_getglobal(L,"gscript_handle_system_message");
	lua_pushinteger(L,gamestate_ID);
	lua_pushstring(L,command.c_str());
	lua_pushstring(L,params.c_str());
	if(count_limited_script_call(L,3,1,-5))
	{
		string err_str=str(boost::format("Error calling gscript_handle_system_message: %1%") % lua_tostring(L,-1));
		Log(err_str.c_str());
		lua_pop(L,1);
		group->metrics.CommitEvent("logic_lua_script_call_errors");
		return false;
	}

	// Zwracamy na konsole to co zwrocila funkcja LUA.
	if (lua_isstring(L, -1))
	{
		s << lua_tostring(L, -1) << "\r\n";
	}
	lua_pop(L,1);
	return true;
}
/***************************************************************************/
bool GLogicEngineLUA::HandleLocationMessage(const char * location_ID, const std::string & command, const std::string & params)
{
	INT32 gamestate_ID = gamestate_ID_from_location_ID(location_ID);
	if (gamestate_ID == 0)
	{
		return true;
	}

	//call handler in Lua VM
	lua_pushcfunction(L,lua_error_handler);
	lua_getglobal(L,"gscript_handle_gamestate_message");
	lua_pushinteger(L,gamestate_ID);
	lua_pushstring(L,command.c_str());
	lua_pushstring(L,params.c_str());
	if(count_limited_script_call(L,3,0,-5))
	{
		string err_str=str(boost::format("Error calling gscript_handle_gamestate_message: %1%") % lua_tostring(L,-1));
		Log(err_str.c_str());
		lua_pop(L,1);
		group->metrics.CommitEvent("logic_lua_script_call_errors");
		return false;
	}
	return true;
}
/***************************************************************************/
void GLogicEngineLUA::HandlePrintStatistics(strstream & s)
{
	if (L)
	{
		int lua_memory=lua_gc(L,LUA_GCCOUNT,0);
		int lua_memory_per_gamestate=0;
		int gamestates_count = GetLocationsCount();
		if (gamestates_count)
		{
			lua_memory_per_gamestate=lua_memory / gamestates_count;
		}
		s << (boost::format("Lua total memory:\t\t\t\t\t%1% kb (%2% kb/gamestate)\r\n") % lua_memory % lua_memory_per_gamestate);
	}

	if (L)
	{
		//call handler in Lua VM
		lua_pushcfunction(L,lua_error_handler);
		lua_getglobal(L,"gscript_return_meminfo");
		if(count_limited_script_call(L,0,1,-2))
		{
			string err_str=str(boost::format("Error calling gscript_return_meminfo: %1%") % lua_tostring(L,-1));
			Log(err_str.c_str());
			lua_pop(L,1);
			group->metrics.CommitEvent("logic_lua_script_call_errors");
			return;
		}
		// Zwracamy na konsole to co zwrocila funkcja LUA.
		if (lua_isstring(L, -1))
		{
			s << "----------- LUA INTERNAL MEMORY INFO -----------\r\n";
			s << lua_tostring(L, -1) << "\r\n";
		}
		lua_pop(L,1);
	}
}
/***************************************************************************/

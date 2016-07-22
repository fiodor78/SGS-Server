/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"
#include "headers_game.h"
#include "utils_conversion.h"

#ifdef LINUX
#include <dlfcn.h>
#endif

/*
 *  GLogicEngineCPP
 */
/***************************************************************************/
GLogicEngineCPP::GLogicEngineCPP() :
GLogicEngine()
{
	logic_library_copy_path = "";
	logic_library_handle = NULL;
	create_logic = NULL;
	destroy_logic = NULL;
	logic_base = NULL;
}
/***************************************************************************/
void GLogicEngineCPP::InitLogicLibrary()
{
	logic_library_copy_path = "";
	logic_library_handle = NULL;

#ifdef WIN32

	char * dll_path, tmp_path[MAX_PATH + 100], tmp_name[MAX_PATH + 100];
	dll_path = (char*)global_serverconfig->scripting_config.Cpp.lib_path.c_str();
	if (GetTempPathA(MAX_PATH, tmp_path))
	{
		INT32 retries = 100;
		while (--retries > 0)
		{
			sprintf(tmp_name, "%s%08d.dll", tmp_path, truerng.Rnd(100 * 1000000));
			FILE * plik = fopen(tmp_name, "rb");
			if (plik == NULL)
			{
				break;
			}
			fclose(plik);
			*tmp_name = 0;
		}
		if (*tmp_name && CopyFileA(dll_path, tmp_name, true))
		{
			dll_path = tmp_name;
			logic_library_copy_path = dll_path;
		}
	}

	logic_library_handle = (void *)LoadLibraryA(dll_path);
	if (logic_library_handle)
	{
		*(FARPROC *)&create_logic = GetProcAddress((HINSTANCE)logic_library_handle, "create_logic");
		*(FARPROC *)&destroy_logic = GetProcAddress((HINSTANCE)logic_library_handle, "destroy_logic");
	}
	if (!logic_library_handle && logic_library_copy_path != "")
	{
		DeleteFileA(logic_library_copy_path.c_str());
		logic_library_copy_path = "";
	}

#else

	char * dll_path, tmp_name[110];
	dll_path = (char*)global_serverconfig->scripting_config.Cpp.lib_path.c_str();
	strcpy(tmp_name, "/tmp/logicXXXXXX");
	int dst_fd = mkstemp(tmp_name);
	if (dst_fd != -1)
	{
		FILE * src, * dst;
		src = fopen(dll_path, "rb");
		if (src)
		{
			dst = fdopen(dst_fd, "wb");
			if (dst)
			{
				char buffer[64000];
				for (;;)
				{
					int count = fread(buffer, 1, sizeof(buffer), src);
					if (count == 0)
					{
						break;
					}
					fwrite(buffer, 1, count, dst);
				}

				dll_path = tmp_name;
				logic_library_copy_path = dll_path;
			}
			fclose(src);
		}
		close(dst_fd);
	}

	logic_library_handle = dlopen(dll_path, RTLD_LAZY);
	if (logic_library_handle)
	{
		*(void **)&create_logic = (void *)dlsym(logic_library_handle, "create_logic");
		*(void **)&destroy_logic = (void *)dlsym(logic_library_handle, "destroy_logic");
	}
	else
	{
		char temp_string[1024];
		SNPRINTF(temp_string, sizeof(temp_string), "dlerror(): %s", dlerror());
		temp_string[sizeof(temp_string) - 1] = 0;
		Log(temp_string);
	}

	if (!logic_library_handle && logic_library_copy_path != "")
	{
		unlink(logic_library_copy_path.c_str());
		logic_library_copy_path = "";
	}

#endif

	if (!logic_library_handle)
	{
		string err_str = str(boost::format("Error loading logic library: %1%") % global_serverconfig->scripting_config.Cpp.lib_path);
		Log(err_str.c_str());
	}

	logic_base = NULL;
	if (create_logic)
	{
		logic_base = create_logic(this);
	}
}
/***************************************************************************/
void GLogicEngineCPP::DestroyLogicLibrary()
{
	if (logic_base)
	{
		logic_base->on_cleanup();
	}

	if (logic_base && destroy_logic)
	{
		destroy_logic(logic_base);
	}
	if (logic_library_handle)
	{
#ifdef WIN32
		FreeLibrary((HINSTANCE)logic_library_handle);
		if (logic_library_copy_path != "")
		{
			DeleteFileA(logic_library_copy_path.c_str());
		}
#else
		dlclose(logic_library_handle);
		if (logic_library_copy_path != "")
		{
			unlink(logic_library_copy_path.c_str());
		}
#endif
	}

	logic_library_copy_path = "";
	logic_library_handle = NULL;
	create_logic = NULL;
	destroy_logic = NULL;
	logic_base = NULL;
}
/***************************************************************************/
bool GLogicEngineCPP::HandleInitScript(const std::string & config_str)
{
	if (logic_base)
	{
		try
		{
			return logic_base->on_init_global_config(CurrentSTime(), config_str);
		}
		catch (std::exception & e)
		{
			LogException("{%s} HandleInitScript()", e.what());
		}
		catch (...)
		{
			LogException("HandleInitScript()");
		}
	}
	return false;
}
/***************************************************************************/
bool GLogicEngineCPP::HandlePutLocationToScript(const char * location_ID, const std::string & location_data)
{
	if (logic_base)
	{
		try
		{
			INT32 gamestate_ID = gamestate_ID_from_location_ID(location_ID);
			if (gamestate_ID > 0)
			{
				return logic_base->on_put_gamestate(CurrentSTime(), gamestate_ID, location_data);
			}
			else
			{
				return logic_base->on_put_location(CurrentSTime(), location_ID, location_data.c_str());
			}
		}
		catch (std::exception & e)
		{
			LogException("{%s} HandlePutLocationToScript(\"%s\")", e.what(), location_ID);
		}
		catch (...)
		{
			LogException("HandlePutLocationToScript(\"%s\")", location_ID);
		}
	}
	return false;
}
/***************************************************************************/
bool GLogicEngineCPP::HandleGetLocationFromScript(const char * location_ID, std::string & out, bool server_closing, bool logic_reload)
{
	if (logic_base)
	{
		try
		{
			INT32 gamestate_ID = gamestate_ID_from_location_ID(location_ID);
			if (gamestate_ID > 0)
			{
				logic_base->on_get_gamestate(gamestate_ID, out, server_closing, logic_reload);
			}
			else
			{
				return logic_base->on_get_location(location_ID, out, server_closing, logic_reload);
			}
		}
		catch (std::exception & e)
		{
			LogException("{%s} HandleGetLocationFromScript(\"%s\")", e.what(), location_ID);
		}
		catch (...)
		{
			LogException("HandleGetLocationFromScript(\"%s\")", location_ID);
		}
	}
	return true;
}
/***************************************************************************/
void GLogicEngineCPP::HandleEraseLocation(const char * location_ID)
{
	if (logic_base)
	{
		try
		{
			INT32 gamestate_ID = gamestate_ID_from_location_ID(location_ID);
			if (gamestate_ID > 0)
			{
				logic_base->on_erase_gamestate(gamestate_ID);
			}
			else
			{
				logic_base->on_erase_location(location_ID);
			}
		}
		catch (std::exception & e)
		{
			LogException("{%s} HandleEraseLocation(\"%s\")", e.what(), location_ID);
		}
		catch (...)
		{
			LogException("HandleEraseLocation(\"%s\")", location_ID);
		}
	}
}
/***************************************************************************/
bool GLogicEngineCPP::HandleLogicMessage(INT32 user_id, const char * location_ID, INT32 access, DWORD64 msg_time, const std::string & command, const std::string & params)
{
	try
	{
		INT32 gamestate_ID = gamestate_ID_from_location_ID(location_ID);
		if (gamestate_ID == 0)
		{
			logic_base->handle_user_location_message(msg_time, user_id, location_ID, command, params);
			return true;
		}

		if (access == EGS_PLAYER)
		{
			if (logic_base)
			{
				logic_base->handle_player_message(msg_time, user_id, gamestate_ID, command, params);
			}
		}
		else if (access == EGS_FRIEND)
		{
			if (logic_base)
			{
				logic_base->handle_friend_message(msg_time, user_id, gamestate_ID, command, params);
			}
		}
		else
		{
			if (logic_base)
			{
				logic_base->handle_stranger_message(msg_time, user_id, gamestate_ID, command, params);
			}
		}
	}
	catch (std::exception & e)
	{
		LogException("{%s} HandleLogicMessage(\"%s\", \"%s\", \"%s\")", e.what(), location_ID, command.c_str(), params.c_str());
	}
	catch (...)
	{
		LogException("HandleLogicMessage(\"%s\", \"%s\", \"%s\")", location_ID, command.c_str(), params.c_str());
	}
	return true;
}
/***************************************************************************/
bool GLogicEngineCPP::HandleSystemMessage(strstream & s, const char * location_ID, const std::string & command, const std::string & params)
{
	string output = "";

	if (logic_base)
	{
		try
		{
			INT32 gamestate_ID = gamestate_ID_from_location_ID(location_ID);
			if (gamestate_ID > 0)
			{
				logic_base->handle_system_message(CurrentSTime(), gamestate_ID, command, params, output);
			}
			else
			{
				logic_base->handle_system_location_message(CurrentSTime(), location_ID, command, params, output);
			}
		}
		catch (std::exception & e)
		{
			LogException("{%s} HandleSystemMessage(\"%s\", \"%s\", \"%s\")", e.what(), location_ID, command.c_str(), params.c_str());
		}
		catch (...)
		{
			LogException("HandleSystemMessage(\"%s\", \"%s\", \"%s\")", location_ID, command.c_str(), params.c_str());
		}
	}

	if (output != "")
	{
		s << output << "\r\n";
	}

	return true;
}
/***************************************************************************/
bool GLogicEngineCPP::HandleLocationMessage(const char * location_ID, const std::string & command, const std::string & params)
{
	if (logic_base)
	{
		try
		{
			INT32 gamestate_ID = gamestate_ID_from_location_ID(location_ID);
			if (gamestate_ID > 0)
			{
				logic_base->handle_gamestate_message(CurrentSTime(), gamestate_ID, command, params);
			}
			else
			{
				logic_base->handle_location_message(CurrentSTime(), location_ID, command, params);
			}
		}
		catch (std::exception & e)
		{
			LogException("{%s} HandleLocationMessage(\"%s\", \"%s\", \"%s\")", e.what(), location_ID, command.c_str(), params.c_str());
		}
		catch (...)
		{
			LogException("HandleLocationMessage(\"%s\", \"%s\", \"%s\")", location_ID, command.c_str(), params.c_str());
		}
	}
	return true;
}
/***************************************************************************/
bool GLogicEngineCPP::HandleGlobalMessage(const std::string & command, const std::string & params)
{
	if (logic_base)
	{
		try
		{
			logic_base->handle_global_message(CurrentSTime(), command, params);
		}
		catch (std::exception & e)
		{
			LogException("{%s} HandleGlobalMessage(\"%s\", \"%s\")", e.what(), command.c_str(), params.c_str());
		}
		catch (...)
		{
			LogException("HandleGlobalMessage(\"%s\", \"%s\")", command.c_str(), params.c_str());
		}
	}
	return true;
}
/***************************************************************************/
bool GLogicEngineCPP::HandleSystemGlobalMessage(strstream & s, const std::string & command, const std::string & params)
{
	string output = "";

	if (logic_base)
	{
		try
		{
			logic_base->handle_system_global_message(CurrentSTime(), command, params, output);
		}
		catch (std::exception & e)
		{
			LogException("{%s} HandleSystemGlobalMessage(\"%s\", \"%s\")", e.what(), command.c_str(), params.c_str());
		}
		catch (...)
		{
			LogException("HandleSystemGlobalMessage(\"%s\", \"%s\")", command.c_str(), params.c_str());
		}
	}

	if (output != "")
	{
		s << output << "\r\n";
	}

	return true;
}
/***************************************************************************/
void GLogicEngineCPP::HandlePrintStatistics(strstream & s)
{
	if (logic_base)
	{
		try
		{
			logic_base->on_print_statistics(s);
		}
		catch (std::exception & e)
		{
			LogException("{%s} HandlePrintStatistics()", e.what());
		}
		catch (...)
		{
			LogException("HandlePrintStatistics()");
		}
	}
}
/***************************************************************************/
// Funkcja zwraca informacje, czy gamestate zostal zmieniony podczas wywolania callbacka.
bool GLogicEngineCPP::HandlePlayerAdd(INT32 user_id, const char * location_ID)
{
	if (logic_base)
	{
		try
		{
			INT32 gamestate_ID = gamestate_ID_from_location_ID(location_ID);
			if (gamestate_ID > 0)
			{
				return logic_base->on_connect_player(user_id, gamestate_ID);
			}
			else
			{
				logic_base->on_connect_player_to_location(user_id, location_ID);
			}
		}
		catch (std::exception & e)
		{
			LogException("{%s} HandlePlayerAdd(%d, \"%s\")", e.what(), user_id, location_ID);
		}
		catch (...)
		{
			LogException("HandlePlayerAdd(%d, \"%s\")", user_id, location_ID);
		}
	}
	return false;
}
/***************************************************************************/
void GLogicEngineCPP::HandlePlayerDelete(INT32 user_id, const char * location_ID)
{
	if (logic_base)
	{
		try
		{
			INT32 gamestate_ID = gamestate_ID_from_location_ID(location_ID);
			if (gamestate_ID > 0)
			{
				logic_base->on_disconnect_player(user_id, gamestate_ID);
			}
			else
			{
				logic_base->on_disconnect_player_from_location(user_id, location_ID);
			}
		}
		catch (std::exception & e)
		{
			LogException("{%s} HandlePlayerDelete(%d, \"%s\")", e.what(), user_id, location_ID);
		}
		catch (...)
		{
			LogException("HandlePlayerDelete(%d, \"%s\")", user_id, location_ID);
		}
	}
}
/***************************************************************************/
void GLogicEngineCPP::HandleCreateLocationNotification(const char * location_ID, const char * new_location_ID, const char * new_location_data, bool creation_success)
{
	if (logic_base)
	{
		try
		{
			logic_base->on_create_location(location_ID, new_location_ID, new_location_data, creation_success);
		}
		catch (std::exception & e)
		{
			LogException("{%s} HandleCreateLocationNotification(\"%s\", \"%s\")", e.what(), location_ID, new_location_ID);
		}
		catch (...)
		{
			LogException("HandleCreateLocationNotification(\"%s\", \"%s\")", location_ID, new_location_ID);
		}
	}
}
/***************************************************************************/
void GLogicEngineCPP::HandleFlushLocationNotification(const char * location_ID, const char * location_data, bool flush_success)
{
	if (logic_base)
	{
		try
		{
			logic_base->on_flush_location(location_ID, location_data, flush_success);
		}
		catch (std::exception & e)
		{
			LogException("{%s} HandleFlushLocationNotification(\"%s\")", e.what(), location_ID);
		}
		catch (...)
		{
			LogException("HandleFlushLocationNotification(\"%s\")", location_ID);
		}
	}
}
/***************************************************************************/
void GLogicEngineCPP::HandleInitiateGentleClose()
{
	if (logic_base)
	{
		try
		{
			logic_base->on_initiate_gentle_close();
		}
		catch (std::exception & e)
		{
			LogException("{%s} HandleInitiateGentleClose()", e.what());
		}
		catch (...)
		{
			LogException("HandleInitiateGentleClose()");
		}
	}
}
/***************************************************************************/
void GLogicEngineCPP::HandleClockTick(DWORD64 clock_msec)
{
	if (logic_base)
	{
		try
		{
			logic_base->on_clock_tick(clock_msec);
		}
		catch (std::exception & e)
		{
			LogException("{%s} HandleClockTick()", e.what());
		}
		catch (...)
		{
			LogException("HandleClockTick()");
		}
	}
}
/***************************************************************************/
void GLogicEngineCPP::HandleHTTPResponse(const char * location_ID, INT32 http_request_id, INT32 http_response_code, const std::string & http_response)
{
	if (logic_base)
	{
		try
		{
			logic_base->on_http_response(location_ID, http_request_id, http_response_code, http_response);
		}
		catch (std::exception & e)
		{
			LogException("{%s} HandleHTTPResponse(\"%s\")", e.what(), location_ID);
		}
		catch (...)
		{
			LogException("HandleHTTPResponse(\"%s\")", location_ID);
		}
	}
}
/***************************************************************************/
void GLogicEngineCPP::HandleReplaceVulgarWords(const char * location_ID, const std::string & text, const std::string & params)
{
	if (logic_base)
	{
		try
		{
			logic_base->on_replace_vulgar_words(CurrentSTime(), location_ID, text, params);
		}
		catch (std::exception & e)
		{
			LogException("{%s} HandleReplaceVulgarWords(\"%s\", \"%s\")", e.what(), location_ID, text.c_str());
		}
		catch (...)
		{
			LogException("HandleReplaceVulgarWords(\"%s\", \"%s\")", location_ID, text.c_str());
		}
	}
}
/***************************************************************************/
void GLogicEngineCPP::HandleGetLocationsIdsLike(const char * location_ID, INT32 request_id, const std::set<std::string> & locations_ids)
{
	if (logic_base)
	{
		try
		{
			logic_base->on_get_locations_ids_like(location_ID, request_id, locations_ids);
		}
		catch (std::exception & e)
		{
			LogException("{%s} HandleGetLocationsIdsLike(\"%s\", %d)", e.what(), location_ID, request_id);
		}
		catch (...)
		{
			LogException("HandleGetLocationsIdsLike(\"%s\", %d)", location_ID, request_id);
		}
	}
}
/***************************************************************************/
void GLogicEngineCPP::LogException(const char * msg, ...)
{
	char buffer[4096];
	va_list args;
	va_start(args, msg);
	strcpy(buffer, "LOGIC EXCEPTION: ");
	VSNPRINTF(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), msg, args);
	buffer[sizeof(buffer) - 1] = 0;
	Log(buffer);
}
/***************************************************************************/

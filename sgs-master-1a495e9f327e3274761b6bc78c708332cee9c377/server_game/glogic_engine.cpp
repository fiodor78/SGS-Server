/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
	  Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"
#include "headers_game.h"
#include "utils_conversion.h"

/***************************************************************************/
INT32 gamestate_ID_from_location_ID(const char * location_ID)
{
	if (strncmp(location_ID, "gamestate.", 10) == 0)
	{
		return ATOI(location_ID + 10);
	}
	return 0;
}
/***************************************************************************/
string	location_ID_from_gamestate_ID(INT32 gamestate_ID)
{
	return str(boost::format("gamestate.%1%") % gamestate_ID);
}
/***************************************************************************/
/*
 *  GLogicEngine
 */

GLogicEngine::GLogicEngine() :
write_delay(DEFAULT_GAMESTATE_WRITE_DELAY),
cache_period(DEFAULT_GAMESTATE_CACHE_PERIOD),
gentle_close_initiated(false),
status(ELES_Initializing)
{
}

void GLogicEngine::Init(GGroup * group_p, GSimpleRaportManager * raport_manager_p)
{
	group = group_p;
	raport_manager = raport_manager_p;
	write_delay=global_serverconfig->scripting_config.write_delay;
	cache_period=global_serverconfig->scripting_config.cache_period;

	locationdb_triggers.clear();
	InitLogic();
}

void GLogicEngine::Destroy()
{
	if(!group)
	{
		//Destroy may be called more than once!
		return;
	}
	DestroyLogicLibrary();
	group=NULL;
	status_map.clear();
	debug_data.clear();
}

void GLogicEngine::InitLogic()
{
	status = ELES_Initializing;
	Log("Logic library init");
	InitLogicLibrary();
	Log("Logic library init complete");

	//-------------------------------------------------------

	std::string config_str;
	GetConfig(config_str);

	status = (InitScript(config_str)) ? ELES_Ready : ELES_Init_Failed;

	last_error_report = 0;
	gentle_close_initiated = false;
}

void GLogicEngine::DestroyLogic()
{
	Log("Logic library shutdown");
	DestroyLogicLibrary();
	status_map.clear();
	debug_data.clear();
}

void GLogicEngine::GetConfig(std::string & out_str)
{
	out_str = "";

	ifstream str_file(global_serverconfig->scripting_config.logic_global_config_path.c_str(), std::ios::binary);
	if (str_file.is_open())
	{
		std::stringstream buffer;
		buffer << str_file.rdbuf();
		out_str = buffer.str();
	}
}

void GLogicEngine::WriteChangedGameStatesToDatabase()
{
	if (!group)
	{
		return;
	}
	DWORD64 cur_time=group->Server()->CurrentSTime();
	std::map<string, SLocationStatus>::iterator it = status_map.begin();
	while(it!=status_map.end())
	{
		// Jedynie lokacje bedace gamestatami podlegaja pod mechanizm 'write_delay'.
		INT32 gamestate_ID = gamestate_ID_from_location_ID(it->first.c_str());
		if (gamestate_ID == 0)
		{
			it++;
			continue;
		}

		SLocationStatus & gss=(*it).second;
		if(gss.dirty)
		{
			if ((gss.last_write + write_delay) < cur_time)
			{
				if (gss.locationdb_write_tasks.size() == 0)
				{
					AddLocationDBTaskWrite(ELTA_Write_WriteChangedGameStateToDatabase, it->first.c_str());
				}
			}
		}
		it++;
	}
}

void GLogicEngine::WriteChangedLocationsToDatabaseGroupShutdown()
{
	if (!group)
	{
		return;
	}

	if (!gentle_close_initiated)
	{
		HandleInitiateGentleClose();
		gentle_close_initiated = true;
	}

	DWORD64 cur_time = group->Server()->CurrentSTime();
	std::map<string, SLocationStatus>::iterator it = status_map.begin();
	while (it != status_map.end())
	{
		bool is_gamestate = (gamestate_ID_from_location_ID(it->first.c_str()) > 0);
		SLocationStatus & gss = (*it).second;
		
		if (is_gamestate && !gss.dirty)
		{
			gss.location_ready_to_gently_close = true;
		}

		if (cur_time > group->write_locations_deadline)
		{
			string err_str = str(boost::format("LocationDB write failed! Reached write_locations_deadline for locationID: %1%") % it->first);
			Log(err_str.c_str());
			gss.location_ready_to_gently_close = true;
		}


		if (gss.location_ready_to_gently_close)
		{
			EraseLocation(it->first.c_str());
			group->ReleaseLocation(it->first.c_str());
			status_map.erase(it++);
		}
		else
		{
			if ((gss.locationdb_write_tasks.size() == 0) && ((is_gamestate && gss.dirty) || (!is_gamestate && gss.location_released_by_logic)))
			{
				AddLocationDBTaskWrite(ELTA_Write_GroupShutdown, it->first.c_str());
			}

			++it;
		}
	}
}

void GLogicEngine::EraseOldLocations(bool force)
{
	DWORD64 cur_time = group->Server()->CurrentSTime();
	std::map<string, SLocationStatus>::iterator it = status_map.begin();
	while (it != status_map.end())
	{
		SLocationStatus & gss = (*it).second;
		bool is_old_gamestate = false;
		bool is_old_location = false;

		INT32 gamestate_ID = gamestate_ID_from_location_ID(it->first.c_str());
		if (gamestate_ID > 0)
		{
			// Jedynie lokacje bedace gamestatami podlegaja pod mechanizm 'cache_period'.
			is_old_gamestate = ((gss.last_access + cache_period) < cur_time) || gss.location_error_waiting_for_release;
		}
		else
		{
			is_old_location = gss.location_released_by_logic;
		}

		if (force || is_old_gamestate || is_old_location)
		{
			string location_ID = it->first;

			// Nie pozbywamy sie przypisania gamestate, do ktorego ktos jest jeszcze podlaczony.
			TPlayerGameRange p = group->FindPlayers(location_ID.c_str());
			bool players_connected = (p.first != p.second);

			if (gamestate_ID > 0 && gss.dirty && (is_old_gamestate || !players_connected))
			{
				if (!gss.location_error_waiting_for_release || gss.locationdb_write_tasks.size() > 0)
				{
					if (gss.locationdb_write_tasks.size() == 0)
					{
						AddLocationDBTaskWrite(ELTA_Write_EraseOldGameState, location_ID.c_str());
					}
					it++;
					continue;
				}
			}

			if (gamestate_ID == 0)
			{
				if (!gss.location_released_by_logic || gss.locationdb_write_tasks.size() > 0)
				{
					it++;
					continue;
				}
			}

			if (players_connected)
			{
				if (gss.location_error_waiting_for_release)
				{
					DisconnectPlayers(location_ID.c_str());
				}
				it++;
				continue;
			}

			EraseLocation(location_ID.c_str());
			group->ReleaseLocation(location_ID.c_str());
			status_map.erase(it++);
			std::map<string, SDebugContext>::iterator dbg_it = debug_data.find(location_ID.c_str());
			if(dbg_it!=debug_data.end())
			{
				debug_data.erase(dbg_it);
			}
		}
		else
		{
			it++;
		}
	}
}
/***************************************************************************/
void GLogicEngine::PrintStatistics(strstream & s)
{
	HandlePrintStatistics(s);
}
/***************************************************************************/
void GLogicEngine::ClearStatistics(strstream & s)
{
}
/***************************************************************************/
// Przepinamy dowolnie wybrana lokacje do nowej logiki.
bool GLogicEngine::MoveRandomLocationTo(GLogicEngine * target_engine)
{
	std::map<string, SLocationStatus>::iterator it;
	for (it = status_map.begin(); it != status_map.end(); it++)
	{
		const string & location_ID = it->first;

		// Sprawdzamy czy nie ma jakichs triggerow dotyczacych lokacji.
		// Jesli sa to czekamy az sie wykonaja.
		TLocationDBTaskTriggersMap::iterator pos;
		for (pos = locationdb_triggers.begin(); pos != locationdb_triggers.end(); pos++)
		{
			if (pos->second.location_ID == location_ID)
			{
				break;
			}
		}
		if (pos != locationdb_triggers.end())
		{
			continue;
		}

		string location_data;
		if (this->GetLocationFromScript(location_ID.c_str(), location_data, false, true))
		{
			if (target_engine->PutLocationToScript(location_ID.c_str(), location_data))
			{
				target_engine->status_map[location_ID] = it->second;
			}
		}
		else
		{
			group->ReleaseLocation(location_ID.c_str());
		}

		this->EraseLocation(location_ID.c_str());
		status_map.erase(it);
		return true;
	}

	return false;
}
/***************************************************************************/
bool GLogicEngine::InitScript(const std::string & config_str)
{
	return HandleInitScript(config_str);
}
/***************************************************************************/
bool GLogicEngine::PutLocationToScript(const char * location_ID, const std::string & location_data)
{
	SMetricStats::SCallContext call_ctx = SMetricStats::BeginDuration();
	bool result = HandlePutLocationToScript(location_ID, location_data);
	group->metrics.CommitDuration("on_put_location_call_time", call_ctx);
	return result;
}
/***************************************************************************/
bool GLogicEngine::GetLocationFromScript(const char * location_ID, std::string & out, bool server_closing, bool logic_reload)
{
	SMetricStats::SCallContext call_ctx = SMetricStats::BeginDuration();
	bool result = HandleGetLocationFromScript(location_ID, out, server_closing, logic_reload);
	group->metrics.CommitDuration("on_get_location_call_time", call_ctx);
	return result;
}
/***************************************************************************/
void GLogicEngine::EraseLocation(const char * location_ID)
{
	HandleEraseLocation(location_ID);
}
/***************************************************************************/
void GLogicEngine::DisconnectPlayers(const char * location_ID, const char * reason)
{
	TPlayerGameRange p=group->FindPlayers(location_ID);
	TPlayerGameMultiMap::iterator pos;
	for(pos=p.first;pos!=p.second;++pos)
	{
		GPlayerGame *pg=((*pos).second);
		if(!pg->Valid())
		{
			continue;
		}
		pg->MsgExt(IGM_CONNECTION_CLOSE).WT(*reason == 0 ? "logic action" : reason).A();
	}
}
/***************************************************************************/
DWORD64 GLogicEngine::CurrentSTime()
{
	return group->Server()->CurrentSTime();
}
/***************************************************************************/
bool GLogicEngine::IsLocationLoaded(const char * location_ID)
{
	if (status_map.find(location_ID) != status_map.end())
	{
		return true;
	}

	TLocationDBTaskTriggersMap::iterator pos;
	for (pos = locationdb_triggers.begin(); pos != locationdb_triggers.end(); pos++)
	{
		if (pos->second.location_ID == location_ID)
		{
			return true;
		}
	}

	return false;
}
/***************************************************************************/

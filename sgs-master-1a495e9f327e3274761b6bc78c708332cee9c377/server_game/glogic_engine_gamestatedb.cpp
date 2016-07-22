/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"
#include "headers_game.h"
#include "utils_conversion.h"

extern GServerBase * global_server;
DWORD64 GetTimeUSec();

/***************************************************************************/
INT32 GLogicEngine::AddLocationDBTaskRead(ELocationTriggerAction action, const char * location_ID, const string & command, const string & params, bool flush, INT32 socketid, INT32 user_id)
{
	SLocationDBTaskTrigger trigger;
	trigger.action = action;
	trigger.location_ID = location_ID;
	trigger.command = command;
	trigger.params = params;
	trigger.flush = flush;
	trigger.socketid = socketid;
	trigger.user_id = user_id;
	trigger.call_ctx = GetTimeUSec();

	SLocationDBTask task;
	task.location_ID = location_ID;
	task.group_id = group->GetID();
	task.direction = ELDBTD_Read;
	task.execution_deadline = 0;
	task.taskid = group->seq_locationdb_taskid++;
	task.has_priority = false;
	task.task_executed = false;
	task.location_data = "";

	DWORD64 now = group->Server()->CurrentSTime();

	switch (action)
	{
	case ELTA_Read_LocationsIdsLikeFromDB:
		task.direction = ELDBTD_Read_Locations_Ids;
		task.location_data = params;
		task.execution_deadline = now + 30;
		task.has_priority = true;
		break;
	case ELTA_Read_ProcessSystemMessage:
		task.execution_deadline = now + 30;
		task.has_priority = true;
		break;

	case ELTA_Read_ProcessLogicMessage:
		task.execution_deadline = now + 10;
		break;
	case ELTA_Read_ProcessLocationMessage:
		task.execution_deadline = now + 30;
		break;

	case ELTA_Read_ConnectToLocation:
	case ELTA_Read_DisconnectFromLocation:
		task.execution_deadline = now + 10;
		break;
	}

	if (task.execution_deadline != 0)
	{
		GSERVER->AddLocationDBTask(location_ID, task);
		locationdb_triggers.insert(make_pair(task.taskid, trigger));
	}

	return task.taskid;
}
/***************************************************************************/
void GLogicEngine::AddLocationDBTaskWrite(ELocationTriggerAction action, const char * location_ID, const string & params, INT32 socketid, const string & extra_data)
{

	INT32 taskid = group->seq_locationdb_taskid++;

	SLocationDBTaskTrigger trigger;
	trigger.action = action;
	trigger.location_ID = location_ID;
	trigger.command = "";
	trigger.params = params;
	trigger.flush = false;
	trigger.socketid = socketid;
	trigger.call_ctx = GetTimeUSec();

	std::string location_data;
	if (action == ELTA_Write_CreateLocation || action == ELTA_Write_DeleteLocation)
	{
		location_data = params;
		trigger.command = extra_data;
	}
	else
	{
		bool server_closing = (action == ELTA_Write_GroupShutdown) ? true : false;
		if (!GetLocationFromScript(location_ID, location_data, server_closing, false))
		{
			locationdb_triggers.insert(make_pair(taskid, trigger));
			INT32 gamestate_ID = gamestate_ID_from_location_ID(location_ID);
			ExecuteLocationDBTriggerWrite(taskid, (server_closing && gamestate_ID == 0) ? ELDBTS_OK : ELDBTS_WriteFailedLogicFailure, location_data);
			return;
		}
	}

	SLocationDBTask task;
	task.location_ID = location_ID;
	task.group_id = group->GetID();
	task.direction = ELDBTD_Write;
	task.execution_deadline = 0;
	task.taskid = taskid;
	task.has_priority = false;
	task.task_executed = false;
	task.location_data = location_data;

	DWORD64 now = group->Server()->CurrentSTime();

	switch (action)
	{
	case ELTA_Write_ProcessSystemMessage:
		task.execution_deadline = now + 30;
		task.has_priority = true;
		break;

	case ELTA_Write_WriteChangedGameStateToDatabase:
		{
			TPlayerGameRange p = group->FindPlayers(location_ID);
			bool players_connected = (p.first != p.second);
			task.execution_deadline = (players_connected) ? now + 60 * 5 : now + 3600 * 24;
		}
		break;

	case ELTA_Write_GroupShutdown:
		// Ustawiamy +1, zeby umozliwic zwrot statusu ELDBTS_WriteDeadlineReached, a nie ELDBTS_ExecutionDeadline.
		task.execution_deadline = group->write_locations_deadline + 1;
		task.has_priority = true;
		break;

	case ELTA_Write_EraseOldGameState:
		task.execution_deadline = now + 3600 * 24;
		break;

	case ELTA_Write_CreateLocation:
		task.execution_deadline = now + 30;
		task.direction = ELDBTD_Insert;
		break;

	case ELTA_Write_FlushLocation:
		task.execution_deadline = now + 30;
		task.has_priority = true;
		break;

	case ELTA_Write_DeleteLocation:
		task.execution_deadline = now + 30;
		task.direction = ELDBTD_Delete;
		break;
	}

	if (task.execution_deadline != 0)
	{
		std::map<string, SLocationStatus>::iterator it = status_map.find(location_ID);
		if (it != status_map.end())
		{
			if (task.direction != ELDBTD_Insert &&
				task.direction != ELDBTD_Delete &&
				it->second.locationdb_write_tasks.size() == 0 &&
				MD5(location_data.c_str())() == it->second.last_confirmed_location_MD5)
			{
				locationdb_triggers.insert(make_pair(taskid, trigger));
				ExecuteLocationDBTriggerWrite(taskid, ELDBTS_OK, location_data);
				return;
			}
			it->second.locationdb_write_tasks.insert(taskid);
		}

		locationdb_triggers.insert(make_pair(taskid, trigger));
		GSERVER->AddLocationDBTask(location_ID, task);
	}
}
/***************************************************************************/
void GLogicEngine::ExecuteLocationDBTriggerRead(INT32 taskid, ELocationDBTaskStatus status, const string & location_data)
{
	TLocationDBTaskTriggersMap::iterator pos;
	pos = locationdb_triggers.find(taskid);
	if (pos == locationdb_triggers.end())
	{
		return;
	}
	SLocationDBTaskTrigger & trigger = pos->second;
	group->metrics.CommitDuration(gamestate_ID_from_location_ID(trigger.location_ID.c_str()) > 0 ? "load_gamestate_time" : "load_location_time", trigger.call_ctx);

	DWORD64 cur_time = group->Server()->CurrentSTime();

	if (status != ELDBTS_OK)
	{
		string err_str = str(boost::format("LocationDB read failed! location_ID: %1%, status: %2%") %
								trigger.location_ID %
								(status == ELDBTS_ExecutionDeadline ? "ELDBTS_ExecutionDeadline" :
								status == ELDBTS_ReadFailed ? "ELDBTS_ReadFailed" : ""));
		Log(err_str.c_str());
	}

	if (status != ELDBTS_OK)
	{
		EraseLocation(trigger.location_ID.c_str());
		status_map.erase(trigger.location_ID);
	}

	bool location_corrupted = false;
	if (status == ELDBTS_OK)
	{
		if (status_map.find(trigger.location_ID) == status_map.end())
		{
			//put to script
			if (!PutLocationToScript(trigger.location_ID.c_str(), location_data))
			{
				status = ELDBTS_ReadFailed;
				location_corrupted = true;
			}
			else
			{
				//insert info into status_map
				SLocationStatus gss;
				gss.dirty = false;
				gss.last_access = cur_time;
				gss.last_confirmed_location_MD5 = MD5(location_data.c_str())();
				status_map.insert(make_pair(trigger.location_ID, gss));
				SDebugContext dc;
				dc.location_data = location_data;
				debug_data.insert(make_pair(trigger.location_ID, dc));
			}
		}
	}

	switch (trigger.action)
	{
	case ELTA_Read_ProcessLogicMessage:
		{
			if (status != ELDBTS_OK)
			{
				DisconnectPlayers(trigger.location_ID.c_str(), !location_corrupted ? "cannot load location" : "cannot read location");
				break;
			}

			GSocket * socket = group->Server()->GetPlayerSocketByID(trigger.socketid);
			if (socket)
			{
				GPlayerGame * player = static_cast<GPlayerGame *>(socket->Logic());
				if (player && strcmp(player->player_info.target_location_ID, trigger.location_ID.c_str()) == 0)
				{
					ProcessLogicMessage(player, trigger.command, trigger.params);
				}
			}
		}
		break;

	case ELTA_Read_ProcessLocationMessage:
		{
			if (status != ELDBTS_OK)
			{
				break;
			}

			ProcessLocationMessage(trigger.location_ID.c_str(), trigger.command, trigger.params);
		}
		break;

	case ELTA_Read_ProcessSystemMessage:
		{
			GMemory output_buffer;
			output_buffer.Init(&group->Server()->MemoryManager());
			output_buffer.Allocate(K512);
			strstream s(output_buffer.End(), output_buffer.Free(), ios_base::out);

			if (status != ELDBTS_OK)
			{
				if (!location_corrupted)
				{
					s << "ERROR Cannot load location from database.\r\n";
				}
				else
				{
					s << "ERROR Cannot read location from database.\r\n";
				}
			}
			else
			{
				if (!ProcessSystemMessage(s, trigger.location_ID.c_str(), trigger.command, trigger.params, trigger.flush, trigger.socketid))
				{
					output_buffer.Deallocate();
					break;
				}
			}

			output_buffer.End()[s.pcount()] = 0;

			SConsoleTask console_task;
			console_task.socketid = trigger.socketid;
			console_task.type = ECTT_Response;
			console_task.cmd = CMD_LOGIC_MESSAGE;
			console_task.result = s.str();
			output_buffer.Deallocate();

			global_server->InsertConsoleTask(console_task);
		}
		break;

	case ELTA_Read_ConnectToLocation:
		{
			if (status != ELDBTS_OK)
			{
				DisconnectPlayers(trigger.location_ID.c_str(), !location_corrupted ? "cannot load location" : "cannot read location");
				break;
			}
			ProcessConnectToLocation(trigger.user_id, trigger.location_ID.c_str());
		}
		break;

	case ELTA_Read_DisconnectFromLocation:
		{
			if (status != ELDBTS_OK)
			{
				break;
			}
			ProcessDisconnectFromLocation(trigger.user_id, trigger.location_ID.c_str());
		}
		break;

	case ELTA_Read_LocationsIdsLikeFromDB:
		{
			if (status != ELDBTS_OK)
			{
				break;
			}
			ProcessGetLocationsIdsLike(trigger.location_ID.c_str(), taskid, location_data);
		}
		break;
	}

	locationdb_triggers.erase(pos);
}
/***************************************************************************/
void GLogicEngine::ExecuteLocationDBTriggerWrite(INT32 taskid, ELocationDBTaskStatus status, const string & location_data)
{
	TLocationDBTaskTriggersMap::iterator pos;
	pos = locationdb_triggers.find(taskid);
	if (pos == locationdb_triggers.end())
	{
		return;
	}
	SLocationDBTaskTrigger & trigger = pos->second;

	DWORD64 cur_time = group->Server()->CurrentSTime();

	if (status != ELDBTS_OK)
	{
		string err_str = str(boost::format("LocationDB write failed! location_ID: %1%, status: %2%") %
								trigger.location_ID %
								(status == ELDBTS_ExecutionDeadline ? "ELDBTS_ExecutionDeadline" :
								status == ELDBTS_WriteDeadlineReached ? "ELDBTS_WriteDeadlineReached" :
								status == ELDBTS_WriteFailed ? "ELDBTS_WriteFailed" :
								status == ELDBTS_WriteFailedLogicFailure ? "ELDBTS_WriteFailedLogicFailure" : ""));
		Log(err_str.c_str());
	}

	std::map<string, SLocationStatus>::iterator it = status_map.find(trigger.location_ID);
	if (it != status_map.end())
	{
		SLocationStatus & gss = (*it).second;

		gss.locationdb_write_tasks.erase(taskid);

		if (status == ELDBTS_OK)
		{
			//update status_map
			bool write_tasks_pending = (gss.locationdb_write_tasks.size() > 0);
			gss.dirty = write_tasks_pending;
			gss.last_write = cur_time;
			gss.last_confirmed_location_MD5 = MD5(location_data.c_str())();

			//update debug data
			SDebugContext & dc = debug_data[trigger.location_ID];
			dc.location_data = location_data;
			dc.actions.clear();
		}
	}

	switch (trigger.action)
	{
	case ELTA_Write_ProcessSystemMessage:
		{
			SConsoleTask console_task;
			console_task.socketid = trigger.socketid;
			console_task.type = ECTT_Response;
			console_task.cmd = CMD_LOGIC_MESSAGE;

			bool write_failed = (status != ELDBTS_OK);
			if (write_failed)
			{
				// Jesli komenda konsoli miala 'flush=1', ale logika nie uwaza, zeby trzeba zapisac stan lokacji
				// to go nie zapisujemy do bazy danych.
				INT32 gamestate_ID = gamestate_ID_from_location_ID(trigger.location_ID.c_str());
				if (gamestate_ID == 0 && status == ELDBTS_WriteFailedLogicFailure)
				{
					write_failed = false;
				}
			}

			if (write_failed)
			{
				console_task.result = "ERROR Cannot flush location to database.\r\n";
			}
			else
			{
				console_task.result = trigger.params;
			}
	
			global_server->InsertConsoleTask(console_task);
		}
		break;

	case ELTA_Write_WriteChangedGameStateToDatabase:
		{
			if (status != ELDBTS_OK)
			{
				if (it != status_map.end())
				{
					it->second.location_error_waiting_for_release = true;
				}
				else
				{
					EraseLocation(trigger.location_ID.c_str());
					group->ReleaseLocation(trigger.location_ID.c_str());
				}
				DisconnectPlayers(trigger.location_ID.c_str(), "cannot save gamestate");
			}
		}
		break;

	case ELTA_Write_CreateLocation:
		{
			HandleCreateLocationNotification(trigger.command.c_str(), trigger.location_ID.c_str(), location_data.c_str(), (status == ELDBTS_OK));
		}
		break;

	case ELTA_Write_FlushLocation:
		{
			HandleFlushLocationNotification(trigger.location_ID.c_str(), location_data.c_str(), (status == ELDBTS_OK));
		}
		break;

	case ELTA_Write_DeleteLocation:
		{
			if (it != status_map.end())
			{
				it->second.location_released_by_logic = true;
			}
		}
		break;

	case ELTA_Write_GroupShutdown:
		{
			if (it != status_map.end())
			{
				it->second.location_ready_to_gently_close = true;
			}
		}
		break;
	}

	locationdb_triggers.erase(pos);
}
/***************************************************************************/

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
void GLogicEngine::ProcessLogicMessage(GPlayerGame * player, const std::string & command, const std::string & params)
{
	if (!group->CanProcessLogicMessages() || status == ELES_Init_Failed)
	{
		return;
	}
	int user_id=player->player_info.ID;
	const char * location_ID = player->player_info.target_location_ID;
	DWORD64 cur_time=group->Server()->CurrentSTime();
	std::map<string,SLocationStatus>::iterator it=status_map.find(location_ID);
	if (it == status_map.end())
	{
		AddLocationDBTaskRead(ELTA_Read_ProcessLogicMessage, location_ID, command, params, false, player->Socket()->socketid);
		return;
	}
	if (it->second.location_error_waiting_for_release)
	{
		// Jesli otrzymalismy message od klienta w tym stanie, tzn. ze podlaczyl sie on ponownie
		// do lokacji, z ktora sa problemy. Rozlaczamy go od razu.
		DisconnectPlayers(location_ID);
		return;
	}
	//append action info to commit log
	SDebugAction da;
	da.access=player->player_info.access;
	da.message_id=command;
	da.params=params;
	da.source_id=user_id;
	da.timestamp=cur_time;
	SDebugContext &dc=debug_data[location_ID];
	dc.actions.push_back(da);
	//call handler in logic library
	SMetricStats::SCallContext call_ctx = SMetricStats::BeginDuration();
	group->Server()->action_loop_handled_logic_messages++;

	bool result = HandleLogicMessage(user_id, location_ID, player->player_info.access, cur_time, command, params);
	if (!result)
	{
		ReportScriptErrorLocation(user_id, location_ID, "HandleLogicMessage Error");
		it->second.location_error_waiting_for_release = true;
		DisconnectPlayers(player->player_info.target_location_ID);
	}

	if (result)
	{
		group->metrics.CommitDuration("handle_client_message_call_time", call_ctx);
		group->metrics.CommitDuration(str(boost::format("logic_msg_call_time:%1%") % command).c_str(), call_ctx);
	}

	//update status map
	SLocationStatus &gss=(*it).second;
	gss.last_access=cur_time;
	if(!gss.dirty)
	{
		gss.dirty=true;
		gss.last_write=cur_time;
	}
}
/***************************************************************************/
bool GLogicEngine::ProcessSystemMessage(strstream & s, const char * location_ID, const std::string & command, const std::string & params, bool flush, INT32 socketid)
{
	if (!group->CanProcessLogicMessages())
	{
		return true;
	}
	DWORD64 cur_time = group->Server()->CurrentSTime();

	if (status == ELES_Init_Failed)
	{
		s << "Error: Logic initialization failed.\r\n";
		return true;
	}

	std::map<string, SLocationStatus>::iterator it = status_map.find(location_ID);
	if (it == status_map.end())
	{
		AddLocationDBTaskRead(ELTA_Read_ProcessSystemMessage, location_ID, command, params, flush, socketid);
		return false;
	}
	if (it->second.location_error_waiting_for_release)
	{
		s << "ERROR Location is not available.\r\n";
		return true;
	}

	SMetricStats::SCallContext call_ctx = SMetricStats::BeginDuration();
	group->Server()->action_loop_handled_logic_messages++;

	bool result = HandleSystemMessage(s, location_ID, command, params);
	if (!result)
	{
		ReportScriptErrorLocation(0, location_ID, "HandleSystemMessage Error");
		it->second.location_error_waiting_for_release = true;
		DisconnectPlayers(location_ID);
	}

	if (result)
	{
		group->metrics.CommitDuration("handle_system_message_call_time", call_ctx);
		group->metrics.CommitDuration(str(boost::format("logic_msg_call_time:%1%") % command).c_str(), call_ctx);
	}

	//update status map
	SLocationStatus &gss=(*it).second;
	gss.last_access=cur_time;
	if(!gss.dirty)
	{
		gss.dirty=true;
		gss.last_write=cur_time;
	}
	if (flush)
	{
		s.str()[s.pcount()] = 0;
		AddLocationDBTaskWrite(ELTA_Write_ProcessSystemMessage, location_ID, s.str(), socketid);
		return false;
	}
	return true;
}
/***************************************************************************/
void GLogicEngine::ProcessLocationMessage(const char * location_ID, const std::string & command, const std::string & params)
{
	if (!group->CanProcessLogicMessages() || status == ELES_Init_Failed)
	{
		return;
	}

	std::map<string, SLocationStatus>::iterator it = status_map.find(location_ID);
	if (it == status_map.end())
	{
		AddLocationDBTaskRead(ELTA_Read_ProcessLocationMessage, location_ID, command, params, false);
		return;
	}
	if (it->second.location_error_waiting_for_release)
	{
		return;
	}

	DWORD64 cur_time = group->Server()->CurrentSTime();

	SMetricStats::SCallContext call_ctx = SMetricStats::BeginDuration();
	group->Server()->action_loop_handled_logic_messages++;

	bool result = HandleLocationMessage(location_ID, command, params);
	if (!result)
	{
		ReportScriptErrorLocation(0, location_ID, "HandleLocationMessage Error");
		it->second.location_error_waiting_for_release = true;
		DisconnectPlayers(location_ID);
	}

	if (result)
	{
		group->metrics.CommitDuration("handle_location_message_call_time", call_ctx);
		group->metrics.CommitDuration(str(boost::format("logic_msg_call_time:%1%") % command).c_str(), call_ctx);
	}
	//update status map
	SLocationStatus &gss=(*it).second;
	gss.last_access=cur_time;
	if(!gss.dirty)
	{
		gss.dirty=true;
		gss.last_write=cur_time;
	}
}
/***************************************************************************/
void GLogicEngine::ProcessConnectToLocation(INT32 user_id, const char * location_ID)
{
	if (!group->CanProcessLogicMessages() || status == ELES_Init_Failed)
	{
		return;
	}

	std::map<string, SLocationStatus>::iterator it = status_map.find(location_ID);
	if (it == status_map.end())
	{
		AddLocationDBTaskRead(ELTA_Read_ConnectToLocation, location_ID, "", "", false, -1, user_id);
		return;
	}

	// Klient probuje podlaczyc sie ponownie do lokacji, z ktora sa problemy.
	// Nie wywolujemy w tej sytuacji callbacka do logiki z informacja o podlaczeniu.
	if (it->second.location_error_waiting_for_release)
	{
		return;
	}

	bool location_modified = HandlePlayerAdd(user_id, location_ID);

	INT32 gamestate_ID = gamestate_ID_from_location_ID(location_ID);
	if (gamestate_ID > 0)
	{
		DWORD64 cur_time = group->Server()->CurrentSTime();
		SLocationStatus & gss = (*it).second;
		gss.last_access = cur_time;
		if (location_modified)
		{
			gss.dirty = true;
			gss.last_write = cur_time;
		}
	}
}
/***************************************************************************/
void GLogicEngine::ProcessDisconnectFromLocation(INT32 user_id, const char * location_ID)
{
	if (!group->CanProcessLogicMessages() || status == ELES_Init_Failed)
	{
		return;
	}

	std::map<string, SLocationStatus>::iterator it = status_map.find(location_ID);
	if (it == status_map.end())
	{
		AddLocationDBTaskRead(ELTA_Read_DisconnectFromLocation, location_ID, "", "", false, -1, user_id);
		return;
	}

	// Klient probuje podlaczyc sie ponownie do lokacji, z ktora sa problemy.
	// Nie wywolujemy w tej sytuacji callbacka do logiki z informacja o podlaczeniu.
	if (it->second.location_error_waiting_for_release)
	{
		return;
	}

	HandlePlayerDelete(user_id, location_ID);
}
/***************************************************************************/
void GLogicEngine::ProcessGlobalMessage(const std::string & command, const std::string & params)
{
	if (!group->CanProcessLogicMessages())
	{
		return;
	}

	if (status == ELES_Init_Failed)
	{
		return;
	}

	SMetricStats::SCallContext call_ctx = SMetricStats::BeginDuration();
	group->Server()->action_loop_handled_logic_messages++;

	if (HandleGlobalMessage(command, params))
	{
		group->metrics.CommitDuration("handle_global_message_call_time", call_ctx);
		group->metrics.CommitDuration(str(boost::format("logic_msg_call_time:%1%") % command).c_str(), call_ctx);
	}
}
/***************************************************************************/
void GLogicEngine::ProcessSystemGlobalMessage(strstream & s, const std::string & command, const std::string & params)
{
	if (!group->CanProcessLogicMessages())
	{
		return;
	}

	if (status == ELES_Init_Failed)
	{
		s << "Error: Logic initialization failed.\r\n";
		return;
	}

	SMetricStats::SCallContext call_ctx = SMetricStats::BeginDuration();
	group->Server()->action_loop_handled_logic_messages++;

	if (HandleSystemGlobalMessage(s, command, params))
	{
		group->metrics.CommitDuration("handle_system_global_message_call_time", call_ctx);
		group->metrics.CommitDuration(str(boost::format("logic_msg_call_time:%1%") % command).c_str(), call_ctx);
	}
}
/***************************************************************************/
void GLogicEngine::ProcessGetLocationsIdsLike(const char * location_ID, INT32 request_id, const std::string& location_ids)
{
	std::set<std::string> locations_ids_set;

	boost::char_separator<char> sep(",");
	typedef boost::tokenizer<boost::char_separator<char> > TTokenizer;
	TTokenizer tok(location_ids, sep);
	TTokenizer::iterator beg = tok.begin();
	while (beg != tok.end())
	{
		locations_ids_set.insert(*beg);
		beg++;
	}

	HandleGetLocationsIdsLike(location_ID, request_id, locations_ids_set);
}
/***************************************************************************/

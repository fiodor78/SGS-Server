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
extern const char *	analytics_url;
/***************************************************************************/
void GLogicEngine::Log(const char *str)
{
	if (!raport_manager)
	{
		return;
	}

	raport_manager->Raport(str);
	if (global_serverconfig->scripting_config.always_flush_logs)
	{
		raport_manager->Flush();
	}
}
/***************************************************************************/
void GLogicEngine::SendMessageFromLocation(int user_id, const char * location_ID, const std::string &message, const std::string &params)
{
	GPlayerGame *pg=group->FindPlayer(location_ID, user_id);
	if(!pg)
	{
		return;
	}
	if(!pg->Valid())
	{
		return;
	}

	if (pg->timestamp_igm_connect != 0)
	{
		if (message.find("gamestate") != string::npos || message.find("game_state") != string::npos)
		{
			group->metrics.CommitSampleValue("IGM_CONNECT_to_send_gamestate_time", GSERVER->GetClock().Get() - pg->timestamp_igm_connect, "ms");
			pg->timestamp_igm_connect = 0;
		}
	}
	//pg->MsgExt(IGM_GAME_LOGIC_MESSAGE).WT(message).WT(params).A();
	GNetMsg	msg(&group->Server()->MemoryManager());
	msg.WI(IGM_GAME_LOGIC_MESSAGE).WT(message).WT(params).A();
	pg->MsgExtCompressed(msg);
}
/***************************************************************************/
void GLogicEngine::SendBroadcastMessageFromLocation(int exclude_ID, const char * location_ID, const std::string &message, const std::string &params)
{
	TPlayerGameRange p=group->FindPlayers(location_ID);
	TPlayerGameMultiMap::iterator pos;
	GNetMsg	msg(&group->Server()->MemoryManager());
	msg.WI(IGM_GAME_LOGIC_MESSAGE).WT(message).WT(params).A();
	for(pos=p.first;pos!=p.second;++pos)
	{
		GPlayerGame *pg=((*pos).second);
		if(!pg->Valid())
		{
			continue;
		}
		if(pg->player_info.ID!=exclude_ID)
		{
			//pg->MsgExt(IGM_GAME_LOGIC_MESSAGE).WT(message).WT(params).A();
			//FIXME (minor): not very efficient to compress it over and over, but it's also very unlikely to send large broadcasts
			pg->MsgExtCompressed(msg);
		}
	}
}
/***************************************************************************/
void GLogicEngine::SaveLocationSnapshot(const char * location_ID)
{
	std::string location_data;
	// Ustawiamy flage 'logic_reload' na true, zeby lokacje nie bedace gamestatami nie zwrocily nam false.
	// Ponadto z ta flaga mozemy dostac bardziej szczegolowe informacje.
	if(!GetLocationFromScript(location_ID, location_data, false, true))
	{
		return;
	}

	//write data
	SMetricStats::SCallContext call_ctx = SMetricStats::BeginDuration();
	MYSQL *mysql=&(group->Server()->mysql);
	string command=str(boost::format("INSERT INTO `data_location_snapshot` (`location_id`,`location_data`,`created`) VALUES('%1%','%2%','%3%')") % location_ID % GMySQLReal(mysql, location_data)() % group->Server()->CurrentSTime());
	if(!MySQLQuery(mysql,command.c_str()))
	{
		string err_str=str(boost::format("Error executing query: %1%") % command);
		Log(err_str.c_str());
		return;
	}
	group->metrics.CommitDuration("save_snapshot_time", call_ctx);
	//update status_map
	DWORD64 cur_time=group->Server()->CurrentSTime();
	SLocationStatus &gss=status_map[location_ID];
	gss.dirty=false;
	gss.last_access=cur_time;
	gss.last_write=cur_time;
}
/***************************************************************************/
void GLogicEngine::LoadLocationSnapshot(int snapshot_ID, const char * location_ID)
{
	DWORD64 cur_time=group->Server()->CurrentSTime();
	std::string location_data = "";
	bool location_found = false;
	char command[256];
	SMetricStats::SCallContext call_ctx = SMetricStats::BeginDuration();
	sprintf(command,"SELECT `location_data` FROM `data_location_snapshot` WHERE `snapshot_id`='%d'",snapshot_ID);
	MYSQL *mysql=&(group->Server()->mysql);
	if(MySQLQuery(mysql,command))
	{
		MYSQL_RES * result=mysql_store_result(mysql);
		if (result)
		{
			MYSQL_ROW row=mysql_fetch_row(result);
			if (row)
			{
				location_data = row[0];
				location_found = true;
				group->metrics.CommitDuration("load_snapshot_time", call_ctx);
			}
			mysql_free_result(result);
		}
	}
	if(!location_found)
	{
		Log("Snapshot not found in database!!!");
		return;
	}
	if (PutLocationToScript(location_ID, location_data))
	{
		//insert info into status_map
		if(status_map.find(location_ID)==status_map.end())
		{
			SLocationStatus gss;
			gss.dirty=false;
			gss.last_access=cur_time;
			status_map.insert(make_pair(location_ID,gss));
		}
	}
}
/***************************************************************************/
void GLogicEngine::ReportScriptErrorLocation(int user_id, const char * location_ID, const string & description)
{
	DWORD64 cur_time=group->Server()->CurrentSTime();
	if (cur_time - last_error_report < global_serverconfig->scripting_config.report_script_error_interval)
	{
		return;
	}
	last_error_report=cur_time;
	std::map<string, SDebugContext>::iterator it = debug_data.find(location_ID);
	if(it==debug_data.end())
	{
		Log("INTERNAL ERROR in ReportScriptError - can't find debug data!");
		return;
	}
	SDebugContext &dc=(*it).second;
	std::string config_str;
	GetConfig(config_str);
	MYSQL *mysql=&(group->Server()->mysql);
	std::string command=str(boost::format("INSERT INTO `data_bugs` (`id`,`user_id`,`location_id`,`logic_version`,`timestamp`,`description`,`config`,`location_data`) VALUES "
		"('','%1%','%2%','%3%','%4%','%5%','%6%','%7%')") % user_id % location_ID % "" %
		cur_time % GMySQLReal(mysql,description)() % GMySQLReal(mysql,config_str)() % GMySQLReal(mysql,dc.location_data)());
	if(!MySQLQuery(mysql,command.c_str()))
	{
		Log("Error putting bug to database");
		return;
	}
	unsigned long long bug_id=mysql_insert_id(mysql);
	size_t i;
	for(i=0;i<dc.actions.size();i++)
	{
		SDebugAction &da=dc.actions[i];
		command=str(boost::format("INSERT INTO `data_bug_actions` (`bug_id`,`action_id`,`access`,`user_id`,`location_id`,`timestamp`,`message_id`,`params`) VALUES "
			"('%1%','','%2%','%3%','%4%','%5%','%6%','%7%')") % bug_id % da.access % da.source_id % location_ID % da.timestamp %
			GMySQLReal(mysql,da.message_id)() % GMySQLReal(mysql,da.params)());
		if(!MySQLQuery(mysql,command.c_str()))
		{
			Log("Error putting bug action to database");
			return;
		}
	}
}
/***************************************************************************/
void GLogicEngine::RequestLeaderboard(int user_id, const string & leaderboard_key, DWORD64 leaderboard_subkey)
{
	GPlayerGame * pg = group->FindPlayer(location_ID_from_gamestate_ID(user_id).c_str(), user_id);
	if (!pg || !pg->Valid())
	{
		return;
	}

	GTicketLeaderboardGetPtr ticket_leaderboard(new GTicketLeaderboardGet());
	ticket_leaderboard->lb_key = leaderboard_key;
	ticket_leaderboard->lb_subkey = leaderboard_subkey;
	ticket_leaderboard->user_id = user_id;
	pg->TicketInt(ESTClientGlobalInfo, IGMITIC_LEADERBOARD_GET, ticket_leaderboard);
}
/***************************************************************************/
void GLogicEngine::RequestLeaderboardUserPosition(int user_id, const string & leaderboard_key, DWORD64 leaderboard_subkey)
{
	GPlayerGame * pg = group->FindPlayer(location_ID_from_gamestate_ID(user_id).c_str(), user_id);
	if (!pg || !pg->Valid())
	{
		return;
	}

	GTicketLeaderboardGetPtr ticket_leaderboard(new GTicketLeaderboardGet());
	ticket_leaderboard->lb_key = leaderboard_key;
	ticket_leaderboard->lb_subkey = leaderboard_subkey;
	ticket_leaderboard->user_id = user_id;
	pg->TicketInt(ESTClientGlobalInfo, IGMITIC_LEADERBOARD_GET_USER_POSITION, ticket_leaderboard);
}
/***************************************************************************/
void GLogicEngine::RequestLeaderboardStandings(int user_id, const string & leaderboard_key, DWORD64 leaderboard_subkey, INT32 standings_index, INT32 max_results)
{
	GPlayerGame * pg = group->FindPlayer(location_ID_from_gamestate_ID(user_id).c_str(), user_id);
	if (!pg || !pg->Valid())
	{
		return;
	}

	GTicketLeaderboardStandingsGetPtr ticket_leaderboard(new GTicketLeaderboardStandingsGet());
	ticket_leaderboard->lb_key = leaderboard_key;
	ticket_leaderboard->lb_subkey = leaderboard_subkey;
	ticket_leaderboard->user_id = user_id;
	ticket_leaderboard->standings_index = standings_index;
	ticket_leaderboard->max_results = max_results;
	pg->TicketInt(ESTClientGlobalInfo, IGMITIC_LEADERBOARD_GET_STANDINGS, ticket_leaderboard);
}
/***************************************************************************/
void GLogicEngine::RequestLeaderboardBatchInfo(int user_id, const string & leaderboard_key, DWORD64 leaderboard_subkey, INT32 standings_index, vector<INT32> & users_id)
{
	GPlayerGame * pg = group->FindPlayer(location_ID_from_gamestate_ID(user_id).c_str(), user_id);
	if (!pg || !pg->Valid())
	{
		return;
	}

	GTicketLeaderboardBatchInfoGetPtr ticket_leaderboard(new GTicketLeaderboardBatchInfoGet());
	ticket_leaderboard->lb_key = leaderboard_key;
	ticket_leaderboard->lb_subkey = leaderboard_subkey;
	ticket_leaderboard->user_id = user_id;
	ticket_leaderboard->standings_index = standings_index;
	ticket_leaderboard->users_id = users_id;
	pg->TicketInt(ESTClientGlobalInfo, IGMITIC_LEADERBOARD_GET_BATCH_INFO, ticket_leaderboard);
}
/***************************************************************************/
void GLogicEngine::UpdateLeaderboard(int user_id, const string & leaderboard_key, DWORD64 leaderboard_subkey, vector<INT32> & scores)
{
	GPlayerGame * pg = group->FindPlayer(location_ID_from_gamestate_ID(user_id).c_str(), user_id);
	if (pg && !pg->Valid())
	{
		pg = NULL;
	}

	GTicketLeaderboardSetPtr ticket_leaderboard(new GTicketLeaderboardSet());
	ticket_leaderboard->lb_key = leaderboard_key;
	ticket_leaderboard->lb_subkey = leaderboard_subkey;
	ticket_leaderboard->user_id = user_id;
	ticket_leaderboard->scores = scores;
	ticket_leaderboard->stable_sequence_identifier = str(boost::format("%1%_%2%_%3%") % user_id % leaderboard_key % leaderboard_subkey);
	if (pg)
	{
		pg->TicketInt(ESTClientGlobalInfo, IGMITIC_LEADERBOARD_SET, ticket_leaderboard);
	}
	else
	{
		group->Ticket(ESTClientGlobalInfo, IGMITIC_LEADERBOARD_SET, ticket_leaderboard);
	}
}
/***************************************************************************/
void GLogicEngine::SendMessageToLocation(const char * location_ID, const std::string & command, const std::string & params)
{
	SLogicMessage internal_message;
	internal_message.target_location_ID = location_ID;
	internal_message.command = command;
	internal_message.params = params;
	group->AddOutgoingLogicMessage(internal_message);
}
/***************************************************************************/
void GLogicEngine::SendMessage(int user_id,int gamestate_ID,const std::string &message,const std::string &params)
{
	SendMessageFromLocation(user_id, location_ID_from_gamestate_ID(gamestate_ID).c_str(), message, params);
}
/***************************************************************************/
void GLogicEngine::SendBroadcastMessage(int exclude_ID,int gamestate_ID,const std::string &message,const std::string &params)
{
	SendBroadcastMessageFromLocation(exclude_ID, location_ID_from_gamestate_ID(gamestate_ID).c_str(), message, params);
}
/***************************************************************************/
void GLogicEngine::SaveSnapshot(int gamestate_ID)
{
	SaveLocationSnapshot(location_ID_from_gamestate_ID(gamestate_ID).c_str());
}
/***************************************************************************/
void GLogicEngine::LoadSnapshot(int snapshot_ID, int gamestate_ID)
{
	LoadLocationSnapshot(snapshot_ID, location_ID_from_gamestate_ID(gamestate_ID).c_str());
}
/***************************************************************************/
void GLogicEngine::ReportScriptError(int user_id,int gamestate_ID,const string &description)
{
	group->metrics.CommitEvent("logic_reported_script_errors");
	ReportScriptErrorLocation(user_id, location_ID_from_gamestate_ID(gamestate_ID).c_str(), description);
}
/***************************************************************************/
void GLogicEngine::SendMessageToGamestate(int gamestate_ID, const std::string & command, const std::string & params)
{
	SendMessageToLocation(location_ID_from_gamestate_ID(gamestate_ID).c_str(), command, params);
}
/***************************************************************************/
void GLogicEngine::SubmitAnalyticsEvent(const char * Game_ID, DWORD64 Player_ID, const char * event_name_with_category, const std::map<string, string> & options)
{
	map<string, string> analytics_options = options;
	analytics_options["Game_ID"] = Game_ID;
	analytics_options["Player_ID"] = str(boost::format("%1%") % Player_ID);

	const char * last_dot = strrchr(event_name_with_category, '.');
	if (last_dot == NULL)
	{
		analytics_options["Event_name"] = event_name_with_category;
	}
	else
	{
		string event_category;
		event_category.assign(event_name_with_category, last_dot - event_name_with_category);
		analytics_options["Event_category"] = event_category;
		analytics_options["Event_name"] = last_dot + 1;
	}

	string json_encoded = "{";
	map<string, string>::const_iterator it;
	for (it = analytics_options.begin(); it != analytics_options.end(); )
	{
		string param;
		bool is_number = true;

		if (STRNICMP(it->first.c_str(), "Event_option_", 13) == 0 ||
			STRICMP(it->first.c_str(), "OS_version") == 0)
		{
			is_number = false;
		}
		else
		{
			try
			{
				boost::lexical_cast<double>(it->second);
			}
			catch (boost::bad_lexical_cast )
			{
				is_number = false;
			}
		}

		if (is_number)
		{
			param = str(boost::format("\"%1%\":%2%") % escape_json_string(it->first) % it->second);
		}
		else
		{
			param = str(boost::format("\"%1%\":\"%2%\"") % escape_json_string(it->first) % escape_json_string(it->second));
		}

		json_encoded += param;
		it++;
		if (it != analytics_options.end())
		{
			json_encoded += ",";
		}
	}
	json_encoded += "}";

	map<string, string> http_request_params;
	http_request_params["data"] = json_encoded;

	PerformFireAndForgetHTTPRequest(analytics_url, "POST", &http_request_params);
}
/***************************************************************************/
INT32 GLogicEngine::PerformHTTPRequest(const char * location_ID, const char * url, const char * request_method, const std::map<string, string> * params , const char * secret_key)
{
	SHTTPRequestTask	task;

	task.group_id = group->GetID();
	task.location_ID = location_ID;
	task.execution_deadline = group->Server()->CurrentSTime() + 30;
	task.request_method = request_method;
	task.url = url;
	if (params)
	{
		task.params = *params;
	}
	if (secret_key)
	{
		task.secret_key = secret_key;
	}

	return GSERVER->AddHTTPRequestTask(task);
}
/***************************************************************************/
INT32 GLogicEngine::PerformHTTPRequest(const char * location_ID, const char * url, const char * request_method, const string & payload, const string & content_type, const char * secret_key)
{
	SHTTPRequestTask task;
	task.location_ID = location_ID;
	task.group_id = group->GetID();
	task.execution_deadline = group->Server()->CurrentSTime() + 30;
	task.request_method = request_method;
	task.url = url;
	task.payload = payload;
	task.content_type = content_type;
	if (secret_key)
	{
		task.secret_key = secret_key;
	}
	return GSERVER->AddHTTPRequestTask(task);
}
/***************************************************************************/
void GLogicEngine::PerformFireAndForgetHTTPRequest(const char * url, const char * request_method, const std::map<string, string> * params, const char * secret_key)
{
	SHTTPRequestTask	task;

	task.group_id = group->GetID();
	task.execution_deadline = group->Server()->CurrentSTime() + 30;
	task.request_method = request_method;
	task.url = url;
	if (params)
	{
		task.params = *params;
	}
	if (secret_key)
	{
		task.secret_key = secret_key;
	}

	GSERVER->AddHTTPFireAndForgetRequestTask(task);
}
/***************************************************************************/
void GLogicEngine::PerformFireAndForgetHTTPRequest(const char * url, const char * request_method, const string & payload, const string & content_type, const char * secret_key)
{
	SHTTPRequestTask task;
	task.group_id = group->GetID();
	task.execution_deadline = group->Server()->CurrentSTime() + 30;
	task.request_method = request_method;
	task.url = url;
	task.payload = payload;
	task.content_type = content_type;
	if (secret_key)
	{
		task.secret_key = secret_key;
	}
	GSERVER->AddHTTPFireAndForgetRequestTask(task);
}
/***************************************************************************/
void GLogicEngine::DisconnectPlayer(int user_id, const char * location_ID, const char * reason)
{
	GPlayerGame * pg = group->FindPlayer(location_ID, user_id);
	if (pg)
	{
		if (pg->Valid())
		{
			pg->MsgExt(IGM_CONNECTION_CLOSE).WT(*reason == 0 ? "forced disconnect" : reason).A();
		}
		// Ustawiamy 'transferring_location_timestamp', zeby nie brac juz pod uwage zadnych komunikatow od gracza.
		pg->transferring_location_timestamp = group->Server()->CurrentSTime();
	}
}
/***************************************************************************/
void GLogicEngine::CreateLocation(const char * location_ID, const char * new_location_ID, const char * new_location_data)
{
	if (status_map.find(location_ID) != status_map.end())
	{
		AddLocationDBTaskWrite(ELTA_Write_CreateLocation, new_location_ID, new_location_data, -1, location_ID);
	}
}
/***************************************************************************/
void GLogicEngine::FlushLocation(const char * location_ID)
{
	if (status_map.find(location_ID) != status_map.end())
	{
		AddLocationDBTaskWrite(ELTA_Write_FlushLocation, location_ID);
	}
}
/***************************************************************************/
INT32 GLogicEngine::GetLocationsIdsLike(const char * location_ID, const std::string& pattern)
{
	return AddLocationDBTaskRead(ELTA_Read_LocationsIdsLikeFromDB, location_ID, "", pattern);
}
/***************************************************************************/
void GLogicEngine::ReleaseLocation(const char * location_ID, bool destroy_location)
{
	if (status_map.find(location_ID) != status_map.end())
	{
		if (destroy_location)
		{
			AddLocationDBTaskWrite(ELTA_Write_DeleteLocation, location_ID);
		}
		else
		{
			SLocationStatus & gss = status_map[location_ID];
			gss.location_released_by_logic = true;
		}
	}
}
/***************************************************************************/
void GLogicEngine::GetGloballyUniqueLocationSuffix(string & unique_location_suffix)
{
	unique_location_suffix = str(boost::format(".%1%.%2%") % group->unique_group_id % group->seq_location_suffix_id++);
}
/***************************************************************************/
void GLogicEngine::ReplaceVulgarWords(const char* location_ID, const std::string &text, const std::string& params)
{
	group->MsgInt(ESTClientCenzor, IGMI_CENZOR_MESSAGE).WT(text.substr(0, 511)).WT(location_ID).WT(params).A();
}
/***************************************************************************/
INT32 GLogicEngine::GetGamestateIDFromLocationID(const char * location_ID)
{
	return gamestate_ID_from_location_ID(location_ID);
}
/***************************************************************************/
string GLogicEngine::GetLocationIDFromGamestateID(INT32 gamestate_ID)
{
	return location_ID_from_gamestate_ID(gamestate_ID);
}
/***************************************************************************/
void GLogicEngine::RegisterBuddies(int user_id, const std::set<int>& buddies_ids)
{
	GPlayerGame * pg = group->FindPlayer(location_ID_from_gamestate_ID(user_id).c_str(), user_id);
	if (!pg || pg && !pg->Valid())
	{
		return;
	}

	pg->registered_buddies = buddies_ids;

	SRegisterBuddies register_buddies;
	register_buddies.user_id = user_id;
	register_buddies.buddies_ids = buddies_ids;
	pg->MsgInt(ESTClientGlobalInfo, IGMI_BUDDIES_REGISTER).W(register_buddies).A();
}
/***************************************************************************/
void GLogicEngine::SendGlobalMessage(const std::string &message, const std::string &params)
{
	group->MsgInt(ESTClientGlobalMessage, IGMI_GLOBAL_MESSAGE).WT(message).WT(params).Wb(false).A();
}
/***************************************************************************/

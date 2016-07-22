/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"
#include "headers_game.h"

extern GServerBase * global_server;
extern GNetParserBase	msg_base;
/***************************************************************************/
GGroup::GGroup()
{
	services=NULL;
	server_game=NULL;
	status = EGS_Initializing;
	last_confirmed_heartbeat = 0;
	shutdown_state_timestamp = 0;
	transferring_players_deadline = 0;
	location_address_cache.clear();
	outgoing_logic_messages.clear();
	new_outgoing_logic_messages.clear();
	incoming_logic_messages.clear();
	seq_locationdb_taskid = 1;
	unique_group_id = 0;
	seq_location_suffix_id = 1;
};
/***************************************************************************/
//init klasy GGroup
void GGroup::Init(GServerBase* server_game_p,TServiceMap* services_p, int id_p, EGroupType type_p)
{
	services=services_p;
	server_game=server_game_p;
	identity.id=id_p;
	type = type_p;

	//init logic raport
	string name = game_name;
	name += str(boost::format("_LogicVM%1%") % GetID());
	logic_raport_manager.Init(global_serverconfig->scripting_config.raport_path.c_str(), name.c_str());
	logic_raport_manager.UpdateName(Server()->GetClock());
	logic_raport_manager.AddLineTimeSignature(true);

	if (STRICMP(global_serverconfig->scripting_config.logic_type.c_str(), "cpp") != 0)
	{
		current_scripting_engine = new GLogicEngineLUA();
	}
	else
	{
		current_scripting_engine = new GLogicEngineCPP();
	}

	if (identity.id)
	{
		current_scripting_engine->Init(this, &logic_raport_manager);
	}
	// Grupa moze przejsc w stan EGS_Ready dopiero po potwierdzonym zrzuceniu przypisan lokacji.
	status = EGS_Initializing;
	assigned_locations.clear();
	if (identity.id > 0)
	{
		metrics.CommitCurrentValue("locations_assigned", 0);
		metrics.CommitCurrentValue("clients_connected", 0);
		metrics.CommitCurrentValue("clients_connected_to_gamestate", 0);
	}

	//konfigurujemy zegary
	InitClock();

	write_locations_deadline = 0;

	// Ustalamy 'unique_group_id'
	if (identity.id)
	{
		MYSQL * mysql = &(Server()->mysql);
		char command[512];
		sprintf(command, "SELECT `get_unique_group_id`()");
		if (MySQLQuery(mysql, command))
		{
			MYSQL_RES * result = mysql_store_result(mysql);
			if (result)
			{
				MYSQL_ROW row = mysql_fetch_row(result);
				if (row && row[0])
				{
					unique_group_id = ATOI64(row[0]);
				}
				mysql_free_result(result);
			}
		}
		if (unique_group_id == 0)
		{
			unique_group_id = truerng.Rnd(0x7fff7fff);
		}
	}
};
/***************************************************************************/
void GGroup::Destroy()
{
	if (identity.id)
	{
		WaitForLocationsFlush();

		current_scripting_engine->Destroy();
		INT32 a, count = old_scripting_engines.size();
		for (a = 0; a < count; a++)
		{
			old_scripting_engines[a]->Destroy();
		}

		status = EGS_Stopped;
		GRAPORT(str(boost::format("[GROUP  %1%] Status: EGS_Stopped  Reason: Destroy") % identity.id));

		UpdateGroupHeartBeat();
		ReleaseLocation("");
	}

	logic_raport_manager.Destroy();

	index_rid.clear();
	index_target.clear();
}
/***************************************************************************/
void GGroup::ProcessConnection(GNetMsg & msg)
{
	msg.WI(IGMI_CONNECT).WT(game_name).WI(game_instance);
	msg.WUI(Server()->sock_service.addr_internal).WUS(Server()->sock_service.port_internal);
	msg.WI(identity.id);
	msg.A();
}
/***************************************************************************/
bool GGroup::ProcessGroupTargetMessage(GSocket *socket,GNetMsg & msg,int message,GNetTarget & target)
{
	if (target.timestamp > 0)
	{
		DWORD64 timeout = GSERVER->GetClock().Get() - target.timestamp;
		SNetMsgDesc * format = msg_base.Get(message, ENetCmdInternal);
		GSocketInternalClient * si = static_cast<GSocketInternalClient*>(socket);
		metrics.CommitSampleValue(str(boost::format("service_response_time:%1%/%2%") % GetServiceName(si->GetClientServiceType()) % (!format ? "" : format->name)).c_str(), timeout, "ms");
	}
	
	if(target.tg!=TN && target.tp==TN)
	{
		return ProcessTargetMessage(socket,msg,message,target);
	}
	//players
	if(target.tp==TA)
	{
		TPlayerMap::iterator pos;
		for(pos=index_rid.begin();pos!=index_rid.end();++pos)
		{
			GPlayer *p=pos->second;
			p->ProcessTargetMessage(msg, message);
			if(!p->Continue()) ::Delete(p);
		}
		return true;
	}
	if(target.tp!=TN)
	{
		GPlayer *p=NULL;
		TPlayerMap::iterator pos;
		pos=index_rid.find(target.tp);
		if(pos==index_rid.end()) 
		{
			return true;
		}
		else
		{
			p=pos->second;
			bool ret=p->ProcessTargetMessage(msg, message);
			if(!p->Continue()) ::Delete(p);
			return ret;
		}
	}
	return false;
}
/***************************************************************************/
GNetMsg & GGroup::MsgInt(EServiceTypes type,INT32 message)
{
	Service(type)->AddWrite();
	if(message>IGMIT_FIRST && message<IGMIT_LAST)
	{
		GNetTarget		target;
		target.Init(GetID(),TN,TN);
		Service(type)->Msg().WI(message).W(target);
	}
	else
	{
		Service(type)->Msg().WI(message);
	}
	return Service(type)->Msg();
};
/***************************************************************************/
GNetMsg & GGroup::MsgInt(GSocket * s,GNetMsg & msg, INT32 message)
{
	s->AddWrite();
	if(message>IGMIT_FIRST && message<IGMIT_LAST)
	{
		GNetTarget		target;
		target.Init(GetID(),TN,TN);
		msg.WI(message).W(target);
	}
	else
	{
		msg.WI(message);
	}
	return msg;
};
/***************************************************************************/
void GGroup::Ticket(EServiceTypes type,int message, GTicketPtr ticket)
{
	ticket->message=message;
	ticket->target.Init(GetID(),TN,TN);
	Service(type)->Ticket(ticket);
};
/***************************************************************************/
GTicketInternalClient *	GGroup::Service(EServiceTypes  type)
{
	GTicketInternalClient * service=NULL;
	service=(*services)[type];
	return service;
};
/***************************************************************************/
GGroupManager * GGroup::Server()
{
	return (GGroupManager*)server_game;
};
/***************************************************************************/
void GGroup::AssignLocation(const char * location_ID)
{
	// Nie zapisujemy przydzielenia lokacji w stanie EGS_Stopped, bo nie bedziemy mogli sie potem zreanimowac.
	if (status == EGS_Stopped)
	{
		return;
	}

	assigned_locations.insert(location_ID);
	metrics.CommitCurrentValue("locations_assigned", assigned_locations.size());
}
/***************************************************************************/
void GGroup::ReleaseLocation(const char * location_ID)
{
	if (identity.id)
	{
		if (*location_ID == 0)
		{
			assigned_locations.clear();
			GNetMsg & msg = MsgInt(ESTClientNodeBalancer, IGMI_LOCATION_RELEASE);
			msg.WT("").A();
		}
		else
		{
			set<string>::iterator it = assigned_locations.find(location_ID);
			if (it != assigned_locations.end())
			{
				assigned_locations.erase(it);
				GNetMsg & msg = MsgInt(ESTClientNodeBalancer, IGMI_LOCATION_RELEASE);
				msg.WT(location_ID).A();
			}
		}
		metrics.CommitCurrentValue("locations_assigned", assigned_locations.size());
	}
}
/***************************************************************************/
bool GGroup::IsLocationAssigned(const char * location_ID)
{
	return (assigned_locations.find(location_ID) != assigned_locations.end());
}
/***************************************************************************/
bool GGroup::CanCloseGently()
{
	if (GetLoadedLocationsCount() > 0)
	{
		return false;
	}
	if (assigned_locations.size() > 0)
	{
		return false;
	}
	if (outgoing_logic_messages.size() > 0 ||
		new_outgoing_logic_messages.size() > 0 ||
		incoming_logic_messages.size() > 0)
	{
		return false;
	}
	else
	{
		boost::mutex::scoped_lock lock(mtx_logic_messages_transfer_queue);
		if (logic_messages_transfer_queue.size() > 0)
		{
			return false;
		}
	}
	if (index_rid.size() > 0)
	{
		// W przypadku gdy gasimy grupe 'gently' i od momentu zrzucenia wszystkich lokacji (przejscia w stan EGS_Stopped) minelo 15 sekund to
		// mozemy spokojnie zignorowac podlaczonych graczy. Wszyscy dostali komunikat IGM_CONNECTION_RETRY/IGM_CONNECTION_CLOSE,
		// wiec jesli sie jeszcze nie odlaczyli to ich wina.
		if (status != EGS_Stopped || shutdown_state_timestamp == 0 || shutdown_state_timestamp + 15 > Server()->CurrentSTime())
		{
			return false;
		}
	}
	return true;
}
/***************************************************************************/
bool GGroup::GetLocationAddressFromCache(string & location_ID, SGroupAddress & address)
{
	TGroupAddressMap::iterator pos;
	pos = location_address_cache.find(location_ID);
	if (pos != location_address_cache.end())
	{
		address = pos->second;
		return true;
	}
	return false;
}
/***************************************************************************/
void GGroup::PutLocationAddressToCache(string & location_ID, DWORD32 host, USHORT16 port, INT32 group_id)
{
	TGroupAddressMap::iterator pos;
	pos = location_address_cache.find(location_ID);
	if (pos != location_address_cache.end())
	{
		if (host == 0)
		{
			location_address_cache.erase(pos);
			return;
		}
		pos->second.host = host;
		pos->second.port = port;
		pos->second.group_id = group_id;
		pos->second.expiration_time = Server()->CurrentSTime() + 60 * 60;
		return;
	}

	if (host == 0)
	{
		return;
	}
	SGroupAddress address;
	address.host = host;
	address.port = port;
	address.group_id = group_id;
	address.expiration_time = Server()->CurrentSTime() + 60 * 60;
	location_address_cache.insert(pair<string, SGroupAddress>(location_ID, address));
}
/***************************************************************************/
void GGroup::UpdateLocationAddressCache()
{
	DWORD64 now = Server()->CurrentSTime();

	TGroupAddressMap::iterator pos;
	for (pos = location_address_cache.begin(); pos != location_address_cache.end(); )
	{
		if (pos->second.expiration_time <= now)
		{
			location_address_cache.erase(pos++);
			continue;
		}
		pos++;
	}
}
/***************************************************************************/
bool GGroup::ReloadLogic()
{
	if (!identity.id)
	{
		return false;
	}

	// W przypadku przeladowania logiki nie zwalniamy przypisania lokacji do grupy.
	GLogicEngine * new_engine;

	if (STRICMP(global_serverconfig->scripting_config.logic_type.c_str(), "cpp") != 0)
	{
		new_engine = new GLogicEngineLUA();
	}
	else
	{
		new_engine = new GLogicEngineCPP();
	}
	new_engine->Init(this, &logic_raport_manager);

	bool reload_success = (new_engine->status != ELES_Init_Failed);
	if (reload_success)
	{
		old_scripting_engines.push_back(current_scripting_engine);
		current_scripting_engine = new_engine;
	}
	else
	{
		// Jesli nie udalo sie zainicjalizowac nowego modulu logiki to zostawiamy stary.
		new_engine->Destroy();
		delete new_engine;

		// Przestajemy podlaczac nowych graczy do starego modulu logiki i bedziemy grupe zwracac jako EEGS_BUSY,
		// zeby stopniowo pozbyc sie tez graczy juz podlaczonych.
		if (current_scripting_engine->status != ELES_Init_Failed)
		{
			current_scripting_engine->status = ELES_Old_Logic;
		}
	}

	// Aktualizujemy status, bo mogl sie zmienic EEGS_READY <-> EEGS_BUSY.
	UpdateGroupHeartBeat();

	return reload_success;
}
/***************************************************************************/
void GGroup::WaitForLocationsFlush()
{
	while (GetLoadedLocationsCount() > 0)
	{
		current_scripting_engine->WriteChangedLocationsToDatabaseGroupShutdown();
		INT32 a, count = old_scripting_engines.size();
		for (a = 0; a < count; a++)
		{
			old_scripting_engines[a]->WriteChangedLocationsToDatabaseGroupShutdown();
		}

		ClearExpiredSystemMessages(true);

		ProcessLocationDBTaskResults();
		ProcessHTTPRequestTaskResults();

		// Process outgoing socket traffic (zeby moc wyslac group status = EEGS_BUSY i zamykac komendy konsoli)
		Server()->ProcessTimeOut();
		Server()->ProcessConsoleTasks();
		Server()->ProcessPoll();

		Sleep(100 + truerng.Rnd(250));
		Server()->SetCurrentSTime();
	}
}
/***************************************************************************/
void GGroup::ProcessLocationDBTaskResults()
{
	vector<SLocationDBTaskResult> task_results;
	{
		boost::mutex::scoped_lock lock(GSERVER->mtx_locationdb_tasks_results);
		task_results = GSERVER->locationdb_tasks_results[GetID()];
		GSERVER->locationdb_tasks_results[GetID()].clear();
	}

	INT32 a, count = task_results.size();
	for (a = 0; a < count; a++)
	{
		SLocationDBTaskResult & task_result = task_results[a];

		if (task_result.direction == ELDBTD_Read || task_result.direction == ELDBTD_Read_Locations_Ids)
		{
			GetScriptingEngine(task_result.location_ID.c_str())->ExecuteLocationDBTriggerRead(task_result.taskid, task_result.status, task_result.location_data);
		}
		else
		{
			GetScriptingEngine(task_result.location_ID.c_str())->ExecuteLocationDBTriggerWrite(task_result.taskid, task_result.status, task_result.location_data);
		}
	}
	task_results.clear();
}
/***************************************************************************/
void GGroup::ProcessHTTPRequestTaskResults()
{
	vector<SHTTPRequestTask> task_results;
	{
		boost::mutex::scoped_lock lock(GSERVER->mtx_http_request_tasks_results);
		task_results = GSERVER->http_request_tasks_results[GetID()];
		GSERVER->http_request_tasks_results[GetID()].clear();
	}

	INT32 a, count = task_results.size();
	for (a = 0; a < count; a++)
	{
		SHTTPRequestTask & task_result = task_results[a];
		if (task_result.location_ID != "")
		{
			GetScriptingEngine(task_result.location_ID.c_str())->HandleHTTPResponse(task_result.location_ID.c_str(), task_result.http_request_id, task_result.response_code, task_result.response);
		}
	}
	task_results.clear();
}
/***************************************************************************/
EEffectiveGroupStatus GGroup::GetEffectiveStatus()
{
	if (status == EGS_Ready && current_scripting_engine->status == ELES_Ready)
	{
		return EEGS_READY;
	}
	if (status == EGS_Stopped)
	{
		return EEGS_STOPPED;
	}
	return EEGS_BUSY;
}
/***************************************************************************/
bool GGroup::CanProcessLogicMessages()
{
	return (status == EGS_Ready || status == EGS_Releasing_Gently || status == EGS_Transferring);
}
/***************************************************************************/
INT32 GGroup::GetLoadedLocationsCount()
{
	INT32 result = 0;
	if (current_scripting_engine)
	{
		result += current_scripting_engine->GetLocationsCount();
	}
	INT32 a, count = old_scripting_engines.size();
	for (a = 0; a < count; a++)
	{
		result += old_scripting_engines[a]->GetLocationsCount();
	}
	return result;
}
/***************************************************************************/
GLogicEngine * GGroup::GetScriptingEngine(const char * location_ID)
{
	INT32 a, count = old_scripting_engines.size();
	for (a = 0; a < count; a++)
	{
		if (old_scripting_engines[a]->IsLocationLoaded(location_ID))
		{
			return old_scripting_engines[a];
		}
	}
	
	return current_scripting_engine;
}
/***************************************************************************/
ELogicEngineStatus GGroup::GetScriptingEngineStatus()
{
	return (current_scripting_engine ? current_scripting_engine->status : ELES_Init_Failed);
}
/***************************************************************************/
void GGroup::PrintLogicStatistics(strstream & s)
{
	if (current_scripting_engine)
	{
		current_scripting_engine->PrintStatistics(s);
	}
	INT32 a, count = old_scripting_engines.size();
	for (a = 0; a < count; a++)
	{
		s << "Old logic #" << (a + 1) << ": \r\n";
		old_scripting_engines[a]->PrintStatistics(s);
	}
}
/***************************************************************************/
void GGroup::ProcessGlobalMessage(const std::string & command, const std::string & params)
{
	if (current_scripting_engine)
	{
		current_scripting_engine->ProcessGlobalMessage(command, params);
	}
	INT32 a, count = old_scripting_engines.size();
	for (a = 0; a < count; a++)
	{
		old_scripting_engines[a]->ProcessGlobalMessage(command, params);
	}
}
/***************************************************************************/
void GGroup::ProcessSystemGlobalMessage(strstream & s, const std::string & command, const std::string & params)
{
	if (current_scripting_engine)
	{
		current_scripting_engine->ProcessSystemGlobalMessage(s, command, params);
	}
	INT32 a, count = old_scripting_engines.size();
	for (a = 0; a < count; a++)
	{
		s << "Old logic #" << (a + 1) << ": \r\n";
		old_scripting_engines[a]->ProcessSystemGlobalMessage(s, command, params);
	}
}
/***************************************************************************/
void GGroup::TransferLocationsToCurrentScriptingEngine()
{
	// W jednym wykonaniu funkcji probujemy wykonac tylko 1 operacje: usuniecie wolnej starej logiki, albo przepiecie jednej lokacji.

	INT32 a, count = old_scripting_engines.size();
	for (a = 0; a < count; a++)
	{
		if (old_scripting_engines[a]->GetLocationsCount() == 0 &&
			old_scripting_engines[a]->GetLocationsDBTriggersCount() == 0)
		{
			// W 'old_scripting_engines[a]' nie ma juz zadnych lokacji.
			// Tak wiec Destroy() nie spowoduje zadnego zapisu do bazy danych.
			old_scripting_engines[a]->Destroy();
			delete old_scripting_engines[a];

			old_scripting_engines.erase(old_scripting_engines.begin() + a);
			return;
		}

		if (old_scripting_engines[a]->MoveRandomLocationTo(current_scripting_engine))
		{
			return;
		}
	}
}
/***************************************************************************/

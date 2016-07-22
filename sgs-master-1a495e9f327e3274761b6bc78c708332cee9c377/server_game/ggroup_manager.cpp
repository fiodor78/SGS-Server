/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"
#include "headers_game.h"

/***************************************************************************/
GGroupManager::GGroupManager(const SSockService & sock_service_p):
GServerBase(),
sock_service(sock_service_p)
{
	language_pos=-1;language_max_pos=1;
	int a;
	for (a=0;a<MAX_LANGUAGES;a++) languages[a]=0;
	sql_config_initialized=false;
}
/***************************************************************************/
bool GGroupManager::Init(int group_number_p, EGroupType group_type_p)
{
	RAPORT("---Initializing room manager nr %d---",group_number_p);
	if(!InitSQL()) return false;

	group.Init(this, &socket_internal_map, group_number_p, group_type_p);
	GServerBase::Init();

	return true;
}
/***************************************************************************/
void GGroupManager::Destroy()
{
	group.Destroy();
	// Wywolujemy Communicate() zeby wyslac jeszcze ew. czekajace tickety do serwisow/grup.
	TServiceVector::iterator it;
	for (it = socket_internal.begin(); it != socket_internal.end(); it++)
	{
		(*it)->Communicate();
	}
	DestroySQL();
}
/***************************************************************************/
void GGroupManager::ActionLoopBegin()
{
	action_loop_call_ctx = SMetricStats::BeginDuration();
	action_loop_handled_logic_messages = 0;
}
/***************************************************************************/
void GGroupManager::ActionLoopEnd()
{
	group.metrics.CommitDuration("action_loop_time", action_loop_call_ctx);
	group.metrics.CommitSampleValue("action_loop_handled_logic_messages", action_loop_handled_logic_messages, "msgs");
}
/***************************************************************************/
void GGroupManager::InitInternalClients()
{
	InitInternalClient(i_access,ESTClientAccess,group.GetID());
	i_access->msg_callback_target=boost::bind(&GGroup::ProcessGroupTargetMessage,&group,_1,_2,_3,_4);
	i_access->msg_callback_internal=boost::bind(&GGroup::ProcessInternalMessage,&group,_1,_2,_3);
	i_access->msg_callback_connect=boost::bind(&GGroup::ProcessConnection,&group,_1);
	InitInternalClient(i_cache,ESTClientCache,group.GetID());
	i_cache->msg_callback_target=boost::bind(&GGroup::ProcessGroupTargetMessage,&group,_1,_2,_3,_4);
	i_cache->msg_callback_internal=boost::bind(&GGroup::ProcessInternalMessage,&group,_1,_2,_3);
	i_cache->msg_callback_connect=boost::bind(&GGroup::ProcessConnection,&group,_1);
	InitInternalClient(i_globalinfo,ESTClientGlobalInfo,group.GetID());
	i_globalinfo->msg_callback_target=boost::bind(&GGroup::ProcessGroupTargetMessage,&group,_1,_2,_3,_4);
	i_globalinfo->msg_callback_internal=boost::bind(&GGroup::ProcessInternalMessage,&group,_1,_2,_3);
	i_globalinfo->msg_callback_connect=boost::bind(&GGroup::ProcessConnection,&group,_1);
	InitInternalClient(i_nodebalancer,ESTClientNodeBalancer,group.GetID());
	i_nodebalancer->msg_callback_target=boost::bind(&GGroup::ProcessGroupTargetMessage,&group,_1,_2,_3,_4);
	i_nodebalancer->msg_callback_internal=boost::bind(&GGroup::ProcessInternalMessage,&group,_1,_2,_3);
	i_nodebalancer->msg_callback_connect=boost::bind(&GGroup::ProcessConnection,&group,_1);
	InitInternalClient(i_cenzor, ESTClientCenzor, group.GetID());
	i_cenzor->msg_callback_target = boost::bind(&GGroup::ProcessGroupTargetMessage, &group, _1, _2, _3, _4);
	i_cenzor->msg_callback_internal = boost::bind(&GGroup::ProcessInternalMessage, &group, _1, _2, _3);
	i_cenzor->msg_callback_connect = boost::bind(&GGroup::ProcessConnection, &group, _1);
	InitInternalClient(i_globalmessage, ESTClientGlobalMessage, group.GetID());
	i_globalmessage->msg_callback_target = boost::bind(&GGroup::ProcessGroupTargetMessage, &group, _1, _2, _3, _4);
	i_globalmessage->msg_callback_internal = boost::bind(&GGroup::ProcessInternalMessage, &group, _1, _2, _3);
	i_globalmessage->msg_callback_connect = boost::bind(&GGroup::ProcessConnection, &group, _1);
}
/***************************************************************************/
INT32 GGroupManager::CallAction(strstream & s, ECmdConsole cmd, map<string, string> & options)
{
	switch(cmd)
	{
		case CMD_ACTION_MEMORY_INFO:
			s<<boost::format("-------------------- Group %d --------------------\r\n") % group.GetID();
			GServerBase::CallAction(s, cmd, options);
			break;
		case CMD_ACTION_SERVICE_INFO:
		case CMD_ACTION_SERVICE_REINIT:
			GServerBase::CallAction(s, cmd, options);
			break;
		case CMD_LOGIC_RELOAD:
			s<<boost::format("-------------------- Group %d --------------------\r\n") % group.GetID();
			{
				bool reload_result = group.ReloadLogic();
				s << (reload_result ? "OK" : "ERROR: Cannot initialize logic module.") << "\r\n";
			}
			break;
		case CMD_LOGIC_STATS:
			s<<boost::format("-------------------- Group %d --------------------\r\n") % group.GetID();
			{
				INT32 interval_seconds = ATOI(options["interval"].c_str());
				if (interval_seconds <= 0)
				{
					interval_seconds = 0;
				}
				s << group.GetID() << ";" << interval_seconds << "\r\n";
				group.metrics.Print(s, interval_seconds, ATOI(options["csv"].c_str()) != 1);
			}
			if (ATOI(options["csv"].c_str()) != 1)
			{
				group.PrintLogicStatistics(s);
			}
			break;
		case CMD_LOGIC_CLEARSTATS:
			s<<boost::format("-------------------- Group %d --------------------\r\n") % group.GetID();
			group.metrics.Zero();
			s << "Statistics zeroed.\r\n";
			break;
		case CMD_LOGIC_GLOBAL_MESSAGE:
			s << boost::format("-------------------- Group %d --------------------\r\n") % group.GetID();
			group.ProcessSystemGlobalMessage(s, options["command"], options["params"]);
			break;
		case CMD_LOGIC_MESSAGE:
			string location_ID = options["location_id"];
			INT32 gamestate_ID = ATOI(options["gamestate_id"].c_str());
			if (gamestate_ID > 0)
			{
				location_ID = location_ID_from_gamestate_ID(gamestate_ID);
			}
			if (location_ID == "")
			{
				s << "ERROR: Invalid location_id\r\n";
				break;
			}
			options["location_id"] = location_ID;

			if (!group.IsLocationAssigned(location_ID.c_str()))
			{
				// Weryfikacja czy to biezacej grupie przypisany jest location_ID.
				GTicketLocationAddressGetPtr ticket_location_address_get(new GTicketLocationAddressGet());
				ticket_location_address_get->location_ID = location_ID;
				ticket_location_address_get->connection_type = ESTClientBaseConsole;
				group.Ticket(ESTClientNodeBalancer, IGMITIC_LOCATION_ADDRESS_GET, ticket_location_address_get);

				// Czekamy na weryfikacje przypisania lokacji do grupy przez 60 sek.
				DWORD64 expiration_time = CurrentSTime() + 60;
				SConsoleTask task;
				task.cmd = cmd;
				task.options = options;
				task.socketid = ATOI(options["socketid"].c_str());
				task.type = ECTT_Request;
				group.pending_system_messages.push_back(make_pair(task, expiration_time));
				return -1;
			}

			if (options.find("command") == options.end())
			{
				s << "ERROR: no command id provided\r\n";
				break;
			}
			if (options.find("params") == options.end())
			{
				s << "ERROR: no parameters provided\r\n";
				break;
			}
			bool flush = false;
			if (options.find("flush") != options.end())
			{
				flush = ATOI(options["flush"].c_str()) != 0;
			}

			if (!group.GetScriptingEngine(location_ID.c_str())->ProcessSystemMessage(s, location_ID.c_str(), options["command"], options["params"], flush, ATOI(options["socketid"].c_str())))
			{
				return -1;
			}
			break;

	}
	return 0;
}
/***************************************************************************/
bool GGroupManager::InitSQL()
{
	mysql_init(&mysql);
	int pd=ATOI(global_serverconfig->sql_config.port.c_str());
	if (!mysql_real_connect(&mysql,
		global_serverconfig->sql_config.host.c_str(),
		global_serverconfig->sql_config.user.c_str(),
		global_serverconfig->sql_config.password.c_str(),
		global_serverconfig->sql_config.database.c_str(),
		pd,
		NULL,CLIENT_MULTI_STATEMENTS))
	{
		RAPORT("Failed to connect to database (%s,%s,%s,%s). Error: %s",
			global_serverconfig->sql_config.host.c_str(),
			global_serverconfig->sql_config.user.c_str(),
			global_serverconfig->sql_config.password.c_str(),
			global_serverconfig->sql_config.database.c_str(),
			mysql_error(&mysql));
			sql_config_initialized=false;
			return false;
	}
	else
	{
		sql_config_initialized=true;
		mysql_set_character_set(&mysql, "utf8");
	}
	mysql.reconnect=1;
	return true;
}
/***************************************************************************/
void GGroupManager::DestroySQL()
{
	if(sql_config_initialized)
	{
		mysql_close(&mysql);
	}
}
/***************************************************************************/
void GGroupManager::TicketToGroup(DWORD32 host, USHORT16 port, INT32 group_id, int message, GTicketPtr ticket)
{
	ticket->message = message;
	ticket->target.Init(group_id, TN, TN);

	TServiceVector::iterator it;
	for (it = socket_internal.begin(); it != socket_internal.end(); it++)
	{
		if ((*it)->GetClientServiceType() == ESTClientBase &&
			(*it)->GetAddr().sin_addr.s_addr == host && (*it)->GetAddr().sin_port == htons(port))
		{
			(*it)->Ticket(ticket);
			return;
		}
	}
	
	GTicketInternalClient * new_connection;
	new_connection = new GTicketInternalClient(raport_interface);
	new_connection->SetHostPort(host, port);
	new_connection->Init(ESTClientBase, group.GetID(), &memory_manager, &poll_manager);
	new_connection->dropped_ticket_callback = boost::bind(&GGroup::DroppedTicketCallback, &group, _1, _2);
	new_connection->msg_callback_target = boost::bind(&GGroup::ProcessTargetMessageGroupConnectionReplies,&group,_1,_2,_3,_4);
	new_connection->msg_callback_internal = boost::bind(&GGroup::ProcessInternalMessage,&group,_1,_2,_3);
	new_connection->msg_callback_connect = boost::bind(&GGroup::ProcessConnection,&group,_1);

	socket_internal.push_back(new_connection);
	SRAP(INFO_SOCKET_NEW);

	new_connection->Ticket(ticket);
}
/***************************************************************************/
void GGroupManager::DropOutgoingTickets()
{
	TServiceVector::iterator it;
	for (it = socket_internal.begin(); it != socket_internal.end(); it++)
	{
		(*it)->DropTickets();
	}
}
/***************************************************************************/
void GGroupManager::RaportCurrentStatus()
{
	static const char * EGS_status[] =
	{
		"EGS_Initializing",
		"EGS_Ready",
		"EGS_Releasing_Gently",
		"EGS_Transferring",
		"EGS_Stopped",
		"EGS_Releasing_Urgent",
		"EGS_Releasing_Urgent_Before_Shutdown",
	};
    static const char * ELES_status[] = 
	{
		"ELES_Initializing",
		"ELES_Init_Failed",
		"ELES_Ready",
		"ELES_Old_Logic",
		"ELES_Overload",
	};
	static const char * EEGS_status[] = 
	{
		"EEGS_READY",
		"EEGS_BUSY",
		"EEGS_STOPPED",
	};

	// Status grupy i logiki.
	GRAPORT("[GSTATUS %2d] status: %s + %s = %s, locations_loaded: %d",
		Group().GetID(),
		EGS_status[(int)Group().GetStatus()],
		ELES_status[(int)Group().GetScriptingEngineStatus()],
		EEGS_status[(int)Group().GetEffectiveStatus()],
		Group().GetLoadedLocationsCount());

	// Polaczenie z baza danych.
	GRAPORT("[GSTATUS %2d] mysql_status: %s", Group().GetID(), mysql_ping(&mysql) == 0 ? "OK" : "Error");

	// Polaczenia incoming od graczy.
	GRAPORT("[GSTATUS %2d] incoming_player_connections = %d", Group().GetID(), socket_game_map.size());

	// Polaczenia do serwisow i outgoing do innych grup.
	{
		char line[256];
		strstream out_stream(line, sizeof(line), ios_base::out);

		TServiceVector::iterator pos;
		for (pos = socket_internal.begin(); pos != socket_internal.end(); pos++)
		{
			out_stream.seekp(0);
			(*pos)->ServiceInfo(out_stream);
			line[sizeof(line) - 1] = 0;
			char * eol = strpbrk(line, "\r\n");
			if (eol)
			{
				*eol = 0;
			}
			GRAPORT("[GSTATUS %2d] %s", Group().GetID(), line);
		}
	}
}
/***************************************************************************/

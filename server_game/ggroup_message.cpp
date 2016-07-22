/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"
#include "headers_game.h"


extern GServerBase * global_server;

/***************************************************************************/
//process messagey przychodzacych od serwera statsow
bool GGroup::ProcessInternalMessage(GSocket *s,GNetMsg & msg,int message)
{
	switch(message)
	{
	case IGMI_ACCEPT:
		{
			GSocketInternalClient * si=static_cast<GSocketInternalClient*>(s);
			if(identity.id==0)
			{
			}
			else
			{
				if (si->GetClientServiceType() == ESTClientAccess ||
                    si->GetClientServiceType() == ESTClientGlobalInfo)
				{
					SPlayerInfo player_info;
					GNetMsg & m = MsgInt(si->GetClientServiceType(), IGMI_PLAYERS);
					INT32 size = (INT32) index_rid.size();
					m.WI(size);
					TPlayerMap::iterator pos;
					for (pos = index_rid.begin(); pos != index_rid.end(); ++pos)
					{
						GPlayerGame *p = static_cast<GPlayerGame*>(pos->second);
						player_info = p->player_info;
						m.W(player_info);
					}
					m.A();
				}

				if (si->GetClientServiceType() == ESTClientGlobalInfo)
				{
					GNetMsg & m = MsgInt(si->GetClientServiceType(), IGMI_PLAYERS_BUDDIES);
					INT32 size = (INT32) index_rid.size();
					m.WI(size);

					TPlayerMap::iterator pos;
					for (pos = index_rid.begin(); pos != index_rid.end(); ++pos)
					{
						GPlayerGame *p = static_cast<GPlayerGame*>(pos->second);
						SRegisterBuddies register_buddies;
						register_buddies.user_id = p->player_info.ID;
						register_buddies.buddies_ids = p->registered_buddies;
						m.W(register_buddies);
					}
					m.A();
				}

				if (si->GetClientServiceType() == ESTClientNodeBalancer)
				{
					UpdateGroupHeartBeat();
				}
			}
			return true;
		}

	case IGMI_GROUP_STATUS_UPDATE_RESPONSE:
		{
			DWORD64 timestamp;
			msg.RBUI(timestamp);
			if (timestamp != 0)
			{
				if (status == EGS_Initializing)
				{
					ReleaseLocation("");
				}
				last_confirmed_heartbeat = timestamp;

				if (status != EGS_Stopped)
				{
					write_locations_deadline = timestamp + GROUP_HEARTBEAT_INTERVAL_SECONDS + NODEBALANCER_GETGROUPS_INTERVAL_SECONDS - 5;
				}
			}
		}
		return true;

	case IGMI_LOCATION_RELEASE_RESPONSE:
		{
			string location_ID;
			bool result;

			msg.RT(location_ID).Rb(result);
			if (result)
			{
				if (location_ID == "" && status == EGS_Initializing)
				{
					DWORD64 now = Server()->CurrentSTime();
					if (last_confirmed_heartbeat + GROUP_HEARTBEAT_INTERVAL_SECONDS >= now)
					{
						status = EGS_Ready;
						UpdateGroupHeartBeat();
					}
				}
			}
			return true;
		}

	case IGMI_CENZOR_MESSAGE:
		{
			string text;
			string location_ID;
			string params;

			msg.RT(text).RT(location_ID).RT(params);

			if (GetScriptingEngine(location_ID.c_str()))
			{
				GetScriptingEngine(location_ID.c_str())->HandleReplaceVulgarWords(location_ID.c_str(), text, params);
			}
			return true;
		}
	case IGMI_GLOBAL_MESSAGE:
		{
			string command;
			string params;
			bool system_message;
			msg.RT(command).RT(params).Rb(system_message);

			if (system_message)
			{
				// Konwersja enum EGroupType na std::string
				static const char * group_types[] = { "none", "location", "gamestate", "lobby", "leaderboard", "last" };

				GMemory output_buffer;
				output_buffer.Init(&Server()->MemoryManager());
				output_buffer.Allocate(K512);
				strstream stream(output_buffer.End(), output_buffer.Free(), ios_base::out);

				stream << boost::format("---- %16s:%-6d Group id = %-3d type = %-11s ----\r\n") % 
					AddrToString(Server()->sock_service.addr_internal)() % Server()->sock_service.port_internal % GetID() % group_types[type];

				ProcessSystemGlobalMessage(stream, command, params);

				stream << "\r\n";
				output_buffer.End()[stream.pcount()] = 0;
				
				s->MsgExt(IGMI_SYSTEM_GLOBAL_MESSAGE_RESULT).WT(stream.str()).A();
				
				output_buffer.Deallocate();
			}
			else
			{
				ProcessGlobalMessage(command, params);
			}
		}
		return true;
	}
	return false;
}
/***************************************************************************/
bool GGroup::ProcessTargetMessage(GSocket *,GNetMsg & msg,int message,GNetTarget& target)
{
	switch (message)
	{
	case IGMIT_LOCATION_ADDRESS_RESPONSE:
		{
			EServiceTypes connection_type;
			bool result;
			DWORD32 host;
			string location_ID, ports;
			INT32 group_id, c;

			msg.RT(location_ID).RI(c).Rb(result).RUI(host).RT(ports).RI(group_id);
			connection_type = (EServiceTypes)c;

			if (connection_type != ESTClientBase && connection_type != ESTClientBaseConsole)
			{
				return true;
			}
			DWORD64 now = Server()->CurrentSTime();

			USHORT16 port = (USHORT16)ATOI(ports.c_str());

			if (connection_type == ESTClientBaseConsole)
			{
				// 1. Obslugujemy 'pending_system_messages'.
				INT32 a, count = pending_system_messages.size();
				for (a = 0; a < count; a++)
				{
					SConsoleTask & task = pending_system_messages[a].first;

					string task_location_ID = task.options["location_id"];
					if (task_location_ID == location_ID)
					{
						GMemory output_buffer;
						output_buffer.Init(&Server()->MemoryManager());
						output_buffer.Allocate(K512);
						strstream s(output_buffer.End(), output_buffer.Free(), ios_base::out);

						bool task_execution_delayed = false;

						if (result &&
							group_id == identity.id &&
							host == Server()->sock_service.addr_console &&
							port == Server()->sock_service.port_console)
						{
							AssignLocation(location_ID.c_str());
							// Task trafil do wlasciwej grupy. Wykonujemy go.
							if (task.cmd == CMD_LOGIC_MESSAGE)
							{
								if (Server()->CallAction(s, task.cmd, task.options) == -1)
								{
									task_execution_delayed = true;
								}
							}
						}
						else
						{
							if (!result)
							{
								s << "ERROR Cannot find location.\r\n";
							}
							else
							{
								s << "ERROR REDIRECT " << AddrToString(host)() << " " << ports << " " << group_id << "\r\n";
							}
						}

						if (!task_execution_delayed)
						{
							output_buffer.End()[s.pcount()] = 0;
							task.result = s.str();

							task.type = ECTT_Response;
							global_server->InsertConsoleTask(task);
						}
						output_buffer.Deallocate();

						pending_system_messages.erase(pending_system_messages.begin() + a);
						a--;
						count--;
					}
				}
			}

			if (connection_type == ESTClientBase)
			{
				if (result)
				{
					PutLocationAddressToCache(location_ID, host, port, group_id);
				}

				// 2. Obslugujemy 'outgoing_logic_messages'.
				INT32 a, count = outgoing_logic_messages.size();
				for (a = 0; a < count; a++)
				{
					SLogicMessage & message = outgoing_logic_messages[a];
					if (message.expiration_time != 0 && message.address_request_sent && message.target_group_address == "" &&
						message.target_location_ID == location_ID)
					{
						if (result &&
							group_id == identity.id &&
							host == Server()->sock_service.addr_internal &&
							port == Server()->sock_service.port_internal)
						{
							AssignLocation(location_ID.c_str());
							if (GetScriptingEngine(message.target_location_ID.c_str()))
							{
								GetScriptingEngine(message.target_location_ID.c_str())->ProcessLocationMessage(message.target_location_ID.c_str(), message.command, message.params);
							}
						}
						else
						{
							if (!result)
							{
								LogDroppedLogicMessage(message, true, ETDR_NoGroupAddress);
							}
							else
							{
								message.target_group_address = str(boost::format("%s:%d:%d") % AddrToString(host)() % port % group_id);

								GTicketLogicMessagePtr ticket_logic_message(new GTicketLogicMessage());
								ticket_logic_message->data = message;
								ticket_logic_message->data.ttl_seconds = (INT32)(message.expiration_time > now ? message.expiration_time - now : 1);
								ticket_logic_message->send_deadline = min(message.expiration_time, now + 45);
								ticket_logic_message->stable_sequence_identifier = str(boost::format("ilogic_%1%") % message.target_location_ID);
								Server()->TicketToGroup(host, port, group_id, IGMITIC_LOGIC_MESSAGE, ticket_logic_message);
							}
						}
						// Ustawiajac 'expiration_time' = 0 spowodujemy, ze komunikat zostanie wkrotce usuniety z kolejki.
						message.expiration_time = 0;
					}
				}

				// 3. Obslugujemy 'incoming_logic_messages'.
				count = incoming_logic_messages.size();
				for (a = 0; a < count; a++)
				{
					SLogicMessage & message = incoming_logic_messages[a];
					if (message.expiration_time != 0 && message.address_request_sent && message.target_group_address == "" &&
						message.target_location_ID == location_ID)
					{
						if (result &&
							group_id == identity.id &&
							host == Server()->sock_service.addr_internal &&
							port == Server()->sock_service.port_internal)
						{
							AssignLocation(location_ID.c_str());
							if (GetScriptingEngine(message.target_location_ID.c_str()))
							{
								GetScriptingEngine(message.target_location_ID.c_str())->ProcessLocationMessage(message.target_location_ID.c_str(), message.command, message.params);
							}
						}
						else
						{
							if (!result)
							{
								LogDroppedLogicMessage(message, false, ETDR_NoGroupAddress);
							}
							else
							{
								SGroupAddress address;
								address.host = host;
								address.port = port;
								address.group_id = group_id;

								GGroupAccess & group_access = GSERVER->group;
								message.source_group_address = str(boost::format("%d") % GetID());
								group_access.AddIncomingLogicMessage(message, address);
							}
						}
						// Ustawiajac 'expiration_time' = 0 spowodujemy, ze komunikat zostanie wkrotce usuniety z kolejki.
						message.expiration_time = 0;
					}
				}
			}
		}
		return true;
	}
	return false;
}
/***************************************************************************/
bool GGroup::ProcessTargetMessageGroupConnection(GSocket * socket, GNetMsg & msg, int messageid, GNetTarget & target)
{
	GSocketInternalServer * si = static_cast<GSocketInternalServer *>(socket);

	msg.WI(IGMIT_PROCESSED).W(target).A();
	switch (messageid)
	{
	case IGMITIC_LOGIC_MESSAGE:
		DWORD64 now = Server()->CurrentSTime();

		SLogicMessage message;
		msg.R(message);
		message.expiration_time = now + (INT64)message.ttl_seconds;

		GGroupManager * gmgr = GSERVER->FindGroupManager(target.tg);
		if (gmgr == NULL || gmgr->Group().GetStatus() == EGS_Stopped)
		{
			GNetTarget t;
			if (message.ttl_seconds > 1)
			{
				message.ttl_seconds--;
			}
			msg.WI(IGMIT_LOGIC_MESSAGE_BAD_ADDRESS).W(t).W(message).WUI(0).WUS(0).WI(0).A();
		}
		else
		{
			SGroupAddress address;
			{
				DWORD64 global_group_id = si->data.global_group_id;
				address.group_id = (INT32)(global_group_id & 0xffff);
				global_group_id >>= 16;
				address.port = (INT32)(global_group_id & 0xffff);
				global_group_id >>= 16;
				address.host = (DWORD32)(global_group_id & 0xffffffff);
			}
			message.socketid = socket->socketid;
			gmgr->Group().AddIncomingLogicMessage(message, address);
		}
		return true;
	}
	return false;
}
/***************************************************************************/
bool GGroup::ProcessTargetMessageGroupConnectionReplies(GSocket * socket, GNetMsg & msg, int messageid, GNetTarget & target)
{
	GSocketInternalClient * si = static_cast<GSocketInternalClient *>(socket);

	msg.WI(IGMIT_PROCESSED).W(target).A();
	switch (messageid)
	{
	case IGMIT_LOGIC_MESSAGE_BAD_ADDRESS:
		DWORD64 now = Server()->CurrentSTime();

		SLogicMessage message;
		DWORD32 host;
		USHORT16 port;
		INT32 group_id;
		msg.R(message).RUI(host).RUS(port).RI(group_id);
		message.expiration_time = now + (INT64)message.ttl_seconds;

		if (host != 0)
		{
			PutLocationAddressToCache(message.target_location_ID, host, port, group_id);
		}
		else
		{
			SGroupAddress address;
			if (GetLocationAddressFromCache(message.target_location_ID, address))
			{
				if (address.host == si->GetAddr().sin_addr.s_addr &&
					htons(address.port) == si->GetAddr().sin_port &&
					address.group_id == target.tg)
				{
					PutLocationAddressToCache(message.target_location_ID, 0, 0, 0);
				}
			}
		}

		message.address_request_sent = false;
		message.bad_address_received = true;
		message.source_group_address = str(boost::format("%s:%d:%d") % Server()->sock_service.host_internal % Server()->sock_service.port_internal % GetID());
		message.target_group_address = "";
		new_outgoing_logic_messages.push_back(message);

		return true;
	}

	return false;
}
/***************************************************************************/
void GGroup::WriteMassUpdateMessagesSite(GNetMsg & msg)
{
}
/***************************************************************************/
void GGroup::AddOutgoingLogicMessage(SLogicMessage & message)
{
	if (message.target_location_ID == "")
	{
		return;
	}
	message.address_request_sent = false;
	message.bad_address_received = false;
	message.source_group_address = str(boost::format("%s:%d:%d") % Server()->sock_service.host_internal % Server()->sock_service.port_internal % GetID());
	message.target_group_address = "";
	message.expiration_time = Server()->CurrentSTime() + LOGIC_MESSAGE_EXPIRATION_TIME;
	new_outgoing_logic_messages.push_back(message);
}
/***************************************************************************/
void GGroup::AddIncomingLogicMessage(SLogicMessage & message, SGroupAddress & address)
{
	boost::mutex::scoped_lock lock(mtx_logic_messages_transfer_queue);
	logic_messages_transfer_queue.push(make_pair(message, address));
}
/***************************************************************************/
void GGroup::ProcessLogicMessagesTransferQueue()
{
	boost::mutex::scoped_lock lock(mtx_logic_messages_transfer_queue);

	DWORD64 now = Server()->CurrentSTime();

	while (!logic_messages_transfer_queue.empty())
	{
		TInternalLogicMessageQueue::value_type ma;
		ma = logic_messages_transfer_queue.front();
		logic_messages_transfer_queue.pop();

		SLogicMessage & message = ma.first;
		SGroupAddress & address = ma.second;

		if (GetID() > 0)
		{
			message.source_group_address = str(boost::format("%s:%d:%d") % AddrToString(address.host)() % address.port % address.group_id);
			incoming_logic_messages.push_back(message);
		}
		else
		{
			// Odsylamy odpowiedz 'bad address'
			GSocket * socket = Server()->GetBaseSocketByID(message.socketid);
			if (socket)
			{
				GNetTarget target;
				target.Init(ATOI(message.source_group_address.c_str()), TN, TN);
				message.ttl_seconds = (INT32)(message.expiration_time > now ? message.expiration_time - now : 1);
				socket->MsgExt(IGMIT_LOGIC_MESSAGE_BAD_ADDRESS).W(target).W(message).WUI(address.host).WUS(address.port).WI(address.group_id).A();
			}
		}
	}
}
/***************************************************************************/
void GGroup::ProcessOutgoingLogicMessages()
{
	DWORD64 now = Server()->CurrentSTime();

	set<string> location_address_needed, location_address_request_sent;
	
	outgoing_logic_messages.insert(outgoing_logic_messages.end(), new_outgoing_logic_messages.begin(), new_outgoing_logic_messages.end());
	new_outgoing_logic_messages.clear();

	INT32 a, count = outgoing_logic_messages.size();
	for (a = 0; a < count; a++)
	{
		SLogicMessage & message = outgoing_logic_messages[a];
		if (message.expiration_time == 0)
		{
			continue;
		}

		if (message.address_request_sent)
		{
			if (message.target_group_address == "")
			{
				location_address_request_sent.insert(message.target_location_ID);
			}
			continue;
		}

		if (IsLocationAssigned(message.target_location_ID.c_str()))
		{
			if (GetScriptingEngine(message.target_location_ID.c_str()))
			{
				GetScriptingEngine(message.target_location_ID.c_str())->ProcessLocationMessage(message.target_location_ID.c_str(), message.command, message.params);
			}
			message.expiration_time = 0;
			continue;
		}

		SGroupAddress address;
		if (!GetLocationAddressFromCache(message.target_location_ID, address))
		{
			message.address_request_sent = true;
			message.target_group_address = "";
			location_address_needed.insert(message.target_location_ID);
			continue;
		}

		message.target_group_address = str(boost::format("%s:%d:%d") % AddrToString(address.host)() % address.port % address.group_id);
		message.address_request_sent = true;

		GTicketLogicMessagePtr ticket_logic_message(new GTicketLogicMessage());
		ticket_logic_message->data = message;
		ticket_logic_message->data.ttl_seconds = (INT32)(message.expiration_time > now ? message.expiration_time - now : 1);
		ticket_logic_message->send_deadline = min(message.expiration_time, now + 10);
		ticket_logic_message->stable_sequence_identifier = str(boost::format("ilogic_%1%") % message.target_location_ID);
		Server()->TicketToGroup(address.host, address.port, address.group_id, IGMITIC_LOGIC_MESSAGE, ticket_logic_message);

		message.expiration_time = 0;
	}

	// Wysylamy zapytania o nowe adresy lokacji.
	{
		set<string>::iterator it;
		for (it = location_address_needed.begin(); it != location_address_needed.end(); it++)
		{
			if (location_address_request_sent.find(*it) == location_address_request_sent.end())
			{
				GTicketLocationAddressGetPtr ticket_location_address_get(new GTicketLocationAddressGet());
				ticket_location_address_get->location_ID = *it;
				ticket_location_address_get->connection_type = ESTClientBase;
				Ticket(ESTClientNodeBalancer, IGMITIC_LOCATION_ADDRESS_GET, ticket_location_address_get);
			}
		}
	}

	count = outgoing_logic_messages.size();
	for (a = 0; a < count; a++)
	{
		SLogicMessage & message = outgoing_logic_messages[a];
		if (message.expiration_time <= now)
		{
			if (message.expiration_time != 0)
			{
				LogDroppedLogicMessage(message, true, ETDR_DeadlineReached);
			}
			outgoing_logic_messages.erase(outgoing_logic_messages.begin() + a);
			a--;
			count--;
		}
	}
}
/***************************************************************************/
void GGroup::ProcessIncomingLogicMessages()
{
	DWORD64 now = Server()->CurrentSTime();

	set<string> location_address_needed, location_address_request_sent;

	INT32 a, count = incoming_logic_messages.size();
	for (a = 0; a < count; a++)
	{
		SLogicMessage & message = incoming_logic_messages[a];
		if (message.expiration_time == 0)
		{
			continue;
		}

		if (message.address_request_sent)
		{
			if (message.target_group_address == "")
			{
				location_address_request_sent.insert(message.target_location_ID);
			}
			continue;
		}

		if (IsLocationAssigned(message.target_location_ID.c_str()))
		{
			if (GetScriptingEngine(message.target_location_ID.c_str()))
			{
				GetScriptingEngine(message.target_location_ID.c_str())->ProcessLocationMessage(message.target_location_ID.c_str(), message.command, message.params);
			}
			message.expiration_time = 0;
			continue;
		}

		message.address_request_sent = true;
		message.target_group_address = "";
		location_address_needed.insert(message.target_location_ID);
	}

	// Wysylamy zapytania o nowe adresy lokacji.
	{
		set<string>::iterator it;
		for (it = location_address_needed.begin(); it != location_address_needed.end(); it++)
		{
			if (location_address_request_sent.find(*it) == location_address_request_sent.end())
			{
				GTicketLocationAddressGetPtr ticket_location_address_get(new GTicketLocationAddressGet());
				ticket_location_address_get->location_ID = *it;
				ticket_location_address_get->connection_type = ESTClientBase;
				Ticket(ESTClientNodeBalancer, IGMITIC_LOCATION_ADDRESS_GET, ticket_location_address_get);
			}
		}
	}

	count = incoming_logic_messages.size();
	for (a = 0; a < count; a++)
	{
		SLogicMessage & message = incoming_logic_messages[a];
		if (message.expiration_time <= now)
		{
			if (message.expiration_time != 0)
			{
				LogDroppedLogicMessage(message, false, ETDR_DeadlineReached);
			}
			incoming_logic_messages.erase(incoming_logic_messages.begin() + a);
			a--;
			count--;
		}
	}
}
/***************************************************************************/
void GGroup::DroppedTicketCallback(GTicket & ticket, ETicketDropReason reason)
{
	if (strncmp(ticket.stable_sequence_identifier.c_str(), "ilogic_", 7) != 0)
	{
		return;
	}
	SLogicMessage & message = (static_cast<GTicketLogicMessage *>(&ticket))->data;

	if (reason == ETDR_DeadlineReached)
	{
		// Jesli mamy jeszcze czas to probujemy odpytac 'nodebalancer' o potencjalnie nowy adres lokacji.
		DWORD64 now = Server()->CurrentSTime();
		if (message.expiration_time > now)
		{
			message.address_request_sent = false;
			message.target_group_address = "";
			new_outgoing_logic_messages.push_back(message);

			// Jesli nie udalo sie polaczyc z grupa przez pelny okres czasu to prawdopodobnie jest ona martwa.
			// Usuwamy wiec wpis dot. tej lokacji z cache bo prawdopodobnie jest on juz niewazny.
			// Oszczedzamy w ten sposob prob laczenia z ta grupa przyszlym komunikatom.
			PutLocationAddressToCache(message.target_location_ID, 0, 0, 0);
			return;
		}
	}

	LogDroppedLogicMessage(message, true, reason);
}
/***************************************************************************/
void GGroup::LogDroppedLogicMessage(SLogicMessage & message, bool is_outgoing, ETicketDropReason reason)
{
	DWORD64 now = Server()->CurrentSTime();

	string reason_string = "";
	switch (reason)
	{
	case ETDR_DeadlineReached:
		if (is_outgoing && message.target_group_address != "")
		{
			reason_string = "timeout_no_group_connection";
		}
		else
		{
			if (message.address_request_sent && Service(ESTClientNodeBalancer)->IsConnection())
			{
				reason_string = "timeout_no_group_address";
			}
			else
			{
				reason_string = "timeout_no_nodebalancer";
			}
		}
		break;
	case ETDR_TooManyRetries:
		reason_string = "ticket_too_many_retries";
		break;
	case ETDR_StableSequenceMiss:
		reason_string = "ticket_stable_sequence_miss";
		break;
	case ETDR_ForcedDrop:
		reason_string = "group_stopped";
		break;
	case ETDR_NoProcessedConfirmation:
		reason_string = "no_processed_confirmation";
		break;
	case ETDR_NoGroupAddress:
		reason_string = "group_address_unavailable";
		break;
	}
	INT32 ttl_seconds = (INT32)(message.expiration_time - now);
	string phase = (is_outgoing) ? (!message.bad_address_received ? "outgoing" : "outgoing_retry") : "incoming";
	string our_group = str(boost::format("%s:%d:%d") % Server()->sock_service.host_internal % Server()->sock_service.port_internal % GetID());
	
	std::string command = str(boost::format("INSERT INTO `data_dropped_internal_logic_messages` "
		"(`timestamp`, `reason`, `ttl_seconds`, `phase`, `source_group`, `target_group`, `target_location_ID`, `message_id`, `params`) VALUES "
		"(UNIX_TIMESTAMP(),'%1%','%2%','%3%','%4%','%5%','%6%','%7%','%8%')") %
		reason_string % ttl_seconds % phase %
		message.source_group_address.c_str() % (is_outgoing ? message.target_group_address.c_str() : our_group) %
		GMySQLReal(&mysql, message.target_location_ID)() %
		GMySQLReal(&mysql, message.command)() % GMySQLReal(&mysql, message.params)());

	global_sqlmanager->Add(command, EDB_SERVERS);

	if (Server()->Continue())
	{
		metrics.CommitEvent("dropped_internal_logic_messages");
	}
}
/***************************************************************************/
void GGroup::DropAllPendingLogicMessages()
{
	ProcessLogicMessagesTransferQueue();

	INT32 a, count = incoming_logic_messages.size();
	for (a = 0; a < count; a++)
	{
		SLogicMessage & message = incoming_logic_messages[a];
		if (message.expiration_time != 0)
		{
			SGroupAddress address;
			GGroupAccess & group_access = GSERVER->group;
			message.source_group_address = str(boost::format("%d") % GetID());
			group_access.AddIncomingLogicMessage(message, address);
		}
	}

	count = outgoing_logic_messages.size();
	for (a = 0; a < count; a++)
	{
		SLogicMessage & message = outgoing_logic_messages[a];
		if (message.expiration_time != 0)
		{
			LogDroppedLogicMessage(message, true, ETDR_ForcedDrop);
		}
	}

	Server()->DropOutgoingTickets();
}
/***************************************************************************/
void GGroup::ClearExpiredSystemMessages(bool force)
{
	DWORD64 now = Server()->CurrentSTime();

	INT32 a, count = pending_system_messages.size();
	for (a = 0; a < count; a++)
	{
		SConsoleTask & task = pending_system_messages[a].first;
		DWORD64 expiration_time = pending_system_messages[a].second;

		if (force || expiration_time <= now)
		{
			task.result = "ERROR Cannot verify location address - timeout.\r\n";
			task.type = ECTT_Response;
			global_server->InsertConsoleTask(task);

			pending_system_messages.erase(pending_system_messages.begin() + a);
			a--;
			count--;
		}
	}
}
/***************************************************************************/

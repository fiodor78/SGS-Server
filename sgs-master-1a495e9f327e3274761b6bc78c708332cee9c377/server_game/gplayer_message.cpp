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
/***************************************************************************/
GPlayerAccess::GPlayerAccess(GGroup* group_p,GSocket * socket_p, int rid_p)
:GPlayer(group_p, socket_p,rid_p)
{
	GSocketAccess * sa=static_cast<GSocketAccess*>(socket);
};
/***************************************************************************/
bool GPlayerAccess::ProcessMessage(GNetMsg & msg,int message)
{
	switch(message)
	{
	case IGM_ACCESS:
		{
			if(Valid())
			{
				GSocketAccess * sa=static_cast<GSocketAccess*>(socket);
				GTicketConnectionClient t_connection_client;
				msg.R(t_connection_client);
				if (*t_connection_client.access.target_location_ID == 0)
				{
					if (t_connection_client.access.target_gamestate_ID > 0)
					{
						strcpy(t_connection_client.access.target_location_ID, location_ID_from_gamestate_ID(t_connection_client.access.target_gamestate_ID).c_str());
					}
				}
				else
				{
					t_connection_client.access.target_gamestate_ID = gamestate_ID_from_location_ID(t_connection_client.access.target_location_ID);
				}

				sockaddr_in addr;
				socklen_t len=sizeof(addr);
				getpeername(socket->GetSocket(),(sockaddr*)&addr,&len);
				memcpy(&t_connection_client.client_host.addr_detected_by_server,&addr.sin_addr,4);

				t_connection_client.access.RID=GetRID();

				if (Service(ESTClientAccess)->GetOutQueueSize() >= global_serverconfig->net.ticket_queue_busy_limit ||
					Service(ESTClientAccess)->GetWaitQueueSize() >= global_serverconfig->net.ticket_queue_busy_limit)
				{
					MsgExt(IGM_CONNECTION_CLOSE).WT("server busy").A();
					return true;
				}

				GTicketConnectionPtr ticket(new GTicketConnection());
				Coerce(*ticket,t_connection_client);

				TicketInt(ESTClientAccess,IGMITIC_ACCESS,ticket);
				Coerce(sa->player_info,t_connection_client);
				sa->group_id = t_connection_client.access.group_ID;
			}
		}
		return true;
	}
	return false;
};
/***************************************************************************/
bool GPlayerAccess::ProcessTargetMessage(GNetMsg & msg, int message)
{
	if(!Valid()) return true;
	switch(message)
	{
	case IGMIT_ACCESS_CLOSE:
		{
			string s;
			msg.RT(s);
			MsgExt(IGM_CONNECTION_CLOSE).WT(s).A();
		}
		return true;
	case IGMIT_ACCESS_REJECT:
		{
			string s;
			msg.RT(s);
			MsgExt(IGM_CONNECTION_REJECT).WT(s).A();
		}
		return true;
	case IGMIT_PLAYER:
		{
			GSocketAccess * sa=static_cast<GSocketAccess*>(socket);
			msg.R(sa->player_info);
		}
		return true;
	case IGMIT_ACCESS_ACCEPT:
		{
			GSocketAccess * sa=static_cast<GSocketAccess*>(socket);

			MsgExt(IGM_SESSION).WE(sa->player_info).A();

			GTicketLocationAddressGetPtr ticket_location_address_get(new GTicketLocationAddressGet());
			ticket_location_address_get->location_ID = sa->player_info.target_location_ID;
			ticket_location_address_get->connection_type = ESTClientBaseGame;
			TicketInt(ESTClientNodeBalancer, IGMITIC_LOCATION_ADDRESS_GET, ticket_location_address_get);
		}
		return true;

	case IGMIT_LOCATION_ADDRESS_RESPONSE:
		{
			GSocketAccess * sa = static_cast<GSocketAccess *>(socket);

			EServiceTypes connection_type;
			bool result;
			DWORD32 host;
			string location_ID, ports;
			INT32 group_id, c;

			msg.RT(location_ID).RI(c).Rb(result).RUI(host).RT(ports).RI(group_id);
			connection_type = (EServiceTypes)c;

			if (!result ||
				strcmp(location_ID.c_str(), sa->player_info.target_location_ID) != 0 ||
				connection_type != ESTClientBaseGame)
			{
				MsgExt(IGM_CONNECTION_CLOSE).WT("internal error").A();
				return true;
			}

			GGroupManager * gmgr = GSERVER->FindGroupManager(1);
			if (group_id != sa->group_id ||
				host != gmgr->sock_service.addr_game ||
				ports != gmgr->sock_service.ports_game_str)
			{
				INT32 gamestate_ID = gamestate_ID_from_location_ID(location_ID.c_str());
				MsgExt(IGM_CONNECTION_RETRY).WI(gamestate_ID).WUI(host).WT(ports).WI(group_id).WT(location_ID).A();
				return true;
			}

			gmgr = GSERVER->FindGroupManager(group_id);
			if (gmgr == NULL)
			{
				RAPORT("ERROR: Group manager not found!!!");
				MsgExt(IGM_CONNECTION_CLOSE).WT("Can't find specified group.").A();
				return true;
			}

			if (gmgr->Group().GetStatus() == EGS_Stopped ||
				gmgr->Group().GetScriptingEngineStatus() != ELES_Ready)
			{
				MsgExt(IGM_CONNECTION_CLOSE).WT("logic failure").A();
				return true;
			}

			// Wszystko sie zgadza, mozemy wrzucic gracza do wybranej grupy.
			MsgExt(IGM_CONNECTION_ACCEPT).A();
			socket->Write();
			global_server->UnregisterLogic(socket,USR_MOVE_SOCKET,false);
			global_server->SocketMoveRemoveFromGlobalThread(sa);
			gmgr->SocketMoveAddToRoomThread(sa);
			Close();
		}
		return true;
	}
	return false;
}
/***************************************************************************/
GPlayerGame::GPlayerGame(GGroup* group_p,GSocket * socket_p,int rid_p)
:GPlayer(group_p,socket_p,rid_p)
{
	player_info.Zero();
	registered_buddies.clear();
	transferring_location_timestamp = 0;
}
/***************************************************************************/
GPlayerGame::~GPlayerGame()
{
}
/***************************************************************************/
bool GPlayerGame::ProcessMessage(GNetMsg & msg,int message)
{
	if(!Valid()) return true;

	if (transferring_location_timestamp)
	{
		return true;
	}

	switch(message)
	{
	case IGM_PING:
		{
			INT32 rid,ttl;
			msg.RI(rid).RI(ttl);
			if(ttl>4) ttl=4;
			if(ttl>0 && rid!=0)
			{
			}
			else
				msg.WI(IGM_PING).WI(0).WI(0).A();
		}
		break;
	case IGM_DISCONNECT:
		{
			flags.set(EFPrepareToClose);
		}
		return true;
	case IGM_GAME_LOGIC_MESSAGE:
		{
			std::string message_id;
			std::string params;
			msg.RT(message_id).RT(params);

			// Warunek moze nie byc spelniony jesli wykonalismy DisconnectPlayers(), ale klient sie nie odlaczyl
			// i probuje dalej wysylac komunikaty do logiki.
			if (Group()->IsLocationAssigned(player_info.target_location_ID))
			{
				group->GetScriptingEngine(player_info.target_location_ID)->ProcessLogicMessage(this,message_id,params);
			}
		}
		return true;
		break;
	case IGM_SET_SOCKET_INACTIVITY_TIMEOUT:
		DWORD64 socket_inactivity_timeout;
		msg.RBUI(socket_inactivity_timeout);

		// JM: zabezpieczamy przed smieciami i nie recjonalnymi wartosciami timeoutu
		// wartosci < TIME_MIN_SOCKET_INACTIVITY moga powodowac zbyt czeste disconnecty graczy
		if ((socket_inactivity_timeout >= TIME_MIN_SOCKET_INACTIVITY) &&	
			(socket_inactivity_timeout <= TIME_MAX_SOCKET_INACTIVITY))
		{
			socket->socket_inactivity_timeout = socket_inactivity_timeout;
		}		
		return true;
	default:
		return false;
	}
	return false;
};
/***************************************************************************/
bool GPlayerGame::ProcessTargetMessage(GNetMsg & msg, int message)
{
	char temp_string[1024];

	if(!Valid()) return true;

	switch(message)
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

			if (!transferring_location_timestamp)
			{
				return true;
			}

			if (!result ||
				strcmp(location_ID.c_str(), player_info.target_location_ID) != 0 ||
				connection_type != ESTClientBaseGame)
			{
				MsgExt(IGM_CONNECTION_CLOSE).WT("server closing").A();
				return true;
			}

			// Wysylamy do klienta zadanie przepiecia sie do innej grupy.
			INT32 gamestate_ID = gamestate_ID_from_location_ID(location_ID.c_str());
			MsgExt(IGM_CONNECTION_RETRY).WI(gamestate_ID).WUI(host).WT(ports).WI(group_id).WT(location_ID).A();
		}
		return true;

	case IGMIT_ONLINE_AND_OFFLINE_FRIENDS:
		{
			SPlayerInstance s_player_instance;
			string online_friends_string = "", offline_friends_string = "";

			INT32 count, user_id;
			DWORD64 last_connection;
			msg.RI(count);
			while (count-- > 0)
			{
				msg.R(s_player_instance);

				sprintf(temp_string, "{\"user_id\": %d, \"gamestate_ID\": %d}%s", 
					s_player_instance.user_id, s_player_instance.gamestate_ID, count > 0 ? ", " : "");
				online_friends_string += temp_string;
			}
			msg.RI(count);
			while (count-- > 0)
			{
				msg.RI(user_id).RBUI(last_connection);

				sprintf(temp_string, "{\"user_id\": %d}%s",
					user_id, count > 0 ? ", " : "");
				offline_friends_string += temp_string;
			}

			if (Group()->IsLocationAssigned(player_info.target_location_ID))
			{											 
				string params = str(boost::format("{\"target_user_id\": %1%, \"online_friends\": [%2%], \"offline_friends\": [%3%]}") %
					player_info.ID % online_friends_string % offline_friends_string);
				Group()->GetScriptingEngine(player_info.target_location_ID)->ProcessLocationMessage(player_info.target_location_ID, "online_and_offline_friends", params);
			}
		}
		return true;
	case IGMIT_ONLINE_FRIEND:
		{
			SPlayerInstance s_player_instance;
			msg.R(s_player_instance);

			if (Group()->IsLocationAssigned(player_info.target_location_ID))
			{
				sprintf(temp_string, "{\"target_user_id\": %d, \"user_instance\": {\"user_id\": %d, \"gamestate_ID\": %d} }", 
					player_info.ID, s_player_instance.user_id, s_player_instance.gamestate_ID);
				Group()->GetScriptingEngine(player_info.target_location_ID)->ProcessLocationMessage(player_info.target_location_ID, "online_friend", temp_string);
			}
		} 
		return true;
	case IGMIT_ONLINE_FRIEND_REMOVE:
		{
			SPlayerInstance s_player_instance;
			msg.R(s_player_instance);

			if (Group()->IsLocationAssigned(player_info.target_location_ID))
			{
				sprintf(temp_string, "{\"target_user_id\": %d, \"user_instance\": {\"user_id\": %d, \"gamestate_ID\": %d} }", 
					player_info.ID, s_player_instance.user_id, s_player_instance.gamestate_ID);
				Group()->GetScriptingEngine(player_info.target_location_ID)->ProcessLocationMessage(player_info.target_location_ID, "online_friend_remove", temp_string);
			}
		}
		return true;

	case IGMIT_LEADERBOARD_DATA:
	case IGMIT_LEADERBOARD_DATA_STANDINGS:
	case IGMIT_LEADERBOARD_DATA_BATCH_INFO:
	case IGMIT_LEADERBOARD_DATA_USER_POSITION:
		{
			string lb_key;
			DWORD64 lb_subkey;
			INT32 user_id, standings_index = -1, count, a;
			vector<pair<INT32, INT32> > result;

			msg.RT(lb_key).RBUI(lb_subkey).RI(user_id);

			string location_ID = location_ID_from_gamestate_ID(user_id).c_str();
			
			if (message == IGMIT_LEADERBOARD_DATA_STANDINGS ||
				message == IGMIT_LEADERBOARD_DATA_BATCH_INFO)
			{
				msg.RI(standings_index);
			}
			msg.RI(count);
			result.resize(count);
			for (a = 0; a < count; a++)
			{
				msg.RI(result[a].first).RI(result[a].second);
			}

			string scores_string;
			for (a = 0; a < count; a++)
			{
				sprintf(temp_string, "{\"user_id\": %d, \"score\": %d}%s", result[a].first, result[a].second, a < count - 1 ? ", " : "");
				scores_string += temp_string;
			}

			if (Group()->IsLocationAssigned(location_ID.c_str()))
			{
				string params = str(boost::format("{\"key\": \"%1%\", \"subkey\": %2%, \"standings_index\": %3%, "
												"\"batch_info\": %4%, \"user_position\": %5%, \"highscores\": [%6%] }") %
					lb_key % (INT64)lb_subkey % (INT32)standings_index %
					(INT32)(message == IGMIT_LEADERBOARD_DATA_BATCH_INFO ? 1 : 0) %
					(INT32)(message == IGMIT_LEADERBOARD_DATA_USER_POSITION ? 1 : 0) %
					scores_string);
				Group()->GetScriptingEngine(location_ID.c_str())->ProcessLocationMessage(location_ID.c_str(), "leaderboard_data", params);
			}
		}
		return true;
	}

	return false;
}
/***************************************************************************/
void GPlayerGame::UpdateLastAction()
{
	last_action_time=Server()->GetClock().Get();
}
/***************************************************************************/

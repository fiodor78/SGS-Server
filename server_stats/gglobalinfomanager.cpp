/***************************************************************************
***************************************************************************
(c) 1999-2006 Ganymede Technologies                 All Rights Reserved
Krakow, Poland                                  www.ganymede.eu
***************************************************************************
***************************************************************************/

#include "headers_all.h"
#include "gconsole.h"
#include "gserver_base.h"
#include "gserver.h"
#include "gtickets.h"
#include "gserver_local.h"
#include "gmsg_internal.h"
#include "utils_conversion.h"
extern GServerBase * global_server;

#define TIME_PENDING_UPDATE_FREQUENCY					4900			// co ile wysylamy do gracza informacje o zmienionych obiektach (w ms.)

/***************************************************************************/
GGlobalInfoManager::GGlobalInfoManager()
{
	console.SetServer(this);
	single_instance_service = false;

	instance_id_seq = 1;
	instances.clear();
	player_instances.clear();
	game_maintenances.clear();
	listeners.set_empty_key(-1);
	listeners.set_deleted_key(-2);

	buddies_map.clear();
	buddies_map_rev.clear();
	social_friends_map.clear();
	social_friends_map_rev.clear();
	social_friends_map_timestamp.clear();

	player_last_connection.clear();
}
/***************************************************************************/
void GGlobalInfoManager::ConfigureClock()
{
	GServerManager::ConfigureClock();

	signal_second.connect(boost::bind(&GGlobalInfoManager::WritePendingUpdates,this));
	signal_minute.connect(boost::bind(&GGlobalInfoManager::ClearExpiredData,this));
}
/***************************************************************************/
bool GGlobalInfoManager::RegisterLogic(GSocket * socket)
{
	UnregisterAllListeners(socket);
	return GServerManager::RegisterLogic(socket);
}
/***************************************************************************/
void GGlobalInfoManager::UnregisterLogic(GSocket * socket, EUSocketMode mode, bool b)
{
	UnregisterAllListeners(socket);
	GServerManager::UnregisterLogic(socket, mode, b);
}
/***************************************************************************/
bool GGlobalInfoManager::ProcessInternalMessage(GSocket * socket, GNetMsg & msg, int message)
{
	GSocketInternalClient * si = static_cast<GSocketInternalClient*>(socket);

	switch (message)
	{
	case IGMI_ACCEPT:
		{
			if (si->GetClientServiceType() == ESTClientAccess)
			{
				ClearAllInstances();
				msg.WI(IGMI_REGISTER_SERVER).A();
			}
			if (si->GetClientServiceType() == ESTClientLeaderBoard)
			{
				ProcessPendingLeaderboardRequests();
			}
			return true;
		}
		break;

	case IGMI_PLAYERS:
		{
			INT32 count;
			msg.RI(count);
			while (count--)
			{
				SPlayerInfo	s_player;
				msg.R(s_player);
				RegisterListener(socket, s_player);
			}
		}
		return true;

	case IGMI_BUDDIES_REGISTER:
		{
			SRegisterBuddies register_buddies;
			msg.R(register_buddies);
			RegisterBuddies(register_buddies);
		}
		return true;

	case IGMI_PLAYERS_BUDDIES:
		{
			INT32 count;
			msg.RI(count);

			while (count--)
			{				
				SRegisterBuddies register_buddies;
				msg.R(register_buddies);
				RegisterBuddies(register_buddies);
			}
		}
		return true;

	case IGMI_REGISTER_INSTANCE:
		{
			SPlayerInstance s_instance;
			msg.R(s_instance);
			bool is_new_instance = true;

			TPlayerInstanceMap::iterator ppos;
			pair<TPlayerInstanceMap::iterator, TPlayerInstanceMap::iterator> p = player_instances.equal_range(s_instance.user_id);
			for (ppos = p.first; ppos != p.second; ppos++)
			{
				INT64 instance_id = ppos->second;
				TInstanceMap::iterator ipos = instances.find(instance_id);
				if (ipos != instances.end())
				{
					if (ipos->second == s_instance)
					{
						is_new_instance = false;
						break;
					}
				}
			}

			if (is_new_instance)
			{
				INT64 instance_id = instance_id_seq++;

				instances.insert(pair<INT64, SPlayerInstance>(instance_id, s_instance));
				player_instances.insert(pair<INT32, INT64>(s_instance.user_id, instance_id));

				player_last_connection[s_instance.user_id] = CurrentSTime();

				UpdateOnlineFriend(message, s_instance, social_friends_map_rev);
				UpdateOnlineFriend(message, s_instance, buddies_map_rev);
			}
		}
		return true;

	case IGMI_UNREGISTER_INSTANCE:
		{
			SPlayerInstance s_instance;
			msg.R(s_instance);

			TPlayerInstanceMap::iterator ppos;
			pair<TPlayerInstanceMap::iterator, TPlayerInstanceMap::iterator> p = player_instances.equal_range(s_instance.user_id);
			for (ppos = p.first; ppos != p.second; ppos++)
			{
				INT64 instance_id = ppos->second;
				TInstanceMap::iterator ipos = instances.find(instance_id);
				if (ipos != instances.end())
				{
					if (ipos->second == s_instance)
					{
						UpdateOnlineFriend(message, s_instance, social_friends_map_rev);
						UpdateOnlineFriend(message, s_instance, buddies_map_rev);
						player_instances.erase(ppos);
						instances.erase(ipos);
						break;
					}
				}
			}
		}
		return true;
	}
	return false;
};
/***************************************************************************/
bool GGlobalInfoManager::ProcessTargetMessage(GSocket * socket, GNetMsg & msg, int message, GNetTarget & target)
{
	msg.WI(IGMIT_PROCESSED).W(target).A();

	switch(message)
	{
	case IGMITIC_REGISTER:
		{
			SPlayerInfo	s_player;
			msg.R(s_player);
			RegisterListener(socket, s_player);
		}
		return true;
	case IGMITIC_UNREGISTER:
		{
			SPlayerInfo	s_player;
			msg.R(s_player);
			UnregisterListener(socket, s_player);
		}
		return true;

	case IGMIT_RESOURCES_EXTENDED:
		{
			UpdateSocialFriends(target.tp_id, msg);
			ProcessPendingLeaderboardRequests(target.tp_id);
		}
		return true;

	case IGMITIC_LEADERBOARD_GET:
	case IGMITIC_LEADERBOARD_GET_STANDINGS:
	case IGMITIC_LEADERBOARD_GET_USER_POSITION:
		{
			string lb_key;
			DWORD64 lb_subkey;
			INT32 user_id, standings_index = -1, max_results = -1, count;

			msg.RT(lb_key).RBUI(lb_subkey).RI(user_id);
			if (message == IGMITIC_LEADERBOARD_GET_STANDINGS)
			{
				msg.RI(standings_index).RI(max_results);
			}
			msg.RI(count);
			while (count-- > 0)
			{
				msg.RI();
			}
			bool global_rankings = (lb_key.substr(0, 7) == "GLOBAL:");

			INT32 socketId = socket->socketid;

			// Praktycznie niemozliwa jest sytuacja, zeby gracz nie byl w tym momencie juz zarejestrowany w serwisie globalinfo.
			// Jesli nie jest to ignorujemy request.
			TListenerInstanceMap::iterator pos = listeners.find(user_id);
			if (pos != listeners.end())
			{
				SPlayerListener & s_player_listener = (*pos).second;
				GTicketInternalClient * socket = InternalClient(ESTClientLeaderBoard);

				if ((!s_player_listener.have_social_friends_data && !global_rankings) ||
					socket == NULL ||
					!socket->IsConnection(EConnectionEstabilished))
				{
					SLeaderboardRequest request;
					switch (message)
					{
					case IGMITIC_LEADERBOARD_GET:
						request.type = ELRT_get;
						break;
					case IGMITIC_LEADERBOARD_GET_STANDINGS:
						request.type = ELRT_get_standings;
						break;
					case IGMITIC_LEADERBOARD_GET_USER_POSITION:
						request.type = ELRT_get_user_position;
						break;
					}
					request.target = target;
					request.socketId = socketId;
					request.lb_key = lb_key;
					request.lb_subkey = lb_subkey;
					request.standings_index = standings_index;
					request.max_results = max_results;
					s_player_listener.pending_leaderboard_requests.push_back(request);
				}
				else
				{
					GNetTarget t = target;
					t.tp_id = socketId;
					GNetMsg & msg = socket->MsgExt(message).W(t).WT(lb_key).WBUI(lb_subkey).WI(user_id);
					if (message == IGMITIC_LEADERBOARD_GET_STANDINGS)
					{
						msg.WI(standings_index).WI(max_results);
					}

					if (!global_rankings)
					{
						TFriendsList::iterator fpos;
						pair<TFriendsList::iterator, TFriendsList::iterator> fp = social_friends_map.equal_range(user_id);
						INT32 count = social_friends_map.count(user_id);
						msg.WI(count);
						for (fpos = fp.first; fpos != fp.second; fpos++)
						{
							msg.WI(fpos->second);
						}
					}
					else
					{
						INT32 count = 0;
						msg.WI(count);
					}
					msg.A();
				}
			}
		}
		return true;

	case IGMITIC_LEADERBOARD_GET_BATCH_INFO:
		{
			string lb_key;
			DWORD64 lb_subkey;
			INT32 user_id, standings_index = -1, max_results = -1, count, a;
			vector<INT32> users_id;

			msg.RT(lb_key).RBUI(lb_subkey).RI(user_id).RI(standings_index).RI(count);
			users_id.resize(count);
			for (a = 0; a < count; a++)
			{
				users_id[a] = msg.RI();
			}

			INT32 socketId = socket->socketid;

			// Praktycznie niemozliwa jest sytuacja, zeby gracz nie byl w tym momencie juz zarejestrowany w serwisie globalinfo.
			// Jesli nie jest to ignorujemy request.
			TListenerInstanceMap::iterator pos = listeners.find(user_id);
			if (pos != listeners.end())
			{
				SPlayerListener & s_player_listener = (*pos).second;
				GTicketInternalClient * socket = InternalClient(ESTClientLeaderBoard);

				if (socket == NULL ||
					!socket->IsConnection(EConnectionEstabilished))
				{
					SLeaderboardRequest request;
					request.type = ELRT_get_batch_info;
					request.target = target;
					request.socketId = socketId;
					request.lb_key = lb_key;
					request.lb_subkey = lb_subkey;
					request.standings_index = standings_index;
					request.max_results = max_results;
					request.users_id = users_id;
					s_player_listener.pending_leaderboard_requests.push_back(request);
				}
				else
				{
					GNetTarget t = target;
					t.tp_id = socketId;
					GNetMsg & msg = socket->MsgExt(message).W(t).WT(lb_key).WBUI(lb_subkey).WI(user_id).WI(standings_index);
					INT32 a, count = users_id.size();
					msg.WI(count);
					for (a = 0; a < count; a++)
					{
						msg.WI(users_id[a]);
					}
					msg.A();
				}
			}
		}
		return true;

		// przelotka gra -> serwis leaderboard
	case IGMITIC_LEADERBOARD_SET:
		{
			string lb_key;
			DWORD64 lb_subkey;
			INT32 user_id, count, a;
			vector<INT32> scores;

			msg.RT(lb_key).RBUI(lb_subkey).RI(user_id).RI(count);
			scores.resize(count);
			for (a = 0; a < count; a++)
			{
				msg.RI(scores[a]);
			}

			GTicketInternalClient * socket = InternalClient(ESTClientLeaderBoard);
			if (socket && socket->IsConnection(EConnectionEstabilished))
			{
				GNetMsg & msg = socket->MsgExt(IGMITIC_LEADERBOARD_SET).W(target).WT(lb_key).WBUI(lb_subkey).WI(user_id).WI(count);
				for (a = 0; a < count; a++)
				{
					msg.WI(scores[a]);
				}
				msg.A();
			}
			else
			{
				TListenerInstanceMap::iterator pos = listeners.find(user_id);
				if (pos != listeners.end())
				{
					SPlayerListener & s_player_listener = (*pos).second;
					SLeaderboardRequest request;
					request.type = ELRT_set;
					request.target = target;
					request.lb_key = lb_key;
					request.lb_subkey = lb_subkey;
					request.scores = scores;
					s_player_listener.pending_leaderboard_requests.push_back(request);
				}
			}
		}
		return true;

		// przelotka serwis leaderboard -> gra
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

			INT32 socketId = target.tp_id;

			TSocketIdMap::iterator its = socket_base_map.find(socketId);
			if (its != socket_base_map.end())
			{
				GSocket * socket = its->second;
				GNetTarget t = target;
				t.tp_id = user_id;
				GNetMsg & msg = socket->MsgExt(message).W(t).WT(lb_key).WBUI(lb_subkey).WI(user_id);
				if (message == IGMIT_LEADERBOARD_DATA_STANDINGS ||
					message == IGMIT_LEADERBOARD_DATA_BATCH_INFO)
				{
					msg.WI(standings_index);
				}
				msg.WI(count);
				for (a = 0; a < count; a++)
				{
					msg.WI(result[a].first).WI(result[a].second);
				}
				msg.A();
			}
		}
		return true;
	}

	return false;
};
/***************************************************************************/
void GGlobalInfoManager::InitInternalClients()
{
	InitInternalClient(i_access, ESTClientAccess, 0);
	i_access->msg_callback_target = boost::bind(&GGlobalInfoManager::ProcessTargetMessage,this,_1,_2,_3,_4);
	i_access->msg_callback_internal = boost::bind(&GGlobalInfoManager::ProcessInternalMessage,this,_1,_2,_3);
	i_access->msg_callback_connect = boost::bind(&GGlobalInfoManager::ProcessConnection,this,_1);
	InitInternalClient(i_cache, ESTClientCache, 0);
	i_cache->msg_callback_target = boost::bind(&GGlobalInfoManager::ProcessTargetMessage,this,_1,_2,_3,_4);
	i_cache->msg_callback_internal = boost::bind(&GGlobalInfoManager::ProcessInternalMessage,this,_1,_2,_3);
	i_cache->msg_callback_connect = boost::bind(&GGlobalInfoManager::ProcessConnection,this,_1);
	InitInternalClient(i_leaderboard, ESTClientLeaderBoard, 0);
	i_leaderboard->msg_callback_target = boost::bind(&GGlobalInfoManager::ProcessTargetMessage,this,_1,_2,_3,_4);
	i_leaderboard->msg_callback_internal = boost::bind(&GGlobalInfoManager::ProcessInternalMessage,this,_1,_2,_3);
	i_leaderboard->msg_callback_connect = boost::bind(&GGlobalInfoManager::ProcessConnection,this,_1);
}
/***************************************************************************/
void GGlobalInfoManager::ProcessConnection(GNetMsg & msg)
{
	msg.WI(IGMI_CONNECT).WT("globalinfo").WI(game_instance).WUI(0).WUS(0).WI(0);
	msg.A();
}
/***************************************************************************/
void GGlobalInfoManager::ClearAllInstances()
{
	instances.clear();
	player_instances.clear();

	// Czyscimy graczom listy przyjaciol online.
	TListenerInstanceMap::iterator pos;
	for (pos = listeners.begin(); pos != listeners.end(); pos++)
	{
		WriteOnlineFriends((*pos).first);
	}
}
/***************************************************************************/
void GGlobalInfoManager::ClearExpiredData()
{
	// Usuwamy informacje o dlugo nieobecnych listenerach.
	DWORD64 current_time = CurrentSTime();
	TListenerInstanceMap::iterator pos;
	for (pos = listeners.begin(); pos != listeners.end(); )
	{
		SPlayerListener & s_player_listener = (*pos).second;
		if (s_player_listener.expiration != 0 &&
			s_player_listener.expiration < current_time &&
			s_player_listener.listeners.size() == 0)
		{
			INT32 user_id = pos->first;

			// Czyscimy listy buddies
			EraseFromFriendsLists(user_id, buddies_map, buddies_map_rev);

			// Czyscimy friendow social
			EraseFromFriendsLists(user_id, social_friends_map, social_friends_map_rev);
			social_friends_map_timestamp.erase(user_id);

			listeners.erase(pos++);
			continue;
		}
		pos++;
	}
}
/***************************************************************************/
void GGlobalInfoManager::RegisterListener(GSocket * socket, SPlayerInfo & s_player)
{
	INT32 socketid = socket->socketid;

	TListenerInstanceMap::iterator pos = listeners.find(s_player.ID);
	if (pos == listeners.end())
	{
		SPlayerListener listener;
		listeners.insert(pair<INT32, SPlayerListener>(s_player.ID, listener));
		pos = listeners.find(s_player.ID);
		if (pos == listeners.end())
		{
			return;
		}
	}
	SPlayerListener & s_player_listener = (*pos).second;

	bool flush_pending_update = false;

	if (!s_player_listener.have_social_friends_data)
	{
		GTicketInternalClient * socket = InternalClient(ESTClientCache);
		if (socket != NULL)
		{
			GNetTarget target;
			target.tp_id = s_player.ID;
			socket->MsgExt(IGMIT_RESOURCES_VERSION).W(target).A();
		}
	}

	if (s_player_listener.have_social_friends_data)
	{
		flush_pending_update = true;
		WriteOnlineFriends(s_player.ID, socket, s_player.RID);
	}

	if (flush_pending_update)
	{
		WritePendingUpdate(s_player.ID, s_player_listener);
	}

	SListenerInstance listener_instance(socketid, s_player.RID);
	vector<SListenerInstance>::iterator itv;
	for (itv = s_player_listener.listeners.begin(); itv != s_player_listener.listeners.end(); itv++)
	{
		if (*itv == listener_instance)
		{
			*itv = listener_instance;
			break;
		}
	}
	if (itv == s_player_listener.listeners.end())
	{
		s_player_listener.listeners.push_back(listener_instance);
	}

	s_player_listener.expiration = 0;
}
/***************************************************************************/
void GGlobalInfoManager::UnregisterListener(GSocket * socket, SPlayerInfo & s_player)
{
	INT32 socketid = socket->socketid;

	TListenerInstanceMap::iterator pos = listeners.find(s_player.ID);
	if (pos == listeners.end())
	{
		return;
	}
	SPlayerListener & s_player_listener = (*pos).second;

	SListenerInstance listener_instance(socketid, s_player.RID);
	vector<SListenerInstance>::iterator itv;
	for (itv = s_player_listener.listeners.begin(); itv != s_player_listener.listeners.end(); itv++)
	{
		if (*itv == listener_instance)
		{
			s_player_listener.listeners.erase(itv);
			break;
		}
	}

	if (s_player_listener.listeners.size() == 0 && s_player_listener.expiration == 0)
	{
		s_player_listener.expiration = CurrentSTime() + 60 * TIME_MAX_SESSION_HASH;
	}
}
/***************************************************************************/
void GGlobalInfoManager::UnregisterAllListeners(GSocket * socket)
{
	INT32 socketid = socket->socketid;

	TListenerInstanceMap::iterator pos;
	for (pos = listeners.begin(); pos != listeners.end(); pos++)
	{
		SPlayerListener & s_player_listener = (*pos).second;
		INT32 a;
		for (a = 0; a < (INT32)s_player_listener.listeners.size(); )
		{
			if (s_player_listener.listeners[a].socketId == socketid)
			{
				s_player_listener.listeners.erase(s_player_listener.listeners.begin() + a);
				continue;
			}
			a++;
		}
		if (s_player_listener.listeners.size() == 0 && s_player_listener.expiration == 0)
		{
			s_player_listener.expiration = CurrentSTime() + 60 * TIME_MAX_SESSION_HASH;
		}
	}
}
/***************************************************************************/
void GGlobalInfoManager::WritePendingUpdates()
{
	DWORD64 current_timestamp = GetClock().Get();
	TListenerInstanceMap::iterator pos;
	for (pos = listeners.begin(); pos != listeners.end(); pos++)
	{
		SPlayerListener & s_player_listener = (*pos).second;
		if (s_player_listener.last_update_sent + TIME_PENDING_UPDATE_FREQUENCY < current_timestamp)
		{
			WritePendingUpdate((*pos).first, s_player_listener);
			s_player_listener.last_update_sent = current_timestamp;
		}
	}
}
/***************************************************************************/
void GGlobalInfoManager::WritePendingUpdate(INT32 playerId, SPlayerListener & s_player_listener)
{
	GNetTarget target;
	vector<SListenerInstance>::iterator itl;
	for (itl = s_player_listener.listeners.begin(); itl != s_player_listener.listeners.end(); itl++)
	{
		TSocketIdMap::iterator its = socket_base_map.find((*itl).socketId);
		if (its == socket_base_map.end())
		{
			continue;
		}

		GSocket * socket = its->second;
		target.tp = (*itl).RID;

		// Informacje o instancjach przyjaciol
		vector<pair<INT32, SPlayerInstance> >::iterator itp;
		for (itp = s_player_listener.pending_online_social_friends_and_buddies.begin(); itp != s_player_listener.pending_online_social_friends_and_buddies.end(); itp++)
		{
			socket->MsgExt(itp->first == IGMI_REGISTER_INSTANCE ? IGMIT_ONLINE_FRIEND : IGMIT_ONLINE_FRIEND_REMOVE).W(target).W(itp->second).A();
		}
	}

	s_player_listener.pending_online_social_friends_and_buddies.clear();
}
/***************************************************************************/
void GGlobalInfoManager::ProcessPendingLeaderboardRequests(INT32 user_id)
{
	GTicketInternalClient * socket = InternalClient(ESTClientLeaderBoard);
	if (socket == NULL || !socket->IsConnection(EConnectionEstabilished))
	{
		return;
	}

	TListenerInstanceMap::iterator pos;
	for (pos = (user_id == -1) ? listeners.begin() : listeners.find(user_id); pos != listeners.end(); pos++)
	{
		SPlayerListener & s_player_listener = (*pos).second;

		if (s_player_listener.have_social_friends_data)
		{
			INT32 a, count;
			count = s_player_listener.pending_leaderboard_requests.size();
			for (a = 0; a < count; a++)
			{
				SLeaderboardRequest & request = s_player_listener.pending_leaderboard_requests[a];
				bool global_rankings = (request.lb_key.substr(0, 7) == "GLOBAL:");

				if (request.type == ELRT_get)
				{
					GNetTarget t = request.target;
					t.tp_id = request.socketId;
					GNetMsg & msg = socket->MsgExt(IGMITIC_LEADERBOARD_GET).W(t).WT(request.lb_key).WBUI(request.lb_subkey).WI(pos->first);

					if (!global_rankings)
					{
						TFriendsList::iterator fpos;
						pair<TFriendsList::iterator, TFriendsList::iterator> fp = social_friends_map.equal_range(pos->first);
						INT32 fcount = social_friends_map.count(pos->first);
						msg.WI(fcount);
						for (fpos = fp.first; fpos != fp.second; fpos++)
						{
							msg.WI(fpos->second);
						}
					}
					else
					{
						INT32 fcount = 0;
						msg.WI(fcount);
					}
					msg.A();
					continue;
				}

				if (request.type == ELRT_get_standings)
				{
					GNetTarget t = request.target;
					t.tp_id = request.socketId;
					GNetMsg & msg = socket->MsgExt(IGMITIC_LEADERBOARD_GET_STANDINGS).W(t).WT(request.lb_key).WBUI(request.lb_subkey).WI(pos->first);
					msg.WI(request.standings_index).WI(request.max_results);
					
					if (!global_rankings)
					{
						TFriendsList::iterator fpos;
						pair<TFriendsList::iterator, TFriendsList::iterator> fp = social_friends_map.equal_range(pos->first);
						INT32 fcount = social_friends_map.count(pos->first);
						msg.WI(fcount);
						for (fpos = fp.first; fpos != fp.second; fpos++)
						{
							msg.WI(fpos->second);
						}
					}
					else
					{
						INT32 fcount = 0;
						msg.WI(fcount);
					}
					msg.A();
					continue;
				}

				if (request.type == ELRT_get_batch_info)
				{
					GNetTarget t = request.target;
					t.tp_id = request.socketId;
					GNetMsg & msg = socket->MsgExt(IGMITIC_LEADERBOARD_GET_BATCH_INFO).W(t).WT(request.lb_key).WBUI(request.lb_subkey).WI(pos->first);
					msg.WI(request.standings_index);
					INT32 b, ucount = request.users_id.size();
					msg.WI(ucount);
					for (b = 0; b < ucount; b++)
					{
						msg.WI(request.users_id[b]);
					}
					msg.A();
					continue;
				}

				if (request.type == ELRT_get_user_position)
				{
					GNetTarget t = request.target;
					t.tp_id = request.socketId;
					GNetMsg & msg = socket->MsgExt(IGMITIC_LEADERBOARD_GET_USER_POSITION).W(t).WT(request.lb_key).WBUI(request.lb_subkey).WI(pos->first);

					if (!global_rankings)
					{
						TFriendsList::iterator fpos;
						pair<TFriendsList::iterator, TFriendsList::iterator> fp = social_friends_map.equal_range(pos->first);
						INT32 fcount = social_friends_map.count(pos->first);
						msg.WI(fcount);
						for (fpos = fp.first; fpos != fp.second; fpos++)
						{
							msg.WI(fpos->second);
						}
					}
					else
					{
						INT32 fcount = 0;
						msg.WI(fcount);
					}
					msg.A();
					continue;
				}

				if (request.type == ELRT_set)
				{
					INT32 b, scount = request.scores.size();
					GNetMsg & msg = socket->MsgExt(IGMITIC_LEADERBOARD_SET).W(request.target).WT(request.lb_key).WBUI(request.lb_subkey).WI(pos->first).WI(scount);
					for (b = 0; b < scount; b++)
					{
						msg.WI(request.scores[b]);
					}
					msg.A();
					continue;
				}
			}
			s_player_listener.pending_leaderboard_requests.clear();
		}

		if (user_id != -1)
		{
			break;
		}
	}
}
/***************************************************************************/
void GGlobalInfoManager::RegisterBuddies(const SRegisterBuddies& register_buddies)
{
	EraseFromFriendsLists(register_buddies.user_id, buddies_map, buddies_map_rev);

	for (std::set<INT32>::const_iterator cit = register_buddies.buddies_ids.begin();
		cit != register_buddies.buddies_ids.end(); ++cit)
	{
		if (register_buddies.user_id != *cit)
		{
			buddies_map.insert(make_pair(register_buddies.user_id, *cit));
			buddies_map_rev.insert(make_pair(*cit, register_buddies.user_id));
		}
	}

	TListenerInstanceMap::iterator it_listener = listeners.find(register_buddies.user_id);
	if (it_listener != listeners.end())
	{
		// Gdy dane z sociala juz zostaly wyslane a buddies zostali dodani pozniej, 
		// no to musimy wyslac calosc ponownie do klienta lacznie z lista buddies.
		if (it_listener->second.have_social_friends_data)
		{
			WriteOnlineFriends(register_buddies.user_id);
			WritePendingUpdate(register_buddies.user_id, it_listener->second);
		}
	}
}
/***************************************************************************/
void GGlobalInfoManager::EraseFromFriendsLists(INT32 user_id, TFriendsList& friends_map, TFriendsList& friends_map_rev)
{
	TFriendsList::iterator fpos, fpos_rev;
	pair<TFriendsList::iterator, TFriendsList::iterator> fp = friends_map.equal_range(user_id);
	if (fp.first != fp.second)
	{
		for (fpos = fp.first; fpos != fp.second; fpos++)
		{
			pair<TFriendsList::iterator, TFriendsList::iterator> fp_rev = friends_map_rev.equal_range(fpos->second);
			for (fpos_rev = fp_rev.first; fpos_rev != fp_rev.second; fpos_rev++)
			{
				if (fpos_rev->second == user_id)
				{
					friends_map_rev.erase(fpos_rev);
					break;
				}
			}
		}
		friends_map.erase(fp.first, fp.second);
	}
}
/***************************************************************************/

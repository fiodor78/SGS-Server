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

/***************************************************************************/
void GGlobalInfoManager::FillOnlineOfflineFriends(GNetMsg& msg, INT32 playerId, INT32& count, set<INT32>& online_friends, set<INT32>& offline_friends, const TFriendsList& friends_list)
{
	TFriendsList::const_iterator fpos;
	pair<TFriendsList::const_iterator, TFriendsList::const_iterator> fp = friends_list.equal_range(playerId);
	if (fp.first != fp.second)
	{
		for (fpos = fp.first; fpos != fp.second; fpos++)
		{
			INT32 friendId = fpos->second;
			if (online_friends.find(friendId) != online_friends.end())
			{
				continue;
			}

			bool offline_friend = true;
			TPlayerInstanceMap::iterator ppos;
			pair<TPlayerInstanceMap::iterator, TPlayerInstanceMap::iterator> p = player_instances.equal_range(friendId);
			for (ppos = p.first; ppos != p.second; ppos++)
			{
				TInstanceMap::iterator ipos = instances.find(ppos->second);
				if (ipos != instances.end())
				{
					msg.W(ipos->second);
					count++;
					offline_friend = false;
				}
			}
			if (!offline_friend)
			{
				online_friends.insert(friendId);
				offline_friends.erase(friendId);
			}
			else
			{
				offline_friends.insert(friendId);
			}
		}
	}
}
/***************************************************************************/
// socket == NULL: send to all listeners
void GGlobalInfoManager::WriteOnlineFriends(INT32 playerId, GSocket * socket, INT32 RID)
{
	TListenerInstanceMap::iterator pos = listeners.find(playerId);
	if (pos == listeners.end())
	{
		return;
	}
	SPlayerListener & s_player_listener = (*pos).second;

	GNetMsg msg(&MemoryManager());

	GetFriendsLastConnectionTime(playerId, social_friends_map);
	GetFriendsLastConnectionTime(playerId, buddies_map);

	INT32 a;
	INT32 listeners_count = s_player_listener.listeners.size();

	for (a = (socket == NULL ? 0 : -1); a < (socket == NULL ? listeners_count : 0); a++)
	{
		GSocket * current_socket = NULL;
		INT32 current_RID = 0;

		if (a == -1)
		{
			current_socket = socket;
			current_RID = RID;
		}
		else
		{
			TSocketIdMap::iterator its = socket_base_map.find(s_player_listener.listeners[a].socketId);
			if (its != socket_base_map.end())
			{
				current_socket = its->second;
			}
			current_RID = s_player_listener.listeners[a].RID;
		}

		if (current_socket == NULL)
		{
			continue;
		}

		// 'count' zostanie uzupelniony po zapisaniu wszystkich pojedynczych message.
		INT32 count = 0;

		set<INT32> online_friends, offline_friends;
		online_friends.clear();
		offline_friends.clear();

		GNetTarget target;
		msg.Reset();
		msg.WI(IGMIT_ONLINE_AND_OFFLINE_FRIENDS).W(target).WI(count);

		FillOnlineOfflineFriends(msg, playerId, count, online_friends, offline_friends, social_friends_map);
		FillOnlineOfflineFriends(msg, playerId, count, online_friends, offline_friends, buddies_map);

		// pod offsetem +0 bedzie rozmiar chunka, +4 to IGMIT_ONLINE_FRIENDS, +4+4+UI+UI+iii to count
		*((DWORD32*) (msg.OutChunk().ptr + 4 + 4 + sizeof(DWORD64) + sizeof(DWORD64) + sizeof(INT32) * 3)) = count;
		msg.WI(offline_friends.size());
		set<INT32>::iterator its;
		for (its = offline_friends.begin(); its != offline_friends.end(); its++)
		{
			msg.WI(*its).WBUI(player_last_connection[*its]);
		}
		msg.A();

		// pod offsetem +4+4+UI+UI+i jest target.tp
		*((DWORD32*) (msg.LastChunk().ptr + 4 + 4 + sizeof(DWORD64) + sizeof(DWORD64) + sizeof(INT32) * 1)) = current_RID;
		current_socket->MsgExt() += msg;
	}

	if (socket == NULL)
	{
		s_player_listener.pending_online_social_friends_and_buddies.clear();
	}
}
/***************************************************************************/
void GGlobalInfoManager::UpdateSocialFriends(INT32 playerId, GNetMsg & msg)
{
	INT32 count, friendId;
	DWORD64 updated;

	msg.RBUI(updated);

	bool skip_update = true;
	TListenerInstanceMap::iterator pos = listeners.find(playerId);
	if (pos != listeners.end())
	{
		skip_update = false;
		if ((*pos).second.have_social_friends_data && social_friends_map_timestamp[playerId] >= updated)
		{
			skip_update = true;
		}
	}

	if (skip_update)
	{
		msg.RI(count);
		while (count-- > 0)
		{
			msg.RI();
		}
		return;
	}
	SPlayerListener & s_player_listener = (*pos).second;

	if (social_friends_map_timestamp[playerId] < updated)
	{
		// Czyscimy wszystkich friendow 
		EraseFromFriendsLists(playerId, social_friends_map, social_friends_map_rev);

		msg.RI(count);
		while (count-- > 0)
		{
			msg.RI(friendId);
			social_friends_map.insert(TFriendsList::value_type(playerId, friendId));
			social_friends_map_rev.insert(TFriendsList::value_type(friendId, playerId));
		}

		social_friends_map_timestamp[playerId] = updated;
	}

	s_player_listener.have_social_friends_data = true;
	if (s_player_listener.have_social_friends_data)
	{
		// Rozsylamy uaktualniona liste online friends do wszystkich instancji gracza.
		WriteOnlineFriends(playerId);
	}
}
/***************************************************************************/
void GGlobalInfoManager::UpdateOnlineFriend(INT32 message, SPlayerInstance & s_instance, const TFriendsList& friends_list)
{
	// social_friends_map
	{
		TFriendsList::const_iterator fpos_rev;
		pair<TFriendsList::const_iterator, TFriendsList::const_iterator> fp_rev = friends_list.equal_range(s_instance.user_id);
		for (fpos_rev = fp_rev.first; fpos_rev != fp_rev.second; fpos_rev++)
		{
			TListenerInstanceMap::iterator pos = listeners.find(fpos_rev->second);
			if (pos != listeners.end())
			{
				SPlayerListener & s_player_listener = (*pos).second;

				vector<pair<INT32, SPlayerInstance> >::iterator itp;
				for (itp = s_player_listener.pending_online_social_friends_and_buddies.begin(); itp != s_player_listener.pending_online_social_friends_and_buddies.end(); itp++)
				{
					if (itp->second == s_instance)
					{
						itp->first = message;
						break;
					}
				}
				if (itp == s_player_listener.pending_online_social_friends_and_buddies.end())
				{
					s_player_listener.pending_online_social_friends_and_buddies.push_back(make_pair(message, s_instance));
				}
			}
		}
	}
}
/***************************************************************************/
void GGlobalInfoManager::GetFriendsLastConnectionTime(INT32 playerId, const TFriendsList& friends_list)
{
	{
		TFriendsList::const_iterator fpos;
		pair<TFriendsList::const_iterator, TFriendsList::const_iterator> fp = friends_list.equal_range(playerId);
		for (fpos = fp.first; fpos != fp.second; fpos++)
		{
			if (player_last_connection.find(fpos->second) == player_last_connection.end())
			{
				player_last_connection[fpos->second] = 0;
			}
		}
	}

	// TODO(Marian)
	// Tu nalezy umiescic kod do pobierania daty ostatniego logowania, jak juz bedzie to zapisywane w bazie danych.
	// Kod bedzie analogiczny jak w kodzie serwerow core3.
}
/***************************************************************************/

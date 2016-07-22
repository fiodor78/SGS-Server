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
#pragma warning(disable: 4996 4239)

extern GServerBase * global_server;
#define GSERVER ((GServerStats*)global_server)

/***************************************************************************/
void GAccessManager::InitInternalClients()
{
	InitInternalClient(i_cache, ESTClientCache, 0);
	i_cache->msg_callback_target = boost::bind(&GAccessManager::ProcessTargetMessage,this,_1,_2,_3,_4);
	i_cache->msg_callback_internal = boost::bind(&GAccessManager::ProcessInternalMessage,this,_1,_2,_3);
	i_cache->msg_callback_connect = boost::bind(&GAccessManager::ProcessConnection,this,_1);
}
/***************************************************************************/
void GAccessManager::ProcessConnection(GNetMsg & msg)
{
	msg.WI(IGMI_CONNECT).WT("access").WI(game_instance).WUI(0).WUS(0).WI(0);
	msg.A();
}
/***************************************************************************/
void GAccessManager::UpdateSocialFriends(INT32 playerId, GNetMsg & msg)
{
	INT32 count, friendId;
	DWORD64 updated;

	msg.RBUI(updated);

	bool skip_update = false;
	TSocialFriendsTimes::iterator pos;
	pos = social_friends_map_timestamp.find(playerId);
	if (pos != social_friends_map_timestamp.end())
	{
		if (pos->second.first != 0 && pos->second.first >= updated)
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

	// Czyscimy wszystkich friendow
	social_friends_map.erase(playerId);
	msg.RI(count);
	while (count-- > 0)
	{
		msg.RI(friendId);
		social_friends_map.insert(TFriendsList::value_type(playerId, friendId));
	}

	DWORD64 expiration_time = CurrentSTime() + 60 * TIME_MAX_SESSION_HASH;
	social_friends_map_timestamp.insert(TSocialFriendsTimes::value_type(playerId, pair<DWORD64, DWORD64>(updated, expiration_time)));
}
/***************************************************************************/
void GAccessManager::ClearExpiredSocialFriends()
{
	// Usuwamy informacje o przyjaciolach graczy, ktorzy dawno sie nie wchodzili na gamestaty przyjaciol.
	DWORD64 current_time = CurrentSTime();

	TSocialFriendsTimes::iterator pos;
	for (pos = social_friends_map_timestamp.begin(); pos != social_friends_map_timestamp.end(); )
	{
		if (pos->second.second != 0 && pos->second.second < current_time)
		{
			social_friends_map.erase(pos->first);
			social_friends_map_timestamp.erase(pos++);
			continue;
		}
		pos++;
	}
}
/***************************************************************************/
void GAccessManager::ProcessPendingRegistrationTasks(INT32 playerId)
{
	// Process pedning registration tasks.
	INT32 a, count;
	count = tasks_registration.size();
	for (a = 0; a < count; a++)
	{
		SAccessRegistrationTask & task = tasks_registration[a];
		if (task.t_player.data.ID == playerId)
		{
			task.t_player.data.access = EGS_UNKNOWN;

			TSocialFriendsTimes::iterator pos;
			pos = social_friends_map_timestamp.find(task.t_player.data.ID);
			if (pos != social_friends_map_timestamp.end() && pos->second.first != 0)
			{
				// Mamy w serwisie aktualna liste przyjaciol.
				TFriendsList::iterator fpos;
				pair<TFriendsList::iterator, TFriendsList::iterator> fp = social_friends_map.equal_range(task.t_player.data.ID);
				for (fpos = fp.first; fpos != fp.second; fpos++)
				{
					INT32 friendId = fpos->second;
					if (friendId == task.t_player.data.target_gamestate_ID)
					{
						task.t_player.data.access = EGS_FRIEND;
						break;
					}
				}
			}

			TSocketIdMap::iterator it = socket_base_map.find(task.socketid);
			if (it != socket_base_map.end())
			{
				GSocket * socket = it->second;
				if (socket)
				{
					SRAP(INFO_ACCESS_GRANTED);
					socket->MsgExt().WI(IGMIT_PLAYER).W(task.target).W(task.t_player).A();
					socket->MsgExt().WI(IGMIT_ACCESS_ACCEPT).W(task.target).WT("").A();
				}
			}

			tasks_registration.erase(tasks_registration.begin() + a);
			count--;
			a--;
		}
	}
}
/***************************************************************************/
void GAccessManager::ProcessExpiredRegistrationTasks()
{
	DWORD64 current_time = CurrentSTime();
	INT32 a, count;
	count = tasks_registration.size();
	for (a = 0; a < count; a++)
	{
		SAccessRegistrationTask & task = tasks_registration[a];
		if (task.expiration_time < current_time)
		{
			task.t_player.data.access = EGS_UNKNOWN;

			TSocketIdMap::iterator it = socket_base_map.find(task.socketid);
			if (it != socket_base_map.end())
			{
				GSocket * socket = it->second;
				if (socket)
				{
					SRAP(INFO_ACCESS_GRANTED);
					socket->MsgExt().WI(IGMIT_PLAYER).W(task.target).W(task.t_player).A();
					socket->MsgExt().WI(IGMIT_ACCESS_ACCEPT).W(task.target).WT("").A();
				}
			}

			tasks_registration.erase(tasks_registration.begin() + a);
			count--;
			a--;
		}
	}
}
/***************************************************************************/

#pragma warning(default: 4239)
#pragma warning(default: 4996)

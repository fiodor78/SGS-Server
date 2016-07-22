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
extern GServerBase * global_server;


/***************************************************************************/
GCacheManager::GCacheManager()
{
	console.SetServer(this);
}
/***************************************************************************/
bool GCacheManager::ProcessInternalMessage(GSocket *, GNetMsg &, int)
{
	return false;
};
/***************************************************************************/
bool GCacheManager::ProcessTargetMessage(GSocket *socket, GNetMsg & msg, int message, GNetTarget & target)
{
	msg.WI(IGMIT_PROCESSED).W(target).A();
	switch(message)
	{
	case IGMIT_RESOURCES_VERSION:
		GetSocialFriendsResources(msg, target);
		return true;
	}
	return false;
};
/***************************************************************************/
void GCacheManager::ReloadSocialFriends(INT32 user_id)
{
	social_friends_date.erase(user_id);
	social_friends_list.erase(user_id);
	social_friends_date.insert(pair<INT32, times>(user_id, make_pair(CurrentSTime(), CurrentSTime())));

	GetSocialFriendsResourcesFromSQL(user_id);

	// Przekazujemy do serwisu globalinfo informacje o nowych przyjaciolach gracza.
	// Rozesle on uaktualnione dane o przyjaciolach do wszystkich listenerow gracza ID.
	// Tak samo odswiezamy liste przyjaciol gracza w serwisie access.
	{
		GServerBase::TSocketIdMap::iterator its;
		for (its = socket_base_map.begin(); its != socket_base_map.end(); ++its)
		{
			GSocketInternalServer * socket = static_cast<GSocketInternalServer*>(its->second);
			if (socket->data.game_instance == game_instance &&
				(socket->data.game_name == "globalinfo" || socket->data.game_name == "access"))
			{
				GNetTarget t;
				t.Init(TN, TN, user_id);
				SendSocialFriendsResources(socket->MsgExt(), t, CurrentSTime());
			}
		}
	}
}
/***************************************************************************/
void GCacheManager::EraseSocialFriends(INT32 user_id)
{
	social_friends_date.erase(user_id);
	social_friends_list.erase(user_id);
}
/***************************************************************************/
void GCacheManager::GetSocialFriendsResources(GNetMsg & msg, GNetTarget & target)
{
	TFriendsDate::iterator it;

	it = social_friends_date.find(target.tp_id);
	if (it == social_friends_date.end())
	{
		social_friends_date.insert(pair<INT32, times >(target.tp_id, make_pair(CurrentSTime(), CurrentSTime())));
		GetSocialFriendsResourcesFromSQL(target.tp_id);
		SendSocialFriendsResources(msg, target, CurrentSTime());
	}
	else
	{
		(it->second).first = CurrentSTime();
		SendSocialFriendsResources(msg, target, (it->second).second);
	}
}
/***************************************************************************/
void GCacheManager::SendSocialFriendsResources(GNetMsg & msg, GNetTarget & target, DWORD64 updated)
{
	msg.WI(IGMIT_RESOURCES_EXTENDED).W(target).WBUI(updated);

	INT32 friends_count = 0;

	TSocialFriendsList::iterator pos;
	pos = social_friends_list.find(target.tp_id);
	if (pos != social_friends_list.end())
	{
		friends_count = pos->second.size();
		msg.WI(friends_count);
		TPlayerSocialAppFriendsList::iterator it;

		for (it = pos->second.begin(); it != pos->second.end(); it++)
		{
			msg.WI(*it);
		}
	}
	else
	{
		msg.WI(friends_count);
	}
	msg.A();
}
/***************************************************************************/
void GCacheManager::GetSocialFriendsResourcesFromSQL(INT32 user_id)
{
	boost::char_separator<char> sep(";, ");
	typedef boost::tokenizer<boost::char_separator<char> > TTokenizer;
	TPlayerSocialAppFriendsList	player_friends_list;

	social_friends_list.erase(user_id);

	char command[256];
	sprintf(command, "SELECT `friends` FROM `data_user_friends` "
		"WHERE `user_id` = '%d'", user_id);
	boost::recursive_mutex::scoped_lock lock(mtx_sql);
	if (MySQLQuery(&mysql, command))
	{
		MYSQL_RES * result = mysql_store_result(&mysql);
		if (result)
		{
			MYSQL_ROW row;
			row = mysql_fetch_row(result);
			while (row)
			{
				if (row[0] && row[1])
				{
					string friends_string = row[0];

					TTokenizer tok(friends_string, sep);
					TTokenizer::iterator it;
					for (it = tok.begin(); it != tok.end(); it++)
					{
						INT32 friend_id = ATOI(it->c_str());
						if (friend_id > 0)
						{
							player_friends_list.insert(friend_id);
						}
					}
				}
				row = mysql_fetch_row(result);
			}
			mysql_free_result(result);
		}
	}
	social_friends_list.insert(TSocialFriendsList::value_type(user_id, player_friends_list));
}
/***************************************************************************/

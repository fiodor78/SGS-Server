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

#pragma warning (disable: 4996)
extern GServerBase * global_server;
/***************************************************************************/
bool GAccessManager::RegisterLogic(GSocket * socket)
{
	SRAP(INFO_ACCESS_REGISTER_LOGIC);
	UnregisterAllInstances(socket);
	return GServerManager::RegisterLogic(socket);
}
/***************************************************************************/
void GAccessManager::UnregisterLogic(GSocket * socket,EUSocketMode mode,bool b)
{
	SRAP(INFO_ACCESS_UNREGISTER_LOGIC);
	UnregisterAllInstances(socket);
	return GServerManager::UnregisterLogic(socket, mode,b);
}
/***************************************************************************/
bool GAccessManager::ProcessInternalMessage(GSocket *socket,GNetMsg & msg,int message)
{
	switch(message)
	{
	case IGMI_PLAYERS:
		{

			INT32 size;
			msg.RI(size);
			int a;
			for (a=0;a<size;a++)
			{
				SPlayerInfo	s_player;
				msg.R(s_player);
				RegisterPlayer(s_player);
			}
		}
		return true;

	case IGMI_REGISTER_SERVER:
		{
			// Wysylamy informacje o wszystkich obecnych instancjach do serwisu globalinfo.
			GSocketInternalServer * si = static_cast<GSocketInternalServer*>(socket);
			if (si->data.game_instance == game_instance &&
				si->data.game_name == "globalinfo")
			{
				SPlayerInstance s_player_instance;

				TInstanceMap::iterator pos;
				for (pos = instances.begin(); pos != instances.end(); pos++)
				{
					Coerce(s_player_instance, pos->second);
					si->MsgExt(IGMI_REGISTER_INSTANCE).W(s_player_instance).A();
				}
			}
		}
		return true;
	}
	return false;
};	
/***************************************************************************/
bool GAccessManager::ProcessTargetMessage(GSocket *socket,GNetMsg & msg,int message,GNetTarget & target)
{
	msg.WI(IGMIT_PROCESSED).W(target).A();
	switch(message)
	{
	case IGMITIC_ACCESS:
		{
			TestRegisterPlayer(socket,msg,message,target);
		}
		return true;
	case IGMITIC_REGISTER:
		{
			SPlayerInfo	s_player;
			msg.R(s_player);
			RegisterPlayer(s_player);
			RL("IGMITIC_REGISTER   %8d %7d %7d %s", s_player.RID, s_player.ID, s_player.target_gamestate_ID, s_player.SID);
			msg.WI(IGMIT_REGISTRATION_END).W(target).A();
		}
		return true;
	case IGMITIC_UNREGISTER:
		{
			SPlayerInfo	s_player;
			msg.R(s_player);
			RL("IGMITIC_UNREGISTER %8d %7d %7d %s", s_player.RID, s_player.ID, s_player.target_gamestate_ID, s_player.SID);
			UnregisterPlayer(s_player);
		}
		return true;
	case IGMIT_RESOURCES_EXTENDED:
		{
			UpdateSocialFriends(target.tp_id, msg);
			ProcessPendingRegistrationTasks(target.tp_id);
		}
		return true;
	}
	return false;
};	
/***************************************************************************/
bool GAccessManager::RegisterPlayer(SPlayerInfo & s_player)
{
	stats.registered++;
	SRAP(INFO_ACCESS_REGISTER_INSTANCE);

	instances.insert(pair<INT32, SPlayerInfo>(s_player.ID, s_player));
	NotifyGlobalInfoManagerAboutInstance(IGMI_REGISTER_INSTANCE, s_player);

	TSessionMap::iterator it = sessions.find(s_player.ID);
	if (it != sessions.end())
	{
		it->second.last_connection = CurrentSTime();
	}

	return true;
}
/***************************************************************************/
bool GAccessManager::UnregisterPlayer(SPlayerInfo & s_player)
{
	stats.unregistered++;
	SRAP(INFO_ACCESS_UNREGISTER_INSTANCE);

	TInstanceMap::iterator pos;
	pair<TInstanceMap::iterator, TInstanceMap::iterator> p = instances.equal_range(s_player.ID);
	for (pos = p.first; pos != p.second; ++pos)
	{
		if (pos->second == s_player)
		{
			NotifyGlobalInfoManagerAboutInstance(IGMI_UNREGISTER_INSTANCE, pos->second);
			instances.erase(pos);
			break;
		}
	}

	return true;
}
/***************************************************************************/
void GAccessManager::UnregisterAllInstances(GSocket * socket)
{
    SRAP(INFO_ACCESS_UNREGISTER_ALL_INSTANCES);
    GSocketInternalServer * server=static_cast<GSocketInternalServer*>(socket);

    TInstanceMap::iterator pos;
    for (pos = instances.begin(); pos != instances.end();)
    {
        if (pos->second.game_instance == server->data.game_instance &&
            pos->second.global_group_id == server->data.global_group_id)
        {
            NotifyGlobalInfoManagerAboutInstance(IGMI_UNREGISTER_INSTANCE, pos->second);
            instances.erase(pos++);
        }
        else
        {
            ++pos;
        }
    }
}
/***************************************************************************/
void GAccessManager::NotifyGlobalInfoManagerAboutInstance(INT32 message, SPlayerInfo & s_player)
{
	if (message != IGMI_REGISTER_INSTANCE && message != IGMI_UNREGISTER_INSTANCE)
	{
		return;
	}

	SPlayerInstance s_player_instance;
	Coerce(s_player_instance, s_player);

	GServerBase::TSocketIdMap::iterator its;
	for (its = socket_base_map.begin(); its != socket_base_map.end(); ++its)
	{
		GSocketInternalServer * socket = static_cast<GSocketInternalServer*>(its->second);
		if (socket->data.game_instance == game_instance &&
			socket->data.game_name == "globalinfo")
		{
			socket->MsgExt(message).W(s_player_instance).A();
		}
	}
}
/***************************************************************************/
void GAccessManager::ClearFailedLoginBans()
{
	failed_login_count.clear();
}
/***************************************************************************/
bool GAccessManager::TestRegisterPlayer(GSocket *socket,GNetMsg & msg,int message,GNetTarget & target)
{
	SRAP(INFO_ACCESS_TEST_REGISTER_PLAYER);
	GSocketInternalServer * server=static_cast<GSocketInternalServer*>(socket);
	GTicketConnection t_access(message,target);
	msg.R(t_access);

	RLVERBOSE("---------------------------------------- %ld ----------------------------------------",stats.registered);
	RLVERBOSE("Access/Access/ID: %d",t_access.access.ID);
	RLVERBOSE("Access/Access/target_gamestate_ID: %d", t_access.access.target_gamestate_ID);
	RLVERBOSE("Access/Access/target_location_ID: %s", t_access.access.target_location_ID);
	RLVERBOSE("Access/Access/RID: %d",t_access.access.RID);
	RLVERBOSE("Access/Access/SID: %s",t_access.access.SID);
	RLVERBOSE("Access/Server/Host: %s",AddrToString(t_access.server_host.server_addr_connected_by_client)());
	RLVERBOSE("Access/Server/Port: %d",t_access.server_host.server_port_connected_by_client);

	GTicketPlayer t_player(IGMIT_PLAYER,target);
	//przepisujemy access to player_info - uzyskujemy w ten sposob w player info to co faktycznie 
	//jest nam potrzebne, a nie caly smietnik zwiazany z info o graczu
	//reszta trafia do loga logowania
	Coerce(t_player.data,t_access);
	t_player.data.game_instance=server->data.game_instance;
	t_player.data.global_group_id = 0;								// to zostanie ustawione pozniej w serwerze gry

	TFailedLoginCountMap::iterator flit;
	flit = failed_login_count.find(t_access.client_host.addr_detected_by_server);
	if (flit != failed_login_count.end() && flit->second >= 10)
	{
		flit->second++;
		// Zabezpieczenie przed zbytnim zaspamowaniem logow.
		if (flit->second <= 15 || (flit->second % 100) == 0)
		{
			RL("AccessFailed login flood detected (attempt count: %d). Sending incorrect password message.", flit->second);
			RL("AccessFailed/Access/ID: %d", t_access.access.ID);
			RL("AccessFailed/Access/target_gamestate_ID: %d", t_access.access.target_gamestate_ID);
			RL("AccessFailed/Access/target_location_ID: %s", t_access.access.target_location_ID);
			RL("AccessFailed/Client/ComputerIP(server): %s", AddrToString(t_access.client_host.addr_detected_by_server)());
		}
		msg.WI(IGMIT_ACCESS_CLOSE).W(target).WT("").A();
		return true;
	}

	bool error=false;
	//mamy SID
	if(t_access.access.ID!=0 && strlen(t_access.access.SID))
	{
		if(TestRegisterID_SID(msg,target,t_access, t_player)) error=true;
	}
	else
	{
		SRAP(WARNING_ACCESS_INCORRECT_DATA);
		msg.WI(IGMIT_ACCESS_REJECT).W(target).WT("").A();
		return true;
	}

	if (error)
	{
		//IP firmowe Ganymede:
		// 95.48.89.90 == 0x5a59305f
		// 193.25.3.40 == 0x280319c1
		// 192.168.x.x == polaczenia lokalne
		if (t_access.client_host.addr_detected_by_server != 0x5a59305f && 
			t_access.client_host.addr_detected_by_server != 0x280319c1 &&
			(t_access.client_host.addr_detected_by_server & 0xffff) != 0xa8c0)
		{
			if (flit != failed_login_count.end())
			{
				flit->second++;
			}
			else
			{
				failed_login_count.insert(pair<DWORD32, INT32>(t_access.client_host.addr_detected_by_server, 1));
			}
		}
		return true;
	}

	//find social relation
	if (t_player.data.target_gamestate_ID == 0)
	{
		t_player.data.access = EGS_UNKNOWN;
	}
	else
	{
		if (t_player.data.ID == t_player.data.target_gamestate_ID)
		{
			t_player.data.access = EGS_PLAYER;
		}
		else
		{
			t_player.data.access = EGS_UNKNOWN;

			TSocialFriendsTimes::iterator pos;
			pos = social_friends_map_timestamp.find(t_player.data.ID);
			if (pos != social_friends_map_timestamp.end() && pos->second.first != 0)
			{
				// Mamy w serwisie aktualna liste przyjaciol.
				TFriendsList::iterator fpos;
				pair<TFriendsList::iterator, TFriendsList::iterator> fp = social_friends_map.equal_range(t_player.data.ID);
				for (fpos = fp.first; fpos != fp.second; fpos++)
				{
					INT32 friendId = fpos->second;
					if (friendId == t_player.data.target_gamestate_ID)
					{
						t_player.data.access = EGS_FRIEND;
						break;
					}
				}
				// Przedluzamy 'expiration time' listy przyjaciol gracza.
				pos->second.second = CurrentSTime() + 60 * TIME_MAX_SESSION_HASH;
			}
			else
			{
				// Nie mamy w serwisie aktualnej listy przyjaciol. Musimy ja pobrac z serwisu cache.
				GTicketInternalClient * socket_cache = InternalClient(ESTClientCache);
				if (socket_cache != NULL && socket_cache->IsConnection())
				{
					GNetTarget target_cache;
					target_cache.tp_id = t_player.data.ID;
					socket_cache->MsgExt(IGMIT_RESOURCES_VERSION).W(target_cache).A();

					// Insert registration task to queue.
					{
						SAccessRegistrationTask new_task;
						new_task.socketid = socket->socketid;
						new_task.target = target;
						new_task.t_player = t_player;
						new_task.expiration_time = CurrentSTime() + 15;			// dajemy 15 sek. na odpowiedz serwisu cache
						tasks_registration.push_back(new_task);
						return true;
					}
				}

				// Jesli nie ma polaczenia z serwisem cache to nie czekamy na jego nawiazanie,
				// bo nie wiadomo ile to moze potrwac.
				// Uznajemy na ten moment, ze gracze nie sa przyjaciolmi.
			}
		}
	}

	SRAP(INFO_ACCESS_GRANTED);
	msg.WI(IGMIT_PLAYER).W(target).W(t_player).A();
	msg.WI(IGMIT_ACCESS_ACCEPT).W(target).WT("").A();
	return true;
}
/***************************************************************************/
bool GAccessManager::TestRegisterID_SID(GNetMsg & msg,GNetTarget & target,GTicketConnection & t_access, GTicketPlayer & t_player)
{
	RLVERBOSE("Test ID-SID");
	RLVERBOSE("-----------");
	SRAP(INFO_ACCESS_TEST_ID_SID);
	int ID=0;
	SSession s;
	bool s_from_hash=false;
	bool s_from_sql=false;

	//po SID'ie nie znajdziemy nic w naszym hash'u wiec szukamy w bazie danych czy taki SID wystepuje
	//sprawdzamy czy jest otwarta sesja dla tego elementu
	TSessionMapRev::iterator it_rev;
	it_rev=sessions_rev.find(t_access.access.SID);
	if(it_rev!=sessions_rev.end())
	{
		ID=it_rev->second;
	}
	if(ID!=0)
	{
		//sprawdzamy czy jest otwarta sesja dla tego elementu
		TSessionMap::iterator it;
		it=sessions.find(ID);
		if(it!=sessions.end())
		{
			s=it->second;
			s_from_hash=true;
			boost::format txt("expiration: %s");
			txt % ToTime(s.expiration)();
			RLVERBOSE("Session found in hash: %s",txt.str().c_str());
			SRAP(INFO_ACCESS_SESSION_FROM_HASH);
		}
	}
	else
	{
#if 1
		// Backdoor dla polaczen z sieci lokalnej (192.168.x.x i 127.0.0.1)
		// Jesli dostaniemy sid "VivaGanymede" to wpuszczamy gracza na istniejaca sesje (jesli jest wazna), albo tworzymy nowa sesje jak nie ma zadnej w bazie.
		if (((t_access.client_host.addr_detected_by_server & 0xffff) == 0xa8c0 ||
			(t_access.client_host.addr_detected_by_server & 0xffffffff) == 0x0100007f) &&
			strncmp(t_access.access.SID, "VivaGanymede", 12) == 0)
		{
			bool session_found = false;
			char command[256];
			sprintf(command, "SELECT `sid`, `expiration` FROM `rtm_sessions` WHERE `user_id` = '%d'", t_access.access.ID);
			boost::recursive_mutex::scoped_lock lock(mtx_sql);
			if (MySQLQuery(&mysql, command))
			{
				MYSQL_RES * result = mysql_store_result(&mysql);
				if (result)
				{
					MYSQL_ROW row = mysql_fetch_row(result);
					if (row)
					{
						if (ATOI64(row[1]) >= (INT64)CurrentSTime() + 60 * 2)
						{
							session_found = true;
							strncpy(t_access.access.SID, row[0], MAX_SID);
							t_access.access.SID[MAX_SID - 1] = 0;
						}
						else
						{
							sprintf(command, "DELETE FROM `rtm_session` WHERE `user_id` = '%d'", t_access.access.ID);
							MySQLQuery(&mysql, command);
						}
					}
					mysql_free_result(result);
				}
			}

			if (!session_found)
			{
				sprintf(t_access.access.SID, "%d-%lld", t_access.access.ID, CurrentSTime());

				sprintf(command, "INSERT INTO `rtm_sessions` (`user_id`, `expiration`, `sid`) VALUES ('%d', '%lld', '%s')",
					t_access.access.ID, CurrentSTime() + 60 * TIME_MAX_SESSION_HASH, GMySQLReal(&mysql, t_access.access.SID)() );
				MySQLQuery(&mysql, command);
			}
		}
#endif

		//sprawdzamy czy sesja jest w SQL'u, 
		char command[256];
		sprintf(command,"SELECT `user_id`,`expiration` FROM `rtm_sessions` WHERE `sid`='%s'",t_access.access.SID);
		boost::recursive_mutex::scoped_lock lock(mtx_sql);
		if(MySQLQuery(&mysql,command))
		{
			MYSQL_RES * result=mysql_store_result(&mysql);
			if (result)
			{
				MYSQL_ROW row=mysql_fetch_row(result);
				if(row)
				{
					ID=ATOI(row[0]);
					s.expiration=ATOI64(row[1]);
					strncpy(s.SID,t_access.access.SID,MAX_SID);
					t_access.access.SID[MAX_SID-1]=0;
					s_from_sql=true;
					boost::format txt("expiration: %s");
					txt % ToTime(s.expiration)();
					RLVERBOSE("Session found in SQL: %s",txt.str().c_str());
					SRAP(INFO_ACCESS_SESSION_FROM_SQL);
				}
				mysql_free_result(result);
			}
		}
	}

	if((!s_from_hash && !s_from_sql)||(ID==0))
	{
		RLVERBOSE("Session has expired");
		SRAP(INFO_ACCESS_SESSION_EXPIRED);
		//nie ma otartej sesji wiec logowanie poprzez SID'a jest niemozliwe
		msg.WI(IGMIT_ACCESS_REJECT).W(target).WT("").A();
		return true;
	}

	strncpy(t_player.data.SID,s.SID,MAX_SID);
	t_player.data.SID[MAX_SID-1]=0;

	//patrzymy czy ID zgadza sie z ID sesji, jesli nie - ktos oszukuje i zmienia sobie ID gracza, 
	//poniewaz ID moze w postaci jawnej trafic do klienta jest duze prawdopodobienstwo takiego dzialania
	if(ID!=t_access.access.ID)
	{
		RLVERBOSE("Incorrect ID");
		SRAP(WARNING_ACCESS_INCORRECT_ID);
		msg.WI(IGMIT_ACCESS_CLOSE).W(target).WT("").A();
		return true;
	}
	RLVERBOSE("ID-Session active, access to server granted");
	SRAP(WARNING_ACCESS_ID_SID_ACCESS_GRANTED);
	//wszystko gra, mozemy dodac sesje to tabel sesji
	if(s_from_sql) AddSessionFromSQL(t_access.access.ID,s);
	return false;
}
/***************************************************************************/

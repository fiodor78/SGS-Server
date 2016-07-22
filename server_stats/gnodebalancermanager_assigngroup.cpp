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
#include "gtruerng.h"
extern GServerBase * global_server;

/***************************************************************************/
bool GNodeBalancerManager::ProcessTargetMessage(GSocket *socket,GNetMsg & msg,int message,GNetTarget & target)
{
	msg.WI(IGMIT_PROCESSED).W(target).A();

	switch (message)
	{
	case IGMITIC_LOCATION_ADDRESS_GET:
		{
			INT32 group_id;
			EServiceTypes connection_type;
			string location_ID, host, ports;

			msg.RT(location_ID);
			connection_type = (EServiceTypes)msg.RI();

			if (!GetLocationAddress(location_ID, connection_type, host, ports, group_id))
			{
				RL("GetLocationAddress('%s', '%s') failed - IGMITIC_LOCATION_ADDRESS_GET",
					location_ID.c_str(),
					(connection_type == ESTClientBase) ? "group" : (connection_type == ESTClientBaseConsole) ? "console" : "client");
				msg.WI(IGMIT_LOCATION_ADDRESS_RESPONSE).W(target).WT(location_ID).WI((INT32)connection_type).Wb(false).WUI(0).WT("").WI(0).A();
			}
			else
			{
				DWORD32 host_addr = GetAddrFromName(host.c_str());
				msg.WI(IGMIT_LOCATION_ADDRESS_RESPONSE).W(target).WT(location_ID).WI((INT32)connection_type).Wb(true).WUI(host_addr).WT(ports).WI(group_id).A();
			}
		}
		return true;
	}
	return false;
}
/***************************************************************************/
DWORD64 GNodeBalancerManager::PickReadyGroup(EGroupType group_type)
{
	DWORD64 now = CurrentSTime();
	DWORD64 ready_group_id = 0;

	while (ready_group_id == 0)
	{
		INT32 game_groups_ready_count = game_groups_ready[group_type].size();
		if (game_groups_ready_count == 0)
		{
			return 0;
		}
		DWORD64 global_group_id = game_groups_ready[group_type][truerng.Rnd(game_groups_ready_count)];
		TGameGroups::iterator it = game_groups.find(global_group_id);
		if (it == game_groups.end())
		{
			return 0;
		}

		SGameGroup & group = it->second;
		// Jesli grupa ma przedawniony heartbeat, to ja usuwamy z game_groups_ready.
		if (group.last_beat + (GROUP_HEARTBEAT_INTERVAL_SECONDS + NODEBALANCER_GETGROUPS_INTERVAL_SECONDS) < now)
		{
			group.status = EEGS_STOPPED;
			vector<DWORD64>::iterator it;
			for (it = game_groups_ready[group.type].begin(); it != game_groups_ready[group.type].end(); it++)
			{
				if (*it == group.global_group_id)
				{
					game_groups_ready[group.type].erase(it);
					break;
				}
			}
			continue;
		}

		ready_group_id = global_group_id;
	}

	return ready_group_id;
}
/***************************************************************************/
bool GNodeBalancerManager::GetLocationAddress(string & location_ID, EServiceTypes connection_type, string & host, string & ports, INT32 & group_id)
{
	bool mysql_failed;

 	if (location_ID == "")
	{
		return false;
	}

	char command[512], column_names[256];
	switch (connection_type)
	{
	case ESTClientBase:
		strcpy(column_names, ", `host`, `port`");
		break;
	case ESTClientBaseConsole:
		strcpy(column_names, ", `host_console`, `port_console`");
		break;
	case ESTClientBaseGame:
		strcpy(column_names, ", `host_external`, `ports_external`");
		break;
	default:
		return false;
	}

	// 1. Sprawdzamy czy istnieje juz wazne przypisanie grupy do lokacji.
	sprintf(command, "SELECT `status`, `host`, `port`%s, `group_id` "
						"FROM `rtm_locations` WHERE `location_id` = '%s' AND `game_instance` = '%d'",
						(connection_type == ESTClientBase) ? "" : column_names, GMySQLReal(&mysql, location_ID.c_str())(), game_instance);
	boost::recursive_mutex::scoped_lock lock(mtx_sql);
	group_id = -1;
	mysql_failed = true;
	if (MySQLQuery(&mysql, command))
	{
		MYSQL_RES * result = mysql_store_result(&mysql);
		if (result)
		{
			mysql_failed = false;

			MYSQL_ROW row;
			row = mysql_fetch_row(result);
			if (row)
			{
				INT32 pos = 0;
				string status = row[pos++];
				if (status == "valid")
				{
					string group_host, group_port;
					group_host = row[pos++];
					group_port = row[pos++];
					if (connection_type == ESTClientBase)
					{
						host = group_host;
						ports = group_port;
					}
					else
					{
						host = row[pos++];
						ports = row[pos++];
					}
					group_id = ATOI(row[pos++]);

					// Sprawdzamy czy zwrocona grupa nie jest potencjalnie martwa.
					DWORD32 host_addr = GetAddrFromName(group_host.c_str());
					INT32 port = ATOI(group_port.c_str());
					DWORD64 global_group_id = GLOBAL_GROUP_ID(host_addr, port, group_id);
					TGameGroups::iterator it = game_groups.find(global_group_id);
					if (it == game_groups.end() || (it->second).status == EEGS_STOPPED)
					{
						potentially_dead_game_groups.insert(global_group_id);
						// Chcemy jak najszybciej zwolnic przypisania potencjalnie martwych grup.
						SetGroupsInfoReloadDeadline(3);
					}
				}
			}
			mysql_free_result(result);
		}
	}
	if (mysql_failed || group_id != -1)
	{
		return !mysql_failed;
	}

	// 2. Sprawdzamy czy mozemy utworzyc powiazanie i do ktorej grupy ma trafic.
	EGroupType group_type = EGT_LOCATION;
	if (strncmp(location_ID.c_str(), "gamestate", 9) == 0)
	{
		group_type = EGT_GAMESTATE;
	}
	if (strncmp(location_ID.c_str(), "lobby", 5) == 0)
	{
		group_type = EGT_LOBBY;
	}
	if (strncmp(location_ID.c_str(), "leaderboard", 11) == 0)
	{
		group_type = EGT_LEADERBOARD;
	}

	if (group_type != EGT_GAMESTATE)
	{
		sprintf(command, "SELECT `location_id` FROM `data_location_blob` WHERE `location_id` = '%s'",
							GMySQLReal(&mysql, location_ID.c_str())());
		bool location_exists = false;
		if (MySQLQuery(&mysql, command))
		{
			MYSQL_RES * result = mysql_store_result(&mysql);
			if (result)
			{
				if (mysql_fetch_row(result))
				{
					location_exists = true;
				}
				mysql_free_result(result);
			}
		}
		if (!location_exists)
		{
			return false;
		}
	}

	// 3. Brak przypisania, albo jest niewazne. Wybieramy losowo grupe kategorii group_type, ktorej przypiszemy lokacje.
	DWORD64 ready_group_id = PickReadyGroup(group_type);
	if (ready_group_id == 0)
	{
		return false;
	}
	SGameGroup & group = game_groups[ready_group_id];

	// 4. Grupa zostala wybrana. Ustawiamy powiazanie w bazie danych.
	if (!MySQLQuery(&mysql, "START TRANSACTION"))
	{
		return false;
	}

	group_id = -1;
	mysql_failed = true;
	sprintf(command, "SELECT `status`%s, `group_id` "
						"FROM `rtm_locations` WHERE `location_id` = '%s' AND `game_instance` = '%d' FOR UPDATE",
						column_names, GMySQLReal(&mysql, location_ID.c_str())(), game_instance);
	if (MySQLQuery(&mysql, command))
	{
		MYSQL_RES * result = mysql_store_result(&mysql);
		if (result)
		{
			mysql_failed = false;

			MYSQL_ROW row;
			row = mysql_fetch_row(result);
			if (row)
			{
				INT32 pos = 0;
				string status = row[pos++];
				if (status == "valid")
				{
					host = row[pos++];
					ports = row[pos++];
					group_id = ATOI(row[pos++]);
				}
			}
			mysql_free_result(result);
		}
	}
	if (mysql_failed || group_id != -1)
	{
		MySQLQuery(&mysql, "ROLLBACK");
		return !mysql_failed;
	}

	// 5. Zaden inny nodebalancer nie ustawil nam juz w miedzyczasie powiazania, mozemy wiec ustawic swoje.
	mysql_failed = true;
	sprintf(command, "REPLACE `rtm_locations` SET `location_id` = '%s', `game_instance` = '%d', `host` =  '%s', `port` = '%s', `group_id` = '%d', "
						"`host_console` = '%s', `port_console` = '%s', `host_external` = '%s', `ports_external` = '%s', "
						"`status` = '%s'",
						GMySQLReal(&mysql, location_ID.c_str())(), game_instance, group.host_str.c_str(), group.port_str.c_str(), group.group_id, 
						group.host_console.c_str(), group.port_console.c_str(), group.host_external.c_str(), group.ports_external.c_str(),
						"valid");
	if (MySQLQuery(&mysql, command))
	{
		if (MySQLQuery(&mysql, "COMMIT"))
		{
			mysql_failed = false;

			switch (connection_type)
			{
			case ESTClientBase:
				host = group.host_str;
				ports = group.port_str;
				group_id = group.group_id;
				break;
			case ESTClientBaseConsole:
				host = group.host_console;
				ports = group.port_console;
				group_id = group.group_id;
				break;
			case ESTClientBaseGame:
				host = group.host_external;
				ports = group.ports_external;
				group_id = group.group_id;
				break;
			}
		}
	}

	if (mysql_failed)
	{
		MySQLQuery(&mysql, "ROLLBACK");
		return false;
	}

	return true;
}
/***************************************************************************/

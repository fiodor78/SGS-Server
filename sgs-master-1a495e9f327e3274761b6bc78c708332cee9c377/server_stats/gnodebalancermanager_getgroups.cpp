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
bool GNodeBalancerManager::ReadGroupsInfoFromSQL(TGameGroups & game_groups)
{
	DWORD64 now = CurrentSTime();

	game_groups.clear();

	bool mysql_failed = true;
	char command[512];
	sprintf(command, "SELECT `host`, `port`, `group_id`, `type`, `host_console`, `port_console`, `host_external`, `ports_external`, `status`, `last_beat`, UNIX_TIMESTAMP() "
						"FROM `rtm_group_heartbeat` WHERE `game_instance` = '%d'", game_instance);
	boost::recursive_mutex::scoped_lock lock(mtx_sql);
	if (MySQLQuery(&mysql, command))
	{
		MYSQL_RES * result = mysql_store_result(&mysql);
		if (result)
		{
			mysql_failed = false;

			MYSQL_ROW row;
			row = mysql_fetch_row(result);
			while (row)
			{
				INT32 pos = 0;
				SGameGroup group;
				group.host_str = row[pos++];
				group.port_str = row[pos++];
				group.host = GetAddrFromName(group.host_str.c_str());
				group.port = (USHORT16)ATOI(group.port_str.c_str());
				group.group_id = ATOI(row[pos++]);
				string group_type = row[pos++];
				group.type = (group_type == "gamestate") ? EGT_GAMESTATE : (group_type == "lobby") ? EGT_LOBBY : (group_type == "leaderboard") ? EGT_LEADERBOARD : EGT_LOCATION;
				group.global_group_id = GLOBAL_GROUP_ID(group.host, group.port, group.group_id);
				group.host_console = row[pos++];
				group.port_console = row[pos++];
				group.host_external = row[pos++];
				group.ports_external = row[pos++];
				string group_status = row[pos++];
				group.status = (group_status == "ready") ? EEGS_READY : (group_status == "busy") ? EEGS_BUSY : EEGS_STOPPED;
				group.last_beat = ATOI64(row[pos++]);
				INT64 mysql_timestamp = ATOI64(row[pos++]);
				group.last_beat += (INT64)now - mysql_timestamp;

				game_groups[group.global_group_id] = group;

				row = mysql_fetch_row(result);
			}
			mysql_free_result(result);
		}
	}

	return !mysql_failed;
}
/***************************************************************************/
// Szukamy grup, ktorych adres host_external:port_external zostal przejety przez inny serwer.
void GNodeBalancerManager::GetDeprecatedGroups(TGameGroups & game_groups, set<DWORD64> & deprecated_group_ids)
{
	deprecated_group_ids.clear();

	map<string, DWORD64> external_ports_last_beat;				// "host_external:port_external" -> najnowszy heart beat
	map<string, DWORD64> external_ports_last_beat_server_id;	// "host_external:port_external" -> (host_internal+port_internal == global_group_id >> 16)

	TGameGroups::iterator pos;
	for (pos = game_groups.begin(); pos != game_groups.end(); pos++)
	{
		SGameGroup & group = pos->second;

		boost::char_separator<char> sep(",");
		typedef boost::tokenizer<boost::char_separator<char> > TTokenizer;
		TTokenizer tok(group.ports_external, sep);
		TTokenizer::iterator it;
		for (it = tok.begin(); it != tok.end(); it++)
		{
			string external_host_port = group.host_external + ":" + (*it);
			if (group.last_beat > external_ports_last_beat[external_host_port])
			{
				external_ports_last_beat[external_host_port] = group.last_beat;
				external_ports_last_beat_server_id[external_host_port] = group.global_group_id >> 16;
			}
		}
	}

	for (pos = game_groups.begin(); pos != game_groups.end(); pos++)
	{
		SGameGroup & group = pos->second;

		boost::char_separator<char> sep(",");
		typedef boost::tokenizer<boost::char_separator<char> > TTokenizer;
		TTokenizer tok(group.ports_external, sep);
		TTokenizer::iterator it;
		for (it = tok.begin(); it != tok.end(); it++)
		{
			string external_host_port = group.host_external + ":" + (*it);
			if (group.global_group_id >> 16 != external_ports_last_beat_server_id[external_host_port])
			{
				// Grupa ma przypisany co najmniej jeden adres external, ktory zostal przejety przez inna grupe.
				// Wniosek: grupa jest martwa.
				deprecated_group_ids.insert(group.global_group_id);
				break;
			}
		}
	}
}
/***************************************************************************/
void GNodeBalancerManager::SetGroupsInfoReloadDeadline(DWORD64 deadline_seconds)
{
	DWORD64 now = CurrentSTime();
	if (now + deadline_seconds < next_groups_info_reload_deadline)
	{
		next_groups_info_reload_deadline = now + deadline_seconds;
	}
}
/***************************************************************************/
void GNodeBalancerManager::GetGroupsInfoFromSQL()
{
	DWORD64 now = CurrentSTime();
	if (now < next_groups_info_reload_deadline)
	{
		return;
	}
	next_groups_info_reload_deadline = now + (NODEBALANCER_GETGROUPS_INTERVAL_SECONDS * 2 / 3) + truerng.Rnd(NODEBALANCER_GETGROUPS_INTERVAL_SECONDS * 1 / 3);

	{
		TGameGroups::iterator pos;
		for (pos = game_groups.begin(); pos != game_groups.end(); pos++)
		{
			SGameGroup & group = pos->second;
			if (group.last_beat + (GROUP_HEARTBEAT_INTERVAL_SECONDS + NODEBALANCER_GETGROUPS_INTERVAL_SECONDS) < now)
			{
				potentially_dead_game_groups.insert(group.global_group_id);
			}
		}
	}

	// Jak nie uda nam sie odczytac informacji o grupach z bazy danych to efekt bedzie taki,
	// ze nie bedziemy mogli tworzyc nowych przypisan, a jedynie zwracac informacje o juz istniejacych.
	// Takie zachowanie jest pozadane.
	game_groups.clear();
	INT32 a;
	for (a = 0; a < EGT_LAST; a++)
	{
		game_groups_ready[a].clear();
	}

	// Jesli nie uda nam sie odczytac informacji o grupach z bazy (mysql_failed) to nic nie robimy.
	// Wstrzymujemy sie rowniez ze zwalnianiem przypisania lokacji do grup, ktore staly sie martwe.

	// Zwolnienie przypisania jesli grupa caly czas dziala i trzyma lokacje jest niebezpieczne.
	// Natomiast nie zwolnienie przypisania jesli grupa jest martwa powoduje niemoznosc podpiecia sie do lokacji,
	// a z tym mozna poczekac do momentu gdy tabela `rtm_group_heartbeat` bedzie dzialac poprawnie.
	TGameGroups game_groups_sql;
	if (!ReadGroupsInfoFromSQL(game_groups_sql))
	{
		return;
	}

	// Oznaczamy jako martwe grupy, ktorych adres host_external:port_external zostal przejety przez inny serwer.
	{
		set<DWORD64> deprecated_group_ids;
		GetDeprecatedGroups(game_groups_sql, deprecated_group_ids);

		set<DWORD64>::iterator it;
		for (it = deprecated_group_ids.begin(); it != deprecated_group_ids.end(); it++)
		{
			game_groups_sql[*it].status = EEGS_STOPPED;
			potentially_dead_game_groups.insert(*it);
		}
	}

	// Wpisujemy do 'game_groups' i 'game_groups_ready' informacje o zyjacych grupach.
	for (a = 0; a < EGT_LAST; a++)
	{
		game_groups_ready[a].reserve(game_groups_sql.size());
	}

	TGameGroups::iterator pos;
	for (pos = game_groups_sql.begin(); pos != game_groups_sql.end(); pos++)
	{
		SGameGroup & group = pos->second;

		if (group.last_beat + (GROUP_HEARTBEAT_INTERVAL_SECONDS + NODEBALANCER_GETGROUPS_INTERVAL_SECONDS) > now && group.status != EEGS_STOPPED)
		{
			potentially_dead_game_groups.erase(group.global_group_id);

			game_groups[group.global_group_id] = group;
			// 15 sek. to opoznienie jakie dopuszczamy na przeslanie heartbeatu z grupy do nodebalancera i jego zapis do bazy danych.
			if (group.last_beat + (GROUP_HEARTBEAT_INTERVAL_SECONDS + 15) > now && group.status == EEGS_READY)
			{
				game_groups_ready[group.type].push_back(group.global_group_id);
			}
		}
	}

	// Zwalniamy przypisania lokacji do grup, ktore uznalismy za martwe.
	if (potentially_dead_game_groups.size() > 0)
	{
		boost::recursive_mutex::scoped_lock lock(mtx_sql);
		char command[512];

		set<DWORD64>::iterator pos;
		for (pos = potentially_dead_game_groups.begin(); pos != potentially_dead_game_groups.end(); pos++)
		{
			DWORD64 dead_global_group_id = *pos;
			ReleaseLocation(dead_global_group_id, "");

			INT32 port = (INT32)(dead_global_group_id & 0xffff);
			dead_global_group_id >>= 16;
			DWORD32 host = (DWORD32)(dead_global_group_id & 0xffffffff);
			sprintf(command, "UPDATE `rtm_group_heartbeat` SET `status` = '%s' WHERE `host` =  '%s' AND `port` = '%d'",
								"stopped", AddrToString(host)(), (INT32)port);
			MySQLQuery(&mysql, command);
		}

		potentially_dead_game_groups.clear();
	}
}
/***************************************************************************/
void GNodeBalancerManager::RaportCurrentStatus()
{
	DWORD64 now = CurrentSTime();

	// Polaczenie z baza danych.
	RL("[STATUS] mysql_status: %s", mysql_ping(&mysql) == 0 ? "OK" : "Error");

	TGameGroups::iterator pos;
	for (pos = game_groups.begin(); pos != game_groups.end(); pos++)
	{
		SGameGroup & group = pos->second;
		RL("[STATUS] group: %15s:%-5d, id: %2d, type: %-10s, status: %-10s, last_beat: %lld (%lld seconds ago)",
			AddrToString(group.host)(), group.port, group.group_id,
			(group.type == EGT_GAMESTATE) ? "gamestate" : (group.type == EGT_LOBBY) ? "lobby" : (group.type == EGT_LEADERBOARD) ? "leaderboard" : "location",
			(group.status == EEGS_READY) ? "ready" : (group.status == EEGS_BUSY) ? "busy" : "stopped",
			group.last_beat, now - group.last_beat);
	}
}
/***************************************************************************/

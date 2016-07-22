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
GNodeBalancerManager::GNodeBalancerManager()
{
	game_groups.clear();
	INT32 a;
	for (a = 0; a < EGT_LAST; a++)
	{
		game_groups_ready[a].clear();
	}
	potentially_dead_game_groups.clear();
	console.SetServer(this);
	single_instance_service = false;
	next_groups_info_reload_deadline = 0;
}
/***************************************************************************/
bool GNodeBalancerManager::Init()
{
	if (!GServerManager::Init())
	{
		return false;
	}
	GetGroupsInfoFromSQL();
	return true;
}
/***************************************************************************/
void GNodeBalancerManager::ConfigureClock()
{
	GServerManager::ConfigureClock();
	signal_second.connect(boost::bind(&GNodeBalancerManager::GetGroupsInfoFromSQL, this));
	signal_minute.connect(boost::bind(&GNodeBalancerManager::ClearReleasedLocations, this));
	signal_minutes_5.connect(boost::bind(&GNodeBalancerManager::RaportCurrentStatus, this));
}
/***************************************************************************/
bool GNodeBalancerManager::ProcessInternalMessage(GSocket * socket, GNetMsg & msg, int message)
{
	GSocketInternalServer * si = static_cast<GSocketInternalServer*>(socket);

	switch (message)
	{
	case IGMI_GROUP_STATUS_UPDATE:
		ProcessGroupStatusUpdate(socket, msg);
		return true;

	case IGMI_LOCATION_RELEASE:
		{
			string location_ID;
			msg.RT(location_ID);

			bool result = ReleaseLocation(si->data.global_group_id, location_ID.c_str());
			msg.WI(IGMI_LOCATION_RELEASE_RESPONSE).WT(location_ID).Wb(result).A();
		}
		return true;
	}
	return false;
}
/***************************************************************************/
void GNodeBalancerManager::ProcessGroupStatusUpdate(GSocket * socket, GNetMsg & msg)
{
	GSocketInternalServer * si = static_cast<GSocketInternalServer*>(socket);

	DWORD64 now, timestamp;
	unsigned int host, host_console, host_external;
	unsigned short port, port_console;
	string ports_external;
	INT32 group_id, t, s, locations_loaded;
	EGroupType group_type;
	EEffectiveGroupStatus group_status;

	now = CurrentSTime();

	msg.RBUI(timestamp);
	msg.RUI(host).RUS(port).RI(group_id).RI(t).RI(s).RI(locations_loaded);
	group_type = (EGroupType)t;
	group_status = (EEffectiveGroupStatus)s;
	if (group_status != EEGS_READY && group_status != EEGS_BUSY)
	{
		group_status = EEGS_STOPPED;
	}
	msg.RUI(host_console).RUS(port_console).RUI(host_external).RT(ports_external);

	// Nie powinno sie nigdy zdarzyc.
	if (si->data.game_instance != game_instance ||
		si->data.global_group_id != GLOBAL_GROUP_ID(host, port, group_id))
	{
		return;
	}

	struct in_addr IPaddr;
	string host_str, host_console_str, host_external_str;
	IPaddr.s_addr = host;
	host_str = inet_ntoa(IPaddr);
	IPaddr.s_addr = host_console;
	host_console_str = inet_ntoa(IPaddr);
	IPaddr.s_addr = host_external;
	host_external_str = inet_ntoa(IPaddr);

	// Zapobiegamy przydzialowi lokacji do grupy, ktora jeszcze nie zdazyla sie zbindowac pod wlasciwy adres.
	if (host == 0 || port == 0 || host_console == 0 || port_console == 0 || host_external == 0 || ports_external == "0")
	{
		group_status = EEGS_BUSY;
	}

	const char * group_type_str = (group_type == EGT_GAMESTATE) ? "gamestate" :
		(group_type == EGT_LOBBY) ? "lobby" : 
		(group_type == EGT_LEADERBOARD) ? "leaderboard" : "location";

	char command[512];
	sprintf(command, "REPLACE `rtm_group_heartbeat` SET `host` =  '%s', `port` = '%d', `group_id` = '%d', `game_instance` = '%d', `type` = '%s', "
						"`host_console` = '%s', `port_console` = '%d', `host_external` = '%s', `ports_external` = '%s', "
						"`status` = '%s', `last_beat` = UNIX_TIMESTAMP(), `locations_loaded` = '%d'",
						host_str.c_str(), (INT32)port, group_id, game_instance, group_type_str,
						host_console_str.c_str(), (INT32)port_console, host_external_str.c_str(), ports_external.c_str(),
						group_status == EEGS_READY ? "ready" : group_status == EEGS_BUSY ? "busy" : "stopped",
						locations_loaded);

	boost::recursive_mutex::scoped_lock lock(mtx_sql);
	if (MySQLQuery(&mysql, command))
	{
		msg.WI(IGMI_GROUP_STATUS_UPDATE_RESPONSE).WBUI(timestamp).A();
	}
	else
	{
		msg.WI(IGMI_GROUP_STATUS_UPDATE_RESPONSE).WBUI(0).A();
	}

	//-----------------------------------------------------------------------

	char temp_string[32];

	SGameGroup group;
	group.global_group_id = GLOBAL_GROUP_ID(host, port, group_id);
	group.host = host;
	group.port = port;
	group.group_id = group_id;
	group.type = group_type;
	group.status = group_status;
	group.last_beat = now;
	group.host_str = host_str;
	sprintf(temp_string, "%d", port);
	group.port_str = temp_string;
	group.host_console = host_console_str;
	sprintf(temp_string, "%d", port_console);
	group.port_console = temp_string;
	group.host_external = host_external_str;
	group.ports_external = ports_external;

	bool was_ready = false;
	TGameGroups::iterator it = game_groups.find(group.global_group_id);
	if (it != game_groups.end())
	{
		was_ready = ((it->second).status == EEGS_READY);
		it->second = group;
	}
	else
	{
		game_groups.insert(make_pair(group.global_group_id, group));
	}

	// Update 'game_groups_ready'.
	if (group.status == EEGS_READY && !was_ready)
	{
		game_groups_ready[group.type].push_back(group.global_group_id);

		// Wymuszamy przeladowanie w ciagu 3 sekund informacji o grupach z bazy danych.
		// Po to, zeby uznac za martwe ew. deprecated groups, ktorych 'spadkobierca' jest obecna grupa.
		// Sytuacja taka zachodzi gdy serwer gry padnie i podniesie sie na innym porcie internal niz poprzednio.
		// Pozostale nodebalancery i tak jeszcze przez jakis czas moga uznawac te martwe grupy za zywe,
		// ale przynajmniej ten nodebalancer uniknie szybko przyznawania do nich lokacji.
		// Jest to przydatne zwlaszcza w srodowisku developerskim z jednym nodebalancerem.
		SetGroupsInfoReloadDeadline(3);
	}
	if (group.status != EEGS_READY && was_ready)
	{
		vector<DWORD64>::iterator it;
		for (it = game_groups_ready[group.type].begin(); it != game_groups_ready[group.type].end(); it++)
		{
			if (*it == group.global_group_id)
			{
				game_groups_ready[group.type].erase(it);
				break;
			}
		}
	}
}
/***************************************************************************/
bool GNodeBalancerManager::ReleaseLocation(DWORD64 global_group_id, const char * location_ID)
{
	DWORD32 host = 0;
	INT32 port = 0;
	INT32 group_id = 0;

	group_id = (INT32)(global_group_id & 0xffff);
	global_group_id >>= 16;
	port = (INT32)(global_group_id & 0xffff);
	global_group_id >>= 16;
	host = (DWORD32)(global_group_id & 0xffffffff);

	if (*location_ID != 0)
	{
		char command[512];
		boost::recursive_mutex::scoped_lock lock(mtx_sql);

		sprintf(command, "UPDATE `rtm_locations` SET `status` = '%s' "
							"WHERE `location_id` = '%s' AND `host` = '%s' AND `port` = '%d' AND `group_id` = '%d'",
							"invalid", GMySQLReal(&mysql, location_ID)(), AddrToString(host)(), port, group_id);
		return MySQLQuery(&mysql, command);
	}
	
	if (*location_ID == 0)
	{
		char command[512];
		boost::recursive_mutex::scoped_lock lock(mtx_sql);

		sprintf(command, "UPDATE `rtm_locations` SET `status` = '%s' "
							"WHERE `host` = '%s' AND `port` = '%d' AND `group_id` = '%d'",
							"invalid", AddrToString(host)(), port, group_id);
		return MySQLQuery(&mysql, command);
	}

	return false;
}
/***************************************************************************/
void GNodeBalancerManager::ClearReleasedLocations()
{
	// Jeden serwis wykonuje zapytanie srednio raz na 15 minut.
	// Losowosc jest wprowadzona po to, zeby w miare mozliwosci serwisy wykonywaly je w innym momencie.
	if (truerng.Rnd(15) == 0)
	{
		char command[512];
		sprintf(command, "DELETE FROM `rtm_locations` WHERE `status` = '%s'", "invalid");
		global_sqlmanager->Add(command, EDB_SERVERS);
	}
}
/***************************************************************************/

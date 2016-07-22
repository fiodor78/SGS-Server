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

/***************************************************************************/
bool GServerStats::Init()
{
	if(!GServer::Init()) return false;
	InitThreads();
	return true;
}
/***************************************************************************/
void GServerStats::InitServices()
{
	GServerBaseConsole::InitServices();
	service_manager.services.push_back(SSockService(ESTAcceptBaseConsole,ESTAcceptBaseConsole));
	max_service_size=global_serverconfig->net.poll_room_limit;
}
/***************************************************************************/
void GServerStats::ConfigureClock()
{
	GServer::ConfigureClock();
	signal_second.connect(boost::bind(&GRaportManager::UpdateTime,&raport_manager_global,boost::ref(clock)));
	signal_second.connect(boost::bind(&GServerStats::ServicesWatchdog, this));
}
/***************************************************************************/
void GServerStats::Destroy()
{
	global_signals->close_slow();

	TManagersMap::iterator pos;
	for (pos = managers.begin(); pos != managers.end(); pos++)
	{
		pos->second.second->join();
		Delete(pos->second.second);
		Delete(pos->second.first);
	}

	GServer::Destroy();
}
/***************************************************************************/
#define INIT(MANAGERTYPE) \
	{ \
		MANAGERTYPE * _manager = new MANAGERTYPE(); \
		if (_manager->Init()) \
		{ \
			boost::thread * _thread = new boost::thread(boost::bind(&MANAGERTYPE::Action, _manager)); \
			assert(_thread); \
			managers[#MANAGERTYPE] = make_pair(_manager, _thread); \
		} \
		else \
		{ \
			Delete(_manager); \
		} \
	}

bool GServerStats::InitThreads()
{
	INIT(GAccessManager);
	INIT(GCacheManager);
	INIT(GSlowSQLManager);
	INIT(GFlashPolicyManager);
	INIT(GGlobalInfoManager);
	INIT(GNodeBalancerManager);
	INIT(GLeaderBoardManager);
	INIT(GCenzorManager);
	INIT(GGlobalMessageManager);
	return true;
}
/***************************************************************************/
bool GServerStats::InitSQL()
{
	if(!GServer::InitSQL()) return false;
	return true;
}
/***************************************************************************/
#define WATCHDOG(MANAGERTYPE, BINDINGS_CONFIG_NAME) \
	if (global_serverconfig->bindings_config[BINDINGS_CONFIG_NAME] != "") \
	{ \
		if (managers.find(#MANAGERTYPE) == managers.end()) \
		{ \
			INIT(MANAGERTYPE); \
		} \
		if (managers.find(#MANAGERTYPE) != managers.end() && managers[#MANAGERTYPE].first->last_heartbeat == 0) \
		{ \
			managers[#MANAGERTYPE].first->Close(); \
			managers[#MANAGERTYPE].second->join(); \
			Delete(managers[#MANAGERTYPE].second); \
			Delete(managers[#MANAGERTYPE].first); \
			managers.erase(#MANAGERTYPE); \
			INIT(MANAGERTYPE); \
		} \
	}

void GServerStats::ServicesWatchdog()
{
	WATCHDOG(GAccessManager, "access");
	WATCHDOG(GCacheManager, "cache");
	WATCHDOG(GSlowSQLManager, "slowsql");
	WATCHDOG(GFlashPolicyManager, "flashpolicy");
	WATCHDOG(GGlobalInfoManager, "globalinfo");
	WATCHDOG(GNodeBalancerManager, "nodebalancer");
	WATCHDOG(GLeaderBoardManager, "leaderboard");
	WATCHDOG(GCenzorManager, "cenzor");
	WATCHDOG(GGlobalMessageManager, "globalmessage");
}
#undef WATCHDOG
#undef INIT
/***************************************************************************/





/***************************************************************************/
GServerManager::GServerManager():GServerBaseConsole(),raport_interface_local(&raport_manager_local)
{
	sql_config_initialized=false;
	last_heartbeat = -1;
	single_instance_service = true;
}
/***************************************************************************/
bool GServerManager::Init()
{
	if(!GServerBaseConsole::Init()) return false;

	if (service_manager.services.size() == 0)
	{
		return false;
	}

	EServiceTypes type=service_manager.services[0].type_internal;
	GClock clock;
	string fn("Service");
	fn+=GetServiceName(type);
	raport_manager_local.Init(global_serverconfig->log_path.c_str(),fn.c_str());
	raport_manager_local.SetOverflow(global_serverconfig->misc.raport_overflow);
	raport_manager_local.UpdateName(clock);
	RL("Raport for %s initialized.",fn.c_str());

	if(!InitSQL())
	{
        RL("SQL Initialization error");
		return false;
	}

	{
		string fn("start ");
			fn+=GetServiceName(type);
		RaportRestartSQL(fn.c_str());
	}
	CheckForOtherInstances(true);

	return true;
}
/***************************************************************************/
bool GServerManager::InitSQL()
{
	mysql_init(&mysql);
	int pd=ATOI(global_serverconfig->sql_config.port.c_str());
	if (!mysql_real_connect(&mysql,
		global_serverconfig->sql_config.host.c_str(),
		global_serverconfig->sql_config.user.c_str(),
		global_serverconfig->sql_config.password.c_str(),
		global_serverconfig->sql_config.database.c_str(),
		pd,
		NULL,CLIENT_MULTI_STATEMENTS))
	{
		RL("Failed to connect to games database (%s,%s,%s,%s). Error: %s",
			global_serverconfig->sql_config.host.c_str(),
			global_serverconfig->sql_config.user.c_str(),
			global_serverconfig->sql_config.password.c_str(),
			global_serverconfig->sql_config.database.c_str(),
			mysql_error(&mysql));
		sql_config_initialized = false;
		return false;
	}
	else
	{
		sql_config_initialized = true;
		mysql_set_character_set(&mysql, "utf8");
	}
	mysql.reconnect=1;

	RL("SQL Client version %s",mysql_get_client_info());
	RL("SQL Host info %s",mysql_get_host_info(&mysql));
	RL("SQL Server info %s",mysql_get_server_info(&mysql));
	RL("SQL Stats %s",mysql_stat(&mysql));
	RL("SQL CharSet %s",mysql_character_set_name(&mysql));
	RL("SQL library initialized");

	return true;
}
/***************************************************************************/
void GServerManager::Destroy()
{
	UpdateHeartBeat(true);

	if (sql_config_initialized)
	{
		mysql_close(&mysql);
	}

	if(service_manager.services.size())
	{
		EServiceTypes type=service_manager.services[0].type_internal;
		string fn("stop ");
		fn+=GetServiceName(type);
		RaportRestartSQL(fn.c_str());
	}

	raport_interface_local.Destroy();
}
/***************************************************************************/
bool GServerManager::RegisterLogic(GSocket * socket)
{
	GSocketInternalServer * server=static_cast<GSocketInternalServer*>(socket);

	server->msg_callback_internal=boost::bind(&GServerManager::PreProcessInternalMessage,this,_1,_2,_3);
	server->msg_callback_target=boost::bind(&GServerManager::PreProcessTargetMessage,this,_1,_2,_3,_4);
	return true;
}
/***************************************************************************/
void GServerManager::UnregisterLogic(GSocket * socket,EUSocketMode mode,bool)
{
	GSocketInternalServer * server=static_cast<GSocketInternalServer*>(socket);

	server->msg_callback_internal=NULL;
	server->msg_callback_target=NULL;
}
/***************************************************************************/
void GServerManager::ConfigureClock()
{
	GServerBaseConsole::ConfigureClock();

	signal_second.connect(boost::bind(&GRaportInterface::UpdateTime,&raport_interface_local,boost::ref(clock)));
	signal_second.connect(boost::bind(&GRaportManager::UpdateTime,&raport_manager_local,boost::ref(clock)));
	signal_seconds_15.connect(boost::bind(&GServerManager::CheckForOtherInstances,this,false));
	signal_second.connect(boost::bind(&GServerBase::TestSocketsBinding, this));
}
/***************************************************************************/
void GServerManager::UpdateHeartBeat(bool zero_heartbeat)
{
	if (zero_heartbeat && last_heartbeat == 0)
	{
		return;
	}
	if (!zero_heartbeat && last_heartbeat != -1 && last_heartbeat + 20 < (INT64)CurrentSTime())
	{
		// Po dluzszym braku aktualizacji heartbeatu inny serwis mogl juz przejac nasza role. Przechodzimy w stan zgaszenia.
		zero_heartbeat = true;
	}
	last_heartbeat = !zero_heartbeat ? CurrentSTime() : 0;

	if (service_manager.services.size())
	{
		SSockService & service = service_manager.services[0];
		char command[512];
		sprintf(command, "REPLACE `rtm_service_heartbeat` SET `type` = '%s', `host` =  '%s', `port` = '%d', `game_instance` = '%d', "
							"`host_console` = '%s', `port_console` = '%d', `last_beat` = %s",
							GetServiceName(service.type_internal), 
							service.host_internal.c_str(), (INT32)service.port_internal, game_instance,
							service.host_console.c_str(), (INT32)service.port_console,
							!zero_heartbeat ? "UNIX_TIMESTAMP()" : "0");
		global_sqlmanager->Add(command, EDB_SERVERS);
	}
}
/***************************************************************************/
void GServerManager::CheckForOtherInstances(bool server_init)
{
	if (last_heartbeat != -1)
	{
		return;
	}
	if (service_manager.services.size() == 0)
	{
		return;
	}
	SSockService & service = service_manager.services[0];

	if (single_instance_service)
	{
		boost::recursive_mutex::scoped_lock lock(mtx_sql);
		char command[256];

		int pass;
		for (pass = 0; pass < 2; pass++)
		{
			bool is_other_instance_running = true;

			DWORD64 query_start_time = GetTime();
			sprintf(command, "SELECT `host`, `port` FROM `rtm_service_heartbeat` "
								"WHERE `type` = '%s' AND `game_instance` = '%d' AND `last_beat` > UNIX_TIMESTAMP() - 20%s",
								GetServiceName(service.type_internal), game_instance, (pass == 0) ? "" : " FOR UPDATE");
			if (MySQLQuery(&mysql, command))
			{
				is_other_instance_running = false;

				// Jesli wykonanie query zajelo dluzej niz 1 sekunde to uznajemy jego wyniki za niewiarygodne.
				// Nie wiemy bowiem wg jakiego UNIX_TIMESTAMP() sprawdzany byl warunek dot. `last_beat`.
				DWORD64 query_end_time = GetTime();
				if (query_end_time - query_start_time > GSECOND_1)
				{
					is_other_instance_running = true;
				}

				MYSQL_RES * result = mysql_store_result(&mysql);
				if (result)
				{
					MYSQL_ROW row = mysql_fetch_row(result);
					while (row && !is_other_instance_running)
					{
						string host = row[0];
						INT32 port = ATOI(row[1]);
						if (host != service.host_internal.c_str())
						{
							is_other_instance_running = true;
						}
						else
						{
							// Probujemy zbindowac sie na port starego serwisu, zeby sprawdzic czy nie jest juz martwy.
							if (port != service.port_internal)
							{
								int socket = make_netsocket(service.host_internal.c_str(), port);
								if (socket < 0)
								{
									is_other_instance_running = true;
								}
								else
								{
									closesocket(socket);
								}
							}
						}
						row = mysql_fetch_row(result);
					}
					mysql_free_result(result);
				}
			}

			if (is_other_instance_running)
			{
				if (pass == 1)
				{
					MySQLQuery(&mysql, "ROLLBACK");
				}
				return;
			}

			if (pass == 0)
			{
				if (!MySQLQuery(&mysql, "START TRANSACTION"))
				{
					return;
				}
			}
		}

		// Mozliwe, ze podczas inicjalizacji serwisu porty byly zajete i dlatego nie mamy socketow.
		// Probujemy wtedy dokonac ponownej inicjalizacji zamykajac serwis. Watchdog uruchomi jego nowa kopie.
		// Jesli za drugim razem sie to nie uda to juz trudno. Mozliwe, ze porty w ogole nie sa zdefiniowane.
		if (!server_init && (service.socket_internal <= 0 || service.socket_console <= 0))
		{
			MySQLQuery(&mysql, "ROLLBACK");
			last_heartbeat = 0;
			return;
		}

		// Probujemy ustawic serwis jako jedyna dzialajaca instancje.
		sprintf(command, "DELETE FROM `rtm_service_heartbeat` WHERE `type` = '%s' AND `game_instance` = '%d'",
							GetServiceName(service.type_internal), game_instance);
		if (!MySQLQuery(&mysql, command))
		{
			MySQLQuery(&mysql, "ROLLBACK");
			return;
		}

		sprintf(command, "REPLACE `rtm_service_heartbeat` SET `type` = '%s', `host` =  '%s', `port` = '%d', `game_instance` = '%d', "
							"`host_console` = '%s', `port_console` = '%d', `last_beat` = %s",
							GetServiceName(service.type_internal), 
							service.host_internal.c_str(), (INT32)service.port_internal, game_instance,
							service.host_console.c_str(), (INT32)service.port_console,
							"UNIX_TIMESTAMP()");
		if (!MySQLQuery(&mysql, command))
		{
			MySQLQuery(&mysql, "ROLLBACK");
			return;
		}

		if (!MySQLQuery(&mysql, "COMMIT"))
		{
			return;
		}

		last_heartbeat = CurrentSTime();
		WakeUpService();
	}
	else
	{
		// Usuwamy w miare mozliwosci z tabeli wpisy dot. poprzednich instancji serwisu z tego hosta, ktore juz sa martwe.
		// Dzieki temu rzadziej bedzie dochodzilo do sytuacji, ze konsola probuje podpiac sie do martwego serwisu.
		boost::recursive_mutex::scoped_lock lock(mtx_sql);
		char command[256];

		if (MySQLQuery(&mysql, "START TRANSACTION"))
		{
			sprintf(command, "SELECT `port`, `last_beat` FROM `rtm_service_heartbeat` "
								"WHERE `type` = '%s' AND `game_instance` = '%d' AND `host` = '%s' FOR UPDATE",
								GetServiceName(service.type_internal), game_instance, service.host_internal.c_str());
			if (MySQLQuery(&mysql, command))
			{
				MYSQL_RES * result = mysql_store_result(&mysql);
				if (result)
				{
					MYSQL_ROW row = mysql_fetch_row(result);
					while (row)
					{
						INT32 port = ATOI(row[0]);
						DWORD64 last_beat = ATOI64(row[1]);

						bool can_delete = (last_beat == 0);

						// Probujemy zbindowac sie na port starego serwisu, zeby sprawdzic czy nie jest juz martwy.
						if (!can_delete && port != service.port_internal)
						{
							int socket = make_netsocket(service.host_internal.c_str(), port);
							if (socket >= 0)
							{
								closesocket(socket);
								can_delete = true;
							}
						}

						if (can_delete)
						{
							sprintf(command, "DELETE FROM `rtm_service_heartbeat` "
												"WHERE `type` = '%s' AND `game_instance` = '%d' AND `host` = '%s' AND `port` = '%d'",
												GetServiceName(service.type_internal), game_instance, service.host_internal.c_str(), port);
							MySQLQuery(&mysql, command);
						}

						row = mysql_fetch_row(result);
					}
					mysql_free_result(result);
				}
			}

			MySQLQuery(&mysql, "COMMIT");
		}

		UpdateHeartBeat(false);
	}

	// Startujemy serwis.
	signal_seconds_15.connect(boost::bind(&GServerManager::UpdateHeartBeat, this, false));
}
/***************************************************************************/
bool GServerManager::PreProcessInternalMessage(GSocket * socket,GNetMsg & msg,int message)
{
	if (last_heartbeat <= 0)
	{
		return false;
	}
	return ProcessInternalMessage(socket,msg,message);
}
/***************************************************************************/
bool GServerManager::PreProcessTargetMessage(GSocket * socket,GNetMsg & msg,int message,GNetTarget & target)
{
	if (last_heartbeat <= 0)
	{
		return false;
	}
	return ProcessTargetMessage(socket,msg,message,target);
}
/***************************************************************************/

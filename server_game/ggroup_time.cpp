/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"
#include "headers_game.h"

extern GServerBase * global_server;

/***************************************************************************/
void GGroup::InitClock()
{
	server_game->signal_ms.connect(boost::bind(&GGroup::IntervalMSecond,this));
	server_game->signal_second.connect(boost::bind(&GGroup::IntervalSecond,this));
	server_game->signal_seconds_5.connect(boost::bind(&GGroup::IntervalSeconds_5,this));
	server_game->signal_minute.connect(boost::bind(&GGroup::IntervalMinute,this));
	server_game->signal_minutes_5.connect(boost::bind(&GGroup::IntervalMinutes_5,this));
	server_game->signal_minutes_15.connect(boost::bind(&GGroup::IntervalMinutes_15,this));

	global_server->signal_new_sockets_initialized.connect(boost::bind(&GGroup::UpdateGroupHeartBeat, this));
}
/***************************************************************************/
void GGroup::IntervalSecond()
{
	if (identity.id)
	{
		logic_raport_manager.UpdateTime(Server()->GetClock());

		if (status != EGS_Transferring && status != EGS_Stopped)
		{
			current_scripting_engine->WriteChangedGameStatesToDatabase();
			current_scripting_engine->EraseOldLocations();
			INT32 a, count = old_scripting_engines.size();
			for (a = 0; a < count; a++)
			{
				old_scripting_engines[a]->WriteChangedGameStatesToDatabase();
				old_scripting_engines[a]->EraseOldLocations();
			}
		}

		ClearExpiredSystemMessages();

		DWORD64 now = Server()->CurrentSTime();
		if (status != EGS_Initializing)
		{
			if (last_confirmed_heartbeat + GROUP_HEARTBEAT_INTERVAL_SECONDS + 15 < now &&
				status != EGS_Stopped)
			{
				// Za dlugo nie dostalismy potwierdzenia heartbeatu.
				// Uznajemy sie za martwa grupe i zaczynamy zrzucac lokacje.
				status = (!Server()->IsPreparingToClose()) ? EGS_Releasing_Urgent : EGS_Releasing_Urgent_Before_Shutdown;
				UpdateGroupHeartBeat();

				WaitForLocationsFlush();
				current_scripting_engine->DestroyLogic();
				current_scripting_engine->InitLogic();

				status = EGS_Stopped;
				GRAPORT(str(boost::format("[GROUP  %1%]  Status: EGS_Stopped  Reason: last_confirmed_heartbeat expired:  %2%  %3%") % identity.id % last_confirmed_heartbeat % now));
				UpdateGroupHeartBeat();
				ReleaseLocation("");
				DropAllPendingLogicMessages();
			}

			if (status == EGS_Stopped)
			{
				// Skoro nie mamy potwierdzenia heartbeatu to zapewne nie mozemy tez pobrac adresu nowej grupy,
				// do ktorej moze zostac przetransferowana lokacja. Rozlaczamy wiec graczy od razu.
				// Nie bawimy sie w zamykanie gently przez IGM_CONNECTION_CLOSE, bo gracze moga nam sie w ogole nie rozlaczyc,
				// przez co potem grupa sie nie zreanimuje.
				while (index_rid.begin() != index_rid.end())
				{
					GPlayerGame * p = static_cast<GPlayerGame *>(index_rid.begin()->second);
					Server()->SocketRemove(p->Socket(), USR_SERVER_CLOSE);
				}
			}
		}

		// Akcje zwiazane z zamykaniem grupy w sposob 'gently'.
		if (Server()->IsPreparingToClose())
		{
			if (status == EGS_Initializing || status == EGS_Ready)
			{
				status = EGS_Releasing_Gently;
				UpdateGroupHeartBeat();
			}

			if (status == EGS_Releasing_Gently)
			{
				// Zrzucamy powiazania z gamestatami, do ktorych nikt juz nie jest podlaczony.
				current_scripting_engine->EraseOldLocations(true);
				INT32 a, count = old_scripting_engines.size();
				for (a = 0; a < count; a++)
				{
					old_scripting_engines[a]->EraseOldLocations(true);
				}
			}

			// Grupa zostala powszechnie uznana za busy/stopped.
			// Rozlaczamy graczy i kierujemy ich do innej grupy.
			// Zadne operacje na lokacjach nie beda juz wykonywane.
			if (status == EGS_Releasing_Gently &&
				GSERVER->close_gently_timestamp != 0 &&
				GSERVER->close_gently_timestamp + GROUP_HEARTBEAT_INTERVAL_SECONDS + 5 < now)
			{
				status = EGS_Transferring;
				UpdateGroupHeartBeat();
			}

			if (status == EGS_Transferring)
			{
				current_scripting_engine->WriteChangedLocationsToDatabaseGroupShutdown();
				INT32 a, count = old_scripting_engines.size();
				for (a = 0; a < count; a++)
				{
					old_scripting_engines[a]->WriteChangedLocationsToDatabaseGroupShutdown();
				}
				ClearExpiredSystemMessages(true);

				TPlayerMap::iterator pos;
				for (pos = index_rid.begin(); pos != index_rid.end(); ++pos)
				{
					GPlayerGame * p = static_cast<GPlayerGame *>(pos->second);
					if (p->transferring_location_timestamp == 0 && !IsLocationAssigned(p->player_info.target_location_ID))
					{
						p->TransferLocation();
					}
					// Jesli nie udalo sie skierowac gracza do innej grupy przez 30 sekund to rozlaczamy go od razu.
					if (p->transferring_location_timestamp != 0 && p->transferring_location_timestamp + 30 < now)
					{
						if (p->Valid())
						{
							p->MsgExt(IGM_CONNECTION_CLOSE).WT("server terminated").A();
						}
					}
				}
				
				if (GetLoadedLocationsCount() == 0 && transferring_players_deadline == 0)
				{
					transferring_players_deadline = 30 + now;
				}

				if (transferring_players_deadline != 0 && (index_rid.size() == 0 || transferring_players_deadline < now))
				{
					status = EGS_Stopped;
					GRAPORT(str(boost::format("[GROUP  %1%] Status: EGS_Stopped  Reason: Releasing gently") % identity.id));

					UpdateGroupHeartBeat();
					ReleaseLocation("");
				}
			}

			if (status == EGS_Stopped && shutdown_state_timestamp == 0)
			{
				shutdown_state_timestamp = now;
			}
		}

		// Sprawdzamy czy nie mozemy zreanimowac grupy.
		if (status == EGS_Stopped && !Server()->IsPreparingToClose() && Server()->Continue())
		{
			if (last_confirmed_heartbeat + 15 >= now)
			{
				Reanimate();
			}
		}
	}
}
/***************************************************************************/
void GGroup::IntervalSeconds_5()
{
	if (identity.id)
	{
		DWORD64 now = Server()->CurrentSTime();
		if (last_confirmed_heartbeat + GROUP_HEARTBEAT_INTERVAL_SECONDS + 5 < now)
		{
			UpdateGroupHeartBeat();
		}

		// Probujemy otrzymac potwierdzenie zrzucenia wszystkich lokacji po starcie grupy - do skutku.
		if (status == EGS_Initializing)
		{
			ReleaseLocation("");

			// Zabezpieczamy sie przed sytuacja, ze polaczenie z serwisem nodebalancer jest nawiazane, ale serwis sie powiesil.
			// Probujemy wtedy nawiazac polaczenie z jakims innym.
			if (truerng.Rnd(15) == 0)
			{
				Service(ESTClientNodeBalancer)->ReInitConnection();
			}
		}
	}
}
/***************************************************************************/
void GGroup::IntervalMinute()
{
	UpdateGroupHeartBeat();
	UpdateLocationAddressCache();
}
/***************************************************************************/
void GGroup::IntervalMinutes_5()
{
}
/***************************************************************************/
void GGroup::IntervalMinutes_15()
{
}
/***************************************************************************/
void GGroup::IntervalMSecond()
{
	current_scripting_engine->HandleClockTick(Server()->GetClock().Get());
	INT32 a, count = old_scripting_engines.size();
	for (a = 0; a < count; a++)
	{
		old_scripting_engines[a]->HandleClockTick(Server()->GetClock().Get());
	}

	metrics.CheckRotate();

	ProcessLogicMessagesTransferQueue();
	ProcessIncomingLogicMessages();
	ProcessOutgoingLogicMessages();

	ProcessLocationDBTaskResults();
	ProcessHTTPRequestTaskResults();

	if (old_scripting_engines.size() > 0)
	{
		DWORD64 start_time = GetTime();
		// W jednym cyklu serwera wykonujemy do 11 operacji przepinania lokacji.
		// Ew. konczymy wczesniej jak zajmie to wiecej niz 0,5 sek.
		for (a = 0; a < 11; a++)
		{
			TransferLocationsToCurrentScriptingEngine();
			if (GetTime() - start_time > GSECOND_1 / 2)
			{
				break;
			}
		}
	}
}
/***************************************************************************/
void GGroup::UpdateGroupHeartBeat()
{
	if(identity.id)
	{
		GNetMsg & msg = MsgInt(ESTClientNodeBalancer, IGMI_GROUP_STATUS_UPDATE);
		msg.WBUI(Server()->CurrentSTime());
		msg.WUI(Server()->sock_service.addr_internal).WUS(Server()->sock_service.port_internal);
		msg.WI(identity.id).WI((INT32)type).WI((INT32)GetEffectiveStatus());
		msg.WI(GetLoadedLocationsCount());
		msg.WUI(Server()->sock_service.addr_console).WUS(Server()->sock_service.port_console);
		msg.WUI(Server()->sock_service.addr_game).WT(Server()->sock_service.ports_game_str);
		msg.A();
	}
}
/***************************************************************************/
bool GGroup::Reanimate()
{
	if (status != EGS_Stopped ||
		GetLoadedLocationsCount() != 0)
	{
		return false;
	}
	if (!CanCloseGently())
	{
		return false;
	}

	last_confirmed_heartbeat = 0;

	status = EGS_Initializing;
	UpdateGroupHeartBeat();

	return true;
}
/***************************************************************************/

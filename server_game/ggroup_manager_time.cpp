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
//konfiguracja sygnalow do obslugi czasu
void GGroupManager::ConfigureClock()
{
	GServerBase::ConfigureClock();
	signal_second.connect(boost::bind(&GGroupManager::IntervalSecond,this));
	signal_seconds_15.connect(boost::bind(&GGroupManager::ProcessTimeOut,this));
	signal_minutes_5.connect(boost::bind(&GGroupManager::RaportCurrentStatus,this));
}
/***************************************************************************/
void GGroupManager::IntervalSecond()
{
	if (Continue() && flags[EFPrepareToClose])
	{
		// Sprawdzamy czy grupa zrzucila wszystkie lokacje i jest gotowa do zamkniecia.
		// Jesli tak, to ustawiamy flage EFClose, dajac znac glownemu watkowi, ze ta grupa jest gotowa do zamkniecia.
		if (group.CanCloseGently())
		{
			Close();
			Destroy();
		}
	}
}
/***************************************************************************/
//to jest wolane co 15 sek, jest wpiete w sygnaly
void GGroupManager::ProcessTimeOut()
{
	if (socket_internal.size())
	{
		DWORD64 now = GSERVER->GetClock().Get();

		INT32 a, count = socket_internal.size();
		for (a = count - 1; a >= 0; a--)
		{
			GTicketInternalClient * & socket = socket_internal[a];
			if (socket->GetClientServiceType() == ESTClientBase &&
				socket->last_connection_activity + GMINUTES_15 <= now &&
				socket->GetOutQueueSize() == 0)
			{
				SocketDestroy(socket, USR_DEAD_TIME);
				socket_internal.erase(socket_internal.begin() + a);
			}
		}
	}

	if (socket_game.size())
	{
		//sprawdza timeouty dla socketow, i usuwamy nie zestawione w max. czasie polaczenia
		for_each(socket_game.begin(), socket_game.end(),boost::bind(&GGroupManager::TestTimeOut,this,_1));
		socket_game.resize(0);
	}
}
/***************************************************************************/
//wolane z ProcessTimeOut, sprawdza czy polaczenie nie wisi martwe dluzej niz TIME_MAX_PING_IN, 
//jesli tak to trzeba go zerwac odrejestrowuje element, 
bool GGroupManager::TestTimeOut(GSocket * socket)
{
	if(socket->Flags()[ESockTimeOut] || socket->IsDeadTime(clock))
	{
		SocketRemove(socket,USR_TIMEOUT_PING);
		return true;
	}

	if (socket->GetServiceType() == ESTClientBaseGame)
	{
		//ma rozpiac gracza po czasie okreslonym w socket_inactivity_timeout (sek) jesli nie ma pinga
		if (clock.Get() > (socket->GetTimeLastAction() + socket->socket_inactivity_timeout))
		{
			SocketRemove(socket,USR_TIMEOUT_PING);
			return true;
		}
	}

	return false;
}
/***************************************************************************/

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
inline void Coerce(GPlayerGame * player, GSocketAccess * sa)
{
	player->player_info=sa->player_info;
}
/***************************************************************************/
bool GGroupManager::RegisterLogic(GSocket * socket,GSocket * socket_old)
{
	SRAP(INFO_LOGIC_REGISTER);
	int room_id=0;
	GSocketAccess * sa=static_cast<GSocketAccess*>(socket_old);
	GPlayerGame * logic=new GPlayerGame(&group,socket,sa->player_info.RID);
	if(logic==NULL)
	{
		socket->MsgExt(IGM_MESSAGE_DIALOG).WI(DLG_ERROR_END).WT("internal error").WI(0).A();
		SRAP(ERROR_MEMORY_ALLOCATION);
		RAPORT("%s %s %d",GSTR(ERROR_MEMORY_ALLOCATION),__FILE__,__LINE__);
		return false;
	}
	Coerce(logic,sa);

	// Skoro gracz zostal wpuszczony do grupy, tzn. ze dostalismy potwierdzenie od 'nodebalancer', ze lokacja
	// jest do niej przypisana.
	group.AssignLocation(logic->player_info.target_location_ID);

	logic->player_info.global_group_id = GLOBAL_GROUP_ID(sock_service.addr_internal, sock_service.port_internal, group.GetID());

	socket->LinkLogic(logic);
	logic->ServicesRegister();

	//usuwa inne instancje zerowe
	EULogicMode mode = ULR_PLAYER_CONNECT;
	GPlayerGame * second_instance=group.PlayerDetectOtherInstances(logic);	
	if(second_instance) 
	{
		UnregisterLogicServer(second_instance,ULR_ROOM_SECOND_INSTANCE);
		mode=ULR_ROOM_SECOND_INSTANCE;
	}

	// Wysylamy IGM_REGISTRATION_END przed notyfikacja logiki o dodaniu gracza,
	// bo moze to spowodowac wyslanie do gracza juz komunikatow logiki gry.
	SRAP(INFO_LOGIC_ADD);
	socket->MsgExt(IGM_REGISTRATION_END).A();

	group.metrics.CommitSampleValue("IGM_CONNECT_to_IGM_REGISTRATION_END_time", GSERVER->GetClock().Get() - sa->timestamp_igm_connect, "ms");
	logic->timestamp_igm_connect = sa->timestamp_igm_connect;
	
	group.PlayerGameAdd(logic);						//dodaje element to tabeli w grupie

	return true;
}
/***************************************************************************/
//ta funkcja jest wolana gdy serwer jest kasowany na skutek dzialan od dolu, czyli np. zamkniecia
//socket'a, 
void GGroupManager::UnregisterLogic(GSocket * socket,EUSocketMode mode,bool)
{
	SRAP(INFO_LOGIC_UNREGISTER);
	//tu juz nie wolno pisac do socket'a bo beda bledy! socket jest zamkniety
	GPlayerGame* logic=static_cast<GPlayerGame*>(socket->Logic());
	if(logic==NULL) return;
	{
		group.PlayerGameDelete(logic,ULR_PLAYER_DISCONNECT);
		logic->ServicesUnregister();
		socket->UnlinkLogic();
		Delete(logic);
		SRAP(INFO_LOGIC_DELETE);
	}
}
/***************************************************************************/
//ta funkcja wolana jest z gory - jako reakcja wynikajaca z dzialan logiki
void GGroupManager::UnregisterLogicServer(GPlayerGame * logic,EULogicMode mode)
{
	SRAP(INFO_LOGIC_UNREGISTER);
	
	group.PlayerGameDelete(logic,mode);
	logic->ServicesUnregister();
	if(logic->Valid()) 
	{
		logic->Socket()->SetDeadTime(GetClock());
		logic->Socket()->UnlinkLogic();
	}
	Delete(logic);
	SRAP(INFO_LOGIC_DELETE);
}
/***************************************************************************/

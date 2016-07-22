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
GPlayer::GPlayer(GGroup* group_p, GSocket * socket_p,int rid_p):GLogic(),raport_interface(group_p->Server()->raport_interface)
{
	group=group_p;socket=socket_p;identity.rid=rid_p;
	close_comment_id=close_raport_id=0;
};
/***************************************************************************/
//podpinajacy sie player rejestrowany jest w serwisach
void GPlayerGame::ServicesRegister()
{
	GTicketPlayerPtr ticket_access(new GTicketPlayer());
	ticket_access->data = player_info;
	TicketInt(ESTClientAccess, IGMITIC_REGISTER, ticket_access);

	GTicketPlayerPtr ticket_globalinfo(new GTicketPlayer());
	ticket_globalinfo->data = player_info;
	TicketInt(ESTClientGlobalInfo, IGMITIC_REGISTER, ticket_globalinfo);
}
/***************************************************************************/
//odpinajacy sie jest usuwany z tych serwisow
void GPlayerGame::ServicesUnregister()
{
	GTicketPlayerPtr ticket_access(new GTicketPlayer());
	ticket_access->data = player_info;
	TicketInt(ESTClientAccess, IGMITIC_UNREGISTER, ticket_access);

	GTicketPlayerPtr ticket_globalinfo(new GTicketPlayer());
	ticket_globalinfo->data = player_info;
	TicketInt(ESTClientGlobalInfo, IGMITIC_UNREGISTER, ticket_globalinfo);
}
/***************************************************************************/

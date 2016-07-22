/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"
#include "headers_game.h"
extern GServerBase * global_server;

#include "include_ssl.h"
/***************************************************************************/
void GSocketGame::Parse(GRaportInterface & raport_interface)
{
	INT32 pre_message=0;
	while(msg.Parse(pre_message))
	{
		bool stop=false;
		GLogic* logic=Logic();
		if(logic)
		{
			GPlayerGame* player=static_cast<GPlayerGame*>(logic);
			if(player->flags[EFPrepareToClose]) stop=true;
		}
		if(stop) break;


		INT32 message=0;
		msg.RI(message);
		if(!msg.GetErrorCode())
		{
			GLogic* logic=Logic();
			if(logic)
			{
				GPlayerGame* player=static_cast<GPlayerGame*>(logic);
				player->ProcessMessage(msg,message);
			}
		}
	}
	if(memory_in.Used()==0) DeallocateIn();
	TestWrite();

	if(AnalizeMsgError(raport_interface))
	{
		msg.SetCryptOut(ENetCryptNone);
		flags.set(ESockError);
	}
}


/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/


#include "headers_all.h"
#include "headers_game.h"
#include "zlib.h"

extern GServerBase * global_server;
/***************************************************************************/
bool GPlayer::ProcessMessage(GNetMsg &,int)
{
	return false;
}
/***************************************************************************/
bool GPlayer::ProcessInternalMessage(GNetMsg &,int)
{
	return false;
}
/***************************************************************************/
bool GPlayer::ProcessTargetMessage(GNetMsg &, int)
{
	return false;
}
/***************************************************************************/
GNetMsg & GPlayer::MsgInt(EServiceTypes type,int message)
{
	Service(type)->AddWrite();
	if(message>IGMIT_FIRST && message<IGMIT_LAST)
	{
		GNetTarget		target;
		target.Init(Group()->GetID(),GetRID(),GetID());
		Service(type)->Msg().WI(message).W(target);
	}
	else
	{
		Service(type)->Msg().WI(message);
	}
	return Service(type)->Msg();
};
/***************************************************************************/
GNetMsg & GPlayer::MsgExt(INT32 message)
{
	socket->AddWrite();
	socket->Msg().WI(message);
	return socket->Msg();
};
/***************************************************************************/
GNetMsg & GPlayer::MsgExt()
{
	socket->AddWrite();
	return socket->Msg();
};
/***************************************************************************/
GNetMsg & GPlayer::MsgExtCompressed(GNetMsg & msg)
{
	DWORD32 uncompressed_size = msg.LastChunk().size;
	if(uncompressed_size<MSG_COMPRESS_TRESHOLD)
	{
		MsgExt() += msg;
		return socket->Msg();
	}
	unsigned long compressed_size = uncompressed_size * 2 + 100;

	GMemory embedded_msg_raw;
	embedded_msg_raw.Init(&Server()->MemoryManager());
	embedded_msg_raw.ReallocateToFit(compressed_size);

	int error = compress((BYTE*)embedded_msg_raw.Start(), &compressed_size, (BYTE*)msg.LastChunk().ptr, uncompressed_size);
	if (error || compressed_size >= (uncompressed_size * 8) / 10)
	{
		MsgExt() += msg;
	}
	else
	{
		MsgExt(IGM_COMPRESSED_MESSAGE).WUI(uncompressed_size).WUI(compressed_size).W(NPair((BYTE*)embedded_msg_raw.Start(),(DWORD32)compressed_size)).A();
	}
	embedded_msg_raw.Deallocate();

	return socket->Msg();
}
/***************************************************************************/
void GPlayer::TicketInt(EServiceTypes type,int message, GTicketPtr ticket)
{
	ticket->message=message;
	ticket->target.Init(Group()->GetID(),GetRID(),GetID());
	Service(type)->Ticket(ticket);
};
/***************************************************************************/
GTicketInternalClient *	GPlayer::Service(EServiceTypes  type)
{
	return group->Service(type);
};
/***************************************************************************/
GGroupManager * GPlayer::Server()
{
	return group->Server();
};

/***************************************************************************/
GGroup * GPlayer::Group()
{
	return group;
};
/***************************************************************************/
void GPlayer::Zero()
{
	socket=NULL;
	group=NULL;
}
/***************************************************************************/







/***************************************************************************/
bool GPlayerAccess::ProcessInternalMessage(GNetMsg & ,int )
{
	return false;
}
/***************************************************************************/









/***************************************************************************/
GNetMsg & GPlayerGame::MsgInt(EServiceTypes type,INT32 message)
{
	Service(type)->AddWrite();
	if(message>IGMIT_FIRST && message<IGMIT_LAST)
	{
		GNetTarget		target;
		target.Init(Group()->GetID(),GetRID(),GetID());
		Service(type)->Msg().WI(message).W(target);
	}
	else
	{
		Service(type)->Msg().WI(message);
	}
	return Service(type)->Msg();
};
/***************************************************************************/
void GPlayerGame::TicketInt(EServiceTypes type,int message, GTicketPtr ticket)
{
	ticket->message=message;
	ticket->target.Init(Group()->GetID(),GetRID(),GetID());
	Service(type)->Ticket(ticket);
};
/***************************************************************************/
void GPlayerGame::Zero()
{
	GPlayer::Zero();
	flags.reset();
	registered_buddies.clear();
}
/***************************************************************************/
void GPlayerGame::TransferLocation()
{
	if (transferring_location_timestamp)
	{
		return;
	}
	transferring_location_timestamp = Server()->CurrentSTime();

	GTicketLocationAddressGetPtr ticket_location_address_get(new GTicketLocationAddressGet());
	ticket_location_address_get->location_ID = player_info.target_location_ID;
	ticket_location_address_get->connection_type = ESTClientBaseGame;
	TicketInt(ESTClientNodeBalancer, IGMITIC_LOCATION_ADDRESS_GET, ticket_location_address_get);
}
/***************************************************************************/

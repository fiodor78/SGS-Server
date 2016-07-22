/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"
#include "gmsg_internal.h"

#pragma warning (disable: 4996)

/***************************************************************************/
GSocket::GSocket()
{
	Reset();
};
/***************************************************************************/
GSocket::~GSocket()
{
	Destroy();
};
/***************************************************************************/
void GSocket::Init(GRaportInterface & raport_interface,SOCKET socket_p,EServiceTypes service_type_p,GMemoryManager * memory_mgr_p,GPollManager * poll_mgr_p)
{
	Reset();
	data.socket=socket_p;
	data.service_type=service_type_p;
	time.Zero();
	memory_manager=memory_mgr_p;
	poll_manager=poll_mgr_p;
	memory_in.Init(memory_manager);
	memory_out.Init(memory_manager);
	msg.Init(GetCmdType(),&memory_in,&memory_out);

	Duplicate(raport_interface);
	Format(global_serverconfig->net.socket_size);
}
/***************************************************************************/
void GSocket::Init(GRaportInterface & raport_interface,GSocket * socket_p,GMemoryManager * memory_mgr_p,GPollManager * poll_mgr_p)
{
	Reset();
	data=socket_p->data;
	time=socket_p->time;
	memory_manager=memory_mgr_p;
	poll_manager=poll_mgr_p;
	memory_in.Init(memory_manager);
	memory_out.Init(memory_manager);
	msg.Init(GetCmdType(),&memory_in,&memory_out);

	Format(global_serverconfig->net.socket_size);
	SetCryptMode(data.crypt_mode);
}
/***************************************************************************/
void GSocket::Destroy()
{
	Close();
}
/***************************************************************************/
void GSocket::Reset()
{
	socket_inactivity_timeout = TIME_MAX_SOCKET_INACTIVITY;
	flags.reset();
	poll_manager=NULL;
	memory_manager=NULL;
	data.Zero();
	logic=NULL;
	spool_buffer_out_size = 0;
}
/***************************************************************************/
bool GSocket::Duplicate(GRaportInterface & raport_interface)
{
	//sprawdzamy czy nalezy dupliowac socket'u
	if(!global_serverconfig->net.dup) return false;

	bool socket_duplicate=false;
	SOCKET new_fd=stack_mgr.Pop();
	if(new_fd!=SOCKET_NONE)
	{
		//duplikumey
		int ret=dup2((int)data.socket,(int)new_fd);
		if (ret!=-1)
		{
			//jesli sie udalo to oryginal mozna zamknac
			closesocket(data.socket);
			flags.set(ESockDuplicate);
			data.socket = ret;
			socket_duplicate=true;
		}
		else
		{
			//jak sie nie udalo to oddajemy na stos nasze id socketu
			SRAP(ERROR_SOCKET_DUPLICATION);
			RAPORT("%s %s %d",GSTR(ERROR_CONSOLE_DUP),__FILE__,__LINE__);
			stack_mgr.Push(new_fd);
		}
	}
	else
	{
		SRAP(WARNING_SOCKET_STACK_EMPTY);
		RAPORT(GSTR(WARNING_SOCKET_STACK_EMPTY));
	}
	return socket_duplicate;
}
/***************************************************************************/
void GSocket::Unduplicate()
{
	if(global_serverconfig->net.dup)
	if(flags[ESockDuplicate]) stack_mgr.Push(data.socket);
	flags.reset(ESockDuplicate);
}
/***************************************************************************/
bool GSocket::RegisterPoll()
{
	if(poll_manager==NULL) return false;
	bool ret=poll_manager->AddPoll(this);
	if(ret) flags.set(ESockPoll);
	return ret;
}
/***************************************************************************/
bool GSocket::UnregisterPoll()
{
	if(poll_manager==NULL) return false;
	if(flags[ESockPoll]) poll_manager->RemovePoll(this);
	flags.reset(ESockPoll);
	return true;
}
/***************************************************************************/
void GSocket::Close()
{
	if(data.socket==0) return;
	//ustawiamy nolinger aby faktycznie pozbyc sie wszystkiego z bufora
#ifndef LINUX
	setsockopt(data.socket,SOL_SOCKET,SO_DONTLINGER ,0,0);
#else
	linger l;
	memset(&l,0,sizeof(l));
	setsockopt(data.socket,SOL_SOCKET,SO_LINGER ,&l,sizeof(linger));
#endif
	UnregisterPoll();
	closesocket(data.socket);
	Unduplicate();
	memory_in.Deallocate();
	memory_out.Deallocate();

	Reset();
}
/***************************************************************************/
void GSocket::Move()
{
	spool_buffer_out_size = min((int)sizeof(spool_buffer_out), (int)memory_out.Used());
	memcpy(spool_buffer_out, memory_out.Start(), spool_buffer_out_size);

	UnregisterPoll();
	flags.set(ESockMove);
	memory_in.Deallocate();
	memory_out.Deallocate();
}
/***************************************************************************/
void GSocket::Format(int size_p)
{
	bool buf=false;
	setsockopt(data.socket,IPPROTO_TCP,SO_KEEPALIVE,(const char *)&buf,sizeof(buf));
	int s=4;
	int size=size_p;
	setsockopt(data.socket,SOL_SOCKET,SO_RCVBUF,(char *)&size,s);
	setsockopt(data.socket,SOL_SOCKET,SO_SNDBUF,(char *)&size,s);
	unsigned long l=1;
	ioctl(data.socket,FIONBIO,&l);
	bool n=false;
	s=4;
	setsockopt(data.socket,IPPROTO_TCP,TCP_NODELAY ,(char *)&n,s);
	
	socket_inactivity_timeout = TIME_MAX_PING_IN;
}
/***************************************************************************/
bool GSocket::Read()
{
	if(!memory_in.GetAllocated()) 
		FirstAllocateIn();

	int size=recv(data.socket,memory_in.End(),memory_in.Free(),0);
	SVAL(INFO_NET_IN,size);
	if (size==SOCKET_ERROR || size==0)
	{
		flags.set(ESockExit);
		return false;
	}
	else
	{
		memory_in.IncreaseUsed(size);
		return true;
	}
}
/***************************************************************************/
void GSocket::Write()
{
	int size=send(data.socket,memory_out.Start(),memory_out.Used(),0);
	if (size==SOCKET_ERROR)
	{
#ifdef LINUX
		if(errno!=EAGAIN && errno!=EWOULDBLOCK) 
#else
		if(GET_SOCKET_ERROR()!=EWOULDBLOCK) 
#endif
			flags.set(ESockExit);
	}
	else
	{
		SVAL(INFO_NET_OUT,size);
		if(memory_out.Move(size))
		{
			DeallocateOut();
		}
	}
	if(memory_out.Used()) 
	{
		flags.set(ESockControlWrite);
		poll_manager->TestWritePoll(this,true);
	}
	else
	{
		if(flags[ESockControlWrite])
		{
			poll_manager->TestWritePoll(this,false);
			flags.reset(ESockControlWrite);
		}
	}
}
/***************************************************************************/
void GSocket::TestWrite()
{
	if(memory_out.Used()) Write();
}
/***************************************************************************/
bool GSocket::AnalizeMsgError(GRaportInterface & raport_interface)
{
	if(msg.IsError())
	{
		if(msg.GetErrorCode())
		{
			switch(msg.GetErrorCode())
			{
			case ENetErrorNotAllocated:	RAPORT("NETERROR ENetErrorNotAllocated");break;
			case ENetErrorReallocFailed: RAPORT("NETERROR ENetErrorReallocFailed");break;
			case ENetErrorChunkNotInitialized: RAPORT("NETERROR ENetErrorChunkNotInitialized");break;
			case ENetErrorOutOfData:
				RAPORT("NETERROR ENetErrorOutOfData");
				break;
			case ENetErrorOutOfLimit:RAPORT("NETERROR ENetErrorOutOfLimit");break;
			case ENetErrorDataError:RAPORT("NETERROR ENetErrorDataError");break;
			case ENetErrorIncorrectFormat:RAPORT("NETERROR ENetErrorIncorrectFormat");RAPORT(msg.format_error);break;
			}
			SRAP(ERROR_DATA_MSG_DATA);
			RAPORT(GSTR(ERROR_DATA_MSG_DATA));
			RAPORT("%s error: %d, line: %d, last_in: %d, current_in: %d, last_out: %d",GSTR(ERROR_SERVER_REINIT),msg.GetErrorCode(),msg.GetErrorLine(),msg.GetErrorIn(),msg.GetErrorCurrentIn(),msg.GetErrorOut());
			return true;
		}
	}
	return false;
}
/***************************************************************************/
void GSocket::LinkLogic(GLogic * logic_p)
{
	logic=logic_p;
	logic->socket=this;
}
/***************************************************************************/
void GSocket::UnlinkLogic()
{
	if(logic)
	{
		logic->socket=NULL;
	}
	logic=NULL;
}
/***************************************************************************/
GLogic * GSocket::Logic()
{
	return logic;
}
/***************************************************************************/
void GLogic::LinkSocket(GSocket* socket_p)
{
	socket=socket_p;
	socket->logic=this;
}
/***************************************************************************/
void GLogic::UnlinkSocket()
{
	if(socket)
	{
		socket->logic=NULL;
	}
	socket=NULL;
}
/***************************************************************************/
GSocket * GLogic::Socket()
{
	return socket;
}
/***************************************************************************/
void GSocket::AddWrite()
{
	if(!poll_manager) return;
	return poll_manager->AddForceWritePoll(this);
}
/***************************************************************************/
void GSocket::SetCryptMode(ENetCryptType mode)
{
	data.crypt_mode=mode;
	switch(data.crypt_mode)
	{
		case ENetCryptXOR:msg.SetKey(&data.keys.XOR_key);break;
		case ENetCryptAES:msg.SetKey(&data.keys.AES_key);break;
		case ENetCryptDES:msg.SetKey(&data.keys.DES_key);break;
		case ENetCryptDES3:msg.SetKey(&data.keys.DES_key);break;
	}
	msg.SetCryptIn(data.crypt_mode);
	msg.SetCryptOut(data.crypt_mode);
	
}
/***************************************************************************/

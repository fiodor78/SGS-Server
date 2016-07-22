/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"
#include "gmsg_internal.h"
#include "gtruerng.h"

#include "mysql.h"
extern MYSQL			mysql;

struct SConnectionDesc 
{
	int id;
	char * desc;
};

SConnectionDesc connection_desc[]={
#define GCONNECTION(id,desc)		{id,#id},
#include "tbl/gconnection.tbl"
#undef GCONNECTION
};

/***************************************************************************/
GSocketInternalClient::GSocketInternalClient(GRaportInterface & raport_interface_):raport_interface(raport_interface_)
{
	Reset();
	addr_is_set_manually = false;
	addr.sin_family=AF_INET;
	addr.sin_addr.s_addr=0;
	addr.sin_port=0;
	global_signals->reinit_connections.connect(boost::bind(&GSocketInternalClient::Write,this));
	msg_callback_target=NULL;
	msg_callback_internal=NULL;
	msg_callback_connect=NULL;
	client_service_group=0;
	client_service_type=ESTInternalClient;
	connected=boost::posix_time::second_clock::local_time();
	disconnected=boost::posix_time::second_clock::local_time();
};
/***************************************************************************/
GSocketInternalClient::~GSocketInternalClient()
{
	Destroy();
};
/***************************************************************************/
void GSocketInternalClient::Init(EServiceTypes type_p,int group_p,GMemoryManager * memory_mgr_p,GPollManager * poll_mgr_p)
{
	Reset();

	memory_manager=memory_mgr_p;
	poll_manager=poll_mgr_p;
	client_service_type=type_p;
	client_service_group=group_p;

	data.service_type=ESTInternalClient;
	memory_in.Init(memory_manager);
	memory_out.Init(memory_manager);
	msg.Init(GetCmdType(),&memory_in,&memory_out);
	msg.SetTestIn();
	msg.SetTestOut();
	connection_mode=EConnectionInit;
	connection_try=0;
	connection_error=0;

	ping.in.SetInterval(TIME_MAX_PING_IN);
	ping.out.SetInterval(TIME_MAX_PING_OUT);
	ping.connection.SetInterval(3000);
	ping.connection_next.SetInterval(15000);
	ping.connection_error.SetInterval(1000);
	Communicate();
}
/***************************************************************************/
void GSocketInternalClient::Close()
{
	if (IsConnection())
	{

		GTimeInterval t;
		msg.WI(IGMI_DISCONNECT).A();
		t.SetInterval(100);
		while(memory_out.Used() && !t(clock))
		{
			clock.Probe();
			WriteInternal();
			Sleep(1);
		}
	}
	GSocket::Close();
}
/***************************************************************************/
void GSocketInternalClient::GetHostPort()
{
	boost::recursive_mutex::scoped_lock lock(mtx_sql);

	char command[256];
	sprintf(command, "SELECT `host`, `port` FROM `rtm_service_heartbeat` WHERE `type` = '%s' AND `game_instance` = '%d' AND `last_beat` > UNIX_TIMESTAMP() - 90",
						GetServiceName(client_service_type), game_instance);
	if (MySQLQuery(&mysql, command))
	{
		MYSQL_RES * result = mysql_store_result(&mysql);
		if (result)
		{
			MYSQL_ROW row = NULL;

			INT32 count = (INT32)mysql_num_rows(result);
			if (count > 0)
			{
				INT32 index = (count == 1) ? 0 : truerng.Rnd(count);
				while (index-- >= 0)
				{
					row = mysql_fetch_row(result);
				}
			}
			if (row)
			{
				addr.sin_addr.s_addr = GetAddrFromName(row[0]);
				addr.sin_port = htons((USHORT16)ATOI(row[1]));
				RAPORT("INTERNAL (%d:%s:%s) trying to connect",client_service_group,GetServiceName(client_service_type),AddrToString(addr)());
			}
			else
			{
				RAPORT("INTERNAL  '%s' unknown service %s",GetServiceName(client_service_type),GetServiceName(client_service_type));
			}
			mysql_free_result(result);
		}
		else
		{
			RAPORT("INTERNAL '%s' unknown service %s",GetServiceName(client_service_type),GetServiceName(client_service_type));
		}
	}
}
/***************************************************************************/
void GSocketInternalClient::SetHostPort(DWORD32 host, USHORT16 port)
{
	addr_is_set_manually = true;
	addr.sin_addr.s_addr = host;
	addr.sin_port = htons(port);
}
/***************************************************************************/
void GSocketInternalClient::InitConnection()
{
	ping.in.Init();
	ping.out.Init();
	ping.connection.Init();
	ping.connection_next.Init();
	ping.connection_error.Init();

	CloseConnection();

	memory_in.Allocate(K32);
	memory_out.Allocate(K32);
	msg.Init(ENetCmdInternal,&memory_in,&memory_out);
	msg.SetChunkLimit(K1024);

	data.socket=socket(AF_INET, SOCK_STREAM,0);

	unsigned long l=1;
	ioctl(data.socket,FIONBIO,&l);

	struct	sockaddr_in	server_address;
	if (!addr_is_set_manually || addr.sin_addr.s_addr == 0)
	{
		GetHostPort();
	}
	server_address=addr;
	memset(&server_address.sin_zero,0,8);

	int ret=connect(data.socket,(struct sockaddr *)&server_address,sizeof(server_address));
	if(ret==SOCKET_ERROR)
	{
		ret=GET_SOCKET_ERROR();
#ifndef LINUX
		if (ret==EWOULDBLOCK || ret==0)
#else
		if (ret==EWOULDBLOCK || ret==EINPROGRESS || ret==0)
#endif
		{
		}
		else
		{
			closesocket(data.socket);
			EServiceTypes save_service_type = data.service_type;
			data.Zero();
			data.service_type = save_service_type;
			connection_mode=EConnectionInit;
			return;
		}
	}
	Duplicate(raport_interface);
	Format(K16);
	RegisterPoll();

	RAPORT("INTERNAL (%d:%s:%s) connecting",client_service_group,GetServiceName(client_service_type),AddrToString(addr)());

	flags.set(ESockControlWrite);
	poll_manager->TestWritePoll(this,true);
}
/***************************************************************************/
void GSocketInternalClient::CloseConnection()
{
	if(data.socket)
	{
		RAPORT("INTERNAL (%d:%s:%s) closing",client_service_group,GetServiceName(client_service_type),AddrToString(addr)());
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
		data.socket=0;
	}
	memory_in.Deallocate();
	memory_out.Deallocate();
}
/***************************************************************************/
bool GSocketInternalClient::SleepConnection()
{
	if (!IsConnection()) 
	{
		connection_try++;
		if(connection_try>=3) 
		{
			connection_try=0;
			CloseConnection();
			connection_mode=EConnectionNextConnection;
			RAPORT("INTERNAL (%d:%s:%s) sleeping",client_service_group,GetServiceName(client_service_type),AddrToString(addr)());
			return true;
		}
	}
	return false;
}
/***************************************************************************/
void GSocketInternalClient::TestConnection()
{
	if (IsConnection()) return;
	if (IsConnection(EConnectionNextConnection)) connection_mode=EConnectionInit;
	GSocketInternalClient::Communicate();
}
/***************************************************************************/
void GSocketInternalClient::Communicate(bool force)
{
	if(force) 
		if(connection_mode==EConnectionNextConnection) 
		{
			InitConnection();connection_mode=EConnectionTest;
		};

	EConnectionMode	m=EConnectionNone;
	clock.Probe();
	int counter=0;
	do{
		counter++;
		m=connection_mode;
		CommunicateStep();
	}while(m!=connection_mode && counter>=10);
	if(counter==10)
	{
		SRAP(ERROR_PROCESS_LOOP);
		RAPORT(GSTR(ERROR_PROCESS_LOOP));
	}
}
/***************************************************************************/
void GSocketInternalClient::CommunicateStep()
{
	switch(connection_mode)
	{
	case EConnectionInit:
		{
			if(connection_error>=5)
			{
				if(!ping.connection_error.Test(clock)) return;
			}
			InitConnection();
			connection_mode=EConnectionTest;
		}
		break;
	case EConnectionTest:
	case EConnectionEstabilishing:
		{
			if (ping.connection.Test(clock))
			{ 
				if(SleepConnection()) return;
				connection_mode=EConnectionConnectionTimeOut;
			}
		}
		break;
	case EConnectionNextConnection:
		{
			if (ping.connection_next.Test(clock))
			{ 
				connection_mode=EConnectionInit;
				connection_try=0;
			}
		}
		break;
	case EConnectionEstabilished:
		{
			connection_try=0;
			TestPing();
			if(memory_out.Used()==0 && memory_out.Free()!=K32)
			{
				memory_out.Deallocate();
				memory_out.Allocate(K32);
			}
			if(memory_in.Used()==0 && memory_in.Free()!=K32)
			{
				memory_in.Deallocate();
				memory_in.Allocate(K32);
			}
		}
		break;
	case EConnectionConnectionTimeOut:
	case EConnectionRefused:
	case EConnectionShutdown:
	case EConnectionBadVersion:
	case EConnectionClosed:
	case EConnectionDisconnection:
	case EConnectionPingTimeOut:
	case EConnectionBroken:
	case EConnectionNoConnection:
	case EConnectionError: 
	case EConnectionMsgOverFlow:
	case EConnectionTransmitionError:
	case EConnectionReConnect:
		{
			connection_error++;
			CloseConnection();
			SRAP(ERROR_INTERNAL_CONNECTION_BROKEN);
			RAPORT(GSTR(ERROR_INTERNAL_CONNECTION_BROKEN));
			RAPORT("INTERNAL (%d:%s:%s) error %s",client_service_group,GetServiceName(client_service_type),AddrToString(addr)(),connection_desc[connection_mode].desc);
			connection_mode=EConnectionInit;
		}
		break;
	case EConnectionForceReInit:
		{
			RAPORT("INTERNAL (%d:%s:%s) %s",client_service_group,GetServiceName(client_service_type),AddrToString(addr)(),connection_desc[connection_mode].desc);
			if (!addr_is_set_manually)
			{
				addr.sin_addr.s_addr = 0;
				addr.sin_port = 0;
			}
			connection_mode=EConnectionInit;
		}
		break;
	}


	if(IsConnection()) 
	if(AnalizeMsgError(raport_interface))
	{
		connection_mode=EConnectionError;
	}
}
/***************************************************************************/
bool GSocketInternalClient::Read()
{
	ping.in.Update(clock);
	if (IsConnection())
	{
		if(ReadInternal()) return true;
	}
	else
	{
		if(IsConnection(EConnectionEstabilishing))
		{
			SRAP(INFO_INTERNAL_CONNECTION_ESTABILISHED);
			RAPORT(GSTR(INFO_INTERNAL_CONNECTION_ESTABILISHED));
			RAPORT("INTERNAL (%d:%s:%s) estabilished",client_service_group,GetServiceName(client_service_type),AddrToString(addr)());
			connection_mode=EConnectionEstabilished;
			connected=boost::posix_time::second_clock::local_time();
		}
		if(ReadInternal()) 
		{
			return true;
		}
	}
	Communicate();
	return false;
}
/***************************************************************************/
void GSocketInternalClient::Write()
{
	if (IsConnection()) 
	{
		if (memory_out.Used()) WriteInternal();
	}
	else
	{
		if(IsConnection(EConnectionTest))
		{
			connection_mode=EConnectionEstabilishing;
			if(msg_callback_connect) 
				msg_callback_connect(msg);
			else 
			{
				msg.WI(IGMI_CONNECT).WT(game_name).WI(game_instance).WUI(0).WUS(0).WI(0);
				msg.A();	
			}
			WriteInternal();
		}
	}
	Communicate(true);
}
/***************************************************************************/
void GSocketInternalClient::Error()
{
	RAPORT("INTERNAL (%d:%s:%s) Error(1) fd = %d, connection_mode = %s",client_service_group,GetServiceName(client_service_type),AddrToString(addr)(), (int)data.socket, connection_desc[connection_mode].desc);
	if(SleepConnection()) return;
	RAPORT("INTERNAL (%d:%s:%s) Error(2) fd = %d, connection_mode = %s",client_service_group,GetServiceName(client_service_type),AddrToString(addr)(), (int)data.socket, connection_desc[connection_mode].desc);
	if(IsConnection())
		connection_mode=EConnectionBroken;
	else
		connection_mode=EConnectionNoConnection;
	disconnected=boost::posix_time::second_clock::local_time();
}
/***************************************************************************/
bool GSocketInternalClient::ReadInternal()
{
	if(!memory_in.GetAllocated()) 
	{
		AllocateIn(K32);
	}

	int size=recv(data.socket,memory_in.End(),memory_in.Free(),0);
	SVAL(INFO_NET_INTERNAL_IN,size);
	if (size==SOCKET_ERROR || size==0)
	{
		if(size==0)
		{
			connection_mode=EConnectionDisconnection;
			return false;
		}
		else
		{
			int error=GET_SOCKET_ERROR();
			RAPORT("INTERNAL (%d:%s:%s) recv() errno = %d, fd = %d, connection_mode = %s",client_service_group,GetServiceName(client_service_type),AddrToString(addr)(), error, (int)data.socket, connection_desc[connection_mode].desc);
			if (error==ECONNRESET)
			{
				connection_mode=EConnectionClosed;
				return false;
			}
			else
			{
				connection_mode=EConnectionBroken;
				return false;
			}
		}
	}
	else
	{
		memory_in.IncreaseUsed(size);
		return true;
	}
}
/***************************************************************************/
bool GSocketInternalClient::WriteInternal()
{
	int size=send(data.socket,memory_out.Start(),memory_out.Used(),0);
	SVAL(INFO_NET_INTERNAL_OUT,size);
	if (size==SOCKET_ERROR)
	{
		int error=GET_SOCKET_ERROR();
#ifdef LINUX
		if(errno!=EAGAIN && errno!=EWOULDBLOCK) 
#else
		if(GET_SOCKET_ERROR()!=EWOULDBLOCK) 
#endif
		{
			RAPORT("INTERNAL (%d:%s:%s) send() errno = %d, fd = %d, connection_mode = %s",client_service_group,GetServiceName(client_service_type),AddrToString(addr)(), error, (int)data.socket, connection_desc[connection_mode].desc);
			if (error==ECONNRESET)
			{
				connection_mode=EConnectionClosed;
				return false;
			}
			else
			{
				connection_mode=EConnectionBroken;
				return false;
			}
		}
	}
	else
	if(size==0)
	{
		connection_mode=EConnectionDisconnection;
		return false;
	}
	else
	{
		memory_out.Move(size);
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
	return true;
}
/***************************************************************************/
void GSocketInternalClient::TestPing()
{
	if (ping.out(clock))
	{
		if(IsConnection())
		{
			msg.WI(IGMI_PING).A();
			Write();
		}
	}
	if(ping.in.Test(clock))
	{
		connection_mode=EConnectionPingTimeOut;
	}
}
/***************************************************************************/
bool GSocketInternalClient::IsConnection(EConnectionMode m)
{
	if(m==EConnectionNone)
	{
		return (connection_mode==EConnectionEstabilished || connection_mode==EConnectionCrypted);
	}
	else
	{
		return connection_mode==m;
	}
}
/***************************************************************************/
void GSocketInternalClient::Parse()
{
	INT32 pre_message=0;
	while(msg.Parse(pre_message))
	{
		INT32 message=0;
		msg.RI(message);
		if(!msg.GetErrorCode())
		switch(message)
		{
			case IGMI_ACCEPT:
				{
					SRAP(INFO_INTERNAL_CONNECTION_ACCEPTED);
					RAPORT(GSTR(INFO_INTERNAL_CONNECTION_ACCEPTED));
					RAPORT("INTERNAL (%d:%s:%s)",client_service_group,GetServiceName(client_service_type),AddrToString(addr)());

					if(msg_callback_internal) msg_callback_internal(this,msg,IGMI_ACCEPT);
				}
				break;
			case IGMI_PING:
				{
				}
				break;
			default:
				{
					if(message>IGMI_FIRST && message<IGMI_LAST) 
					{
						if(ProcessInternalMessage(message)) break;
						break;
					}
					if(message>IGMIT_FIRST && message<IGMIT_LAST) 
					{
						if(ProcessTargetMessage(message)) break;
						break;
					}

					SRAP(ERROR_INTERNAL_MSG_UNKNOWN);
					RAPORT("%s %d",GSTR(ERROR_INTERNAL_MSG_UNKNOWN),message);

				}
				break;
		}
	}
	TestWrite();
}
/***************************************************************************/
void GSocketInternalClient::ServiceInfo(strstream & s)
{
	boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
	string t;
	IsConnection()?t=to_simple_string(connected):t=to_simple_string(disconnected);
	s<<boost::format("%|-8d|%|-12s|%|-20s|%|-12s|%|-22s|")% 
		client_service_group % GetServiceName(client_service_type) % AddrToString(addr)() % (IsConnection()?"Connected":"Error") %t;
	s<<lend;
}
/***************************************************************************/
void GSocketInternalClient::ServiceReinit(strstream & s,SSocketInternalReinit & reinit)
{
	bool reinit_connection=false;
	if(reinit.group==-1 && reinit.name.size()==0 && reinit.addr.sin_port==0 && reinit.addr.sin_addr.s_addr==0) reinit_connection=true;
	else
	{
		string str=GetServiceName(client_service_type);
		reinit_connection=true;
		if(reinit.group!=-1) if (client_service_group!=reinit.group) reinit_connection=false;
		if(reinit.addr.sin_port!=0) if (addr.sin_port!=reinit.addr.sin_port) reinit_connection=false;
		if(reinit.addr.sin_addr.s_addr!=0) if (addr.sin_addr.s_addr!=reinit.addr.sin_addr.s_addr) reinit_connection=false;
		if(reinit.name.size()!=0) if(str!=reinit.name) reinit_connection=false;
	}
	if(reinit_connection)
	{
		boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
		s<<boost::format("%|-8d|%|-10s|%|-20s|%|-12s|")% client_service_group % GetServiceName(client_service_type) % AddrToString(addr)() % (IsConnection()?"Reconnected":"Retry");
		IsConnection()?s<<to_simple_string(connected):s<<to_simple_string(disconnected);
		s<<lend;
		connection_mode=EConnectionReConnect;
	}
}
/***************************************************************************/
bool GSocketInternalClient::ProcessInternalMessage(int message)
{
	if(!msg_callback_internal)
	{
		SRAP(ERROR_UNKNOWN);
		RAPORT("ERROR Undefined callback");
		return false;
	}
	else
	{
		if(!msg_callback_internal(this,msg,message))
		{
			return false;
		}
	}
	return true;
}
/***************************************************************************/
bool GSocketInternalClient::ProcessTargetMessage(int message)
{
	GNetTarget target;
	target.Init(TA,TN,TN);

	msg.R(target);

	if (message == IGMIT_PROCESSED) 
	{
		PreProcessMessage(message,target);
		return true;
	};
	if(!msg_callback_target)
	{
		SRAP(ERROR_UNKNOWN);
		RAPORT("ERROR Undefined callback");
		return false;
	}
	else
	{
		if(!msg_callback_target(this,msg,message,target))
		{
			return false;
		}
	}
	PostProcessMessage(message,target);
	return true;
}
/***************************************************************************/

/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#undef max

#define GLOBAL_GROUP_ID(host, port, group_id)		((DWORD64)((((DWORD64)host & 0xffffffff) << 32) | (((DWORD64)port & 0xffff) << 16) | ((DWORD64)group_id & 0xffff)))

inline EServiceTypes GetServiceTypeBasic(EServiceTypes type)
{
	EServiceFlags t1,t2;
	t1=t2=ESFUnknown;
#define GSERVICE(x1,y1,x2,y2,z,u) case x1:t1=y1;t2=u;break;case x2:t1=y2;t2=u;break;
	switch(type)
	{
#include "tbl/gservices.tbl"
	};
#undef GSERVICE
	if(t1==ESFAccept && t2==ESFBasic) return ESTAcceptBase;
	if(t1==ESFClient && t2==ESFBasic) return ESTClientBase;
	if(t1==ESFAccept && t2==ESFConsole) return ESTAcceptBaseConsole;
	if(t1==ESFClient && t2==ESFConsole) return ESTClientBaseConsole;
	return type;
}
inline EServiceTypes GetServiceTypeAssociate(EServiceTypes type)
{
#define GSERVICE(x1,y1,x2,y2,z,u) case x1: return x2;
	switch(type)
	{
#include "tbl/gservices.tbl"
	}
#undef GSERVICE
	return ESTUnknown;
}
inline char * GetServiceName(EServiceTypes type)
{
#define GSERVICE(x1,y1,x2,y2,z,u) case x1:return z;case x2: return z;
	switch(type)
	{
#include "tbl/gservices.tbl"
	}
#undef GSERVICE
	return "unknown";
}



/***************************************************************************/
class GPollManager;
/**
\brief bazowa klasa odpowiadajaca za komunikacje, klasy pochodne procesuja konsole, gracza i inne elementy
aby uzyc elementu trzeba go
- zainicjalizowac podajac deksryptor socketu i typ polaczenia
- ewentualnie zduplikowac jesli wymaga tego konfiguracja serwera
- sformatowac aby bufory mialy wpasciwa wielkosc, byl nie blokowalny itd
- zarejestrowac w poll_managerze aby sprawdzac jego status
Poll_manager jest niezalezny od set'u zawierajacego pointery na elementy. Pozwala to na podwojna iteracje.
Poll sprawdza to co sie tyczy polaczen. Set odpowiada za to co sie tyczy czasu.
*/
class GSocketConsole;
class GLogic;
class GSocket : boost::noncopyable
{
	friend class GPollManager;
	friend class GLogic;
	friend class GPlayer;
public:
	GSocket();
	GSocket(GRaportInterface & raport_interface,SOCKET socket,EServiceTypes type,GMemoryManager * memory_mgr=NULL,GPollManager * poll_mgr=NULL)
	{
		Init(raport_interface,socket,type,memory_mgr,poll_mgr);
	}
	virtual					~GSocket();
	void					Init(GRaportInterface & raport_interface,SOCKET socket,EServiceTypes type,GMemoryManager * memory_mgr=NULL,GPollManager * poll_mgr=NULL);
	void					Init(GRaportInterface & raport_interface,GSocket * socket,GMemoryManager * memory_mgr=NULL,GPollManager * poll_mgr=NULL);
	virtual void			Destroy();
	virtual bool			RegisterPoll();
	virtual bool			UnregisterPoll();
	virtual void			Close();
	virtual void			Move();
	inline SOCKET			GetSocket(){return data.socket;};
	inline EServiceTypes	GetServiceType(){return data.service_type;};
	virtual bool			Read();
	virtual void			Write();
	void					TestWrite();
	virtual void			Parse(GRaportInterface & ){};
	virtual void			Reset();
	virtual void			Format(int size);
	virtual bool			Duplicate(GRaportInterface & raport_interface);
	virtual void			Unduplicate();

	bool					AllocateIn(int size=K1){return memory_in.Allocate(size);};
	bool					AllocateOut(int size=K4){return memory_out.Allocate(size);};
	virtual void			FirstAllocateIn(){AllocateIn(K2);};
	virtual void			FirstAllocateOut(){AllocateOut(K2);};
	void					DeallocateIn(){memory_in.Deallocate();};
	void					DeallocateOut(){memory_out.Deallocate();};
	bool					ReallocateInTo(int size=K1){return memory_in.ReallocateTo(size);};
	bool					ReallocateOutTo(int size=K4){return memory_out.ReallocateTo(size);};

	GMemory &				MemoryIn(){return memory_in;};
	GMemory	&				MemoryOut(){return memory_out;};
	GNetMsg &				Msg(){return msg;};
	GNetMsg &				MsgExt(){AddWrite();return msg;};
	GNetMsg &				MsgExt(INT32 message){msg.WI(message);AddWrite();return msg;};
	void					AddWrite();
	virtual ENetCmdType		GetCmdType(){return ENetCmdGame;};

	bool					AnalizeMsgError(GRaportInterface & raport_interface);

	void					LinkLogic(GLogic *);
	void					UnlinkLogic();
	GLogic *				Logic();

	void					SetCryptMode(ENetCryptType mode);
	void					SetTimeLastAction(DWORD64 t){time.last_action=t;};
	DWORD64					GetTimeLastAction(){return time.last_action;};
	void					SetTimeConnection(DWORD64 t){time.connection=t;};
	DWORD64					GetTimeConnection(){return time.connection;};
	bitset<numSockFlags>&	Flags(){return flags;};
	void					SetDeadTime(GClock & clock){time.dead_time=clock.Get()+TIME_DEAD;};
	bool					IsDeadTime(GClock & clock){if(time.dead_time==0) return false;return time.dead_time<clock.Get();};
	NPair					GetSpoolBufferOut() { return NPair((BYTE *)spool_buffer_out, spool_buffer_out_size); };

protected:
	bitset<numSockFlags>	flags;

	struct SData
	{
		void Zero()
		{
			socket=0;
			service_type=ESTUnknown;
			MEMRESET(keys);
			keys.AES_key.Init();
			crypt_mode=ENetCryptNone;
		};
		SOCKET				socket;
		EServiceTypes		service_type;
		ENetCryptType		crypt_mode;
		union{
			SAESKey			AES_key;
			SDESKey			DES_key;
			SXORKey			XOR_key;
		}keys;
	} data;
	struct STime
	{
		void Zero()
		{
			last_action=0;
			connection=0;
			dead_time=0;
		}
		DWORD64				last_action;
		DWORD64				connection;
		DWORD64				dead_time;
	} time;

	GLogic *				logic;
	GPollManager *			poll_manager;
	GMemoryManager *		memory_manager;
	GMemory					memory_in;
	GMemory					memory_out;
	GNetMsg					msg;

	// Bufor wykorzystywany przy przepinaniu socketu miedzy watkami.
	// Zawiera dane, ktore nie zdazyly zostac wyslane w starym watku.
	char					spool_buffer_out[128];
	int						spool_buffer_out_size;

public:
	INT32					socketid;
	DWORD64					socket_inactivity_timeout;
};
/***************************************************************************/
class GLogicStatistics
{
public:
	GLogicStatistics(){Zero();};
	void					Zero()
	{
		int a;
		for (a=0;a<STAT_ALL;a++)
		{
			counter[a][0]=0;
		}
		counter[STAT_PRIVATECHAT][1]=30;
	};
	bool AddAction(EStatsAction action)
	{
		counter[action][0]++;
		return (counter[action][0]<=counter[action][1]);
	}

private:
	INT32					counter[STAT_ALL][2];
};
/***************************************************************************/
class GLogic: public GNetStructure
{
	friend class GSocket;
public:
	GLogic(){Zero();};
	void					Zero(){socket=NULL;flags.reset();};
	void					LinkSocket(GSocket * socket);
	void					UnlinkSocket();
	GSocket *				Socket();
	virtual void			Register(){};
	virtual void			Unregister(){};
	bool					Continue(){return !flags[ELogExit];};
	void					Close(){flags.set(ELogExit);};
protected:
	bitset<numLogFlags>		flags;
	GSocket *				socket;

	GLogicStatistics		action_stats;
};
/***************************************************************************/




enum EConnectionMode{
#define GCONNECTION(x,y)		x,
#include "tbl/gconnection.tbl"
#undef GCONNECTION
};
/***************************************************************************/
#pragma warning (disable: 4511)
/***************************************************************************/
typedef boost::function<bool (GSocket *,GNetMsg&,int,GNetTarget &)> MSG_CALLBACK_TARGET;
typedef boost::function<bool (GSocket *,GNetMsg&,int)> MSG_CALLBACK_INTERNAL;
typedef boost::function<void (GNetMsg&)> MSG_CALLBACK_CONNECT;
/***************************************************************************/
struct SSocketInternalReinit
{
	SSocketInternalReinit(){group=-1;memset(&addr,0,sizeof(sockaddr_in));};
	int					group;
	string				name;
	sockaddr_in			addr;
};
/***************************************************************************/
class GSocketInternalClient: public GSocket, public boost::signals2::trackable
{
public:
	GSocketInternalClient(GRaportInterface & raport_interface_);
	~GSocketInternalClient();
	void					Init(EServiceTypes type,int level,GMemoryManager * memory_mgr,GPollManager * poll_mgr);
	virtual void			Communicate(bool force=false);
	void					Parse();
	bool					Read();
	void					Write();
	void					Error();
	void					ReInitConnection(){connection_mode=EConnectionForceReInit;};
	bool					IsConnection(EConnectionMode m=EConnectionNone);
	virtual void			PreProcessMessage(int , GNetTarget & ){};
	virtual void			PostProcessMessage(int , GNetTarget & ){};
	void					Close();
	void					TestConnection();
	virtual void			ServiceInfo(strstream & s);
	virtual void			ServiceReinit(strstream & s,SSocketInternalReinit & reinit);
	inline EServiceTypes	GetClientServiceType(){return client_service_type;};
	sockaddr_in &			GetAddr() { return addr; };
	void					SetHostPort(DWORD32 host, USHORT16 port);

	MSG_CALLBACK_TARGET		msg_callback_target;
	MSG_CALLBACK_INTERNAL	msg_callback_internal;
	MSG_CALLBACK_CONNECT	msg_callback_connect;
protected:
	void					InitConnection();
	void					CloseConnection();
	bool					SleepConnection();
	void					SetMode(EConnectionMode mode){connection_mode=mode;}
	void					CommunicateStep();
	bool					ReadInternal();
	bool					WriteInternal();
	void					TestPing();
	void					GetHostPort();
	ENetCmdType				GetCmdType(){return ENetCmdInternal;};
	virtual bool			ProcessInternalMessage(int message);
	virtual bool			ProcessTargetMessage(int message);

	EConnectionMode			connection_mode;
	int						connection_try;
	int						connection_error;
	GClock					clock;

	struct SPing{
		GTimeInterval		in;
		GTimeInterval		out;
		GTimeInterval		connection;
		GTimeInterval		connection_next;
		GTimeInterval		connection_error;
	} ping;
	EServiceTypes			client_service_type;
	int						client_service_group;
	sockaddr_in				addr;
	bool					addr_is_set_manually;
	boost::posix_time::ptime 	connected;
	boost::posix_time::ptime 	disconnected;
	GRaportInterface &		raport_interface;
};
/***************************************************************************/
class GSocketInternalServer: public GSocket
{
public:
	GSocketInternalServer(GRaportInterface & raport_interface_):GSocket(),raport_interface(raport_interface_){data.Zero();};
	GSocketInternalServer(GRaportInterface & raport_interface_,SOCKET socket,EServiceTypes service_type,GMemoryManager * memory_mgr=NULL,GPollManager * poll_mgr=NULL):GSocket(raport_interface_,socket,service_type,memory_mgr,poll_mgr),raport_interface(raport_interface_){data.Zero();};
	virtual void			Parse(GRaportInterface & raport_interface);
	virtual bool			ProcessInternalMessage(int message);
	virtual bool			ProcessTargetMessage(int message);
	virtual void			FirstAllocateIn(){AllocateIn(K32);msg.SetChunkLimit(K1024);};
	ENetCmdType				GetCmdType(){return ENetCmdInternal;};
	MSG_CALLBACK_TARGET		msg_callback_target;
	MSG_CALLBACK_INTERNAL	msg_callback_internal;

	struct SData
	{
		void Zero()
		{
			game_instance = group_id = 0;
			global_group_id = 0;
			game_name = "";
		};
		string				game_name;
		INT32				game_instance;
		INT32				group_id;
		DWORD64				global_group_id;
	}data;
	GRaportInterface&		raport_interface;
protected:
};
/***************************************************************************/


class GSocketConsole: public GSocket
{
public:
	GSocketConsole();
	GSocketConsole(GRaportInterface & raport_interface_,SOCKET socket,EServiceTypes type,GMemoryManager * memory_mgr=NULL,GPollManager * poll_mgr=NULL):GSocket(raport_interface_,socket,type,memory_mgr,poll_mgr){};
	virtual bool			Read();
	virtual void			Write();

public:
	INT32					awaiting_console_tasks_responses_count;
	queue<string>			pending_console_commands;
};
/***************************************************************************/
#pragma warning (default: 4511)

/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "mysql.h"

extern MYSQL			mysql;

#pragma warning(disable: 4511)
#pragma warning(disable: 4505)
/***************************************************************************/
enum EConsoleTaskType
{
    ECTT_None,
	ECTT_Request,
	ECTT_Response,
};
/***************************************************************************/
struct SConsoleTask
{
	SConsoleTask()
	{
		socketid = -1;
		type = ECTT_None;
	}
	SConsoleTask(const SConsoleTask & t)
	{
		operator=(t);
	}

	INT32				socketid;
	EConsoleTaskType	type;

	ECmdConsole			cmd;
	map<string, string>	options;
	string				result;

	void operator=(const SConsoleTask & t)
	{
		socketid = t.socketid;
		type = t.type;
		cmd = t.cmd;
		options = t.options;
		result = t.result;
	}
};
typedef queue<SConsoleTask> TConsoleTaskQueue;
/***************************************************************************/
typedef map<EServiceTypes,GTicketInternalClient* >	TServiceMap;
typedef vector<GTicketInternalClient*>				TServiceVector;
class GServerBase: private boost::noncopyable, public boost::signals2::trackable
{
	friend class GSocket;
	friend class GTicketInternalClient;
public:
	GServerBase();
	virtual ~GServerBase(){Destroy();};

	virtual bool				Init();
	void						Destroy();
	virtual void				Action();
	virtual void				ActionLoopBegin() {};
	virtual void				ActionLoopEnd() {};

	GSocket *					SocketAdd(EServiceTypes type);
	virtual void				SocketRemove(GSocket *,EUSocketMode);
	virtual void				SocketDestroy(GSocket *,EUSocketMode);
	bool						DuplicateSocket(SOCKET socket);

	virtual GSocket *			SocketNew(EServiceTypes type){GSocket * s=new GSocket();return s;};
	GSocket *					GetBaseSocketByID(INT32 socketid);
	GSocket *					GetPlayerSocketByID(INT32 socketid);

	bool						Continue(){return !flags[EFClose];};
	void						Close(){flags.set(EFClose);};

	virtual bool				ConfigureServices();
	virtual void				InitServices();
	virtual void				InitService(SSockService & service,int & ok);
	SSockService *				FindService(EServiceTypes type);

	virtual bool				ConfigureSockets();
	void						TestSocketsBinding();

	virtual void				InitInternalClients(){};
	virtual void				InitInternalClient(GTicketInternalClient *& target,EServiceTypes type,int level);
	GTicketInternalClient *		InternalClient(EServiceTypes type);


	void						AnalizeAccept(epoll_event * ev);
	void						AnalizeClient(epoll_event * ev);

	void						AnalizeServerClient(epoll_event * ev);

	virtual	void				ProcessPoll();
	virtual void				ProcessTimeOut();
	bool						TestTimeOut(GSocket * socket);

	virtual bool				RegisterLogic(GSocket *){return true;};
	virtual bool				RegisterLogic(GSocket *,GSocket *){return true;};
	virtual void				UnregisterLogic(GSocket *,EUSocketMode , bool){};

	//funkcje zwiazane z przejsciem miedzy watkami
	virtual	void				ProcessMove();
	virtual void				SocketMoveAddToGlobalThread(vector<GSocket *> &);
	virtual void				SocketMoveRemoveFromGlobalThread(GSocket *);
	virtual void				SocketMoveAddToRoomThread(GSocket *);
	virtual void				SocketMoveToSocketGameInRoomThread(GSocket *);

	void						SetCurrentSTime();
	void						Clock();
	virtual void				ConfigureClock();
	GClock &					GetClock(){return clock;};
	DWORD64						ActionLoopCounter() { return action_loop_counter; };
	virtual void				ServiceInfo(strstream &);
	virtual void				ServiceReinit(strstream & s,SSocketInternalReinit & reinit);

	virtual INT32				CallAction(strstream &, ECmdConsole, map<string, string> &);
	void						InsertConsoleTask(SConsoleTask & task);
	virtual void				ProcessConsoleTasks();

	virtual bool				TestFirewall(const in_addr){return true;};
	GMemoryManager &			MemoryManager(){return memory_manager;};
	int							GenerateClientRID(){client_rid++;if(client_rid==-1) client_rid=1;return client_rid;};
	DWORD64						StartSTime(){return start_s_time;};
	DWORD64						CurrentSTime(){return current_s_time;};

	void						RaportRestartSQL(const char * status);

#ifndef LINUX
	virtual void				ParseKeyboard(){};
#endif

	boost::signals2::signal<void()>		signal_ms;
	boost::signals2::signal<void()>		signal_second;
	boost::signals2::signal<void()>		signal_seconds_5;
	boost::signals2::signal<void()>		signal_seconds_15;
	boost::signals2::signal<void()>		signal_minute;
	boost::signals2::signal<void()>		signal_minutes_5;
	boost::signals2::signal<void()>		signal_minutes_15;
	boost::signals2::signal<void()>		signal_hour;
	boost::signals2::signal<void()>		signal_day;
	GRaportInterface			raport_interface;

	bool						MySQL_Query_Raport(MYSQL * mysql, const char * query, char * qa_filename, int qa_line);

	boost::signals2::signal<void()>		signal_new_sockets_initialized;
	
protected:

	SServiceManager				service_manager;
	vector<GSocket*>			socket_listen;
	vector<GSocket*>			socket_move;
	vector<GSocket*>			socket_move_destroy;
	INT32						seq_socket_id;
	bool						all_sockets_initialized;

	struct hash_addr
	{ 
		size_t operator()(GSocket * x) const { return (size_t)x;} ;
	};
	struct eq_addr
	{
		bool operator()(GSocket* a1, GSocket* a2) const
		{
			return (a1==a2);
		}
	};
	typedef dense_hash_set<GSocket*, hash_addr, eq_addr>	TSocketHash;
	typedef hash_map<INT32, GSocket *> TSocketIdMap;

	TSocketHash					socket_game;
	TSocketIdMap				socket_game_map;

	TSocketIdMap				socket_base_map;

	TServiceVector				socket_internal;
	TServiceMap					socket_internal_map;

	int							max_service_size;
	int							client_rid;

	GClock						clock;
	GClocks						clocks;

	DWORD64						action_loop_counter;
	DWORD64						action_loop_sql_queries_times;
	DWORD32						action_loop_sql_queries_count;

	bitset<numEFlags>			flags;
	GPollManager				poll_manager;
	GMemoryManager				memory_manager;

	DWORD64						start_s_time;
	DWORD64						current_s_time;

	TConsoleTaskQueue			console_task_queue;

	boost::mutex				mtx_move;
	boost::mutex				mtx_console_task_queue;
};
/***************************************************************************/
class GServerBaseConsole: public GServerBase
{
	typedef GServerBaseConsole	base_t;
public:
	GServerBaseConsole():GServerBase() {};
	~GServerBaseConsole(){Destroy();};
	void						Destroy();
	void						AnalizeAcceptConsole(epoll_event * ev);
	void						AnalizeClientConsole(epoll_event * ev);
	virtual void				ParseConsole(GSocketConsole *);
	virtual void				ExecuteConsoleCommand(GSocketConsole * socket, string & cmd) {};
	virtual	void				ProcessPoll();
	GSocketConsole *			SocketConsoleAdd();
	virtual void				SocketConsoleRemove(GSocketConsole *);
	virtual void				SocketConsoleDestroy(GSocketConsole *);

	virtual void				ProcessConsoleTasks();

private:
	vector<GSocketConsole*>		socket_console_accept;
	TSocketIdMap				socket_console_map;
};
/***************************************************************************/
#pragma warning(default: 4505)
#pragma warning(default: 4511)

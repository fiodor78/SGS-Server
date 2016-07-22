/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#pragma warning(disable: 4511)

/***************************************************************************/
class GSocketAccess: public GSocket
{
public:
	GSocketAccess():GSocket(){};
	GSocketAccess(GRaportInterface & raport_interface,SOCKET socket,EServiceTypes service_type,GMemoryManager * memory_mgr=NULL,GPollManager * poll_mgr=NULL):GSocket(raport_interface,socket,service_type,memory_mgr,poll_mgr){};
	virtual void				Parse(GRaportInterface & raport_interface);
	SPlayerInfo					player_info;
	INT32						group_id;

	DWORD64						timestamp_igm_connect;
private:
};
/***************************************************************************/
class GSocketGame: public GSocket
{
public:
	GSocketGame():GSocket(){};
	GSocketGame(GRaportInterface & raport_interface,SOCKET socket,EServiceTypes service_type,GMemoryManager * memory_mgr=NULL,GPollManager * poll_mgr=NULL):GSocket(raport_interface,socket,service_type,memory_mgr,poll_mgr){};
	virtual void			Parse(GRaportInterface & raport_interface);
};

/***************************************************************************/
class GGroupManager: public GServerBase
{
public:
	GGroupManager(const SSockService & sock_service_p);
	~GGroupManager(){Destroy();};
	bool					Init(int group_number_p, EGroupType group_type_p);
	void					PrepareToClose() {flags.set(EFPrepareToClose);};
	bool					IsPreparingToClose() { return flags[EFPrepareToClose]; };
	void					Destroy();
	void					ActionLoopBegin();
	void					ActionLoopEnd();
	void					RaportCurrentStatus();

	void					InitInternalClients();
	bool					RegisterLogic(GSocket *,GSocket *);
	void					UnregisterLogic(GSocket *,EUSocketMode, bool);
	void					UnregisterLogicServer(GPlayerGame *,EULogicMode mode);
	GSocket *				SocketNew(EServiceTypes){GSocket * s=new GSocketGame();return s;};
	INT32					CallAction(strstream &, ECmdConsole, map<string, string> &);

	bool					InitSQL();
	void					DestroySQL();

	void					ConfigureClock();
	void					ProcessTimeOut();
	bool					TestTimeOut(GSocket * socket);

	void					IntervalSecond();

	GTicketInternalClient *			i_access;
	GTicketInternalClient *			i_cache;
	GTicketInternalClient *			i_globalinfo;
	GTicketInternalClient *			i_nodebalancer;
	GTicketInternalClient *			i_cenzor;
	GTicketInternalClient *			i_globalmessage;

	GGroup &						Group(){return group;};

	void					TicketToGroup(DWORD32 host, USHORT16 port, INT32 group_id, int message, GTicketPtr ticket);
	void					DropOutgoingTickets();

	MYSQL					mysql;
	bool					sql_config_initialized;

	const SSockService &	sock_service;

	SMetricStats::SCallContext		action_loop_call_ctx;
	DWORD64							action_loop_handled_logic_messages;

private:
	GGroup							group;

	INT32							languages[MAX_LANGUAGES];
	INT32							language_pos;
	INT32							language_max_pos;
};
/***************************************************************************/
class DynamoDBConnection
{
public:
	DynamoDBConnection();
	~DynamoDBConnection();
	bool				Init();
	bool				PerformRequest(const char * action, const char * params, string & result);

private:
	bool				RefreshSessionToken();
	static size_t		receive_data(void * buffer, size_t size, size_t nmemb, void * connection);

	CURL *				curl;
	string				response;

	static boost::mutex	mtx_aws_session;
	static struct SAWSSession
	{
		SAWSSession()
		{
			last_session_request = 0;
			expiration_time = 0;
		}

		DWORD64			last_session_request;
		DWORD64			expiration_time;
		string			session_token;
		string			access_key;
		string			secret_key;
	} aws_session;
};
/***************************************************************************/
class GLocationDBWorkerThread : private boost::noncopyable, public boost::signals2::trackable
{
public:
	GLocationDBWorkerThread();
	~GLocationDBWorkerThread()	{ Destroy(); }
	bool						Init();
	void						Action();
	void						Destroy();
	bool						Continue(){return !flags[EFClose];};
	void						Close(){flags.set(EFClose);};

protected:
	bool						InitLocationDB();
	void						DestroyLocationDB();
	bool						Compress(const string & str, unsigned long * compressed_size);
	bool						Uncompress(const char * compressed_data, unsigned long compressed_size, string & uncompressed_data);
	bool						ReallocateCompressBuffers(INT32 new_compress_buffer_size, INT32 new_uncompress_buffer_size);

	bool						InitDatabaseMySQL();
	void						DestroyDatabaseMySQL();
	bool						LoadLocationFromDatabaseMySQL(const char * location_ID, string & location_data);
	std::string					LoadLocationsIdsFromDatabaseMySQL(const string & pattern);
	bool						WriteLocationToDatabaseMySQL(const char * location_ID, const string & location_data);
	bool						DeleteLocationFromDatabaseMySQL(const char * location_ID);

	bool						InitDatabaseDynamoDB();
	void						DestroyDatabaseDynamoDB();
	bool						LoadLocationFromDatabaseDynamoDB(const char * location_ID, string & location_data);
	bool						WriteLocationToDatabaseDynamoDB(const char * location_ID, const string & location_data);
	bool						DeleteLocationFromDatabaseDynamoDB(const char * location_ID);

	SLocationDBTask				GetLocationDBTaskToRun();
	void						ProcessLocationDBTask(SLocationDBTask & current_task);
	void						ProcessLocationDBTaskResult(SLocationDBTask & current_task, ELocationDBTaskStatus status);
	void						ClearExpiredDBTasks();

protected:
	bitset<numEFlags>			flags;
	boost::thread *				worker_thread;
	GRaportInterface			raport_interface;

	MYSQL						mysql_locations;
	MYSQL						mysql_gamestates;
	DynamoDBConnection			dynamodb_gamestates;
	bool						locationdb_initialized;

	static boost::mutex			mtx_locationdb_multipart_locations;
	static map<string, INT32>	multipart_locations_total_parts;					// location_ID -> total_parts
	static map<string, INT32>	multipart_locations_second_partID;					// location_ID -> partID, od ktorego rozpoczyna sie reszta gamestate

	//buffers are allocated in constructor, deallocated in destructor
	//compression is happening into compress buffer
	//decompression is happening into uncompress buffer
	INT32						compress_buffer_size;
	INT32						uncompress_buffer_size;
	unsigned char				*compress_buffer;
	unsigned char				*uncompress_buffer;
	//we need big destination for building INSERTs/UPDATEs into blobl table, and for mysql_real_escape_string
	//no point in reallocating it at runtime
	char						*query_buffer;
	INT64						last_compressed_size;
};
/***************************************************************************/
struct SHTTPRequestTask
{
	SHTTPRequestTask()
	{
		http_request_id = 0;
		group_id = 0;
		execution_deadline = 0;
		response_code = 0;
	}
	INT32						http_request_id;
	INT32						group_id;
	string						location_ID;
	DWORD64						execution_deadline;					// do kiedy zezwalamy na wrzucenie taska do watku wykonujacego go

	string						request_method;
	string						url;
	string						secret_key;
	string						content_type;
	string						payload;
	map<string, string>			params;
	INT32						response_code;
	string						response;
};
/***************************************************************************/
typedef map<INT32, vector<SHTTPRequestTask> >  THTTPRequestTasksResultsMap; // group_id -> task results
/***************************************************************************/
class GHTTPRequestWorkerThread : private boost::noncopyable, public boost::signals2::trackable
{
public:
	GHTTPRequestWorkerThread(vector<SHTTPRequestTask> &	http_request_tasks, boost::mutex & mtx_http_request_tasks, bool fire_and_forget, const string & metrics_prefix);

	~GHTTPRequestWorkerThread()	{ Destroy(); }
	bool						Init();
	void						Action();
	void						Destroy();
	bool						Continue(){return !flags[EFClose];};
	void						Close(){flags.set(EFClose);};

private:
	bool						GetHTTPRequestTaskToRun(SHTTPRequestTask& current_task);
	void						ProcessHTTPRequestTask(SHTTPRequestTask & current_task);
	void						ProcessHTTPRequestTaskResult(SHTTPRequestTask & current_task, bool success);
	void						ClearExpiredHTTPRequestTasks();

	// Funkcje PerformHttp...Request() zwracaja HTTP response code, albo 0 w przypadku bledu.
	INT32						PerformHttpGetRequest(const char * url, const char * secret_key, string & result);
	INT32						PerformHttpCustomRequest(const char * url, const char * request_method, const char * post_payload, const char * content_type, const char * secret_key, string & result);
	static size_t				receive_data(void * buffer, size_t size, size_t nmemb, void * connection);

private:
	vector<SHTTPRequestTask> &	http_request_tasks;
	boost::mutex &				mtx_http_request_tasks;
	bool						fire_and_forget;
	string						metrics_prefix;

	bitset<numEFlags>			flags;
	boost::thread *				worker_thread;
	GRaportInterface			raport_interface;

	CURL *						curl;
	string						response;

	// Zmienne wykorzystywane do throttlowania requestow w razie ich niepowodzenia.
	DWORD64						last_submit_operation_success;
	DWORD64						last_submit_operation_fail;
	INT32						last_submit_fails_count;
	INT32						random_interval_change;					// procentowo o ile modyfikujemy odstepy miedzy kolejnymi udanymi/nieudanymi operacjami,
																		// zeby unikac peakow zapytan wykonywanych w jednym momencie
};
/***************************************************************************/
#pragma warning(disable: 4355)
class GServerGame: public GServer
{
public:
	GServerGame():GServer() {};
	bool			Init();
	bool			InitSQL();
	void			PrepareToClose();
	void			CheckGentleClose();
	void			Destroy();
	void			InitInternalClients();
	bool			InitThreads();
	bool			InitThread(int nr, EGroupType group_type);
	bool			RegisterLogic(GSocket *);
	void			UnregisterLogic(GSocket *,EUSocketMode, bool);
	INT32			CallAction(strstream &, ECmdConsole, map<string, string> &);
	GGroupManager*	FindGroupManager(int group);
	void			ConfigureClock();
	void			ProcessTimeOut();
	bool			TestTimeOut(GSocket * socket);
	GSocket *		SocketNew(EServiceTypes);
	void			ParseConsole(GSocketConsole *socket){console.ParseConsole(socket);};
	void			ExecuteConsoleCommand(GSocketConsole * socket, string & cmd) { console.ExecuteConsoleCommand(socket, cmd); };
	void			IntervalSecond();
	void			RaportCurrentStatus();

	DWORD64			GetGroupWriteLocationsDeadline(INT32 group_id);
	void			AddLocationDBTask(const char * location_ID, SLocationDBTask & task);
	void			AddLocationDBTaskResult(INT32 group_id, SLocationDBTaskResult & task_result);

	INT32			AddHTTPRequestTask(SHTTPRequestTask & task);
	void			AddHTTPRequestTaskResult(INT32 group_id, SHTTPRequestTask & task_result);
	void			AddHTTPFireAndForgetRequestTask(SHTTPRequestTask & task);

	GTicketInternalClient *			i_access;
	GTicketInternalClient *			i_nodebalancer;

	vector<GGroupManager *>			groups;
	vector<boost::thread *>			group_threads;
	GGroupAccess					group;
	vector<GLocationDBWorkerThread *>		locationdb_worker_threads;
	vector<GHTTPRequestWorkerThread *>		http_request_worker_threads;
	vector<GHTTPRequestWorkerThread *>		http_fire_and_forget_request_worker_threads;

	boost::mutex					mtx_locationdb_tasks;
	boost::mutex					mtx_locationdb_tasks_results;
	TLocationDBTasksMap				locationdb_tasks;
	TLocationDBTasksResultsMap		locationdb_tasks_results;

	boost::mutex					mtx_http_request_tasks;
	boost::mutex					mtx_http_fire_and_forget_request_tasks;
	boost::mutex					mtx_http_request_tasks_results;
	vector<SHTTPRequestTask>		http_request_tasks;
	vector<SHTTPRequestTask>		http_fire_and_forget_request_tasks;
	THTTPRequestTasksResultsMap		http_request_tasks_results;
	INT32							seq_http_request_id;

	boost::mutex					mtx_global_metrics;
	SMetrics						global_metrics;

	DWORD64							close_gently_timestamp;

private:
	GConsole						console;
};
/***************************************************************************/
#pragma warning(default: 4355)
#pragma warning(default: 4511)
/***************************************************************************/

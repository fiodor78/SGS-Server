/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#pragma warning(disable: 4511)
#pragma warning(disable: 4355)

/***************************************************************************/

#define LOGIC_MESSAGE_EXPIRATION_TIME					300				// po jakim czasie nie ma juz sensu dostarczac wiadomosci 'internal logic'

#define ETDR_NoGroupAddress ETDR_Custom1

class GGroup;
class GGroupManager;
class GSocketAccess;
/***************************************************************************/
enum EGroupStatus
{
	EGS_Initializing,
	EGS_Ready,
	EGS_Releasing_Gently,
	EGS_Transferring,
	EGS_Stopped,
	EGS_Releasing_Urgent,
	EGS_Releasing_Urgent_Before_Shutdown,
};
/***************************************************************************/
enum ELogicEngineStatus
{
	ELES_Initializing,
	ELES_Init_Failed,
	ELES_Ready,
	ELES_Old_Logic,
	ELES_Overload,
};
/***************************************************************************/
struct SMetricStats
{
	typedef DWORD64 SCallContext;

	DWORD64			min_value;
	DWORD64			max_value;
	DWORD64			sum;
	DWORD64			count;
	DWORD64			denominator;
	string			unit_name;

	DWORD64			last_value;
	SCallContext	last_value_update_time;

	SMetricStats()
	{
		Zero();
	}

	void Zero(bool save_unit_name = false)
	{
		min_value = 0;
		max_value = 0;
		sum = 0;
		count = 0;
		denominator = 1;
		if (!save_unit_name)
		{
			unit_name = "";
		}

		last_value = 0;
		last_value_update_time = 0;
	}

	static SCallContext BeginDuration();
	void	CommitSampleValue(DWORD64 value, const char * unit_name = "");
	void	CommitEvent(const char * unit_name = "1");
	void	CommitDuration(SCallContext ctx);
	void	CommitCurrentValue(DWORD64 value, const char * unit_name = "");
	void	CommitCurrentValueChange(INT64 change, const char * unit_name = "");

	string	ToString(bool pretty_format);
};
/***************************************************************************/
struct SMetrics
{
	SMetrics()
	{
		Zero();
	}

	void	Zero();
	void	CheckRotate();
	void	Print(strstream & s, INT32 interval_seconds, bool pretty_format);

	bool	ValidateMetricName(const char * metric_name);
	void	CommitSampleValue(const char * metric_name, DWORD64 value, const char * unit_name = "");
	void	CommitEvent(const char * metric_name, const char * unit_name = "");
	void	CommitDuration(const char * metric_name, SMetricStats::SCallContext ctx);
	void	CommitCurrentValue(const char * metric_name, DWORD64 value, const char * unit_name = "");
	void	CommitCurrentValueChange(const char * metric_name, INT64 change, const char * unit_name = "");

	// [0] - all, [1] - 300 sek. interval, [2] - 60 sek. interval
	struct
	{
		INT32			interval_seconds;
		DWORD64			current_metrics_collecting_time_start;

		map<string, SMetricStats>		current_metrics;				
		map<string, SMetricStats>		previous_metrics;
	} data[3];
};
/***************************************************************************/
/*! 
\brief najnizsza klasa w hierarchi logiki, umieszczana w GGroup, 
powi¹zana z klas¹ GSocket, odpowiadaj¹c¹ za przetwarzanie danych w sieci,
*/
class GPlayer: public GLogic
{
public: 
	GPlayer(GGroup* group_p, GSocket * socket_p,int rid_p);
	~GPlayer(){};
	void					Init(){Zero();};
	INT32					GetRID()const{return identity.rid;}
	virtual INT32			GetID()const{return 0;};
	bool					ProcessMessage(GNetMsg &,INT32);
	virtual bool			ProcessInternalMessage(GNetMsg &,INT32);
	virtual bool			ProcessTargetMessage(GNetMsg &, INT32);
	virtual GTicketInternalClient *	Service(EServiceTypes  type);
	virtual GGroupManager *			Server();
	virtual GNetMsg &				MsgInt(EServiceTypes type,INT32 message);
	virtual GNetMsg &				MsgExt(INT32 message);
	virtual GNetMsg &				MsgExt();
	virtual GNetMsg &				MsgExtCompressed(GNetMsg & msg);
	virtual void					TicketInt(EServiceTypes type,INT32 message,GTicketPtr ticket);
	GGroup *				Group();
	virtual void			Zero();
	void					InvalidateGroup(){group=NULL;};
	GSocket *				Socket(){return socket;};
	bool					Valid(){return socket!=NULL;};
	void					SetCloseComment(const char * comment){close_comment=comment;};
	void					SetCloseCommentID(INT32 raport,INT32 comment){close_raport_id=raport;close_comment_id=comment;};
	string					close_comment;
	INT32					close_raport_id;
	INT32					close_comment_id;
protected:
	struct SIdentity
	{
		SIdentity(){Zero();};
		void				Zero(){rid=0;};
		INT32				rid;
	} identity;
	GGroup *				group;
	GRaportInterface &		raport_interface;
};
//typedef boost::shared_ptr<GPlayer> GPlayerPtr;
/***************************************************************************/
/*! 
\brief - klasa przetwarzajaca logikê po przeniesieniu socketu do w¹tka groupy
*/
class GPlayerGame: public GPlayer
{
public: 
	GPlayerGame();
	GPlayerGame(GGroup* group_p,GSocket * socket_p,int rid_p);
	~GPlayerGame();
	bool					ProcessMessage(GNetMsg &,INT32);
	bool					ProcessInternalMessage(GNetMsg &,INT32){return false;};
	bool					ProcessTargetMessage(GNetMsg &, INT32);
	GNetMsg &				MsgInt(EServiceTypes type,INT32 message);
	void					TicketInt(EServiceTypes type,INT32 message,GTicketPtr ticket);
	void					ServicesRegister();
	void					ServicesUnregister();
	void					Zero();
	INT32					GetID()const{return player_info.ID;};
	void					UpdateLastAction();
	void					IntervalMinute(){action_stats.Zero();};
	void					TransferLocation();

	SPlayerInfo				player_info;
	std::set<INT32>			registered_buddies;

	GNetMsg &				WE(GNetMsg &msg)
	{
		player_info.WE(msg);
		return msg;
	}
	DWORD64					last_action_time;
	bitset<numEFlags>		flags;
		
	DWORD64					timestamp_igm_connect;
	DWORD64					transferring_location_timestamp;		// Serwer jest gaszony. Szukamy nowej grupy, do ktorej zostanie przeniesiona lokacja.
};

/***************************************************************************/
/***************************************************************************/
/*!
\brief klasa przetwarzaj¹ca logikê w w¹tku g³ównym, czyli de facto przetwarzaj¹ca informacje niezbêdne do zestawienia po³¹czenia i zlokalizwania celu dla u¿ytkownika
*/
class GPlayerAccess: public GPlayer
{
public: 
	GPlayerAccess(GGroup* group_p,GSocket * socket_p, int rid_p);
	virtual ~GPlayerAccess(){};
	bool					ProcessMessage(GNetMsg &,INT32);
	bool					ProcessInternalMessage(GNetMsg &,INT32);
	bool					ProcessTargetMessage(GNetMsg &, INT32);
};
/***************************************************************************/
class isPlayerRID
{
public:
	isPlayerRID(){id=0;};
	isPlayerRID(const INT32 t){id=t;};
	bool operator()(GPlayer * r){return id==r->GetRID();};
private:
	INT32 id;
};
/***************************************************************************/
typedef multimap<string, GPlayerGame *>		TPlayerGameMultiMap;
typedef pair<TPlayerGameMultiMap::iterator,
	TPlayerGameMultiMap::iterator>			TPlayerGameRange;

typedef map<INT32,GPlayer*>				TPlayerMap;
typedef TPlayerMap::value_type			TPlayerMapPair;
typedef multimap<INT32,GPlayer*>		TPlayerMultiMap;
typedef vector<GPlayer*>				TPlayerVector;
/***************************************************************************/
class GGroup;
/***************************************************************************/
struct SGroupAddress
{
	SGroupAddress()
	{
		host = 0;
		port = 0;
		group_id = 0;
		expiration_time = 0;
	}
	SGroupAddress(const SGroupAddress & t)
	{
		operator=(t);
	}

	SGroupAddress & operator=(const SGroupAddress & t)
	{
		host = t.host;
		port = t.port;
		group_id = t.group_id;
		expiration_time = t.expiration_time;
		return *this;
	}

	DWORD32			host;
	USHORT16		port;
	INT32			group_id;
	DWORD64			expiration_time;
};
typedef map<string, SGroupAddress>	TGroupAddressMap;
/***************************************************************************/
typedef vector<SLogicMessage>						TInternalLogicMessageVector;
typedef queue<pair<SLogicMessage, SGroupAddress> >	TInternalLogicMessageQueue;
/***************************************************************************/
class GGroupManager;
class GLogicEngine;
/*!
\brief klasa grupy (czyli w¹tku z graczami)
*/
class GGroup: private boost::noncopyable, public boost::signals2::trackable
{
public:
	GGroup();
	~GGroup(){};
	void					Init(GServerBase *,TServiceMap*, INT32 id_p, EGroupType type_p);
	void					InitClock();
	void					Destroy();
	void					WaitForLocationsFlush();

	INT32					GetID()const{return identity.id;}
	bool					ProcessInternalMessage(GSocket*,GNetMsg &,INT32);
	bool					ProcessGroupTargetMessage(GSocket*,GNetMsg & msg,INT32,GNetTarget &);
	bool					ProcessTargetMessage(GSocket*,GNetMsg & msg,INT32,GNetTarget &);
	bool					ProcessTargetMessageGroupConnection(GSocket*,GNetMsg & msg,INT32,GNetTarget &);
	bool					ProcessTargetMessageGroupConnectionReplies(GSocket*,GNetMsg & msg,INT32,GNetTarget &);
	void					ProcessConnection(GNetMsg & msg);
	virtual void			IntervalMSecond();
	GTicketInternalClient *	Service(EServiceTypes  type);
	GGroupManager *			Server();
	bool					Lang();
	INT32					LangID();
	INT32					LangReset();
	const wchar_t *			Res(INT32 id,int lng_=-1);
	boost::wformat			ResF(INT32 id,int lng_=-1);
	GNetMsg &				MsgInt(EServiceTypes type,INT32 message);
	GNetMsg &				MsgInt(GSocket * s,GNetMsg & msg, INT32 message);
	void					Ticket(EServiceTypes type,INT32 message,GTicketPtr ticket);
	void					PlayerAdd(GPlayer* player);
	void					PlayerDelete(GPlayer* ,EULogicMode );
	void					PlayerGameAdd(GPlayerGame* player);
	void					PlayerGameDelete(GPlayerGame* ,EULogicMode );
	void					RemovePlayerFromTargetMap(GPlayerGame*);
	GPlayerGame *			FindPlayer(const char * target_location_ID, int ID);
	TPlayerGameRange		FindPlayers(const char * target_location_ID);
	void					IntervalSecond();
	void					IntervalSeconds_5();
	void					IntervalMinute();
	void					IntervalMinutes_5();
	void					IntervalMinutes_15();
	void					UpdateGroupHeartBeat();
	bool					Reanimate();
	bool					CanCloseGently();
	bool					ReloadLogic();
	EGroupStatus			GetStatus() { return status; };
	EEffectiveGroupStatus	GetEffectiveStatus();
	bool					CanProcessLogicMessages();

	void					WriteMassUpdateMessagesSite(GNetMsg & msg);
	GPlayerGame *			PlayerDetectOtherInstances(GPlayerGame * );

	void					AssignLocation(const char * location_ID);
	void					ReleaseLocation(const char * location_ID);
	bool					IsLocationAssigned(const char * location_ID);

	GLogicEngine *			GetScriptingEngine(const char * location_ID);
	ELogicEngineStatus		GetScriptingEngineStatus();
	INT32					GetLoadedLocationsCount();
	void					PrintLogicStatistics(strstream & s);
	void					TransferLocationsToCurrentScriptingEngine();

	bool					GetLocationAddressFromCache(string & location_ID, SGroupAddress & address);
	void					PutLocationAddressToCache(string & location_ID, DWORD32 host, USHORT16 port, INT32 group_id);
	void					UpdateLocationAddressCache();

	void					ClearExpiredSystemMessages(bool force = false);

	void					AddOutgoingLogicMessage(SLogicMessage & message);
	void					AddIncomingLogicMessage(SLogicMessage & message, SGroupAddress & address);
	void					ProcessOutgoingLogicMessages();
	void					ProcessIncomingLogicMessages();
	void					ProcessLogicMessagesTransferQueue();
	void					DroppedTicketCallback(GTicket & ticket, ETicketDropReason reason);
	void					LogDroppedLogicMessage(SLogicMessage & message, bool is_outgoing, ETicketDropReason reason);
	void					DropAllPendingLogicMessages();
	void					ProcessGlobalMessage(const std::string & command, const std::string & params);
	void					ProcessSystemGlobalMessage(strstream & s, const std::string & command, const std::string & params);

	void					ProcessLocationDBTaskResults();
	void					ProcessHTTPRequestTaskResults();

	bitset<numEFlags>		flags;

	INT32					seq_locationdb_taskid;

	// Komendy konsoli, ktore nie zostaly jeszcze wykonane, bo czekamy na weryfikacje czy zostaly skierowane do wlasciwej grupy.
	// <SConsoleTask, expiration_time>
	vector<pair<SConsoleTask, DWORD64> >	pending_system_messages;

	// Do kiedy mamy prawo zapisywania lokacji do bazy danych.
	DWORD64					write_locations_deadline;

	// Globalnie unikalny identyfikator grupy.
	DWORD64					unique_group_id;
	DWORD64					seq_location_suffix_id;

	SMetrics				metrics;

private:
	struct SIdentity
	{
		SIdentity(){Zero();};
		void				Zero(){id=0;};
		INT32				id;
	} identity;
	TPlayerMap				index_rid;
	TPlayerGameMultiMap		index_target;
	TServiceMap*			services;
	GServerBase *			server_game;

	EGroupType				type;
	EGroupStatus			status;
	DWORD64					last_confirmed_heartbeat;
	set<string>				assigned_locations;					// ID lokacji, o ktorych wiemy, ze sa aktualnie przydzielone do grupy.
	DWORD64					shutdown_state_timestamp;			// Moment, kiedy grupa przeszla w stan EGS_Stopped po otrzymaniu sygnalu SIGINT.

	DWORD64					transferring_players_deadline;		// Max czas jaki serwer gry czeka na przejscie ze stanu EGS_Transferring do stanu EGS_Stopped po zrzuceniu przypisanych lokacji. 
																// Potrzebny na odebranie przekierowan do nowych grup z node balacera i przeslanie ich do klienta.

	GLogicEngine *					current_scripting_engine;
	vector<GLogicEngine *>			old_scripting_engines;

	GSimpleRaportManager			logic_raport_manager;

	TGroupAddressMap				location_address_cache;

	boost::mutex					mtx_logic_messages_transfer_queue;
	TInternalLogicMessageQueue		logic_messages_transfer_queue;		// Kolejka sluzaca do przesylania komunikatow miedzy watkami (grupami).
	TInternalLogicMessageVector		incoming_logic_messages;			// Wiadomosci logiki oczekujace na obsluzenie.
	TInternalLogicMessageVector		outgoing_logic_messages;			// Wiadomosci logiki oczekujace na wyslanie.
	TInternalLogicMessageVector		new_outgoing_logic_messages;		// Wiadomosci logiki oczekujace na przepisanie do kolejki outgoing_logic_messages.

private:
};
typedef boost::shared_ptr<GGroup> GGroupPtr;
/***************************************************************************/
/*!
\brief klasa grup dla glownego watku z accesem, troche inaczej procesowana logika 
 */

class GGroupAccess: public GGroup
{
public:
	GGroupAccess():GGroup(){};
	~GGroupAccess(){};
};
typedef boost::shared_ptr<GGroup> GGroupPtr;
/***************************************************************************/

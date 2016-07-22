/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

//! enum ServiceFlags
enum EServiceFlags
{
ESFUnknown,		/*!< nie zdefiniowany*/
ESFAccept,		/*!< socket nas³uchu*/
ESFClient,		/*!< socket klienta*/
ESFBasic,		/*!< socket podstawowy serwisu*/
ESFConsole		/*!< socket konsoli*/
};

//! enum EServiceTypes
#define GSERVICE(x1,y1,x2,y2,z,u) x1,x2,
enum EServiceTypes{ 
	ESTUnknown,
	ESTInternalClient,
#include "tbl/gservices.tbl"
};
#undef GSERVICE

//! enum EConsoleEnum
#define GCONSOLE(command,function,description)	command,
enum EConsoleEnum{
#ifdef SERVICES
#include "tbl/gconsole_basic.tbl"
#include "tbl/gconsole_services.tbl"
#include "tbl/gconsole_access.tbl"
#else
#include "tbl/gconsole_basic.tbl"
#include "tbl/gconsole.tbl"
#endif
};
#undef GCONSOLE

//! enum ESockFlags
enum ESockFlags{ 
	ESockDuplicate,			/*! socket podlegal duplikacji */
	ESockPoll,				/*! socket jest zarejestrowany w poll'u */
	ESockExit,				/*! nalezy zamknac socket */
	ESockError,				/*! wystapil blad transmisji*/
	ESockTimeOut,			/*! przekroczony max czas dla operacji */
	ESockControlWrite,		/*! ustawiono sprawdzanie flagi zapisu dla socketu w syspoll'u*/
	ESockDisconnect,		/*! klient sie odpial */
	ESockMove,				/*! socket jest w trakcie przesuniecia, nie mozna go kasowac */
	numSockFlags
};
//! enum ELogFlags
enum ELogFlags{ 
	ELogExit,				/*! nalezy zamknac logic */
	ELogError,				/*! wystapil blad transmisji*/
	ELogGroup,				/*! element jest zarejestrowany, usuniecie wymaga odrejestrowania*/
	numLogFlags
};


//! enum EFlags - flagi steruj¹ce prac¹ w¹tków
enum EFlags{ 
	EFClose,				/*! nalezy zamkn¹æ w¹tek*/
	EFPrepareToClose,		/*!	nalezy przygotowaæ w¹tek od zamkniêcia*/
	numEFlags
};

enum ECmdConsole
{
	CMD_HELP_NONE,
	CMD_HELP_COMMANDS,
	CMD_HELP_COMMAND,
	CMD_SERVICE_REINIT,
	CMD_SERVICE_GROUP,
	CMD_SERVICE_NAME,
	CMD_SERVICE_ADDRESS,
	CMD_SERVICE_PORT,
	CMD_SERVICE_SHOW,
	CMD_SERVICE_TICKETQUEUELIMIT,
	CMD_SQLSTATS_TXT,
	CMD_SQLSTATS_CSV,
	CMD_SQLSTATS_SLOWEST,
	CMD_RAPORT_VERBOSE,
	CMD_ACTION_MEMORY_INFO,
	CMD_ACTION_SERVICE_INFO,
	CMD_ACTION_SERVICE_REINIT,
	CMD_ACTION_RECONNECT_STATS,
	CMD_ACCESS_SESSION_NONE,
	CMD_ACCESS_ROOM_NONE,
	CMD_ACCESS_PLAYER_NONE,
	CMD_ACCESS_PLAYER_UPDATE,
	CMD_CACHE_RESOURCES_ERASE,
	CMD_CACHE_RESOURCES_RELOAD,
	CMD_CACHE_RESOURCES_CLEAR_ALL,
	CMD_CACHE_RESOURCES_SHOW,
	CMD_CACHE_RESOURCES_NONE,
	CMD_SLOWSQL_NONE,
	CMD_SLOWSQL_INFO,
	CMD_FLASHPOLICY_NONE,
	CMD_FLASHPOLICY_INFO,
	CMD_GLOBALINFO_NONE,
	CMD_GLOBALINFO_INFO,
	CMD_NODEBALANCER_NONE,
	CMD_NODEBALANCER_INFO,
	CMD_NODEBALANCER_GETGAMESTATEADDRESS,
	CMD_NODEBALANCER_GETLOCATIONADDRESS,
	CMD_LEADERBOARD_NONE,
	CMD_LEADERBOARD_INFO,
	CMD_LOGIC_RELOAD,
	CMD_LOGIC_STATS,
	CMD_LOGIC_CLEARSTATS,
	CMD_LOGIC_MESSAGE,
	CMD_LOGIC_GLOBAL_MESSAGE,
	CMD_CENZOR_NONE,
	CMD_CENZOR_INFO,
	CMD_GLOBALMESSAGE_NONE,
	CMD_GLOBALMESSAGE_INFO,
	CMD_GLOBALMESSAGE_ALL
};

// Czestotliwosc tworzenia nowego pliku raportu.
enum ERapMode
{
	RAP_DAY,
	RAP_HOUR,
	RAP_15MIN,
	RAP_5MIN,
};

enum EUSocketMode
{
	USR_TIMEOUT_PING,					//nie doszedl ping od klienta, rozpinamy polaczenie
	USR_TIMEOUT_CONNECTION,				//nie zestawiono polaczenia w wymaganym czasie
	USR_ERROR_REGISTER_POLL,			//nie udala sie rejestracja w poll'u
	USR_ERROR_REGISTER_LOGIC,			//nie udala sie rejestracja logiki
	USR_CONNECTION_CLOSED,				//klient zamknal polaczenie
	USR_DEAD_TIME,						//koniec zycia socketu
	USR_MOVE_SOCKET,					//nastepuje przesuniecie socketu
	USR_SERVER_CLOSE,					//zamykamy serwery gry
	USR_MOVE_CLOSE,						//usuwamy socket ktory mial robionego move'a, jest juz zbedny
};


enum EULogicMode
{	
	ULR_ROOM_SECOND_INSTANCE,			//wykryta 2 instancja gracza w pokoju
	ULR_PLAYER_CONNECT,					//gracz sie podlaczyl
	ULR_PLAYER_DISCONNECT,				//gracz sie odlaczyl					
};

enum EResChange
{
	RESID_ADD,
	RESID_DELETE,
};

enum EDatabase
{
	EDB_SERVERS,
};

enum EStatsAction
{
	STAT_PRIVATECHAT,
	STAT_ALL,
};

enum EUIStats
{
	EUIStatTabsOpened,
	EUIStatLast,
};

enum EGameStateAccessLevel
{
	EGS_UNKNOWN,
	EGS_FRIEND,
	EGS_PLAYER,
	EGS_SYSTEM
};

enum EEffectiveGroupStatus
{
	EEGS_READY,
	EEGS_BUSY,
	EEGS_STOPPED
};

enum EGroupType
{
	EGT_NONE,
	EGT_LOCATION,
	EGT_GAMESTATE,
	EGT_LOBBY,
	EGT_LEADERBOARD,
	EGT_LAST
};

/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

/***************************************/
/***************************************/
//DEFINES
/***************************************/
/***************************************/
#define lend		"\r\n"

#define K1			0x400			/*!< \def K1 - definiuje 1kb pamieci*/
#define K2			0x800			/*!< \def K2 - definiuje 2kb pamieci*/
#define K4			0x1000			/*!< \def K4 - definiuje 4kb pamieci*/
#define K8			0x2000			/*!< \def K8 - definiuje 8kb pamieci*/
#define K16			0x4000			/*!< \def K16 - definiuje 16kb pamieci*/
#define K32			0x8000			/*!< \def K32 - definiuje 32kb pamieci*/
#define K64			0x10000			/*!< \def K64 - definiuje 64kb pamieci*/
#define K128		0x20000			/*!< \def K128 - definiuje 128kb pamieci*/
#define K256		0x40000			/*!< \def K256 - definiuje 256kb pamieci*/
#define K512		0x80000			/*!< \def K512 - definiuje 512kb pamieci*/
#define K1024		0x100000		/*!< \def K1024 - definiuje 1024kb pamieci*/
#define KCOUNT		11

//max liczba elementow akceptowanych przez accept'a na wejsciu na serwer
#define MAX_SERVER_ACCEPT		100			


#define MAX_SERVER_THREADS					16
#define DEFAULT_GAMESTATE_WRITE_DELAY		180
#define DEFAULT_GAMESTATE_CACHE_PERIOD		300
#define DEFAULT_MAX_SCRIPT_OPERATIONS		1000000
#define DEFAULT_REPORT_SCRIPT_ERROR_INTERVAL	60
#define MIN_GUEST_GAMESTATE_ID				1000000000

#define GROUP_HEARTBEAT_INTERVAL_SECONDS				60

// Ponizsza wartosc oznacza max. czas po jakim wszystkie nodebalancery odczytaja na nowo informacje o grupach z bazy danych.
// Nie powinna byc wieksza niz GROUP_HEARTBEAT_INTERVAL_SECONDS,
// bo wtedy nodebalancery czesciej beda trzymaly przeterminowane dane o grupach
// i byc moze przydzielaly lokacje do martwych grup.
#define NODEBALANCER_GETGROUPS_INTERVAL_SECONDS				GROUP_HEARTBEAT_INTERVAL_SECONDS

///////////////////////////////////////////////////////////////////////////
///konfiguracja sieci
///////////////////////////////////////////////////////////////////////////
#define NET_DUP_START			1025			/*!< poczatkowy socket stosu*/	
#define NET_DUP_LIMIT			10000			/*!< liczba alokowanych elementow stosu*/
#define NET_ACCEPT_LIMIT		100				/*!< maksymalna liczba elementow akceptowanych w jednym kroku*/
#define NET_POLL_SERVER_LIMIT	1000			/*!< maksymalna ilosc elementow przetwarzanych przez poll serwera*/
#define NET_POLL_ROOM_LIMIT		10000			/*!< maksymalna ilosc elementow przetwarzanych przez poll pokoju*/
#define NET_WRITE_ROOM_LIMIT	1000			/*!< maksymalna ilosc elementow przetwarzanych przez zapis pokoju*/
#define NET_CONSOLE_LIMIT		100				/*!< maksymalna ilosc otwartych konsol*/
#define NET_SOCKET_SIZE			8192			/*!< rozmiar socketu w kernelu*/
#define NET_TICKET_QUEUE_BUSY_LIMIT		50		/*!< po tylu ticketach w kolejce przestajemy przyjmowac graczy do gry*/
#define MEMORY_RESERVE			1024			/*!< ilosc alokowanych pozycji pod elementy pamieci w memory_managerze*/
#define MSG_COMPRESS_TRESHOLD	4096			//minimum size of message that is subject to compression, below that compression is not performed
	

///////////////////////////////////////////////////////////////////////////
///konfiguracja firewalla
///////////////////////////////////////////////////////////////////////////
#define FIREWALL_1_MINUTE_LIMIT		15			/*!< makxymalna liczba polaczen w ciagu 1 minuty*/
#define FIREWALL_5_MINUTES_LIMIT	50			/*!< makxymalna liczba polaczen w ciagu 5 minut*/
#define FIREWALL_15_MINUTES_LIMIT	75			/*!< makxymalna liczba polaczen w ciagu 15 minut*/
#define FIREWALL_60_MINUTES_LIMIT	150			/*!< makxymalna liczba polaczen w ciagu 60 minut*/


#define CACHE_LIMIT_60_SEC		60				/*!< elementy nie sa dokladane do cacha gdy cos tam lezy ponad 20 sek,*/
#define CACHE_LIMIT_USED_MEMORY	-1				/*!< zalokowano tyle a tyle pamieci w watku dla cacha*/

/***************************************/
/***************************************/
//FORMAT FUNCTIONS
/***************************************/
/***************************************/

#ifndef LINUX
#define PRINT			printf
#define SNWPRINTF		_snwprintf
#define WSWPRINTF		vswprintf
#define STRICMP			_stricmp
#define STRNICMP		_strnicmp
#define VSNPRINTF		_vsnprintf
#define SNPRINTF		_snprintf
#define STRUPR(x) _strupr(x)
#define STRLWR(x) _strlwr(x)

//#define WCSNCPY(x,y,z)	_wcsncpy(x,y,z)
//#define WCSLWR(x)		_wcslwr(x)
//#define WCSUPR(x)		_wcsupr(x)
//#define WCSNICMP		_wcsnicmp
//#define WCSICMP			_wcsicmp

#else
#define PRINT			global_system->LogInfo
#define SNWPRINTF		swprintf
#define WSWPRINTF		vswprintf
#define STRICMP			strcasecmp
#define STRNICMP		strncasecmp
#define VSNPRINTF		vsnprintf
#define SNPRINTF		snprintf
#define STRUPR(x) { char * s = (x); while(*s){ *s = toupper(*s); s++;}}
#define STRLWR(x) { char * s = (x); while(*s){ *s = tolower(*s); s++;}}
#endif


/***************************************/
/***************************************/
//SYSTEM
/***************************************/
/***************************************/
#ifndef LINUX
#define GETCURRENTTHREADID()	GetCurrentThreadId()
#define GET_SOCKET_ERROR()		WSAGetLastError()
#define ioctl					ioctlsocket
#define ATOI64					_atoi64
#define ATOI					atoi
#define ATOF					(float)atof
#define ZERO(x,y)				memset(x,0,y);
#pragma warning(push)
#pragma warning(disable:4005)
#define EWOULDBLOCK				WSAEWOULDBLOCK
#define ECONNRESET				WSAECONNRESET
#pragma warning(pop)
typedef int socklen_t;
#else
const int SOCKET_ERROR = -1;
#define GETCURRENTTHREADID()	getpid()
#define	ioctlsocket				ioctl
#define closesocket				::close
#define GET_SOCKET_ERROR()		errno
#define CONNECTION_RESET(x)		(((x) == ENETDOWN  || (x) == ENETRESET || (x) == ECONNABORTED || (x) == ECONNRESET || (x) == ESHUTDOWN) && ( (x)!=EWOULDBLOCK && (x)!=EINTR && (x)!= EAGAIN ))
#ifndef __TIMESTAMP__
    #define __TIMESTAMP__ __DATE__ " " __TIME__
#endif
#define ATOI64					atoll
#define ATOI					atol
#define ATOF					(float)atof
#endif

#define TIME_MAX_SOCKET_INACTIVITY			180000
#define TIME_MIN_SOCKET_INACTIVITY			5000
#define TIME_MAX_PING_IN					180000				//180 sekund 
#define TIME_WARNING_PING_IN				30000
#define TIME_MAX_PING_OUT					15000				//15 sekund 
#define TIME_MAX_CONNECTION					10000
#define TIME_MAX_CONNECTION_ESTABILISH		60000				//60 sekund 
#define TIME_MAX_SESSION_SQL				45					//w minutach
#define TIME_MAX_SESSION_HASH				45					//w minutach
#define TIME_DEAD							15000				//czas po jakim umiera socket w ms
#define TIME_MAX_RECONNECTION				60000				//max czas na zestawienie polaczenia po ktorym kasujemy polaczenie
#define TIME_MAX_QUERY_SERVER				31000				//max czas na udzielenie odpowiedzi
#define TIME_QUERY_INFO_TIMEOUT				15000
#define TIME_MAX_QUERY_CLIENT				30					//max czas na udzielenie odpowiedzi
#define TIME_MAX_QUERY_INFO_CLOSE_CLIENT	15
#define TIME_MAX_RESULTS_INFO_CLOSE_CLIENT	30
#define TIME_MAX_LAG						3000
#define TIME_PAUSE_FAST_START				15000
#define TIME_TABLE_PING_FREQUENCY			180000				//co ile testujemy pinga graczy przy stoliku
#define TIME_FORCE_IGMI_TABLE_FREQUENCY		5					//w minutach (co ile wymuszamy przeslanie informacji o stoliku do main lobby)
#define TIME_MASSUPDATE_FREQUENCY			1900				//co ile wysylamy do gracza "massupdate" (zbiorczo informacje o zmienionych obiektach)
#define MAX_HOSTNAME				128
#define MAX_SP_VERSION				128
#define MAX_LOGIN					256
#define MAX_NICK					256
#define MAX_SITE					65
#define MAX_SID						33
#define MAX_LANGUAGE				16
#define MAX_FLAG					3
#define MAX_LANGUAGE_DESCRIPTION	128
#define MAX_GID						33
#define MAX_STATUS					256			//max dlugosc opisu statusu
#define MAX_LOCATION				65			// max dlugosc nazwy lokacji

#define MAX_LANGUAGES				256

#define MIN_LEAGUE_ROOM_NUMBER		1000000
//Wide Character TimeStamp definition
#define WIDEN2(x) L ## x
#define WIDEN(x) WIDEN2(x)
#define __WTIMESTAMP__ WIDEN(__TIMESTAMP__)


#define MEMRESET(x)	memset(&x,0,sizeof(x))


#define GROUP_UNKNOWN				0
#define GAME_INSTANCE_UNKNOWN		0


#define DLG_END				128
#define DLG_INFO			(1)
#define	DLG_WARNING			(2)
#define DLG_ERROR			(4)
#define DLG_TIMEOUT			(8)
#define DLG_ERROR_VERSION	(16)
#define DLG_INFO_END		(DLG_INFO|DLG_END)
#define	DLG_WARNING_END		(DLG_WARNING|DLG_END)
#define DLG_ERROR_END		(DLG_ERROR|DLG_END)
#define DLG_TIMEOUT_END		(DLG_TIMEOUT|DLG_END)
#define DLG_ERROR_VERSION_END (DLG_ERROR_VERSION|DLG_END)

// Mozliwe wartosci pola 'data' w IGM_MESSAGE_DIALOG.
#define DLG_DATA_GAME_CHIPS_TOO_LOW		-1

#ifndef min
#define min(a, b) ((a) <= (b) ? (a) : (b))
#endif

#ifndef max
#define max(a, b) ((a) >= (b) ? (a) : (b))
#endif

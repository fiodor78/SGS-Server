/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

using namespace std;
/***************************************************************************/
/*!
\brief konfiguracja SQL'a, czyli pelen opis tego co trzeba podac aby go zainicjalizowac
*/
struct SSQLConfig
{
	SSQLConfig(){Reset();};
	void Reset()
	{
		type.clear();
		host.clear();
		user.clear();
		password.clear();
		port.clear();
		database.clear();
		initialized=false;
	}
	string type;						// "mysql"|"dynamodb"
	string host;
	string user;
	string password;
	string database;
	string port;
	bool initialized;
};
/***************************************************************************/
/*!
\brief konfiguracja enginu skryptowego
*/
struct SScriptingConfig
{
	SScriptingConfig(){Reset();};
	void Reset()
	{
		logic_type = "";
		raport_path.clear();
		logic_global_config_path.clear();
		write_delay=DEFAULT_GAMESTATE_WRITE_DELAY;
		cache_period=DEFAULT_GAMESTATE_CACHE_PERIOD;
		report_script_error_interval = DEFAULT_REPORT_SCRIPT_ERROR_INTERVAL;
		always_flush_logs=false;
		Lua.script_path.clear();
		Lua.max_script_operations=DEFAULT_MAX_SCRIPT_OPERATIONS;
		Cpp.lib_path.clear();
	}

	string				logic_type;
	string				raport_path;
	string				logic_global_config_path;
	unsigned int		write_delay;
	unsigned int		cache_period;
	unsigned int		report_script_error_interval;
	bool				always_flush_logs;

	struct
	{
		string			script_path;
		unsigned int	max_script_operations;
	} Lua;

	struct 
	{
		string			lib_path;
	} Cpp;
};
/***************************************************************************/
/*!
\brief klasa konfiguracji serwera, wypelniana danymi przed startem serwera, potem przekazywana do roznych 
klas w obrebie serwera
*/
class GServerConfig
{
public:
	GServerConfig(){Reset();};
	void Reset()
	{
		log_path.clear();
		vulgar_path.clear();
		entropy_path.clear();

		misc.threads = "";
		misc.user.clear();
		misc.group.clear();
		misc.raport_overflow=500;

		bindings_config.clear();
		sql_config.Reset();
		gamestatedb_config.Reset();
		scripting_config.Reset();

		net.dup=false;
		net.dup_limit=NET_DUP_LIMIT;
		net.accept_limit=NET_ACCEPT_LIMIT;
		net.poll_server_limit=NET_POLL_SERVER_LIMIT;
		net.poll_room_limit=NET_POLL_ROOM_LIMIT;
		net.console_limit=NET_CONSOLE_LIMIT;
		net.socket_size=NET_SOCKET_SIZE;
		net.ticket_queue_busy_limit = NET_TICKET_QUEUE_BUSY_LIMIT;

		firewall.use=true;
		firewall.use_limits=true;
		firewall.limit1=FIREWALL_1_MINUTE_LIMIT;
		firewall.limit5=FIREWALL_5_MINUTES_LIMIT;
		firewall.limit15=FIREWALL_15_MINUTES_LIMIT;
		firewall.limit60=FIREWALL_60_MINUTES_LIMIT;
	};

	string					log_path;
	string					vulgar_path;
	string					entropy_path;
	string					xml_path;

	struct SMisc
	{
		string				threads;				//ilosc watkow roboczych serwera (grupy)
		string				user;					//user - na linuxie zmieniamy uzytkownika na user'a
		string				group;					//grupa - na linuxie zmieniamy grupe na group
		int					raport_overflow;		//limit przepelnienia raportu w liniach na minute, jest wiecej - nie logujemy
	} misc;

	struct SNet
	{
		bool				dup;					//czy uzywaly duplikacji socketow
		unsigned int		dup_limit;				//limit dla duplikacji
		unsigned int		accept_limit;			//limit accept'ow w petli
		unsigned int		poll_server_limit;		//limit poll'a dla glownego serwera
		unsigned int		poll_room_limit;		//limit poll'a dla pokoju
		unsigned int		console_limit;			//limit ilosci odwartych konsol
		unsigned int		socket_size;			//rozmiar buforow socket'a
		unsigned int		ticket_queue_busy_limit;	//limit liczby ticketow w kolejce
	} net;

	struct SFireWall
	{
		bool				use;					//czy uzywamy firewall'a statycznego
		bool				use_limits;				//czy uzywamy firewall'a dynamiczego
		unsigned int		limit1;					//limit podpiec na minute
		unsigned int		limit5;					//limit podpiec na 5 minut
		unsigned int		limit15;				//limit podpiec na 15 minut
		unsigned int		limit60;				//limit podpiec na godzine
	} firewall;

	map<string, string>		bindings_config;
	SSQLConfig				sql_config;
	SSQLConfig				gamestatedb_config;
	SScriptingConfig		scripting_config;
};
/***************************************************************************/

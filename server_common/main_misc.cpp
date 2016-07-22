/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"
#include "headers_game.h"

#include "gmain_server.h"
#include <boost/program_options.hpp>


namespace po = boost::program_options;

boost::recursive_mutex	mtx_sql;
GSocketStack			stack_mgr;


#pragma warning( disable : 4189 )

GRaportManager	raport_manager_global;
MYSQL			mysql;

GTrueRNG		truerng;
GEntropyCache	entropy_cache;

//common variables end

GServerBase *					global_server=NULL;
GStatistics *					global_statistics=NULL;
GSystem *						global_system=NULL;
GSignals *						global_signals=NULL;
GSQLManager	*					global_sqlmanager=NULL;
GServerConfig *					global_serverconfig=NULL;
GServerConfig					server_config;
boost::mutex					mtx_global;
volatile bool					vol_global=0;
boost::mt19937					rng_alg;      
boost::uniform_int<>			rng_dist(-2147483647,2147483647);
boost::variate_generator<boost::mt19937, boost::uniform_int<> > rng(rng_alg, rng_dist);

boost::uniform_int<>			rng_dist_byte(0,255);
boost::variate_generator<boost::mt19937, boost::uniform_int<> > rng_byte(rng_alg, rng_dist_byte);

/***************************************************************************/

void Configure(int ac, char * av[])
{
	global_serverconfig=&server_config;

	//ten nawias jest potrzebny aby wywolac destruktor po
	//jest to konieczne aby uzyskac mozliwosc dostepu do pliku konfiguracji po starcie serwera
	//inaczej plik jest zablokowany do zapisu i odczytu
	{
		//konifuracja danych wejsciowych
		po::options_description po_generic("Generic");
		po_generic.add_options()
			("help,H", "produce help message")
			("console,C",po::value< string >(),"connect to exisiting server console");

		po::options_description po_sql("SQL");
		po_sql.add_options()
			("SQL.Host", po::value< string >()->default_value("localhost") ,"SQL Database host")
			("SQL.User", po::value< string >()->default_value("root") ,"SQL Database user")
			("SQL.Password", po::value< string >(),"SQL Database password")
			("SQL.Database", po::value< string >() ,"SQL Database database")
			("SQL.Port", po::value< string >()->default_value("3306") ,"SQL Database port");

		po::options_description po_gamestatedb("GamestateDB");
		po_gamestatedb.add_options()
			("GamestateDB.Type", po::value< string >()->default_value("mysql"), "GamestateDB type")
			("GamestateDB.Host", po::value< string >(), "GamestateDB host")
			("GamestateDB.User", po::value< string >(), "GamestateDB user")
			("GamestateDB.Password", po::value< string >(), "GamestateDB password")
			("GamestateDB.Database", po::value< string >(), "GamestateDB database")
			("GamestateDB.Port", po::value< string >(), "GamestateDB port");

		po::options_description po_raport("Raport");
		po_raport.add_options()
			("Raport.Location", po::value< string >() ,"Raport file location");

		po::options_description po_vulgar("Vulgar");
		po_vulgar.add_options()
			("Vulgar.Location", po::value< string >(), "Vulgar file location");

		po::options_description po_misc("Misc");
		po_misc.add_options()
			("Misc.Threads", po::value< string >(), "Number of threads to run")
			("Misc.User", po::value< string >() ,"Change user to")
			("Misc.Group", po::value< string >() ,"Change group to")
			("Misc.Game",po::value< string >()->default_value(game_name) ,"Game name")
			("Misc.RaportOverflow", po::value< int>()->default_value(500) ,"Max lines/minute")
			("Misc.Instance",po::value< unsigned int>()->default_value(game_instance) ,"Game instance");

		po::options_description po_bindings("Bindings");
		global_serverconfig->bindings_config["game"] = "";
		po_bindings.add_options()
			("Bindings.game", po::value< string >(), "game");

#define GSERVICE(x1,y1,x2,y2,SERVICE_NAME,u1) \
	if (global_serverconfig->bindings_config.find(SERVICE_NAME) == global_serverconfig->bindings_config.end()) \
	{ \
		global_serverconfig->bindings_config[SERVICE_NAME] = ""; \
		po_bindings.add_options() ("Bindings."SERVICE_NAME, po::value< string >(), SERVICE_NAME); \
	}; \

#include "tbl/gservices.tbl"
#undef GSERVICE

		po::options_description po_logic("Logic");
		po_logic.add_options()
			("Logic.Type",po::value<string>() ,"Logic type (lua|cpp)")
			("Logic.RaportPath",po::value<string>() ,"Path to directory where logic logs are stored")
			("Logic.GlobalConfigPath",po::value<string>() ,"Path to file containing logic config (usually JSON)")
			("Logic.GameStateWriteDelay", po::value<unsigned int>()->default_value(180) ,"Time after which changed data is written to database")
			("Logic.GameStateCachePeriod", po::value<unsigned int>()->default_value(300) ,"Time unused gamestate is kept in cache")
			("Logic.ReportScriptErrorInterval", po::value<unsigned int>()->default_value(60) ,"Min. interval between reporting logic errors.")
			("Logic.AlwaysFlushLogs", po::value<unsigned int>()->default_value(0) ,"Flush logs every time logging is performed (slow!!!!)")
			("Logic.EnableDebugging", po::value<unsigned int>()->default_value(0) ,"Enable debug functions of the server. Not recommended on production server.");

		po::options_description po_lua("Lua");
		po_lua.add_options()
			("Lua.ScriptPath",po::value<string>() ,"Path to directory with main.lua file")
			("Lua.MaxScriptOperations", po::value<unsigned int>()->default_value(1000000) ,"Maximum number of operations allowed in Lua call");

		po::options_description po_cpp("Cpp");
		po_cpp.add_options()
			("Cpp.LibPath",po::value<string>() ,"Path to library file where logic is stored");

		po::options_description po_net("Net");
		po_net.add_options()
			("Net.Dup", po::value< bool >()->default_value(false) ,"Duplicate sockets descriptors")
			("Net.DupLimit", po::value< unsigned int>()->default_value(NET_DUP_LIMIT) ,"Max duplicated socket descriptor id")
			("Net.AcceptLimit", po::value< unsigned int>()->default_value(NET_ACCEPT_LIMIT) ,"Max accepted connections in single loop")
			("Net.PollServerLimit", po::value< unsigned int>()->default_value(NET_POLL_SERVER_LIMIT) ,"Max connections in server thread")
			("Net.PollRoomLimit", po::value< unsigned int>()->default_value(NET_POLL_ROOM_LIMIT) ,"Max connections in room thread")
			("Net.ConsoleLimit", po::value< unsigned int>()->default_value(NET_CONSOLE_LIMIT) ,"Max consoles")
			("Net.SocketSize", po::value< unsigned int>()->default_value(NET_SOCKET_SIZE) ,"Socket size")
			("Net.TicketQueueBusyLimit", po::value< unsigned int>()->default_value(NET_TICKET_QUEUE_BUSY_LIMIT), "Min. number of tickets in tickets queue to consider server busy");

		po::options_description po_firewall("FireWall");
		po_firewall.add_options()
			("FireWall.Use", po::value< bool >()->default_value(true) ,"Use buildin Firewall")
			("FireWall.UseLimits", po::value< bool >()->default_value(true) ,"Use buildin Firewall limits")
			("FireWall.1Limit", po::value< unsigned int>()->default_value(FIREWALL_1_MINUTE_LIMIT) ,"Max number of connections per minute")
			("FireWall.5Limit", po::value< unsigned int>()->default_value(FIREWALL_5_MINUTES_LIMIT) ,"Max number of connections per 5 minutes")
			("FireWall.15Limit", po::value< unsigned int>()->default_value(FIREWALL_15_MINUTES_LIMIT) ,"Max number of connections per 15 minutes")
			("FireWall.60Limit", po::value< unsigned int>()->default_value(FIREWALL_60_MINUTES_LIMIT) ,"Max number of connections per 1 hour");


		po::options_description po_all("Allowed options");
		po::options_description po_config("Config options");
		po_config.add(po_sql).add(po_gamestatedb).add(po_raport).add(po_vulgar).add(po_misc).add(po_bindings).add(po_net).add(po_firewall).add(po_logic).add(po_lua).add(po_cpp);
		po_all.add(po_generic).add(po_sql).add(po_gamestatedb).add(po_raport).add(po_vulgar).add(po_misc).add(po_bindings).add(po_net).add(po_firewall).add(po_logic).add(po_lua).add(po_cpp);

		po::variables_map vm;

		//pobieramy dane z command_line
		try{
			po::store(po::parse_command_line(ac, av, po_all), vm);
		}
		catch(const po::invalid_command_line_syntax& a){
			PRINT(a.what());
			exit(1);
		}

		if(vm.count("console"))
		{
			GMainConsole main_console;
			if(main_console.Init())
			{
				main_console.Action(vm["console"].as<string>());
				main_console.Destroy();
			}
			exit(1);
		}



		PRINT("Reading Configuration...\n");
		//pobieramy dane z server.cfg
		ifstream str_local("server.cfg");
		if(str_local.is_open())
		{
			try{
#if BOOST_VERSION >= 104400
				po::store(po::parse_config_file(str_local, po_config, true), vm);
#else
				po::store(po::parse_config_file(str_local, po_config), vm);
#endif
			}
			catch(const po::unknown_option& a){
				PRINT(a.what());
				exit(1);
			}
			catch(const po::invalid_syntax& a){
				PRINT(a.what());
				exit(1);
			}
		}
		else
		{
			PRINT("File server.cfg not found.\n");
		}
		//pobieramy dane z GlobalConf/server.cfg
		ifstream str_global("../GlobalConf/server.cfg");
		if(str_global.is_open())
		{
			try{
#if BOOST_VERSION >= 104400
				po::store(po::parse_config_file(str_global, po_config, true), vm);
#else
				po::store(po::parse_config_file(str_global, po_config), vm);
#endif
			}
			catch(const po::unknown_option& a){
				PRINT(a.what());
				exit(1);
			}
			catch(const po::invalid_syntax& a){
				PRINT(a.what());
				exit(1);
			}
		}
		else
		{
			PRINT("File ..//GlobalConf//server.cfg not found.\n");
		}



		po::notify(vm);    
		if (vm.count("help")) 
		{
			cout << po_all << "\n";
			exit(1);
		}

		global_serverconfig->sql_config.type = "mysql";
		if(vm.count("SQL.Host")) global_serverconfig->sql_config.host=vm["SQL.Host"].as<string>();
		if(vm.count("SQL.User")) global_serverconfig->sql_config.user=vm["SQL.User"].as<string>();
		if(vm.count("SQL.Password")) global_serverconfig->sql_config.password=vm["SQL.Password"].as<string>();
		if(vm.count("SQL.Database")) global_serverconfig->sql_config.database=vm["SQL.Database"].as<string>();
		if(vm.count("SQL.Port")) global_serverconfig->sql_config.port=vm["SQL.Port"].as<string>();

		if(vm.count("GamestateDB.Type")) global_serverconfig->gamestatedb_config.type=vm["GamestateDB.Type"].as<string>();
		if(vm.count("GamestateDB.Host")) global_serverconfig->gamestatedb_config.host=vm["GamestateDB.Host"].as<string>();
		if(vm.count("GamestateDB.Port")) global_serverconfig->gamestatedb_config.port=vm["GamestateDB.Port"].as<string>();
		if(vm.count("GamestateDB.User")) global_serverconfig->gamestatedb_config.user=vm["GamestateDB.User"].as<string>();
		if(vm.count("GamestateDB.Password")) global_serverconfig->gamestatedb_config.password=vm["GamestateDB.Password"].as<string>();
		if(vm.count("GamestateDB.Database")) global_serverconfig->gamestatedb_config.database=vm["GamestateDB.Database"].as<string>();
		if (global_serverconfig->gamestatedb_config.type == "mysql")
		{
			if (!vm.count("GamestateDB.Host"))
			{
				global_serverconfig->gamestatedb_config.host = global_serverconfig->sql_config.host;
				global_serverconfig->gamestatedb_config.port = global_serverconfig->sql_config.port;
				global_serverconfig->gamestatedb_config.user = global_serverconfig->sql_config.user;
				global_serverconfig->gamestatedb_config.password = global_serverconfig->sql_config.password;
				if (!vm.count("GamestateDB.Database"))
				{
					global_serverconfig->gamestatedb_config.database = global_serverconfig->sql_config.database;
				}
			}
		}

		if(vm.count("Raport.Location")) global_serverconfig->log_path=vm["Raport.Location"].as<string>();
		if(vm.count("Vulgar.Location")) global_serverconfig->vulgar_path = vm["Vulgar.Location"].as<string>();

		if(vm.count("Misc.Threads")) global_serverconfig->misc.threads=vm["Misc.Threads"].as<string>();
		if(vm.count("Misc.User")) global_serverconfig->misc.user=vm["Misc.User"].as<string>();
		if(vm.count("Misc.Group")) global_serverconfig->misc.group=vm["Misc.Group"].as<string>();
		if(vm.count("Misc.Game")) game_name=vm["Misc.Game"].as<string>();
		if(vm.count("Misc.RaportOverflow")) global_serverconfig->misc.raport_overflow=vm["Misc.RaportOverflow"].as<int>();
		if(vm.count("Misc.Instance")) game_instance=vm["Misc.Instance"].as<unsigned int>();

		{
			map<string, string>::iterator itb;
			for (itb = global_serverconfig->bindings_config.begin(); itb != global_serverconfig->bindings_config.end(); itb++)
			{
				string option_name = "Bindings." + itb->first;
				if (vm.count(option_name))
				{
					global_serverconfig->bindings_config[itb->first] = vm[option_name].as<string>();
				}
			}
		}

		if(vm.count("Net.Dup")) global_serverconfig->net.dup=vm["Net.Dup"].as<bool>();
		if(vm.count("Net.DupLimit")) global_serverconfig->net.dup_limit=vm["Net.DupLimit"].as<unsigned int>();
		if(vm.count("Net.AcceptLimit")) global_serverconfig->net.accept_limit=vm["Net.AcceptLimit"].as<unsigned int>();
		if(vm.count("Net.PollServerLimit")) global_serverconfig->net.poll_server_limit=vm["Net.PollServerLimit"].as<unsigned int>();
		if(vm.count("Net.PollRoomLimit")) global_serverconfig->net.poll_room_limit=vm["Net.PollRoomLimit"].as<unsigned int>();
		if(vm.count("Net.ConsoleLimit")) global_serverconfig->net.console_limit=vm["Net.ConsoleLimit"].as<unsigned int>();
		if(vm.count("Net.SocketSize")) global_serverconfig->net.socket_size=vm["Net.SocketSize"].as<unsigned int>();
		if(vm.count("Net.TicketQueueBusyLimit")) global_serverconfig->net.ticket_queue_busy_limit=vm["Net.TicketQueueBusyLimit"].as<unsigned int>();

		if(vm.count("FireWall.Use")) global_serverconfig->firewall.use=vm["FireWall.Use"].as<bool>();
		if(vm.count("FireWall.UseLimits")) global_serverconfig->firewall.use_limits=vm["FireWall.UseLimits"].as<bool>();
		if(vm.count("FireWall.1Limit")) global_serverconfig->firewall.limit1=vm["FireWall.1Limit"].as<unsigned int>();
		if(vm.count("FireWall.5Limit")) global_serverconfig->firewall.limit5=vm["FireWall.5Limit"].as<unsigned int>();
		if(vm.count("FireWall.15Limit")) global_serverconfig->firewall.limit15=vm["FireWall.15Limit"].as<unsigned int>();
		if(vm.count("FireWall.60Limit")) global_serverconfig->firewall.limit60=vm["FireWall.60Limit"].as<unsigned int>();

		if(vm.count("Logic.Type")) global_serverconfig->scripting_config.logic_type=vm["Logic.Type"].as<string>();
		if(vm.count("Logic.RaportPath")) global_serverconfig->scripting_config.raport_path=vm["Logic.RaportPath"].as<string>();
		if(vm.count("Logic.GlobalConfigPath")) global_serverconfig->scripting_config.logic_global_config_path=vm["Logic.GlobalConfigPath"].as<string>();
		if(vm.count("Logic.GameStateWriteDelay")) global_serverconfig->scripting_config.write_delay=vm["Logic.GameStateWriteDelay"].as<unsigned int>();
		if(vm.count("Logic.GameStateCachePeriod")) global_serverconfig->scripting_config.cache_period=vm["Logic.GameStateCachePeriod"].as<unsigned int>();
		if(vm.count("Logic.ReportScriptErrorInterval")) global_serverconfig->scripting_config.report_script_error_interval=vm["Logic.ReportScriptErrorInterval"].as<unsigned int>();
		if(vm.count("Logic.AlwaysFlushLogs")) global_serverconfig->scripting_config.always_flush_logs=(vm["Logic.AlwaysFlushLogs"].as<unsigned int>())!=0;

		if(vm.count("Lua.ScriptPath")) global_serverconfig->scripting_config.Lua.script_path=vm["Lua.ScriptPath"].as<string>();
		if(vm.count("Lua.MaxScriptOperations")) global_serverconfig->scripting_config.Lua.max_script_operations=vm["Lua.MaxScriptOperations"].as<unsigned int>();

		if(vm.count("Cpp.LibPath")) global_serverconfig->scripting_config.Cpp.lib_path=vm["Cpp.LibPath"].as<string>();
	}
	PRINT("%s Server: Game Instance %d, Version %s, Build: %s\n",game_name.c_str(),game_instance,game_version,__TIMESTAMP__);
	PRINT("Copyright by Ganymede Sp. z o.o.\n");
	PRINT("Krakow, Poland 2011\n");
	PRINT("\n");

	PRINT("SQL.Host: %s\n",global_serverconfig->sql_config.host.c_str());
	PRINT("SQL.User: %s\n",global_serverconfig->sql_config.user.c_str());
	PRINT("SQL.Password: %s\n",global_serverconfig->sql_config.password.c_str());
	PRINT("SQL.Database: %s\n",global_serverconfig->sql_config.database.c_str());
	PRINT("SQL.Port: %s\n",global_serverconfig->sql_config.port.c_str());
	PRINT("GamestateDB.Type: %s\n",global_serverconfig->gamestatedb_config.type.c_str());
	PRINT("GamestateDB.Host: %s\n",global_serverconfig->gamestatedb_config.host.c_str());
	PRINT("GamestateDB.User: %s\n",global_serverconfig->gamestatedb_config.user.c_str());
	PRINT("GamestateDB.Password: %s\n",global_serverconfig->gamestatedb_config.password.c_str());
	PRINT("GamestateDB.Database: %s\n",global_serverconfig->gamestatedb_config.database.c_str());
	PRINT("GamestateDB.Port: %s\n",global_serverconfig->gamestatedb_config.port.c_str());
	PRINT("Raport.Location: %s\n",global_serverconfig->log_path.c_str());
	PRINT("Vulgar.Location: %s\n", global_serverconfig->vulgar_path.c_str());
	PRINT("Misc.Threads: %s\n",global_serverconfig->misc.threads.c_str());
	PRINT("Misc.User: %s\n",global_serverconfig->misc.user.c_str());
	PRINT("Misc.Group: %s\n",global_serverconfig->misc.group.c_str());
	PRINT("Misc.Game: %s\n", game_name.c_str());
	PRINT("Misc.RaportOverflow: %s\n",global_serverconfig->misc.raport_overflow?"On":"Off");
	PRINT("Misc.Instance: %d\n",game_instance);
	{
		map<string, string>::iterator itb;
		for (itb = global_serverconfig->bindings_config.begin(); itb != global_serverconfig->bindings_config.end(); itb++)
		{
			if (itb->second != "")
			{
				PRINT("Bindings.%s: %s\n", itb->first.c_str(), itb->second.c_str());
			}
		}
	}
	PRINT("Net.Dup: %s\n",global_serverconfig->net.dup?"On":"Off");
	PRINT("Net.DupLimit: %d\n",global_serverconfig->net.dup_limit);
	PRINT("Net.AcceptLimit: %d\n",global_serverconfig->net.accept_limit);
	PRINT("Net.PollServerLimit: %d\n",global_serverconfig->net.poll_server_limit);
	PRINT("Net.PollRoomLimit: %d\n",global_serverconfig->net.poll_room_limit);
	PRINT("Net.ConsoleLimit: %d\n",global_serverconfig->net.console_limit);
	PRINT("Net.TicketQueueBusyLimit: %d\n",global_serverconfig->net.ticket_queue_busy_limit);
	PRINT("FireWall.Use: %d\n",global_serverconfig->firewall.use);
	PRINT("FireWall.UseLimits: %d\n",global_serverconfig->firewall.use_limits);
	PRINT("FireWall.1Limit: %d\n",global_serverconfig->firewall.limit1);
	PRINT("FireWall.5Limit: %d\n",global_serverconfig->firewall.limit5);
	PRINT("FireWall.15Limit: %d\n",global_serverconfig->firewall.limit15);
	PRINT("FireWall.60Limit: %d\n",global_serverconfig->firewall.limit60);
	PRINT("Logic.Type %s\n",global_serverconfig->scripting_config.logic_type.c_str());
	PRINT("Logic.RaportPath %s\n",global_serverconfig->scripting_config.raport_path.c_str());
	PRINT("Logic.GlobalConfigPath %s\n",global_serverconfig->scripting_config.logic_global_config_path.c_str());
	PRINT("Logic.GameStateWriteDelay %d\n",global_serverconfig->scripting_config.write_delay);
	PRINT("Logic.GameStateCachePeriod %d\n",global_serverconfig->scripting_config.cache_period);
	PRINT("Logic.ReportScriptErrorInterval %d\n",global_serverconfig->scripting_config.report_script_error_interval);
	PRINT("Logic.AlwaysFlushLogs %d\n",global_serverconfig->scripting_config.always_flush_logs ? 1 : 0);
	PRINT("Lua.ScriptPath %s\n",global_serverconfig->scripting_config.Lua.script_path.c_str());
	PRINT("Lua.MaxScriptOperations %d\n",global_serverconfig->scripting_config.Lua.max_script_operations);
	PRINT("Cpp.LibPath %s\n",global_serverconfig->scripting_config.Cpp.lib_path.c_str());
}
/***************************************************************************/

#define BOOST_SPIRIT_THREADSAFE
using namespace boost::spirit;
struct version_gr : public grammar<version_gr>
{
	version_gr(vector<int>& ver_)
		: ver(ver_){}
	template <typename ScannerT>
	struct definition
	{
		definition(version_gr const& self)  
		{ 
			val_r = limit_d(0u,1000u)[uint_p[push_back_a(self.ver)]];
			expression=val_r>>".">>val_r>>".">>val_r>>".">>val_r;
		}
		rule<ScannerT> val_r,expression;
		rule<ScannerT> const& start() const { return expression; }
	};
	vector<int>& ver;
};

void build_version()
{
	string version=game_version;
	vector<int> ver1;
	version_gr gr1(ver1);
	bool ret=parse(version.c_str(),gr1,space_p).full;
	if(ret)
	{
		//budujemy prawidlowy nr wersji aplikacji
		int v1=ver1[0];int v2=ver1[1];int v3=ver1[2];int v4=ver1[3];
		game_version_number=(v1<<24)|(v2<<16)|(v3<<8)|v4;
	}
}
/***************************************************************************/

int main(int ac, char* av[])
{
	build_version();
	GMainServer main_server;
	Configure(ac,av);

	if(main_server.Init()) 
	{	
		PRINT("Basic Level Initialized. All other initialization data can be found in file %s\n",raport_manager_global.GetFileName());
		main_server.RAPORT_LINE
		main_server.RAPORT("%s Server: Version %s, Build: %s",game_name.c_str(),game_version,__TIMESTAMP__);
		main_server.RAPORT("Copyright by Ganymede Technologies s.c.");
		main_server.RAPORT("Krakow, Poland 2005");
		main_server.RAPORT_LINE
		main_server.Action();
	}
	main_server.RAPORT("Basic Level Destruction");
	main_server.Destroy();
	return 0;
}
/***************************************************************************/



/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

/**	
\brief przechowuje bazowe informacje o soketach nasluchu, nic poza tym z nimi nie robi
*/
struct SSockService
{
	SSockService(EServiceTypes type_internal_p, EServiceTypes type_console_p, EServiceTypes type_game_p = ESTUnknown)
	{
		type_internal = type_internal_p;
		type_console = type_console_p;
		type_game = type_game_p;
		Reset();
	};
	~SSockService()
	{
	}
	void Reset()
	{
		host_internal.clear();
		port_internal = 0;
		socket_internal = 0;
		addr_internal = 0;

		host_console.clear();
		port_console = 0;
		socket_console = 0;
		addr_console = 0;

		host_game.clear();
		ports_game_str.clear();
		ports_game.clear();
		sockets_game.clear();
		addr_game = 0;

		initialized = false;
	}

	SSockService& operator=(const SSockService &ss)
	{
		host_internal = ss.host_internal;
		port_internal = ss.port_internal;
		socket_internal = ss.socket_internal;
		type_internal = ss.type_internal;
		addr_internal = ss.addr_internal;

		host_console = ss.host_console;
		port_console = ss.port_console;
		socket_console = ss.socket_console;
		type_console = ss.type_console;
		addr_console = ss.addr_console;

		host_game = ss.host_game;
		ports_game_str = ss.ports_game_str;
		ports_game = ss.ports_game;
		sockets_game = ss.sockets_game;
		type_game = ss.type_game;
		addr_game = ss.addr_game;

		initialized = ss.initialized;
		return *this;
	}

	// Polaczenie 'internal', miedzy serwerami gier i serwisami (grupa <-> grupa, grupa <-> serwis, serwis <-> serwis).
	string						host_internal;
	USHORT16					port_internal;
	SOCKET						socket_internal;
	EServiceTypes				type_internal;
	DWORD32						addr_internal;

	// Polaczenie do konsoli.
	string						host_console;
	USHORT16					port_console;
	SOCKET						socket_console;
	EServiceTypes				type_console;
	DWORD32						addr_console;

	// Polaczenia od klientow - uzywane przez serwery gier.
	string						host_game;
	string						ports_game_str;					// oryginalny string bez tokenizacji
	vector<USHORT16>			ports_game;
	vector<SOCKET>				sockets_game;
	EServiceTypes				type_game;
	DWORD32						addr_game;

	bool						initialized;					// czy udalo sie zbindowac pod wszystkie zadane porty
};
/***************************************************************************/
class SockService_isType
{
public:
	SockService_isType(const EServiceTypes t){ECT=t;};
	bool operator()(SSockService & c){return c.type_internal == ECT;};
private:
	EServiceTypes ECT;
};
/***************************************************************************/
class SockService_isNotInitialized
{
public:
	SockService_isNotInitialized(){};
	bool operator()(SSockService & c){return c.port_internal == 0;};
private:
};
/***************************************************************************/
class isCommand
{
public:
	isCommand(const int t){command=t;};
	bool operator()(int c){return c==command;};
private:
	int command;
};
/***************************************************************************/
/**	
\brief zawiera konfiguracje socketow nasluchu
*/
struct SServiceManager
{
	SServiceManager()
	{
		Reset();
	};
	~SServiceManager()
	{

	}
	void Reset()
	{
		for_each (services.begin(), services.end(),boost::bind(&SSockService::Reset,_1));
	};
	vector<SSockService> services;
};

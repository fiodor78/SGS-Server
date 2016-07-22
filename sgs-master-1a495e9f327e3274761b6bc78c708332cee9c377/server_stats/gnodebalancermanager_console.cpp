/***************************************************************************
***************************************************************************
(c) 1999-2006 Ganymede Technologies                 All Rights Reserved
Krakow, Poland                                  www.ganymede.eu
***************************************************************************
***************************************************************************/

#include "headers_all.h"
#include "gconsole.h"
#include "gserver_base.h"
#include "gserver.h"
#include "gtickets.h"
#include "gserver_local.h"
#include "gmsg_internal.h"
#include "utils_conversion.h"

extern GServerBase * global_server;
#define GSERVER ((GServerStats*)global_server)

using namespace boost::spirit;

extern char * syntax_error;
extern char * ready;
extern char * unknown_command;
extern char * end;

#define READY   socket->MemoryOut()<<ready
#define SYNTAX_ERROR socket->MemoryOut()<<syntax_error
#define UNKNOWN_COMMAND socket->MemoryOut()<<unknown_command;
#define END socket->MemoryOut()<<end;
#define SOUT socket->MemoryOut()

/***************************************************************************/

struct commands_nodebalancer_view: symbols<int>
{
	commands_nodebalancer_view(){add("info", CMD_NODEBALANCER_INFO);}
} commands_nodebalancer_view_p;

struct nodebalancer_gr : public grammar<nodebalancer_gr>
{
	nodebalancer_gr(unsigned& cmd_, map<string, string>& options_, pair<string, string>& p_)
		: cmd(cmd_),options(options_),p(p_){}
	template <typename ScannerT>
	struct definition
	{
		definition(nodebalancer_gr const& self) :
		keyword_p("0-9a-zA-Z_"),
		keyword_d("a-zA-Z0-9_")
		{
			view_name = keyword_d[commands_nodebalancer_view_p[assign_a(self.cmd)]];
			option_name = lexeme_d[ + (anychar_p - '=' - ' ') ];
			option_value = longest_d[lexeme_d[ + (anychar_p - '=' - ' ') ][assign_a(self.p.second)] | (lexeme_d[ '"' >> ( * (anychar_p - '"'))[assign_a(self.p.second)] >> '"'])];
			exp1 = keyword_p("nodebalancer") >> view_name >> *(( option_name[assign_a(self.p.first)] >> '=' >> option_value )[insert_at_a(self.options, self.p.first, self.p.second)]);
			expression=*(exp1);
		}
		distinct_parser<> keyword_p;
		distinct_directive<> keyword_d;
		rule<ScannerT> view_name, option_name, option_value, exp1, expression;
		rule<ScannerT> const& start() const { return expression; }
	};
	unsigned& cmd;
	map<string, string>& options;
	pair<string, string>& p;
};
/***************************************************************************/
GRaportInterface &  GConsoleNodeBalancer::GetRaportInterface()
{
	GNodeBalancerManager * server_nodebalancer = static_cast<GNodeBalancerManager*>(server);
	return server_nodebalancer->raport_interface_local;
}
/***************************************************************************/
void GConsoleNodeBalancer::ConsoleNodeBalancer(GSocketConsole *socket,char * buf)
{
	//GNodeBalancerManager * server_nodebalancer = static_cast<GNodeBalancerManager*>(server);

	PrepareBuffer(socket,buf);

	unsigned command = CMD_NODEBALANCER_NONE;
	map<string, string> options;
	pair<string, string> tmp_pair;
	nodebalancer_gr gr(command,options,tmp_pair);

	strstream s(SOUT.End(),SOUT.Free(),ios_base::out);

	bool ret=parse(buf,gr,space_p).full;
	if(ret==false) SYNTAX_ERROR;
	else
	{
		// Nazwy opcji maja byc case insensitive
		map<string, string>::iterator it;
		char temp_opt[128], *p;
		for (it = options.begin(); it != options.end(); it++)
		{
			strncpy(temp_opt, it->first.c_str(), sizeof(temp_opt));
			temp_opt[sizeof(temp_opt) - 1] = 0;
			for (p = temp_opt; *p != 0; p++)
				*p = (char)tolower(*p);
			options[temp_opt] = it->second;
		}

		switch(command)
		{
		case CMD_NODEBALANCER_INFO:
			{
				s << "INFO\r\n";
			}
			break;
		}
		SOUT.IncreaseUsed(s.pcount());
	}
}
/***************************************************************************/
struct commands_getgamestateaddress: symbols<int>
{
	commands_getgamestateaddress(){add("group",ESTClientBase)("console",ESTClientBaseConsole)("client",ESTClientBaseGame);}
} commands_getgamestateaddress_p;
struct getgamestateaddress_gr : public grammar<getgamestateaddress_gr>
{
	getgamestateaddress_gr(unsigned & cmd_, int & gamestate_id_, int & connection_type_)
		: cmd(cmd_), gamestate_id(gamestate_id_), connection_type(connection_type_){}
	template <typename ScannerT>
	struct definition
	{
		definition(getgamestateaddress_gr const& self) :
		keyword_p("0-9a-zA-Z_"),
		keyword_d("a-zA-Z0-9_")
		{ 
			self.cmd = CMD_NODEBALANCER_GETGAMESTATEADDRESS;
			val_r = limit_d(0u,2100000000u)[uint_p[assign_a(self.gamestate_id)]];
			exp1 = keyword_p("getgamestateaddress") >> val_r >> keyword_d[commands_getgamestateaddress_p[assign_a(self.connection_type)]];
			expression = *(exp1);
		}
		distinct_parser<> keyword_p;
		distinct_directive<> keyword_d;
		rule<ScannerT> exp1, expression, val_r;
		rule<ScannerT> const& start() const { return expression; }
	};
	unsigned & cmd;
	int & gamestate_id;
	int & connection_type;
};
/***************************************************************************/
struct commands_getlocationaddress: symbols<int>
{
	commands_getlocationaddress(){add("group",ESTClientBase)("console",ESTClientBaseConsole)("client",ESTClientBaseGame);}
} commands_getlocationaddress_p;
struct getlocationaddress_gr : public grammar<getlocationaddress_gr>
{
	getlocationaddress_gr(unsigned & cmd_, string & location_id_, int & connection_type_)
		: cmd(cmd_), location_id(location_id_), connection_type(connection_type_){}
	template <typename ScannerT>
	struct definition
	{
		definition(getlocationaddress_gr const& self) :
		keyword_p("0-9a-zA-Z_"),
		keyword_d("a-zA-Z0-9_")
		{ 
			self.cmd = CMD_NODEBALANCER_GETLOCATIONADDRESS;
			val_r = (+(anychar_p - commands_getlocationaddress_p))[assign_a(self.location_id)];
			exp1 = keyword_p("getlocationaddress") >> val_r >> keyword_d[commands_getlocationaddress_p[assign_a(self.connection_type)]];
			expression = *(exp1);
		}
		distinct_parser<> keyword_p;
		distinct_directive<> keyword_d;
		rule<ScannerT> exp1, expression, val_r;
		rule<ScannerT> const& start() const { return expression; }
	};
	unsigned & cmd;
	string & location_id;
	int & connection_type;
};
/***************************************************************************/
void GConsoleNodeBalancer::ConsoleNodeBalancerGetGamestateAddress(GSocketConsole *socket,char * buf)
{
	GNodeBalancerManager * server_nodebalancer = static_cast<GNodeBalancerManager*>(server);
	PrepareBuffer(socket,buf);

	unsigned command = CMD_NODEBALANCER_NONE;
	INT32 gamestate_ID = 0;
	INT32 connection_type;
	getgamestateaddress_gr gr(command, gamestate_ID, connection_type);

	strstream s(SOUT.End(),SOUT.Free(),ios_base::out);

	bool ret=parse(buf,gr,space_p).full;
	if(ret==false) SYNTAX_ERROR;
	else
	{
		switch(command)
		{
		case CMD_NODEBALANCER_GETGAMESTATEADDRESS:
			{
				string host, ports;
				INT32 group_id;
				string location_ID = str(boost::format("gamestate.%1%") % gamestate_ID);
				if (!server_nodebalancer->GetLocationAddress(location_ID, (EServiceTypes)connection_type, host, ports, group_id))
				{
					server_nodebalancer->RL("GetLocationAddress('%s', '%s') failed - getgamestateaddress",
						location_ID.c_str(),
						(connection_type == ESTClientBase) ? "group" : (connection_type == ESTClientBaseConsole) ? "console" : "client");
					s << "ERROR\r\n";
				}
				else
				{
					s << "OK " << host << " " << ports << " " << group_id << "\r\n";
				}
				break;
			}
			break;
		}
		SOUT.IncreaseUsed(s.pcount());
	}
}
/***************************************************************************/
void GConsoleNodeBalancer::ConsoleNodeBalancerGetLocationAddress(GSocketConsole *socket,char * buf)
{
	GNodeBalancerManager * server_nodebalancer = static_cast<GNodeBalancerManager*>(server);
	PrepareBuffer(socket,buf);

	unsigned command = CMD_NODEBALANCER_NONE;
	string location_ID = "";
	INT32 connection_type;
	getlocationaddress_gr gr(command, location_ID, connection_type);

	strstream s(SOUT.End(),SOUT.Free(),ios_base::out);

	bool ret=parse(buf,gr,space_p).full;
	if(ret==false) SYNTAX_ERROR;
	else
	{
		switch(command)
		{
		case CMD_NODEBALANCER_GETLOCATIONADDRESS:
			{
				string host, ports;
				INT32 group_id;
				if (!server_nodebalancer->GetLocationAddress(location_ID, (EServiceTypes)connection_type, host, ports, group_id))
				{
					server_nodebalancer->RL("GetLocationAddress('%s', '%s') failed - getlocationaddress",
						location_ID.c_str(),
						(connection_type == ESTClientBase) ? "group" : (connection_type == ESTClientBaseConsole) ? "console" : "client");
					s << "ERROR\r\n";
				}
				else
				{
					s << "OK " << host << " " << ports << " " << group_id << "\r\n";
				}
				break;
			}
			break;
		}
		SOUT.IncreaseUsed(s.pcount());
	}
}
/***************************************************************************/

#undef READY
#undef SYNTAX_ERROR
#undef SOUT
#undef END

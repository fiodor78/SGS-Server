/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#pragma warning (disable:4512)

#include "headers_all.h"
#include "headers_game.h"
#include "utils_conversion.h"
#undef rest				
#pragma warning (disable:4512)
#pragma warning (disable: 4996)

extern GServerBase * global_server;

char * syntax_error="Syntax Error\r\n";
char * ready="ready\r\n";
char * unknown_command="unknown command\r\n";
char * end="end\r\n";
#define READY	socket->MemoryOut()<<ready
#define SYNTAX_ERROR socket->MemoryOut()<<syntax_error
#define UNKNOWN_COMMAND socket->MemoryOut()<<unknown_command;
#define END socket->MemoryOut()<<end;
#define SOUT socket->MemoryOut()

void StreamSQLStats(strstream &s, int output_type);
/***************************************************************************/
using namespace boost::spirit;
//distinct_parser<> keyword_p("0-9a-zA-Z_");
//distinct_directive<> keyword_d("a-zA-Z0-9_");












/***************************************************************************/
/***************************************************************************/
void GConsoleBasic::InitConsole()
{
#define GCONSOLE(command, function,description)		console_commands.insert(make_pair(string(#command).erase(0,8),boost::bind(&GConsoleBasic::function,this,_1,_2)));
#include "tbl/gconsole_basic.tbl"
#undef GCONSOLE

#define GCONSOLE(command, function,description)		console_descriptions.push_back(make_pair(string(#command).erase(0,8),description));
#include "tbl/gconsole_basic.tbl"
#undef GCONSOLE
}
/***************************************************************************/
GRaportInterface & GConsoleBasic::GetRaportInterface()
{
	return server->raport_interface;
}
/***************************************************************************/
void GConsoleBasic::ExecuteConsoleCommand(GSocketConsole * socket, string & cmd)
{
	GRaportInterface & raport_interface_local=GetRaportInterface();

	boost::char_separator<char> sep;
	typedef boost::tokenizer<boost::char_separator<char> > TTokenizer;
	TTokenizer tok(cmd,sep);
	TTokenizer::iterator beg=tok.begin(); 
	if(beg!=tok.end())
	{
		CONSOLE_FUNCTION f;
		string key=*beg;
		boost::to_upper(key);

		{
			if(console_commands[key]!=console_commands.end())
			{
				f=console_commands[key];
				try{
					if(socket->ReallocateOutTo(K16))
					{
						char buffer[4096];
						strncpy(buffer, cmd.c_str(), sizeof(buffer));
						buffer[sizeof(buffer) - 1] = 0;
						if(f.empty()) 
						{
							SRAP(WARNING_CONSOLE_UNKNOWN_COMMAND);
							RL("%s %s",GSTR(WARNING_CONSOLE_UNKNOWN_COMMAND),key.c_str());
							ConsoleUnknownCommand(socket);
						}
						else 
						{
							SRAP(INFO_CONSOLE_COMMAND);
							f(socket,buffer);
							//pomijamy to co idzie automatycznie do RRD, aby nie bylo syfu w logach
							if (!(strcmp(buffer, "stats") == 0 ||
									strcmp(buffer, "memory") == 0 ||
									strcmp(buffer, "services") == 0))
							{
								// Najczesciej pojawiajace sie komendy logujemy tylko w trybie verbose
								if (strncmp(buffer, "player update ", 14) == 0 ||
									strncmp(buffer, "cache friends reload ", 21) == 0 ||
									strncmp(buffer, "social notify events=", 21) == 0)
								{
									RLVERBOSE("%s %s", GSTR(INFO_CONSOLE_COMMAND), buffer);
								}
								else
								{
									RL("%s %s", GSTR(INFO_CONSOLE_COMMAND), buffer);
								}
							}
						}

						if (socket->awaiting_console_tasks_responses_count == 0)
						{
							strstream s(SOUT.End(),SOUT.Free(),ios_base::out);
							s<<"---END---"<<lend;
							SOUT.IncreaseUsed(s.pcount());
						}
						socket->TestWrite();
					}
					else
					{
						SRAP(ERROR_CONSOLE_MEMORY_ACCOCATION);
						RL(GSTR(ERROR_CONSOLE_MEMORY_ACCOCATION));
						socket->Flags().set(ESockExit);
					}
				}
				catch(const boost::bad_function_call &)
				{
					SRAP(ERROR_CONSOLE_FUNCTION);
					RL(GSTR(ERROR_CONSOLE_FUNCTION));
				}
			}
		}
	};
	for(beg=tok.begin(); beg!=tok.end();++beg)
	{
		string key=*beg;
		boost::to_upper(key);
		if(key==string("EXIT"))
			socket->Flags().set(ESockExit);
	}
}
/***************************************************************************/
void GConsoleBasic::ParseConsole(GSocketConsole * socket)
{
	string cmd;
	cmd.assign(socket->MemoryIn().Start(),0,(DWORD)socket->MemoryIn().Used());

	if (socket->awaiting_console_tasks_responses_count > 0)
	{
		socket->pending_console_commands.push(cmd);
	}
	else
	{
		ExecuteConsoleCommand(socket, cmd);
	}

	socket->MemoryIn().IncreaseParsed(socket->MemoryIn().Used());
	socket->MemoryIn().RemoveParsed();
}
/***************************************************************************/
void GConsoleBasic::ConsoleProcessInfo(GSocketConsole * socket,char * buf)
{
	PrepareBuffer(socket,buf);

	strstream s(SOUT.End(),SOUT.Free(),ios_base::out);
	s<<game_name<<" Server: Version "<<game_version<<", Build: "<<__TIMESTAMP__<<lend;
	ToTime current(global_server->CurrentSTime());
	ToTime start(global_server->StartSTime());
	boost::posix_time::time_duration	work_time=boost::posix_time::from_time_t(global_server->CurrentSTime())-boost::posix_time::from_time_t(global_server->StartSTime());
	ToTimeHour working(work_time);
	s<<"Server started at "<<start<<", Working "<<working<<lend;
	boost::recursive_mutex::scoped_lock lock(mtx_sql);
	s<<"----------------------------------"<<lend;
	s<<"SQL Client Info:"<<lend;
	s<<mysql_get_client_info()<<lend;
	s<<"----------------------------------"<<lend;
	s<<"SQL Client Version:"<<lend;
	s<<mysql_get_client_version()<<lend;
	s<<"----------------------------------"<<lend;
	s<<"SQL Host Info:"<<lend;
	s<<mysql_get_host_info(&mysql)<<lend;
	s<<"----------------------------------"<<lend;
	s<<"SQL Proto Info:"<<lend;
	s<<mysql_get_proto_info(&mysql)<<lend;
	s<<"----------------------------------"<<lend;
	s<<"SQL Server Info:"<<lend;
	s<<mysql_get_server_info(&mysql)<<lend;
	s<<"----------------------------------"<<lend;
	s<<"SQL Server Version:"<<lend;
	s<<mysql_get_server_version(&mysql)<<lend;
	s<<"----------------------------------"<<lend;
	s<<"SQL Stats:"<<lend;
	s<<mysql_stat(&mysql)<<lend;
	s<<"----------------------------------"<<lend;
	s<<"SQL Error State:"<<lend;
	s<<mysql_sqlstate(&mysql)<<lend;

	SOUT.IncreaseUsed(s.pcount());
};
/***************************************************************************/
void GConsoleBasic::ConsoleProcessExit(GSocketConsole * socket,char * buf)
{
	PrepareBuffer(socket,buf);

socket->Flags().set(ESockExit);
};
/***************************************************************************/
void GConsoleBasic::ConsoleProcessQuit(GSocketConsole * socket,char * buf)
{
	PrepareBuffer(socket,buf);

socket->Flags().set(ESockExit);
};
/***************************************************************************/
struct commands_help_1: symbols<int>
{
	commands_help_1(){add("help commands",CMD_HELP_COMMANDS);}
} commands_help_1_p;
struct commands_help_2: symbols<int>
{
	commands_help_2()
	{
		add("help command",CMD_HELP_COMMAND);
		add("help",CMD_HELP_COMMAND);
	}
} commands_help_2_p;
struct help : public grammar<help>
{
	help(unsigned& r_,vector<int>& vi_,string& s_,unsigned& m_)
		: r(r_),vi(vi_),s(s_),m(m_){}

	template <typename ScannerT>
	struct definition
	{
		definition(help const& self) :
		keyword_d("a-zA-Z0-9_")
		{ 
			unsigned m=self.m;
			cmd_r = limit_d(1u,m)[uint_p[push_back_a(self.vi)]];
			cmd_s_r = (*alpha_p)[assign_a(self.s)];
			cmd1_r = keyword_d[commands_help_1_p[assign_a(self.r)]];
			cmd2_r = keyword_d[commands_help_2_p[assign_a(self.r)]]>>(cmd_r|cmd_s_r);
			expression= *(cmd1_r|cmd2_r);
		}
		distinct_directive<> keyword_d;
		rule<ScannerT> cmd_r,cmd_s_r,cmd1_r,cmd2_r,expression;
		rule<ScannerT> const& start() const { return expression; }
	};
	vector<int>& vi;
	unsigned& r;
	unsigned& m;
	string & s;
};
void GConsoleBasic::ConsoleProcessHelp(GSocketConsole * socket,char * buf)
{
	PrepareBuffer(socket,buf);

	vector<int> cmd;
	unsigned command=CMD_HELP_NONE;
	string cmd_s;
	unsigned size=(unsigned)console_descriptions.size();
	help gr(command,cmd,cmd_s,size);


	bool ret=parse(buf,gr,space_p).full;
	if(ret==false) SYNTAX_ERROR;
	else
	{
		if(command==CMD_HELP_COMMAND)
			if(cmd_s.size()==0 && cmd.size()==0)
			{
				command=CMD_HELP_NONE;
			}

			strstream s(SOUT.End(),SOUT.Free(),ios_base::out);
			switch(command)
			{
			case CMD_HELP_NONE:
				{
					int a=0;
					vector< pair<string,string> >::iterator pos;
					for (pos=console_descriptions.begin();pos!=console_descriptions.end();++pos)
					{
						s<<"---------------------------------------------\r\n"<<a+1<<". Command: "<<(*pos).first<<lend<<(*pos).second<<lend;
						a++;
					}
				}
				break;
			case CMD_HELP_COMMANDS:
				{
					int a=0;
					vector< pair<string,string> >::iterator pos;
					for (pos=console_descriptions.begin();pos!=console_descriptions.end();++pos)
					{
						s<<a+1<<". "<<(*pos).first<<lend;
						a++;
					}
				}
				break;
			case CMD_HELP_COMMAND:
				{
					if(cmd.size())
					{
						int a=cmd[0];
						a--;
						s<<a+1<<". Command: "<<console_descriptions[a].first<<lend<<console_descriptions[a].second<<lend;
					}
					else
					{
						int a=0;
						vector< pair<string,string> >::iterator pos;
						for (pos=console_descriptions.begin();pos!=console_descriptions.end();++pos)
						{
							if(STRICMP(console_descriptions[a].first.c_str(),cmd_s.c_str())==0)
							{
								s<<a+1<<". Command: "<<console_descriptions[a].first<<lend<<console_descriptions[a].second<<lend;
								break;
							}
							a++;
						}
						if(pos==console_descriptions.end())
						{
							UNKNOWN_COMMAND;
						}
					}
				}
				break;
			}
			SOUT.IncreaseUsed(s.pcount());
	}
};
/***************************************************************************/
void GConsoleBasic::ConsoleUnknownCommand(GSocketConsole * socket)
{
	SOUT<<"Unknown command\r\n";
};
/***************************************************************************/
void GConsoleBasic::PrepareBuffer(GSocketConsole *socket, char * buffer)
{
	int l = strlen(buffer);
	while(l>0 && (buffer[l-1]==13 || buffer[l-1]==10))
	{
		l--;
		buffer[l]=0;
	}
}
/***************************************************************************/
























/***************************************************************************/
void GConsole::InitConsole()
{
#define GCONSOLE(command, function,description)		console_commands.insert(make_pair(string(#command).erase(0,8),boost::bind(&GConsole::function,this,_1,_2)));
#ifdef SERVICES
#include "tbl/gconsole_services.tbl"
#else
#include "tbl/gconsole.tbl"
#endif
#undef GCONSOLE

#define GCONSOLE(command, function,description)		console_descriptions.push_back(make_pair(string(#command).erase(0,8),description));
#ifdef SERVICES
#include "tbl/gconsole_services.tbl"
#else
#include "tbl/gconsole.tbl"
#endif
#undef GCONSOLE
}
/***************************************************************************/
void GConsole::ConsoleProcessMemory(GSocketConsole * socket,char * buf)
{
	map<string, string> dummy_options;
	PrepareBuffer(socket,buf);

	char temp_string[32];
	sprintf(temp_string, "%d", socket->socketid);
	dummy_options["socketid"] = temp_string;

	socket->ReallocateOutTo(K32);
	strstream s(SOUT.End(),SOUT.Free(),ios_base::out);
	socket->awaiting_console_tasks_responses_count = global_server->CallAction(s, CMD_ACTION_MEMORY_INFO, dummy_options);
	SOUT.IncreaseUsed(s.pcount());
};
/***************************************************************************/
struct commands: symbols<int>
{
	commands()
	{
		add
			("warning"  , SWARNING)
			("info"		, SINFO)
			("error"	, SERROR)
			("used"		, SUSED)
			("all"		, SALL)
			("socket"	, SSOCKET)
			("server"	, SSERVER)
			("logic"	, SLOGIC)
			("console"	, SCONSOLE)
			("data"		, SDATA)
			("poll"		, SPOLL)
			("firewall"	, SFIREWALL)
			("net"		, SNET)
			("internal"	, SINTERNAL)
			("process"	, SPROCESS)
			("access"	, SACCESS)
			;
	}
} commands_p;
struct stats : public grammar<stats>
{
	stats(vector<int>& vc_)
		: vc(vc_){}

		template <typename ScannerT>
		struct definition
		{
			definition(stats const& self) :
			keyword_d("a-zA-Z0-9_")
			{ 
				expression = str_p("stats")>> *(keyword_d[commands_p[push_back_a(self.vc)]]);
			}
			distinct_directive<> keyword_d;
			rule<ScannerT> expression;
			rule<ScannerT> const& start() const { return expression; }
		};
		vector<int>& vc;
};
/***************************************************************************/
void GConsole::ConsoleProcessStats(GSocketConsole * socket,char * buf)
{
	PrepareBuffer(socket,buf);

	vector<int> commands;
	stats gr(commands);

	bool ret=parse(buf,gr,space_p).full;

	if(ret==false) SYNTAX_ERROR;
	else
	{
		socket->ReallocateOutTo(K32);
		strstream s(SOUT.End(),SOUT.Free(),ios_base::out);
		global_statistics->Stream(s,commands);
		SOUT.IncreaseUsed(s.pcount());
	}
};
/***************************************************************************/
struct commands_sqlstats: symbols<int>
{
	commands_sqlstats()
	{
		add("sqlstats csv",CMD_SQLSTATS_CSV);
		add("sqlstats slowest",CMD_SQLSTATS_SLOWEST);
		add("sqlstats",CMD_SQLSTATS_TXT);
	}
} commands_sqlstats_p;

struct sqlstats : public grammar<sqlstats>
{
	sqlstats(int& cmd_)
		: cmd(cmd_){}

		template <typename ScannerT>
		struct definition
		{
			definition(sqlstats const& self) :
			keyword_d("a-zA-Z0-9_")
			{ 
				expression = keyword_d[commands_sqlstats_p[assign_a(self.cmd)]];
			}
			distinct_directive<> keyword_d;
			rule<ScannerT> expression;
			rule<ScannerT> const& start() const { return expression; }
		};
		int& cmd;
};
/***************************************************************************/
void GConsole::ConsoleProcessSQLStats(GSocketConsole * socket,char * buf)
{
	PrepareBuffer(socket,buf);

	int command;
	sqlstats gr(command);
	bool ret=parse(buf,gr,space_p).full;

	if(ret==false) SYNTAX_ERROR;
	else
	{
		socket->ReallocateOutTo(K512);
		strstream s(SOUT.End(),SOUT.Free(),ios_base::out);
		StreamSQLStats(s, command);
		SOUT.IncreaseUsed(s.pcount());
	}
};
/***************************************************************************/
void GConsole::ConsoleProcessCoreDump(GSocketConsole * socket,char * buf)
{
	PrepareBuffer(socket,buf);

	socket->ReallocateOutTo(K4);
	strstream s(SOUT.End(),SOUT.Free(),ios_base::out);
	WriteCore(true);
	s << "Core dumped.\r\n";
	SOUT.IncreaseUsed(s.pcount());
};
/***************************************************************************/
struct commands_firewall: symbols<int>
{
	commands_firewall(){add("firewall",CMD_FIREWALL_SHOW);}
} commands_firewall_p;
struct commands_firewall_1: symbols<int>
{
	commands_firewall_1(){add("add",CMD_FIREWALL_ADD)("delete",CMD_FIREWALL_DELETE)("exist",CMD_FIREWALL_EXIST);}
} commands_firewall_1_p;
struct commands_firewall_2: symbols<int>
{
	commands_firewall_2(){add("size",CMD_FIREWALL_SIZE)("show",CMD_FIREWALL_SHOW)("limits",CMD_FIREWALL_LIMITS)("reset",CMD_FIREWALL_RESET)("reset static",CMD_FIREWALL_RESET_STATIC)("reset dynamic",CMD_FIREWALL_RESET_DYNAMIC)("load sql",CMD_FIREWALL_LOAD_SQL);}
} commands_firewall_2_p;
struct commands_firewall_3: symbols<int>
{
	commands_firewall_3(){add("set limit",CMD_FIREWALL_SET_LIMIT);}
} commands_firewall_3_p;
struct commands_firewall_4: symbols<int>
{
	commands_firewall_4(){add("1",1)("minute",1)("5",5)("5 minutes",5)("15",15)("quarter",15)("60",60)("hour",60);}
} commands_firewall_4_p;

struct firewalls : public grammar<firewalls>
{
	firewalls(unsigned& r_,vector<int>& vi_)
		: r(r_),vi(vi_){}
		template <typename ScannerT>
		struct definition
		{
			definition(firewalls const& self) :
			keyword_p("0-9a-zA-Z_"),
			keyword_d("a-zA-Z0-9_")
			{ 
				uint_parser<unsigned, 10, 1, 3> ip_p;
				ips = limit_d(0u,255u)[ip_p[push_back_a(self.vi)]];
				ip = ips>>'.'>>ips>>'.'>>ips>>'.'>>ips;
				cmd1_r = keyword_d[commands_firewall_1_p[assign_a(self.r)]]>>ip;
				cmd2_r = keyword_d[commands_firewall_2_p[assign_a(self.r)]];
				to_r = keyword_d[commands_firewall_4_p[push_back_a(self.vi)]];
				val_r = limit_d(0u,1000u)[uint_p[push_back_a(self.vi)]];
				cmd3_r = keyword_d[commands_firewall_3_p[assign_a(self.r)]]>>to_r>>keyword_p("to")>>val_r;
				exp1 = keyword_p("firewall")>> (cmd1_r|cmd2_r|cmd3_r);
				exp2 = keyword_d[commands_firewall_p[assign_a(self.r)]];
				expression=*(exp1|exp2);
			}
			distinct_parser<> keyword_p;
			distinct_directive<> keyword_d;
			rule<ScannerT> ip,ips,cmd1_r,cmd2_r,cmd3_r,to_r,val_r,exp1,exp2,expression;
			rule<ScannerT> const& start() const { return expression; }
		};
		vector<int>& vi;
		unsigned& r;
};
/***************************************************************************/
#ifndef SERVICES
void GConsole::ConsoleProcessFireWall(GSocketConsole * socket,char * buf)
{
#define FIRE GSERVER->firewall
	PrepareBuffer(socket,buf);

	vector<int> ip;
	unsigned command=CMD_FIREWALL_NONE;
	firewalls gr(command,ip);
	
	bool ret=parse(buf,gr,space_p).full;
	if(ret==false) SYNTAX_ERROR;
	else
	{
		in_addr addr;
		memset(&addr,0,4);
		if(ip.size()==4)
		{
			DWORD32 a=IPToAddr((BYTE)ip[0],(BYTE)ip[1],(BYTE)ip[2],(BYTE)ip[3]);
			memcpy(&addr,&a,4);
			//addr.S_un.S_un_b.s_b1=(u_char)ip[0];
			//addr.S_un.S_un_b.s_b2=(u_char)ip[1];
			//addr.S_un.S_un_b.s_b3=(u_char)ip[2];
			//addr.S_un.S_un_b.s_b4=(u_char)ip[3];
		}
		switch(command)
		{
		case CMD_FIREWALL_ADD:		FIRE.Add(addr);READY;break;
		case CMD_FIREWALL_DELETE:	FIRE.Delete(addr);READY;break;
		case CMD_FIREWALL_EXIST:	FIRE.Exist(addr)?SOUT<<"exist\r\n":SOUT<<"not exist\r\n";break;
		case CMD_FIREWALL_SIZE:		{
										char tmp[128];
										sprintf(tmp,"size: %d\r\n",FIRE.Size());
										SOUT<<tmp;
									};break;
		case CMD_FIREWALL_SHOW:		
									{
										socket->ReallocateOutTo(K32);
										strstream s(SOUT.End(),SOUT.Free(),ios_base::out);
										FIRE.Stream(s);
										SOUT.IncreaseUsed(s.pcount());
									}break;
		case CMD_FIREWALL_LIMITS:	
									{
										socket->ReallocateOutTo(K32);
										strstream s(SOUT.End(),SOUT.Free(),ios_base::out);
										FIRE.Limits(s);
										SOUT.IncreaseUsed(s.pcount());
									}
									break;
		case CMD_FIREWALL_RESET:	
		case CMD_FIREWALL_RESET_STATIC:	
		case CMD_FIREWALL_RESET_DYNAMIC:	
									FIRE.Reset((ECmdFireWall)command);READY;break;
		case CMD_FIREWALL_SET_LIMIT:
									FIRE.SetLimit(ip[0],ip[1]);READY;break;
		case CMD_FIREWALL_LOAD_SQL:	
									FIRE.GetFromSQL();READY;break;
		}
	}
#undef FIRE
};
#endif
/***************************************************************************/
#ifndef SERVICES

struct commands_service: symbols<int>
{
	commands_service()
	{
		add("services show",CMD_SERVICE_SHOW);
		add("services",CMD_SERVICE_SHOW);
	}
} commands_service_p;
struct commands_service_1: symbols<int>
{
	commands_service_1(){add("group",CMD_SERVICE_GROUP);}
} commands_service_1_p;
struct commands_service_2: symbols<int>
{
	commands_service_2(){add("name",CMD_SERVICE_NAME);}
} commands_service_2_p;
struct commands_service_3: symbols<int>
{
	commands_service_3(){add("address",CMD_SERVICE_ADDRESS);}
} commands_service_3_p;
struct commands_service_4: symbols<int>
{
	commands_service_4(){add("port",CMD_SERVICE_PORT);}
} commands_service_4_p;
struct commands_service_5: symbols<int>
{
	commands_service_5(){add("services reinit",CMD_SERVICE_REINIT);}
} commands_service_5_p;
struct commands_service_6: symbols<int>
{
	commands_service_6(){add("services ticketqueuelimit",CMD_SERVICE_TICKETQUEUELIMIT);}
} commands_service_6_p;
struct service : public grammar<service>
{
	service(vector<int>& cmd_,int& group_,string& name_,vector<int>& address_,int & port_)
		: cmd(cmd_),group(group_),name(name_),address(address_),port(port_){}
		template <typename ScannerT>
		struct definition
		{
			definition(service const& self) :
			keyword_d("a-zA-Z0-9_")
			{ 
				uint_parser<unsigned, 10, 1, 3> ip_p;
				ips = limit_d(0u,255u)[ip_p[push_back_a(self.address)]];
				ip = ips>>'.'>>ips>>'.'>>ips>>'.'>>ips;

				cmd1_r = keyword_d[commands_service_1_p[push_back_a(self.cmd)]]>>limit_d(0u,100u)[uint_p[assign_a(self.group)]];
				cmd2_r = keyword_d[commands_service_2_p[push_back_a(self.cmd)]]>>(*alpha_p)[assign_a(self.name)];
				cmd3_r = keyword_d[commands_service_3_p[push_back_a(self.cmd)]]>>ip;
				cmd4_r = keyword_d[commands_service_4_p[push_back_a(self.cmd)]]>>limit_d(0u,10000u)[uint_p[assign_a(self.port)]];
				exp1 = keyword_d[commands_service_p[push_back_a(self.cmd)]];
				exp2 = keyword_d[commands_service_5_p[push_back_a(self.cmd)]] >> *(cmd1_r|cmd2_r|cmd3_r|cmd4_r);
				exp3 = keyword_d[commands_service_6_p[push_back_a(self.cmd)]] >> (*alnum_p)[assign_a(self.name)];
				expression=*(exp3|exp2|exp1);
			}
			distinct_directive<> keyword_d;
			rule<ScannerT> ip,ips,cmd1_r,cmd2_r,cmd3_r,cmd4_r,exp1,exp2,exp3,expression;
			rule<ScannerT> const& start() const { return expression; }
		};
		vector<int>&	cmd;
		int&			group;
		string &		name;
		int&			port;
		vector<int>&	address;
};
/***************************************************************************/
void GConsole::ConsoleServices(GSocketConsole * socket,char * buf)
{
	PrepareBuffer(socket,buf);

	vector<int> cmd;
	int group=-1;
	string name;
	vector<int> address;
	int port=-1;
	service gr(cmd,group,name,address,port);

	bool ret=parse(buf,gr,space_p).full;
	if(ret==false) SYNTAX_ERROR;
	else
	{
		SSocketInternalReinit reinit;
		vector<int>::iterator pos;
		bool reinit_services=false;
		for(pos=cmd.begin(); pos!=cmd.end();++pos)
		{
			if(*pos==CMD_SERVICE_GROUP)
			{
				reinit.group=group;
				reinit_services=true;
			}
			if(*pos==CMD_SERVICE_NAME)
			{
				reinit.name=name;
				reinit_services=true;
			}
			if(*pos==CMD_SERVICE_ADDRESS)
			{
				reinit.addr.sin_addr.s_addr=IPToAddr((BYTE)address[0],(BYTE)address[1],(BYTE)address[2],(BYTE)address[3]);
				reinit_services=true;
			}
			if(*pos==CMD_SERVICE_PORT)
			{
				reinit.addr.sin_port=htons((USHORT16)port);
				reinit_services=true;
			}
			if(*pos==CMD_SERVICE_REINIT)
			{
				reinit_services=true;
			}
			if(*pos==CMD_SERVICE_SHOW)
			{
			}
			if (*pos == CMD_SERVICE_TICKETQUEUELIMIT)
			{
				socket->ReallocateOutTo(K4);
				strstream s(SOUT.End(),SOUT.Free(),ios_base::out);
				int new_limit = -1;
				if (name != "")
				{
					new_limit = ATOI(name.c_str());
					if (new_limit == 0 && name != "0")
					{
						new_limit = -1;
					}
					if (new_limit >= 0 && new_limit <= 10000)
					{
						global_serverconfig->net.ticket_queue_busy_limit = new_limit;
						s << "Ticket queue limit has been set to: " << global_serverconfig->net.ticket_queue_busy_limit << lend;
					}
					else
					{
						new_limit = -1;
					}
				}
				if (new_limit == -1)
				{
					s << "Current ticket queue limit: " << global_serverconfig->net.ticket_queue_busy_limit << lend;
				}
				SOUT.IncreaseUsed(s.pcount());
				return;
			}
		}

		if(reinit_services)
		{
			char temp_string[32];
			map<string, string> options;
			sprintf(temp_string, "%d", reinit.group);
			options["reinit_group"] = temp_string;
			options["reinit_name"] = reinit.name;
			sprintf(temp_string, "%d", (INT32)reinit.addr.sin_addr.s_addr);
			options["reinit_addr"] = temp_string;
			sprintf(temp_string, "%d", reinit.addr.sin_port);
			options["reinit_port"] = temp_string;

			sprintf(temp_string, "%d", socket->socketid);
			options["socketid"] = temp_string;

			socket->ReallocateOutTo(K32);
			strstream s(SOUT.End(),SOUT.Free(),ios_base::out);
			socket->awaiting_console_tasks_responses_count = global_server->CallAction(s, CMD_ACTION_SERVICE_REINIT, options);
			SOUT.IncreaseUsed(s.pcount());
		}
		else
		{
			map<string, string> dummy_options;
			char temp_string[32];
			sprintf(temp_string, "%d", socket->socketid);
			dummy_options["socketid"] = temp_string;

			socket->ReallocateOutTo(K32);
			strstream s(SOUT.End(),SOUT.Free(),ios_base::out);
			socket->awaiting_console_tasks_responses_count = global_server->CallAction(s, CMD_ACTION_SERVICE_INFO, dummy_options);
			SOUT.IncreaseUsed(s.pcount());
		}
	}
}
#endif
/***************************************************************************/
#ifndef SERVICES
struct commands_action: symbols<int>
{
	commands_action()
	{
		add("action reconnect stats", CMD_ACTION_RECONNECT_STATS);
	}
} commands_action_p;
struct actions : public grammar<actions>
{
	actions(unsigned& cmd_, map<string, string>& options_, pair<string, string>& p_)
		: cmd(cmd_),options(options_),p(p_){}
	template <typename ScannerT>
	struct definition
	{
		definition(actions const& self) :
		keyword_d("a-zA-Z0-9_")
		{ 
			cmd_r = keyword_d[commands_action_p[assign_a(self.cmd)]];
			option_name = lexeme_d[ + (anychar_p - '=' - ' ') ];
			option_value = longest_d[lexeme_d[ + (anychar_p - '=' - ' ') ][assign_a(self.p.second)] | (lexeme_d[ '"' >> ( * (anychar_p - '"'))[assign_a(self.p.second)] >> '"'])];
			exp1 = cmd_r >> *(( option_name[assign_a(self.p.first)] >> '=' >> option_value )[insert_at_a(self.options, self.p.first, self.p.second)]);
			expression=*(exp1);
		}
		distinct_directive<> keyword_d;
		rule<ScannerT> option_name, option_value, cmd_r, exp1, expression;
		rule<ScannerT> const& start() const { return expression; }
	};
	unsigned&		cmd;
	map<string, string>& options;
	pair<string, string>& p;
};
/***************************************************************************/
void GConsole::ConsoleAction(GSocketConsole * socket,char * buf)
{
	PrepareBuffer(socket,buf);

	unsigned cmd;
	map<string, string> options;
	pair<string, string> tmp_pair;
	actions a(cmd, options, tmp_pair);

	bool ret=parse(buf,a,space_p).full;
	if(ret==false) SYNTAX_ERROR;
	else
	{
		char temp_string[32];
		sprintf(temp_string, "%d", socket->socketid);
		options["socketid"] = temp_string;

		socket->ReallocateOutTo(K16);
		strstream s(SOUT.End(),SOUT.Free(),ios_base::out);
		socket->awaiting_console_tasks_responses_count = global_server->CallAction(s, (ECmdConsole)cmd, options);
		SOUT.IncreaseUsed(s.pcount());
	}
}
#endif
/***************************************************************************/
struct raport_gr : public grammar<raport_gr>
{
	raport_gr(map<string, string>& options_, pair<string, string>& p_)
		: options(options_),p(p_){}
	template <typename ScannerT>
	struct definition
	{
		definition(raport_gr const& self):
		keyword_p("0-9a-zA-Z_"),
		keyword_d("a-zA-Z0-9_")
		{
			option_name = lexeme_d[ + (anychar_p - '=' - ' ') ];
			option_value = longest_d[lexeme_d[ + (anychar_p - '=' - ' ') ][assign_a(self.p.second)] | (lexeme_d[ '"' >> ( * (anychar_p - '"'))[assign_a(self.p.second)] >> '"'])];
			exp1 = keyword_p("raport") >> *(( option_name[assign_a(self.p.first)] >> '=' >> option_value )[insert_at_a(self.options, self.p.first, self.p.second)]);
			expression=*(exp1);
		}
		distinct_parser<> keyword_p;
		distinct_directive<> keyword_d;
		rule<ScannerT> option_name, option_value, exp1, expression;
		rule<ScannerT> const& start() const { return expression; }
	};
	map<string, string>& options;
	pair<string, string>& p;
};
/***************************************************************************/
void GConsoleBasic::ConsoleProcessRaport(GSocketConsole * socket,char * buf)
{
	PrepareBuffer(socket,buf);

	map<string, string> options;
	pair<string, string> tmp_pair;
	raport_gr gr(options,tmp_pair);

	bool ret=parse(buf,gr,space_p).full;

	if(ret==false) SYNTAX_ERROR;
	else
	{
		socket->ReallocateOutTo(K32);
		strstream s(SOUT.End(),SOUT.Free(),ios_base::out);
		if (options["verbose"] == "on" || options["verbose"] == "1" || options["verbose"] == "true")
		{
			GetRaportInterface().SetVerboseMode(true);
			s << "Raport verbose mode is now ON." << lend;
		}
		if (options["verbose"] == "off" || options["verbose"] == "0" || options["verbose"] == "false")
		{
			GetRaportInterface().SetVerboseMode(false);
			s << "Raport verbose mode is now OFF." << lend;
		}
		SOUT.IncreaseUsed(s.pcount());
	}
};
/***************************************************************************/
#ifndef SERVICES
struct logic_commands: symbols<int>
{
	logic_commands()
	{
		add
			("reload"	, CMD_LOGIC_RELOAD)
			("stats"	, CMD_LOGIC_STATS)
			("clearstats",	CMD_LOGIC_CLEARSTATS)
			("message"	, CMD_LOGIC_MESSAGE)
			("global_message", CMD_LOGIC_GLOBAL_MESSAGE)
			;
	}
} logic_commands_p;

struct logic : public grammar<logic>
{
	logic(vector<int>& vc_,map<string, string>& options_, pair<string, string>& p_):
	vc(vc_),options(options_),p(p_)
	{}

	template <typename ScannerT>
	struct definition
	{
		definition(logic const& self) :
		keyword_d("a-zA-Z0-9_")
		{ 
			option_name = lexeme_d[ + (anychar_p - '=' - ' ') ];
			option_value = longest_d[lexeme_d[ + (anychar_p - '=' - ' ') ][assign_a(self.p.second)] | (lexeme_d[ '"' >> ( * (anychar_p - '"'))[assign_a(self.p.second)] >> '"'])];
			expression = str_p("logic") >>
						*(keyword_d[logic_commands_p[push_back_a(self.vc)]]) >>
						*(( option_name[assign_a(self.p.first)] >> '=' >> option_value )[insert_at_a(self.options, self.p.first, self.p.second)]);
		}
		distinct_directive<> keyword_d;
		rule<ScannerT> option_name, option_value;
		rule<ScannerT> expression;
		rule<ScannerT> const& start() const { return expression; }
	};

	vector<int>& vc;
	map<string, string>& options;
	pair<string, string>& p;
};
/***************************************************************************/
void GConsole::ConsoleProcessLogic(GSocketConsole * socket,char * buf)
{
	PrepareBuffer(socket,buf);

	vector<int> commands;
	map<string, string> options;
	pair<string, string> tmp_pair;
	logic gr(commands,options,tmp_pair);

	bool ret=parse(buf,gr,space_p).full;

	if(ret==false || (commands.size()==0)) SYNTAX_ERROR;
	else
	{
		bool dont_wait_for_command_execution = (ATOI(gr.options["nowait"].c_str()) == 1);
		char temp_string[32];
		sprintf(temp_string, "%d", dont_wait_for_command_execution ? 0 : socket->socketid);
		gr.options["socketid"] = temp_string;

		socket->ReallocateOutTo(K512);
		strstream s(SOUT.End(),SOUT.Free(),ios_base::out);

		socket->awaiting_console_tasks_responses_count = global_server->CallAction(s, (ECmdConsole)commands[0], gr.options);

		if (dont_wait_for_command_execution)
		{
			s << "---END---" << lend;
		}
		SOUT.IncreaseUsed(s.pcount());
	}
};
#endif
/***************************************************************************/
#undef READY	
#undef SYNTAX_ERROR 
#undef SOUT 
#undef END
/***************************************************************************/
bool GConsoleBasic::MySQL_Query_Raport(MYSQL * mysql, const char * query, char * qa_filename, int qa_line)
{
	return (server) ? server->MySQL_Query_Raport(mysql, query, qa_filename, qa_line) : false;
}
/***************************************************************************/

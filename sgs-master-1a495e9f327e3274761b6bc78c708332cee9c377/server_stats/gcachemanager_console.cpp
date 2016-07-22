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
//
using namespace boost::spirit;
//
extern char * syntax_error;
extern char * ready;
extern char * unknown_command;
extern char * end;
//
#define READY	socket->MemoryOut()<<ready
#define SYNTAX_ERROR socket->MemoryOut()<<syntax_error
#define UNKNOWN_COMMAND socket->MemoryOut()<<unknown_command;
#define END socket->MemoryOut()<<end;
#define SOUT socket->MemoryOut()

/***************************************************************************/
struct commands_cache_1: symbols<int>
{
	commands_cache_1(){add("clear all",CMD_CACHE_RESOURCES_CLEAR_ALL)("show",CMD_CACHE_RESOURCES_SHOW);}
} commands_cache_1_p;
struct commands_cache_2: symbols<int>
{
	commands_cache_2(){add("socialfriends reload",CMD_CACHE_RESOURCES_RELOAD)("socialfriends erase",CMD_CACHE_RESOURCES_ERASE);}
} commands_cache_2_p;

struct cache_resources_gr : public grammar<cache_resources_gr>
{
	cache_resources_gr(unsigned& cmd_,int& val_)
		: cmd(cmd_),val(val_){}
	template <typename ScannerT>
	struct definition
	{
		definition(cache_resources_gr const& self) :
		keyword_p("0-9a-zA-Z_"),
		keyword_d("a-zA-Z0-9_")
		{ 
			uint_parser<int, 10, 1, 32> id_p;
			cmd1_r = keyword_d[commands_cache_1_p[assign_a(self.cmd)]];
			cmd2_r = keyword_d[commands_cache_2_p[assign_a(self.cmd)]]>>id_p[assign_a(self.val)];

			exp1 = keyword_p("cache")>> (cmd1_r|cmd2_r);
			expression=*(exp1);
		}
		distinct_parser<> keyword_p;
		distinct_directive<> keyword_d;
		rule<ScannerT> cmd1_r,cmd2_r,exp1,expression;
		rule<ScannerT> const& start() const { return expression; }
	};
	unsigned& cmd;
	int& val;
};
/***************************************************************************/
void GConsoleCache::ConsoleCache(GSocketConsole *socket,char * buf)
{
	GCacheManager * server_cache=static_cast<GCacheManager*>(server);

	PrepareBuffer(socket,buf);

	int social_friends_list;

	social_friends_list = (int)server_cache->social_friends_date.size();

	int user_id=0;
	unsigned command=CMD_CACHE_RESOURCES_NONE;
	cache_resources_gr gr(command, user_id);

	strstream s(SOUT.End(),SOUT.Free(),ios_base::out);

	bool ret=parse(buf,gr,space_p).full;
	if(ret==false) SYNTAX_ERROR;
	else
	{
		switch(command)
		{
		case CMD_CACHE_RESOURCES_SHOW:
			{
				s << "Social friends cache size: " << social_friends_list << " elements\r\n";
			}
			break;
		case CMD_CACHE_RESOURCES_CLEAR_ALL:
			{
				server_cache->social_friends_list.clear();
				server_cache->social_friends_date.clear();
#ifdef LINUX
				server_cache->social_friends_date.resize(0);
#endif
				s<<"Social friends cache size: "<<0<<"/"<<0<<" elements\r\n";
			}
			break;
		case CMD_CACHE_RESOURCES_RELOAD:
			server_cache->ReloadSocialFriends(user_id);
			READY;
			break;
		case CMD_CACHE_RESOURCES_ERASE:
			server_cache->EraseSocialFriends(user_id);
			READY;
			break;
		}
		SOUT.IncreaseUsed(s.pcount());
	}
}
/***************************************************************************/
GRaportInterface &	GConsoleCache::GetRaportInterface()
{
	GCacheManager * server_cache=static_cast<GCacheManager*>(server);
	return server_cache->raport_interface_local;
}
/***************************************************************************/
#undef READY	
#undef SYNTAX_ERROR 
#undef SOUT 
#undef END

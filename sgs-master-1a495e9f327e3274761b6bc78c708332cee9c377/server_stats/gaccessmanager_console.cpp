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
//extern distinct_parser<> keyword_p;
//extern distinct_directive<> keyword_d;

extern char * syntax_error;
extern char * ready;
extern char * unknown_command;
extern char * end;

#define READY	socket->MemoryOut()<<ready
#define SYNTAX_ERROR socket->MemoryOut()<<syntax_error
#define UNKNOWN_COMMAND socket->MemoryOut()<<unknown_command;
#define END socket->MemoryOut()<<end;
#define SOUT socket->MemoryOut()

/***************************************************************************/
void GConsoleAccess::ConsoleAccessProcessMemory(GSocketConsole * socket,char *buf)
{
    map<string, string> dummy_options;
	GAccessManager * server_access=static_cast<GAccessManager*>(server);
	PrepareBuffer(socket,buf);

	socket->ReallocateOutTo(K32);
	strstream s(SOUT.End(),SOUT.Free(),ios_base::out);
	server_access->CallAction(s, CMD_ACTION_MEMORY_INFO, dummy_options);
	SOUT.IncreaseUsed(s.pcount());
};
/***************************************************************************/
GRaportInterface &	GConsoleAccess::GetRaportInterface()
{
	GServerManager * server_access=static_cast<GServerManager*>(server);
	return server_access->raport_interface_local;
}
/***************************************************************************/
#undef READY	
#undef SYNTAX_ERROR 
#undef SOUT 
#undef END

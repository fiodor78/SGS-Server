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

struct commands_cenzor_view : symbols<int>
{
	commands_cenzor_view(){ add("info", CMD_CENZOR_INFO); }
} commands_cenzor_view_p;

struct cenzor_gr : public grammar<cenzor_gr>
{
	cenzor_gr(unsigned& cmd_, map<string, string>& options_, pair<string, string>& p_)
	: cmd(cmd_), options(options_), p(p_){}
	template <typename ScannerT>
	struct definition
	{
		definition(cenzor_gr const& self) :
		keyword_p("0-9a-zA-Z_"),
		keyword_d("a-zA-Z0-9_")
		{
			view_name = keyword_d[commands_cenzor_view_p[assign_a(self.cmd)]];
			option_name = lexeme_d[+(anychar_p - '=' - ' ')];
			option_value = longest_d[lexeme_d[+(anychar_p - '=' - ' ')][assign_a(self.p.second)] | (lexeme_d['"' >> (*(anychar_p - '"'))[assign_a(self.p.second)] >> '"'])];
			exp1 = keyword_p("cenzor") >> view_name >> *((option_name[assign_a(self.p.first)] >> '=' >> option_value)[insert_at_a(self.options, self.p.first, self.p.second)]);
			expression = *(exp1);
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
GRaportInterface &  GConsoleCenzor::GetRaportInterface()
{
	GCenzorManager * server_cenzor = static_cast<GCenzorManager*>(server);
	return server_cenzor->raport_interface_local;
}
/***************************************************************************/
void GConsoleCenzor::ConsoleCenzor(GSocketConsole *socket, char * buf)
{
	PrepareBuffer(socket, buf);

	unsigned command = CMD_CENZOR_NONE;
	map<string, string> options;
	pair<string, string> tmp_pair;
	cenzor_gr gr(command, options, tmp_pair);

	strstream s(SOUT.End(), SOUT.Free(), ios_base::out);

	bool ret = parse(buf, gr, space_p).full;
	if (ret == false) SYNTAX_ERROR;
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
				*p = (char) tolower(*p);
			options[temp_opt] = it->second;
		}

		switch (command)
		{
			case CMD_CENZOR_INFO:
			{
				s << "INFO\r\n" << "Im alive ... \r\n";
			}
			break;
		}
		SOUT.IncreaseUsed(s.pcount());
	}
}

#undef READY
#undef SYNTAX_ERROR
#undef SOUT
#undef END

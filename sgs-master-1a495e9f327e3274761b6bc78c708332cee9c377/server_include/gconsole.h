/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

class GServerBase;
typedef boost::function<void (GSocketConsole*,char * buf)> CONSOLE_FUNCTION;
/*!
\brief klasa odpowiadajaca za przetwarzanie danych w konsoli, parsuje dane, przetwarza, odsyla
*/
class GConsoleBasic
{
public:
	//! konstruktor
	GConsoleBasic(){server=NULL;InitConsole();};
	//! destruktor
	virtual ~GConsoleBasic(){console_commands.clear();};
	//! inicjalizuje konsole
	virtual void		InitConsole();
	//! parsuje przychodzace dane
	void				ParseConsole(GSocketConsole * socket);
	void				ExecuteConsoleCommand(GSocketConsole * socket, string & cmd);
	//! wolane gdy podalismy nieznan¹ komendê
	void				ConsoleUnknownCommand(GSocketConsole*);
	void				SetServer(GServerBase * s){server=s;};
	void				PrepareBuffer(GSocketConsole * socket,char * buffer);
public:
#undef GCONSOLE
#define GCONSOLE(command, function,description)		void function(GSocketConsole *,char * buffer);
#include "tbl/gconsole_basic.tbl"
#undef GCONSOLE
protected:
	map<string,CONSOLE_FUNCTION >	console_commands;
	vector< pair<string,string> >	console_descriptions;
	GServerBase *		server;
	virtual GRaportInterface &	GetRaportInterface();
	bool				MySQL_Query_Raport(MYSQL * mysql, const char * query, char * qa_filename, int qa_line);
};
/***************************************************************************/
class GConsole: public GConsoleBasic
{
public:
	GConsole():GConsoleBasic(){InitConsole();};
	//! inicjalizuje konsole
	virtual void		InitConsole();
public:
#undef GCONSOLE
#define GCONSOLE(command, function,description)		void function(GSocketConsole *,char * buffer);
#ifdef SERVICES
#include "tbl/gconsole_services.tbl"
#else
#include "tbl/gconsole.tbl"
#endif
#undef GCONSOLE
private:
};
/***************************************************************************/

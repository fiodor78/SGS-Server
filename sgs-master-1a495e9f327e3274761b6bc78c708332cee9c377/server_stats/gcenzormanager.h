/***************************************************************************
***************************************************************************
(c) 1999-2006 Ganymede Technologies                 All Rights Reserved
Krakow, Poland                                  www.ganymede.eu
***************************************************************************
***************************************************************************/
#include "gcenzor_adv.h"

/***************************************************************************/
class GConsoleCenzor : public GConsoleBasic
{
public:
	GConsoleCenzor() : GConsoleBasic(){ InitConsole(); };

	//! inicjalizuje konsole
	void InitConsole()
	{
#define GCONSOLE(command, function, description)		console_commands.insert(make_pair(string(#command).erase(0,8),boost::bind(&GConsoleCenzor::function,this,_1,_2)));
#include "tbl/gconsole_cenzor.tbl"
#undef GCONSOLE
#define GCONSOLE(command, function, description)		console_descriptions.push_back(make_pair(string(#command).erase(0,8),description));
#include "tbl/gconsole_cenzor.tbl"
#undef GCONSOLE
	}
#undef GCONSOLE
#define GCONSOLE(command, function, description)		void function(GSocketConsole *,char * buffer);
#include "tbl/gconsole_cenzor.tbl"
#undef GCONSOLE
	GRaportInterface &	GetRaportInterface();
};
/***************************************************************************/
class GCenzorManager : public GServerManager
{
	friend class GConsoleCenzor;

public:
	GCenzorManager();
	bool			Init();
	void			InitServices()
	{
		GServerManager::InitServices();
		service_manager.services.push_back(SSockService(ESTAcceptCenzor, ESTAcceptCenzorConsole));
	}
	
	bool 			ProcessInternalMessage(GSocket *, GNetMsg &, int);
	bool 			ProcessTargetMessage(GSocket *, GNetMsg &, int, GNetTarget &);
	void			ParseConsole(GSocketConsole *socket){ console.ParseConsole(socket); };
private:
	GCenzorAdv			cenzor;
	GConsoleCenzor		console;
};
/***************************************************************************/

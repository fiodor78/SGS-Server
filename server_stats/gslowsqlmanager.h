/***************************************************************************
***************************************************************************
(c) 1999-2006 Ganymede Technologies                 All Rights Reserved
Krakow, Poland                                  www.ganymede.eu
***************************************************************************
***************************************************************************/

enum ETableMergeFrequency
{
	ETMF_NONE,
	ETMF_MONTHLY,
	ETMF_WEEKLY,
	ETMF_DAILY,
};

class GConsoleSlowSQL: public GConsoleBasic
{
public:
	GConsoleSlowSQL():GConsoleBasic(){InitConsole();};
	//! inicjalizuje konsole
	void InitConsole()
	{
#define GCONSOLE(command, function,description)		console_commands.insert(make_pair(string(#command).erase(0,8),boost::bind(&GConsoleSlowSQL::function,this,_1,_2)));
#include "tbl/gconsole_slowsql.tbl"
#undef GCONSOLE
#define GCONSOLE(command, function,description)		console_descriptions.push_back(make_pair(string(#command).erase(0,8),description));
#include "tbl/gconsole_slowsql.tbl"
#undef GCONSOLE
	}
#undef GCONSOLE
#define GCONSOLE(command, function,description)		void function(GSocketConsole *,char * buffer);
#include "tbl/gconsole_slowsql.tbl"
#undef GCONSOLE
	GRaportInterface &	GetRaportInterface();
};
/////////////////////////////
/***************************************************************************/

class GSlowSQLManager: public GServerManager
{
    friend class GConsoleSlowSQL;
public:
	GSlowSQLManager();
	void			InitInternalClients();
	void			InitServices()
	{
		GServerManager::InitServices();
		service_manager.services.push_back(SSockService(ESTAcceptSlowSQL,ESTAcceptSlowSQLConsole));
	}
	virtual bool	Init();
	void			ConfigureClock();
	void			ClearExpiredHeartbeats();
	void			UpdateMergedTables();
	void			UpdateMostEffective();
	void			BalanceCheckpoint();
	void			IntervalMonth();
	void			IntervalWeek();
	void			UpdateExperiencePast24hrs();

	bool 			ProcessInternalMessage(GSocket *,GNetMsg &,int);
	bool 			ProcessTargetMessage(GSocket *,GNetMsg &,int,GNetTarget &);
	void			ProcessConnection(GNetMsg & msg);
	void			ParseConsole(GSocketConsole *socket){console.ParseConsole(socket);};

private:
	bool			UpdateMergedTable(MYSQL * mysql, char * table, int back_to_keep = 6, ETableMergeFrequency merge_mode = ETMF_MONTHLY, bool drop_old_database = true);

public:

	GConsoleSlowSQL		console;
};

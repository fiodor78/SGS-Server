/***************************************************************************
***************************************************************************
(c) 1999-2006 Ganymede Technologies                 All Rights Reserved
Krakow, Poland                                  www.ganymede.eu
***************************************************************************
***************************************************************************/

/***************************************************************************/
class GConsoleGlobalMessage : public GConsoleBasic
{
public:
	GConsoleGlobalMessage() : GConsoleBasic(){ InitConsole(); };

	//! inicjalizuje konsole
	void InitConsole()
	{
#define GCONSOLE(command, function, description)		console_commands.insert(make_pair(string(#command).erase(0,8),boost::bind(&GConsoleGlobalMessage::function,this,_1,_2)));
#include "tbl/gconsole_globalmessage.tbl"
#undef GCONSOLE
#define GCONSOLE(command, function, description)		console_descriptions.push_back(make_pair(string(#command).erase(0,8),description));
#include "tbl/gconsole_globalmessage.tbl"
#undef GCONSOLE
	}
#undef GCONSOLE
#define GCONSOLE(command, function, description)		void function(GSocketConsole *,char * buffer);
#include "tbl/gconsole_globalmessage.tbl"
#undef GCONSOLE
	GRaportInterface &	GetRaportInterface();
};
/***************************************************************************/
class GGlobalMessageManager : public GServerManager
{
	friend class GConsoleGlobalMessage;

public:
	GGlobalMessageManager();
	virtual void	WakeUpService();
	void			InitServices()
	{
		GServerManager::InitServices();
		service_manager.services.push_back(SSockService(ESTAcceptGlobalMessage, ESTAcceptGlobalMessageConsole));
	}

	bool 			ProcessInternalMessage(GSocket *, GNetMsg &, int);
	bool 			ProcessTargetMessage(GSocket *, GNetMsg &, int, GNetTarget &);
	void			ParseConsole(GSocketConsole *socket){ console.ParseConsole(socket); };

	void			ProcessGlobalMessage(const std::string & command, const std::string & params);
	void			ProcessSystemGlobalMessage(strstream & s, GSocketConsole * socket_console, const std::string & command, const std::string & params);
	void			WriteGroupsInfo(strstream & s);
private:
	string			GroupInfoToString(DWORD64 global_group_id);
	void			ClearExpiredRequests();

	typedef map<INT32, pair<INT32, DWORD64> > GlobalRequests;  // socket_group_id -> <socket_console_id, expiration_time>

	GlobalRequests				globalSystemRequests;
	GConsoleGlobalMessage		console;
};
/***************************************************************************/

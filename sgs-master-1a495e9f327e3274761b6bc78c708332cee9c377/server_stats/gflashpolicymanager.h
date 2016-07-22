/***************************************************************************
***************************************************************************
(c) 1999-2006 Ganymede Technologies                 All Rights Reserved
Krakow, Poland                                  www.ganymede.eu
***************************************************************************
***************************************************************************/


class GConsoleFlashPolicy: public GConsoleBasic
{
public:
	GConsoleFlashPolicy():GConsoleBasic(){InitConsole();};
	//! inicjalizuje konsole
	void InitConsole()
	{
#define GCONSOLE(command, function,description)		console_commands.insert(make_pair(string(#command).erase(0,8),boost::bind(&GConsoleFlashPolicy::function,this,_1,_2)));
#include "tbl/gconsole_flashpolicy.tbl"
#undef GCONSOLE
#define GCONSOLE(command, function,description)		console_descriptions.push_back(make_pair(string(#command).erase(0,8),description));
#include "tbl/gconsole_flashpolicy.tbl"
#undef GCONSOLE
	}
#undef GCONSOLE
#define GCONSOLE(command, function,description)		void function(GSocketConsole *,char * buffer);
#include "tbl/gconsole_flashpolicy.tbl"
#undef GCONSOLE
	GRaportInterface &	GetRaportInterface();
};
/////////////////////////////
/***************************************************************************/
class GSocketFlashPolicyServer: public GSocketInternalServer
{
public:
	GSocketFlashPolicyServer(GRaportInterface & raport_interface_):
	  GSocketInternalServer(raport_interface_){data.Zero();};
	GSocketFlashPolicyServer(GRaportInterface & raport_interface_,SOCKET socket,EServiceTypes service_type,GMemoryManager * memory_mgr=NULL,GPollManager * poll_mgr=NULL):
	  GSocketInternalServer(raport_interface_,socket,service_type,memory_mgr,poll_mgr){data.Zero();};
	void Parse(GRaportInterface & raport_interface);
};
/***************************************************************************/

class GFlashPolicyManager: public GServerManager
{
	friend class GConsoleFlashPolicy;
public:
	GFlashPolicyManager();
	void			InitServices()
	{
		GServerManager::InitServices();
		service_manager.services.push_back(SSockService(ESTAcceptFlashPolicy,ESTAcceptFlashPolicyConsole));
	}
	virtual GSocket *	SocketNew(EServiceTypes){GSocket * s=new GSocketFlashPolicyServer(raport_interface);return s;};
	bool 			ProcessInternalMessage(GSocket *,GNetMsg &,int);
	bool 			ProcessTargetMessage(GSocket *,GNetMsg &,int,GNetTarget &);
	void			ParseConsole(GSocketConsole *socket){console.ParseConsole(socket);};

public:
	GConsoleFlashPolicy		console;
};

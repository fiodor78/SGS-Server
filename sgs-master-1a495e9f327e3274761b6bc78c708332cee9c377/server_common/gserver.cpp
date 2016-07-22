/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"
#include "gserver_base.h"
#include "gconsole.h"
#include "gserver.h"
#include "gtruerng.h"

#include "key_priv_2048.h"
#include "key_priv_1024.h"
#include "key_priv_512.h"

#include "key_pub_2048.h"
#include "key_pub_1024.h"
#include "key_pub_512.h"

#pragma warning (disable: 4996)
//taka prawie finalna klasa serwera,z inicjalizacja, serwisami itd.
/***************************************************************************/
GServer::GServer():GServerBaseConsole(),firewall(raport_interface)
{
}
/***************************************************************************/
bool GServer::Init()
{
	if(!InitSQL()) 
	{
		RAPORT("SQL Initialization error");
		return false;
	}
	RAPORT_LINE
	if(!InitHostName()) 
	{
		RAPORT("HostName initialization error");
		return false;
	}
	RAPORT_LINE
	if(!InitData()) 
	{
		RAPORT("Data initialization error");
		return false;
	}
	RAPORT_LINE
	if(!ConfigureServices())
	{
		RAPORT("Server configuration error");
		return false;
	}
	RAPORT_LINE
	if(!ConfigureSockets())
	{
		RAPORT("Socket configuration error");
		return false;
	}
	RAPORT_LINE
	InitInternalClients();
	RAPORT_LINE
	if(!InitUserPid()) 
	{
		RAPORT("User/Group/Pid initialization error");
		return false;
	}
	RAPORT_LINE
	if(!InitRSA()) 
	{
		RAPORT("RSA key initialization error");
		return false;
	}
	RAPORT_LINE

	entropy_cache.Init(global_serverconfig->entropy_path.c_str(), &memory_manager, K128);

	InitFireWall();
	ConfigureClock();
	return true;
}
/***************************************************************************/
void GServer::Destroy()
{
	entropy_cache.Destroy();

	{
		boost::recursive_mutex::scoped_lock lock(mtx_sql);

		if (global_serverconfig->sql_config.initialized)
		{
			mysql_close(&mysql);
		}
	}

	RAPORT_LINE
		RAPORT("Server destruction");

	RAPORT_LINE
		RAPORT("Pid and socket file unlinked");
#ifdef LINUX
	//usuwamy plik z pidem w przypadku Linux'a
	unlink("pid");
#endif
	unlink("socket");

	//kasujemy RSA
	RAPORT_LINE
	RAPORT("RSA key deallocation");
	RSA_free(priv_key);
	RAPORT_LINE
}
/***************************************************************************/
bool GServer::InitSQL()
{
	mysql_init(&mysql);
	int pd=ATOI(global_serverconfig->sql_config.port.c_str());
	if (!mysql_real_connect(&mysql,
		global_serverconfig->sql_config.host.c_str(),
		global_serverconfig->sql_config.user.c_str(),
		global_serverconfig->sql_config.password.c_str(),
		global_serverconfig->sql_config.database.c_str(),
		pd,
		NULL,CLIENT_MULTI_STATEMENTS))
	{
		RAPORT("Failed to connect to games database (%s,%s,%s,%s). Error: %s",
			global_serverconfig->sql_config.host.c_str(),
			global_serverconfig->sql_config.user.c_str(),
			global_serverconfig->sql_config.password.c_str(),
			global_serverconfig->sql_config.database.c_str(),
			mysql_error(&mysql));
		return false;
	}
	else
	{
		global_serverconfig->sql_config.initialized=true;
		mysql_set_character_set(&mysql, "utf8");
	}
	mysql.reconnect=1;

	RAPORT("SQL Client version %s",mysql_get_client_info());
	RAPORT("SQL Host info %s",mysql_get_host_info(&mysql));
	RAPORT("SQL Server info %s",mysql_get_server_info(&mysql));
	RAPORT("SQL Stats %s",mysql_stat(&mysql));
	RAPORT("SQL CharSet %s",mysql_character_set_name(&mysql));
	RAPORT("SQL library initialized");

	return true;
}
/***************************************************************************/
bool GServer::InitHostName()
{
	char hostName[MAX_HOSTNAME];
	char hostId[MAX_HOSTNAME];

	int ret=gethostname(hostName, MAX_HOSTNAME);
	if (ret==SOCKET_ERROR)
	{
		RAPORT("Error. Cannot determine hostname");
		return false;
	}

	struct hostent	*hostIP= NULL;
	hostIP = gethostbyname(hostName);
	if (hostIP == NULL)
	{
		RAPORT("Warning. Cannot determine host IP");
	}

	u_long address;
	u_long address_temp;
	address = (hostIP) ? *(int *)hostIP->h_addr_list[0] : 0;
	address_temp = ntohl(address);
	sprintf(hostId, "%d.%d.%d.%d", (int)(address_temp >> 24) & 0xff, (int)(address_temp >> 16) & 0xff, (int)(address_temp >> 8) & 0xff, (int)address_temp & 0xff);

	RAPORT("Hostname detected, Host: %s IP: %s",hostName,hostId);
	return true;
}
/***************************************************************************/
bool GServer::ConfigureSockets()
{
	if(!GServerBaseConsole::ConfigureSockets()) return false;
	/***************************************************************************/
	stack_mgr.Init(global_serverconfig->net.dup_limit);
	/***************************************************************************/
	vector<SSockService>::iterator pos;
	pos=find_if(service_manager.services.begin(),service_manager.services.end(),SockService_isType(ESTAcceptBase));//Console));
	if(pos!=service_manager.services.end())
	{
		FILE * consolefile  = fopen("socket","w+");
		if (consolefile != NULL)
		{
			fprintf(consolefile,"%s:%d",pos->host_console.c_str(),pos->port_console);
			fclose(consolefile);
			chmod("socket",0666);
		}
	}
	return true;
}
/***************************************************************************/
bool GServer::InitUserPid()
{
#ifdef LINUX
	char pids[20];
	int pid;
	int one = 1;
	struct passwd * ur;
	struct group * gr;
	FILE * pidfile=NULL;

	pid = getpid();
	sprintf(pids,"%d",(unsigned)pid);
	pidfile  = fopen("pid","w+");
	if (pidfile != NULL)
	{
		fputs(pids,pidfile);
		fclose(pidfile);
		chmod("pid",0666);
	}
	else
	{
		RAPORT("Cannot open/write to pid file");
		return false;
	}

	if (!(gr = getgrnam(global_serverconfig->misc.group.c_str())))
	{
		RAPORT("Invalid group name");
		return false;
	}

	if (!(ur=getpwnam(global_serverconfig->misc.user.c_str())))
	{
		RAPORT("Invalid user name");
		return false;
	}

	if((ur->pw_uid != geteuid()) || (gr->gr_gid != getegid())) 
	{
		// this is not user/group we want
		if(geteuid() != 0) 
		{
			RAPORT("In order to change user/group to (%s/%s) run server as root", 
				global_serverconfig->misc.user.c_str(), 
				global_serverconfig->misc.group.c_str());
			return false;
		}
		else
		{
			/* change pid file user and group */
			chown("pid",ur->pw_uid,gr->gr_gid);
			if (setgid(gr->gr_gid) == -1)
			{
				RAPORT("Group change error");
				return false;
			}
			if (initgroups(global_serverconfig->misc.group.c_str(),gr->gr_gid) == -1)
			{
				RAPORT("Group change error");
				return false;
			}
			if (setuid(ur->pw_uid) == -1)
			{
				RAPORT("User change error");
				return false;
			}
			RAPORT("User/group changed to (%s/%s)", 
				global_serverconfig->misc.user.c_str(),
				global_serverconfig->misc.group.c_str());
		}
	}
#endif
	RAPORT("User/Group/Pid Initialized");
	return true;
}
/***************************************************************************/
//inicjalizacja modulu kryptografii
bool GServer::InitRSA()
{
	RAPORT("RSA Key initialized");
#if RSA_BYTES==256
	const unsigned char *key_priv_tmp=key_priv_2048;
	priv_key=d2i_RSAPrivateKey(NULL,&key_priv_tmp,sizeof(key_priv_2048));
#endif
#if RSA_BYTES==128
	const unsigned char *key_priv_tmp=key_priv_1024;
	priv_key=d2i_RSAPrivateKey(NULL,&key_priv_tmp,sizeof(key_priv_1024));
#endif
#if RSA_BYTES==64
	const unsigned char *key_priv_tmp=key_priv_512;
	priv_key=d2i_RSAPrivateKey(NULL,&key_priv_tmp,sizeof(key_priv_512));
#endif

#if RSA_BYTES==256
	const unsigned char *key_pub_tmp=key_pub_2048;
	pub_key=d2i_RSAPublicKey(NULL,&key_pub_tmp,sizeof(key_pub_2048));
#endif
#if RSA_BYTES==128
	const unsigned char *key_pub_tmp=key_pub_1024;
	pub_key=d2i_RSAPublicKey(NULL,&key_pub_tmp,sizeof(key_pub_1024));
#endif
#if RSA_BYTES==64
	const unsigned char *key_pub_tmp=key_pub_512;
	pub_key=d2i_RSAPublicKey(NULL,&key_pub_tmp,sizeof(key_pub_512));
#endif

	int a;
	for (a=0;a<32;a++) RSA_bits[a]=(BYTE)rng_byte();
	a=RSA_private_encrypt(32,RSA_bits,RSA_crypt,priv_key,RSA_PKCS1_PADDING);
	
	//to taki kawalek kodu do testow poprawnosci szyfrowania
	//BYTE	RSA_decrypt[RSA_BYTES];
	//a=RSA_public_decrypt(RSA_BYTES,RSA_crypt,RSA_decrypt,pub_key,RSA_PKCS1_PADDING);
	//
	//if (memcmp(RSA_bits,RSA_decrypt,a))
	//{
	//	printf("incorrect size\r\n");
	//}

	return priv_key!=NULL;
}
/***************************************************************************/
//inicjalizacja firewalla
void GServer::InitFireWall()
{
	RAPORT("Firewall initialized");
	firewall.Init();
}
/***************************************************************************/
//inicjalizacja serwisow komunikacyjnych
void GServer::InitServices()
{
	GServerBaseConsole::InitServices();
	service_manager.services.push_back(SSockService(ESTAcceptBase, ESTAcceptBaseConsole, ESTAcceptBaseGame));
	max_service_size=global_serverconfig->net.poll_server_limit;
}

/***************************************************************************/
#ifndef LINUX
DWORD last_hit=0;
void GServer::ParseKeyboard()
{
	DWORD t=timeGetTime();
	if(t-last_hit<100) return;
	last_hit=t;

	if (_kbhit())
	{
		int ch=_getch();
		ch=toupper(ch);
		if (ch=='Q')
		{
			flags.set(EFPrepareToClose);
			PRINT("Server Shutdown");
			SigQuit('Q');
		}
		if (ch=='X')
		{
			_getch();
			SigInt('X');
		}
		if(ch=='E')
		{
			_getch();
			PRINT("echo\n");
		}
		if(ch=='W')
		{
			_getch();
			boost::mutex::scoped_lock lock(mtx_global);
			int a=0;
			printf("Waiting...");
			do{
				Sleep(1000);
				a++;
				printf("%d,",a);
			}while(!_kbhit());
			printf("\n");
		}
	}
}
#endif
/***************************************************************************/
void GServer::ReinitRSA()
{
	int a;
	for (a=0;a<32;a++) RSA_bits[a]=(BYTE)rng_byte();
	RSA_private_encrypt(32,RSA_bits,RSA_crypt,priv_key,RSA_PKCS1_PADDING);
}
/***************************************************************************/
void GServer::UpdateHeartBeat()
{
	// Sam serwer nie raportuje swojego heartbeata. Robia to poszczegolne grupy.
	// TODO(Marian): Upewnic sie, ze tak bedzie OK. W razie potrzeby robic wpis do `rtm_group_heartbeat` z group_id = 0.
}
/***************************************************************************/
void GServer::ConfigureClock()
{
	GServerBase::ConfigureClock();
	signal_minute.connect(boost::bind(&GFireWall::UpdateTime,&firewall,boost::ref(clock)));
	signal_minute.connect(boost::bind(&GServer::UpdateHeartBeat,this));
	signal_day.connect(boost::bind(&GServer::ReinitRSA,this));
	signal_seconds_15.connect(boost::bind(&GEntropyCache::FeedEntropy,&entropy_cache,&truerng,((MAIN_MT_SEED_SIZE + SECONDARY_MT_SEED_SIZE) * 4 * 5) / 3,true));
	signal_second.connect(boost::bind(&GServerBase::TestSocketsBinding, this));
}
/***************************************************************************/

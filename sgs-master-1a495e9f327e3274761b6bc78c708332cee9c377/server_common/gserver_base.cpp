/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"
#include "gserver_base.h"
#include "gtruerng.h"

extern GServerBase * global_server;
/***************************************************************************/
GServerBase::GServerBase():clocks(),socket_game(NET_POLL_ROOM_LIMIT),raport_interface(&raport_manager_global)
{
	GSocket * empty;
	GSocket * del;
	empty=(GSocket *)0;
	del=(GSocket *)1;
	socket_game.set_empty_key(empty);
	socket_game.set_deleted_key(del);
	socket_game.resize(0);
	socket_game_map.clear();
	socket_base_map.clear();
	all_sockets_initialized = false;

	//inicjalizujemy klasy w klasie zarzadzajacej sygnalami
	global_signals->close_slow.connect(boost::bind(&GServerBase::Close,this));
	global_signals->close_fast.connect(boost::bind(&GServerBase::Close,this));
	client_rid=1;
	//start_time=boost::posix_time::second_clock::local_time();
	time_t t;
	time(&t);
	start_s_time=t;
	action_loop_counter = 0;
	seq_socket_id = 1;
}
/***************************************************************************/
bool GServerBase::Init()
{
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
	ConfigureClock();


	return true;
}
/***************************************************************************/
void GServerBase::Destroy()
{
	RAPORT_LINE
	RAPORT("Poll manager flush");	
	poll_manager.ForceWritePoll();
#ifdef LINUX
	Sleep(10);
#else
	Sleep(10);
#endif
	RAPORT("Memory cache stopped");	
	memory_manager.StopCache();

	RAPORT("Socket elements destruction and deallocation");
	for_each (socket_game.begin(), socket_game.end(),boost::bind(&GServerBase::SocketDestroy,this,_1,USR_SERVER_CLOSE));
	socket_game.clear();
	socket_game_map.clear();
	for_each (socket_move.begin(), socket_move.end(),boost::bind(&GServerBase::SocketDestroy,this,_1,USR_SERVER_CLOSE));
	socket_move.clear();
	for_each (socket_move_destroy.begin(), socket_move_destroy.end(),boost::bind(&GServerBase::SocketDestroy,this,_1,USR_SERVER_CLOSE));
	socket_move_destroy.clear();
	{
		GServerBase::TSocketIdMap::iterator its;
		for (its = socket_base_map.begin(); its != socket_base_map.end(); its++)
		{
			SocketDestroy(its->second, USR_SERVER_CLOSE);
		}
	}
	for_each (socket_listen.begin(), socket_listen.end(),boost::bind(&GServerBase::SocketDestroy,this,_1,USR_SERVER_CLOSE));
	socket_listen.clear();
	for_each (socket_internal.begin(), socket_internal.end(),boost::bind(&GServerBase::SocketDestroy,this,_1,USR_SERVER_CLOSE));
	socket_internal.clear();
	socket_internal_map.clear();

	RAPORT("Memory destruction");
	//czyscimy cache pamieci
	memory_manager.Destroy();

	RAPORT("Poll manager destruction");
	//usuwamy poll_managera
	poll_manager.Destroy();

	raport_interface.Destroy();
}
/***************************************************************************/
GSocket * GServerBase::SocketAdd(EServiceTypes type)
{
	GSocket * socket=NULL;
	try{
		socket=SocketNew(type);
		SRAP(INFO_SOCKET_NEW);
	}
	catch(const std::bad_alloc &)
	{
		SRAP(ERROR_SOCKET_NEW_ALLOCATION_ERROR);
		RAPORT(GSTR(ERROR_SOCKET_NEW_ALLOCATION_ERROR));
		socket=NULL;
	};
	if (socket)
	{
		socket->socketid = seq_socket_id++;
		if (type != ESTAcceptBaseGame && type != ESTClientBaseGame)
		{
			socket_base_map.insert(pair<INT32, GSocket *>(socket->socketid, socket));
		}
		else
		{
			socket_game.insert(socket);
			socket_game_map.insert(pair<INT32, GSocket *>(socket->socketid, socket));
		}
	}
	return socket;
}
/***************************************************************************/
void GServerBase::SocketRemove(GSocket * socket,EUSocketMode mode)
{
	if (socket->GetServiceType() != ESTClientBaseGame)
	{
		socket_base_map.erase(socket->socketid);
	}
	else
	{
		socket_game_map.erase(socket->socketid);
		socket_game.erase(socket);
	}
	SocketDestroy(socket,mode);
}
/***************************************************************************/
void GServerBase::SocketDestroy(GSocket * socket,EUSocketMode mode)
{
	UnregisterLogic(socket,mode,true);
	poll_manager.RemoveForceWritePoll(socket);
	socket->Close();
	Delete(socket);
	SRAP(INFO_SOCKET_DELETE);
}
/***************************************************************************/
bool GServerBase::ConfigureServices()
{
	InitServices();
	int ok=(int)service_manager.services.size();
	for_each(service_manager.services.begin(), service_manager.services.end(),boost::bind(&GServerBase::InitService,this,_1,boost::ref(ok)));
	service_manager.services.erase(remove_if(service_manager.services.begin(), service_manager.services.end(), SockService_isNotInitialized()), service_manager.services.end());
	RAPORT("Services initialized");
#ifdef SERVICES
	return ok>0;
#else
	return ok==(int)service_manager.services.size();
#endif
}
/***************************************************************************/
void GServerBase::InitServices()
{
	max_service_size=global_serverconfig->net.poll_room_limit;
	RAPORT("Services preconfigured");
}
/***************************************************************************/
void parse_host_specification(const char * host, string & bind_host, string & report_host)
{
	bind_host = host;

	if (bind_host == "")
	{
		bind_host = "*";
	}
	const char * p;
	if ((p = strchr(bind_host.c_str(), '/')) == NULL)
	{
		report_host = bind_host;
	}
	else
	{
		report_host = p + 1;
		bind_host.erase(p - bind_host.c_str());
	}

	if (report_host == "EC2_PUBLIC_IP")
	{
		report_host = get_EC2_public_IP();
	}
	if (strncmp(bind_host.c_str(), "INTERFACE=", 10) == 0)
	{
		bind_host = get_local_IP(bind_host.c_str() + 10);
	}
	if (strncmp(report_host.c_str(), "INTERFACE=", 10) == 0)
	{
		report_host = get_local_IP(report_host.c_str() + 10);
	}
	if (report_host == "" || report_host == "*")
	{
		report_host = get_local_IP();
	}
}
/***************************************************************************/
void GServerBase::InitService(SSockService & service,int & ok)
{
	string bindings_config = "";
	string service_name = GetServiceName(service.type_internal);
#ifndef SERVICES
	if (service_name == "base")
	{
		service_name = "game";
	}
#endif
	bindings_config = global_serverconfig->bindings_config[service_name];
	if (bindings_config == "")
	{
		return;
	}

	map<string, string> options;
	// bindings_config -> options
	{
		boost::char_separator<char> sep(";");
		typedef boost::tokenizer<boost::char_separator<char> > TTokenizer;
		TTokenizer tok(bindings_config, sep);
		TTokenizer::iterator it;
		for (it = tok.begin(); it != tok.end(); it++)
		{
			const char * p;
			if ((p = strchr(it->c_str(), ':')) == NULL)
			{
				continue;
			}
			string option_name;
			option_name.assign(it->c_str(), p);
			options[option_name] = p + 1;
		}
	}

	string bind_host, report_host;
	int result, port;

	bool initialized = true;
	
	// Polaczenie 'internal'.
	if (service.port_internal == 0)
	{
		const char * host_str = options["host"].c_str();
		const char * port_str = options["port"].c_str();
		parse_host_specification(host_str, bind_host, report_host);
		result = make_netsocket_from_range(bind_host.c_str(), port_str, port);
		if (result >= 0)
		{
			service.socket_internal = result;
			service.host_internal = report_host;
			service.port_internal = (USHORT16)port;
			service.addr_internal = GetAddrFromName(service.host_internal.c_str());
		}
		else
		{
			initialized = false;
		}
	}

	// Polaczenie do konsoli.
	if (service.port_console == 0)
	{
		const char * host_str = (options.find("consolehost") != options.end()) ? options["consolehost"].c_str() : options["host"].c_str();
		const char * port_str = (options.find("consoleport") != options.end()) ? options["consoleport"].c_str() : options["port"].c_str();
		parse_host_specification(host_str, bind_host, report_host);
		result = make_netsocket_from_range(bind_host.c_str(), port_str, port);
		if (result >= 0)
		{
			service.socket_console = result;
			service.host_console = report_host;
			service.port_console = (USHORT16)port;
			service.addr_console = GetAddrFromName(service.host_console.c_str());
		}
		else
		{
			initialized = false;
		}
	}

	// Polaczenia od klientow gier.
	if (options.find("gamehost") != options.end() || options.find("gameports") != options.end())
	{
		const char * host_str = (options.find("gamehost") != options.end()) ? options["gamehost"].c_str() : options["host"].c_str();
		const char * port_str = (options.find("gameports") != options.end()) ? options["gameports"].c_str() : options["port"].c_str();
		parse_host_specification(host_str, bind_host, report_host);

		service.host_game = "";

		string p(port_str);
		boost::char_separator<char> sep(",");
		typedef boost::tokenizer<boost::char_separator<char> > TTokenizer;
		TTokenizer tok(p, sep);
		TTokenizer::iterator beg = tok.begin();
		INT32 a = 0;
		while (beg != tok.end())
		{
			if ((int)service.ports_game.size() < a + 1)
			{
				service.ports_game.resize(a + 1);
				service.ports_game[a] = 0;
				service.sockets_game.resize(a + 1);
				service.sockets_game[a] = 0;
			}

			if (service.ports_game[a] == 0)
			{
				result = make_netsocket_from_range(bind_host.c_str(), beg->c_str(), port);
				if (result >= 0)
				{
					if (service.host_game == "")
					{
						service.host_game = report_host;
						service.addr_game = GetAddrFromName(service.host_game.c_str());
					}
					service.sockets_game[a] = result;
					service.ports_game[a] = (USHORT16)port;
				}
				else
				{
					initialized = false;
				}
			}
			beg++;
			a++;
		}

		service.ports_game_str = "";
		INT32 count = service.ports_game.size();
		for (a = 0; a < count; a++)
		{
			if (service.ports_game[a] != 0)
			{
				char append_string[16];
				sprintf(append_string, "%s%d", (service.ports_game_str != "") ? "," : "", service.ports_game[a]);
				service.ports_game_str += append_string;
			}
		}
	}

	service.initialized = initialized;
}
/***************************************************************************/
SSockService * GServerBase::FindService(EServiceTypes type)
{
	vector<SSockService>::iterator pos;
	pos=find_if(service_manager.services.begin(),service_manager.services.end(),SockService_isType(type));
	if(pos!=service_manager.services.end())
	{
		return &(*pos);
	}
	return NULL;
}
/***************************************************************************/
bool GServerBase::ConfigureSockets()
{
	if (poll_manager.raport == NULL)
	{
		bool ret=poll_manager.Init(max_service_size);
		if(ret==false)
		{
			RAPORT("Cannot initialize poll controller");
			return false;
		}
		else
		{
			poll_manager.raport=boost::bind(&GRaportInterface::RaportChar,&raport_interface,_1);
		}
	}
	/***************************************************************************/
	unsigned int a,b;
	for (a=0;a<service_manager.services.size();a++)
	{
		// Polaczenie 'internal'.
		if (service_manager.services[a].port_internal != 0 && !is_listening_socket(service_manager.services[a].socket_internal))
		{
			RAPORT("%s socket: %s:%d %s",
				GetServiceName(service_manager.services[a].type_internal),
				service_manager.services[a].host_internal.c_str(),
				service_manager.services[a].port_internal,
				"(Binary)");
			PRINT("%s socket: %s:%d %s\n",
				GetServiceName(service_manager.services[a].type_internal),
				service_manager.services[a].host_internal.c_str(),
				service_manager.services[a].port_internal,
				"(Binary)");

			if (service_manager.services[a].socket_internal == -1)
			{
				RAPORT("Cannot open socket %s:%d",
					service_manager.services[a].host_internal.c_str(),
					service_manager.services[a].port_internal);
				PRINT("Cannot open socket %s:%d\n",
					service_manager.services[a].host_internal.c_str(),
					service_manager.services[a].port_internal);
				return false;
			}
			else
			{
				RAPORT("Opening socket %s:%d",
					service_manager.services[a].host_internal.c_str(),
					service_manager.services[a].port_internal);
			}

			//wlacza nasluch na sockecie
			int ret=listen(service_manager.services[a].socket_internal,SOMAXCONN);
			if (ret==-1)
			{
				RAPORT("Cannot listen on port %s:%d",
					service_manager.services[a].host_internal.c_str(),
					service_manager.services[a].port_internal);
				return false;
			}
			else
			{
				RAPORT("Listening socket %s:%d",
					service_manager.services[a].host_internal.c_str(),
					service_manager.services[a].port_internal);
			}

			GSocket * s = new GSocket(raport_interface, service_manager.services[a].socket_internal, service_manager.services[a].type_internal);
			socket_listen.push_back(s);
			SRAP(INFO_SOCKET_NEW);
			poll_manager.AddPoll(s);
		}

		// Polaczenie do konsoli.
		if(service_manager.services[a].port_console!=0 && !is_listening_socket(service_manager.services[a].socket_console))
		{
			RAPORT("%s socket: %s:%d %s",
				GetServiceName(service_manager.services[a].type_console),
				service_manager.services[a].host_console.c_str(),
				service_manager.services[a].port_console,
				"(Console)");
			PRINT("%s socket: %s:%d %s\n",
				GetServiceName(service_manager.services[a].type_console),
				service_manager.services[a].host_console.c_str(),
				service_manager.services[a].port_console,
				"(Console)");

			if (service_manager.services[a].socket_console==-1)
			{
				RAPORT("Cannot open socket %s:%d",
					service_manager.services[a].host_console.c_str(),
					service_manager.services[a].port_console);
				PRINT("Cannot open socket %s:%d\n",
					service_manager.services[a].host_console.c_str(),
					service_manager.services[a].port_console);
				return false;
			}
			else
			{
				RAPORT("Opening socket %s:%d",
					service_manager.services[a].host_console.c_str(),
					service_manager.services[a].port_console);
			}

			//wlacza nasluch na sockecie
			int ret=listen(service_manager.services[a].socket_console,SOMAXCONN);
			if (ret==-1)
			{
				RAPORT("Cannot listen on port %s:%d",
					service_manager.services[a].host_console.c_str(),
					service_manager.services[a].port_console);
				return false;
			}
			else
			{
				RAPORT("Listening socket %s:%d",
					service_manager.services[a].host_console.c_str(),
					service_manager.services[a].port_console);
			}

			GSocket * s = new GSocket(raport_interface, service_manager.services[a].socket_console,service_manager.services[a].type_console);
			socket_listen.push_back(s);
			SRAP(INFO_SOCKET_NEW);
			poll_manager.AddPoll(s);
		}

		// Polaczenia od klientow gier.
		for (b = 0; b < service_manager.services[a].ports_game.size(); b++)
		{
			if (service_manager.services[a].ports_game[b] != 0 && !is_listening_socket(service_manager.services[a].sockets_game[b]))
			{
				RAPORT("%s socket: %s:%d %s",
					GetServiceName(service_manager.services[a].type_game),
					service_manager.services[a].host_game.c_str(),
					service_manager.services[a].ports_game[b],
					"(BinaryGame)");
				PRINT("%s socket: %s:%d %s\n",
					GetServiceName(service_manager.services[a].type_game),
					service_manager.services[a].host_game.c_str(),
					service_manager.services[a].ports_game[b],
					"(BinaryGame)");

				if (service_manager.services[a].sockets_game[b]==-1)
				{
					RAPORT("Cannot open socket %s:%d",
						service_manager.services[a].host_game.c_str(),
						service_manager.services[a].ports_game[b]);
					PRINT("Cannot open socket %s:%d\n",
						service_manager.services[a].host_game.c_str(),
						service_manager.services[a].ports_game[b]);
					return false;
				}
				else
				{
					RAPORT("Opening socket %s:%d",
						service_manager.services[a].host_game.c_str(),
						service_manager.services[a].ports_game[b]);
				}

				//wlacza nasluch na sockecie
				int ret=listen(service_manager.services[a].sockets_game[b],SOMAXCONN);
				if (ret==-1)
				{
					RAPORT("Cannot listen on port %s:%d",
						service_manager.services[a].host_game.c_str(),
						service_manager.services[a].ports_game[b]);
					return false;
				}
				else
				{
					RAPORT("Listening socket %s:%d",
						service_manager.services[a].host_game.c_str(),
						service_manager.services[a].ports_game[b]);
				}

				GSocket * s = new GSocket(raport_interface, service_manager.services[a].sockets_game[b], service_manager.services[a].type_game);
				socket_listen.push_back(s);
				SRAP(INFO_SOCKET_NEW);
				poll_manager.AddPoll(s);
			}
		}
	}	
	/***************************************************************************/
	RAPORT("Sockets initialized");
	return true;
}
/***************************************************************************/
void GServerBase::TestSocketsBinding()
{
	if (all_sockets_initialized)
	{
		return;
	}

	vector<SSockService>::iterator pos;
	int ok;
	bool all_initialized = true, try_configure_sockets = false;
	for (pos = service_manager.services.begin(); pos != service_manager.services.end(); pos++)
	{
		if (!(*pos).initialized)
		{
			InitService(*pos, ok);
			try_configure_sockets = true;
		}
		if (!(*pos).initialized)
		{
			all_initialized = false;
		}
	}
	if (try_configure_sockets)
	{
		DWORD32 old_socket_listen_size = socket_listen.size();
		ConfigureSockets();
		if (old_socket_listen_size != socket_listen.size())
		{
			signal_new_sockets_initialized();
		}
	}
	if (all_initialized)
	{
		all_sockets_initialized = true;
	}
}
/***************************************************************************/
void GServerBase::InitInternalClient(GTicketInternalClient *& target,EServiceTypes type,int level)
{
	target=new GTicketInternalClient(raport_interface);
	target->Init(type,level,&memory_manager,&poll_manager);
	socket_internal.push_back(target);
	socket_internal_map.insert(make_pair(type,target));
	SRAP(INFO_SOCKET_NEW);
};
/***************************************************************************/
GTicketInternalClient * GServerBase::InternalClient(EServiceTypes type)
{
	GTicketInternalClient * service=NULL;
	service=socket_internal_map[type];
	return service;
};
/***************************************************************************/
void GServerBase::AnalizeAccept(epoll_event * ev)
{
	if(ev->events & (EPOLLERR|EPOLLHUP)) 
	{
		SRAP(ERROR_SOCKET_ACCEPT_ERROR);
		RAPORT(GSTR(ERROR_SOCKET_ACCEPT_ERROR));
		return;
	}

	unsigned int			accept_count=0;
	SOCKET					client_sock=0;
	struct	sockaddr_in		client_addr;
	DWORD					addr;
	while (client_sock != -1 && 
#ifndef SERVICES
		accept_count<global_serverconfig->net.accept_limit && 
#endif
		poll_manager.Free())
	{
		socklen_t sin_size=sizeof(struct sockaddr_in);
		GSocket * s=reinterpret_cast<GSocket*>(ev->data.ptr);
		SOCKET fd=s->GetSocket();
		client_sock=accept(fd,(struct sockaddr *)&client_addr,&sin_size);
		if(client_sock==-1) break;
		memcpy(&addr,&client_addr.sin_addr,4);
#ifndef SERVICES
		if(TestFirewall(client_addr.sin_addr))
		{
			closesocket(client_sock);
		}
		else
#endif
		{
			accept_count++;
			SRAP(INFO_SOCKET_ACCEPT_COUNT);

			GSocket * socket=SocketAdd(s->GetServiceType());
			if(socket)
			{
				socket->Init(raport_interface,client_sock,GetServiceTypeAssociate(s->GetServiceType()),&memory_manager,&poll_manager);
				socket->Msg().SetTestIn();
				socket->Msg().SetTestOut();
				socket->SetTimeConnection(clock.Get());
				if(!socket->RegisterPoll())
				{
					SRAP(ERROR_SOCKET_REGISTER_POLL);
					GSTR(ERROR_SOCKET_REGISTER_POLL);
					SocketRemove(socket ,USR_ERROR_REGISTER_POLL);
				}
				else
					if(!RegisterLogic(socket))
					{
						SRAP(WARNING_LOGIC_NOT_REGISTERED);
						RAPORT("%s %s %d",GSTR(WARNING_LOGIC_NOT_REGISTERED),__FILE__,__LINE__);
						SocketRemove(socket,USR_ERROR_REGISTER_LOGIC);
					}
			}
			else
			{
				closesocket(client_sock);
			}
		}
	}
	if(client_sock != -1)
	{
#ifndef SERVICES
		if(accept_count==global_serverconfig->net.accept_limit) 
		{
			SRAP(WARNING_SERVER_ACCEPT_LIMIT_EXCEED);
			RAPORT(GSTR(WARNING_SERVER_ACCEPT_LIMIT_EXCEED));
		}
#endif
		if(!poll_manager.Free()) 
		{
			SRAP(WARNING_SERVER_POLL_LIMIT_EXCEED);
			RAPORT(GSTR(WARNING_SERVER_POLL_LIMIT_EXCEED));

			while (client_sock != -1)
			{
				struct	sockaddr_in		client_addr;
				GSocket * s=reinterpret_cast<GSocket*>(ev->data.ptr);
				socklen_t sin_size=sizeof(struct sockaddr_in);
				SOCKET fd=s->GetSocket();
				client_sock=accept(fd,(struct sockaddr *)&client_addr,&sin_size);
				if(client_sock==-1) break;
				closesocket(client_sock);
			}
		}
	}
}
/***************************************************************************/
void GServerBase::AnalizeClient(epoll_event * ev)
{
	GSocket * socket=reinterpret_cast<GSocket*>(ev->data.ptr);

	if(socket->Flags()[ESockMove])
	{
		//TODO(Marian): Usunac jak sie potwierdzi, ze to wlasnie byl problem.
		RAPORT("AnalizeClient() on [ESockMove], ev->events = %d", (int)ev->events);
		//element ma byc procesowany po stronie pokoju,
		//tutaj tak naprawde nie wiem co sie stanie - mamy element z juz ustawiona flaga, wrzucamy go w nowego poll'a pokoju,
		//czy poll pokoju zwroci to samo, blad?? czy tez moze cos tam sobie radosnie padnie?
		return;
	}

	if(ev->events & (EPOLLERR|EPOLLHUP)) 
	{
#ifdef SERVICES
		RAPORT("INTERNAL SOCKET ERROR: ev->events = %d", (int)ev->events);
#endif
		socket->Flags().set(ESockExit);
	}
	if((ev->events & EPOLLIN) && !(ev->events & (EPOLLERR|EPOLLHUP)))
	{
		if(socket->Read()) socket->Parse(raport_interface);
		socket->SetTimeLastAction(clock.Get());
	}
	if((ev->events & EPOLLOUT) && !(ev->events & (EPOLLERR|EPOLLHUP)))
	{
		socket->Write();
	}
	if(ev->events & EPOLLPRI) RAPORT("EPOLLPRI");
	if(ev->events & EPOLLRDNORM) RAPORT("EPOLLRDNORM");
	if(ev->events & EPOLLRDBAND) RAPORT("EPOLLRDBAND");
	if(ev->events & EPOLLWRNORM) RAPORT("EPOLLWRNORM");
	if(ev->events & EPOLLWRBAND) RAPORT("EPOLLWRBAND");
	if(ev->events & EPOLLMSG) RAPORT("EPOLLMSG");
	if(ev->events & EPOLLONESHOT) RAPORT("EPOLLONESHOT");
	if(ev->events & EPOLLET) RAPORT("EPOLLET");

	if (!socket->Flags()[ESockError] && socket->Flags()[ESockControlWrite])
	{
		socket->TestWrite();
	}
	if (socket->Flags()[ESockError] || socket->Flags()[ESockDisconnect])
	{
#ifdef SERVICES
		RAPORT("INTERNAL SOCKET ERROR: ESockError ESockDisconnect ev->events = %d", (int)ev->events);
#endif		
		socket->Flags().set(ESockExit);
	}
	if (socket->Flags()[ESockExit])
	{
#ifdef SERVICES
		// W przypadku 'FlashPolicyManager' polaczenia sa caly czas zamykane w ten sposob,
		// wiec nie zasmiecamy tym komunikatem logow.
		if (game_name != "flashpolicy")
		{
			RAPORT("INTERNAL SOCKET ERROR: ESockExit ev->events = %d", (int)ev->events);
		}
#endif
		SocketRemove(socket,USR_CONNECTION_CLOSED);
	}
}
/***************************************************************************/
void GServerBase::AnalizeServerClient(epoll_event * ev)
{
	GTicketInternalClient * socket=reinterpret_cast<GTicketInternalClient*>(ev->data.ptr);

	if(ev->events & (EPOLLERR|EPOLLHUP)) 
	{
		RAPORT("INTERNAL SOCKET ERROR: ev->events = %d", (int)ev->events);
		socket->Error();
	}
	if((ev->events & EPOLLIN) && !(ev->events & (EPOLLERR|EPOLLHUP)))
	{
		if(socket->Read()) socket->Parse();
		socket->SetTimeLastAction(clock.Get());
	}
	if((ev->events & EPOLLOUT) && !(ev->events & (EPOLLERR|EPOLLHUP)))
	{
		socket->Write();
	}
	if(ev->events & EPOLLPRI) RAPORT("EPOLLPRI");
	if(ev->events & EPOLLRDNORM) RAPORT("EPOLLRDNORM");
	if(ev->events & EPOLLRDBAND) RAPORT("EPOLLRDBAND");
	if(ev->events & EPOLLWRNORM) RAPORT("EPOLLWRNORM");
	if(ev->events & EPOLLWRBAND) RAPORT("EPOLLWRBAND");
	if(ev->events & EPOLLMSG) RAPORT("EPOLLMSG");
	if(ev->events & EPOLLONESHOT) RAPORT("EPOLLONESHOT");
	if(ev->events & EPOLLET) RAPORT("EPOLLET");

	if (!socket->Flags()[ESockError] && socket->Flags()[ESockControlWrite])
	{
		socket->TestWrite();
	}
}
/***************************************************************************/
void GServerBase::ProcessPoll()
{
	int count=poll_manager.Poll();
	if(count)
	{
		int a;

		a = (int)GetTime() ^ count;
		truerng.AddEntropy((char*)&a, truerng.Rnd(4), EENTROPY_NET_EVENT);

		for (a=0;a<count;a++)
		{
			epoll_event * ev=poll_manager.GetEvent(a);
			GSocket * s=reinterpret_cast<GSocket*>(ev->data.ptr);
			switch(GetServiceTypeBasic(s->GetServiceType()))
			{
			case ESTAcceptBase:AnalizeAccept(ev);break;
			case ESTClientBase:AnalizeClient(ev);break;
			case ESTInternalClient:AnalizeServerClient(ev);break;
			default:{SRAP(ERROR_UNKNOWN);
				RAPORT("%ld - UNKNOWN SERVICE",GSTR(ERROR_UNKNOWN));};
				break;
			}
		}
	}
	poll_manager.ForceWritePoll();
}
/***************************************************************************/
void GServerBase::ProcessTimeOut()
{
	for_each(socket_game.begin(), socket_game.end(),boost::bind(&GServerBase::TestTimeOut,this,_1));
	socket_game.resize(0);

	GServerBase::TSocketIdMap::iterator its;
	for (its = socket_base_map.begin(); its != socket_base_map.end(); its++)
	{
		TestTimeOut(its->second);
	}
}
/***************************************************************************/
bool GServerBase::TestTimeOut(GSocket * socket)
{
	//to kasuje zbedne sockety, gdy np. zamkniemy logike
	if(socket->Flags()[ESockTimeOut] || socket->IsDeadTime(clock))
	{
		SocketRemove(socket,USR_DEAD_TIME);
		return true;
	}
	return false;
}
/***************************************************************************/
void GServerBase::ProcessMove()
{
	if(socket_move_destroy.size())
	{
		boost::mutex::scoped_lock lock(mtx_move);
		if(socket_move_destroy.size())
		{
			for_each (socket_move_destroy.begin(), socket_move_destroy.end(),boost::bind(&GServerBase::SocketDestroy,this,_1,USR_MOVE_CLOSE));
			socket_move_destroy.clear();
		}
	}
	if(socket_move.size())
	{
		boost::mutex::scoped_lock lock(mtx_move);
		if(socket_move.size()) 
		{
			for_each (socket_move.begin(), socket_move.end(),boost::bind(&GServerBase::SocketMoveToSocketGameInRoomThread,this,_1));
			global_server->SocketMoveAddToGlobalThread(socket_move);
			socket_move.clear();
		}
	}
	poll_manager.ForceWritePoll();
}
/***************************************************************************/
void GServerBase::SocketMoveAddToGlobalThread(vector<GSocket*> & v)
{
	boost::mutex::scoped_lock lock(mtx_move);
	socket_move_destroy.insert(socket_move_destroy.end(),v.begin(),v.end());
}
/***************************************************************************/
void GServerBase::SocketMoveRemoveFromGlobalThread(GSocket * socket)
{
	socket_game_map.erase(socket->socketid);
	socket_game.erase(socket);
	socket->Move();
}
/***************************************************************************/
void GServerBase::SocketMoveAddToRoomThread(GSocket * socket)
{
	boost::mutex::scoped_lock lock(mtx_move);
	socket_move.push_back(socket);
}
/***************************************************************************/
void GServerBase::SocketMoveToSocketGameInRoomThread(GSocket * socket_p)
{
	GSocket * socket=SocketAdd(socket_p->GetServiceType());
	if(socket)
	{
		socket->Init(raport_interface,socket_p,&memory_manager,&poll_manager);
		socket->Msg().SetTestIn();
		socket->Msg().SetTestOut();
		if(!socket->RegisterPoll())
		{
			SRAP(ERROR_SOCKET_REGISTER_POLL);
			GSTR(ERROR_SOCKET_REGISTER_POLL);
			SocketRemove(socket,USR_ERROR_REGISTER_POLL);
		}
		else
		{
			// Dopisujemy do bufora wyjsciowego dane, ktore nie zdazyly zostac wyslane w poprzednim watku.
			NPair socket_p_spool_data = socket_p->GetSpoolBufferOut();
			if (socket_p_spool_data.size > 0)
			{
				GMemory & memory_out = socket->MemoryOut();
				memory_out.ReallocateToFit(socket_p_spool_data.size);
				memcpy(memory_out.End(), socket_p_spool_data.ptr, socket_p_spool_data.size);
				memory_out.IncreaseUsed(socket_p_spool_data.size);
			}

			if(!RegisterLogic(socket,socket_p))
			{
				SRAP(WARNING_LOGIC_NOT_REGISTERED);
				//RAPORT("%s %s %d",GSTR(WARNING_LOGIC_NOT_REGISTERED),__FILE__,__LINE__);
				socket->SetDeadTime(GetClock());
			}
		}
	}
	else
	{
		socket_p->Close();
	}
	socket_p->Reset();
}
/***************************************************************************/
void GServerBase::Action()
{
	DWORD64 current_timestamp, action_loop_timestamp[6];
	bool exception_occured;
	int i;

	action_loop_counter = 0;
	action_loop_timestamp[0] = GetTime();
	while(Continue())
	{
		try
		{
			while(Continue())
			{
				ActionLoopBegin();
				action_loop_counter++;

				current_timestamp = GetTime();
				if (current_timestamp >= action_loop_timestamp[0] + 1000)
				{
					char temp_string[128];
					*temp_string = 0;
					for (i = 1; i <= 5; i++)
					{
						if (action_loop_timestamp[i] != 0)
						{
							sprintf(temp_string + strlen(temp_string), "%6lld", action_loop_timestamp[i] - action_loop_timestamp[i - 1]);
						}
						else
						{
							sprintf(temp_string + strlen(temp_string), "%6s", "---");
						}
					}

					RAPORT("Long Action() loop duration! counter = %8lld, parts: {%s} = %6lld ms (SQL: %6lld ms /%3d)%s",
						action_loop_counter,
						temp_string,
						current_timestamp - action_loop_timestamp[0],
						action_loop_sql_queries_times,
						action_loop_sql_queries_count,
						exception_occured ? " EXCEPTION" : "");
				}
				action_loop_timestamp[0] = current_timestamp;
				action_loop_sql_queries_times = 0;
				action_loop_sql_queries_count = 0;
				for (i = 1; i < (int)sizeof(action_loop_timestamp) / sizeof(action_loop_timestamp[0]); i++)
				{
					action_loop_timestamp[i] = 0;
				}
				exception_occured = false;

				Sleep(0);
				action_loop_timestamp[1] = GetTime();
				Clock();
				action_loop_timestamp[2] = GetTime();
#ifndef LINUX
				ParseKeyboard();
#endif
				ProcessMove();
				action_loop_timestamp[3] = GetTime();
				ProcessPoll();
				action_loop_timestamp[4] = GetTime();
				for_each (socket_internal.begin(), socket_internal.end(),boost::bind(&GTicketInternalClient::Communicate,_1));
				action_loop_timestamp[5] = GetTime();
				if(vol_global)
				{
					boost::mutex::scoped_lock lock_global(mtx_global);
				}
				ActionLoopEnd();
			}
		}
		catch(...)
		{
			exception_occured = true;
			SRAP(ERROR_INTERNAL_EXCEPTION);
			GSTR(ERROR_INTERNAL_EXCEPTION);
		}
	}

	RAPORT("Action end");
}
/***************************************************************************/
void GServerBase::SetCurrentSTime()
{
	time_t t;
	time(&t);
	current_s_time = t;
}
/***************************************************************************/
void GServerBase::Clock()
{
	DWORD64 action_loop_timestamp[11];
	int i;
	action_loop_timestamp[0] = GetTime();
	for (i = 1; i <= 9; i++)
	{
		action_loop_timestamp[i] = 0;
	}

	clock.Probe();
	if(clocks.interval_ms(clock))
	{
		signal_ms();
		action_loop_timestamp[1] = GetTime();
	}

	if(clocks.interval_second(clock))
	{
		SetCurrentSTime();

		signal_second();
		action_loop_timestamp[2] = GetTime();

		if(clocks.interval_5_seconds(clock))
		{
			signal_seconds_5();
			action_loop_timestamp[3] = GetTime();
		}
		if(clocks.interval_15_seconds(clock))
		{
			signal_seconds_15();
			action_loop_timestamp[4] = GetTime();
		}
		if(clocks.interval_minute(clock))
		{
			signal_minute();
			action_loop_timestamp[5] = GetTime();
			if(clocks.interval_hour(clock))
			{
				signal_hour();
				action_loop_timestamp[6] = GetTime();
				if(clocks.interval_day(clock))
				{
					signal_day();
					action_loop_timestamp[7] = GetTime();
				}
			}
		}
		if(clocks.interval_5_minutes(clock))
		{
			signal_minutes_5();
			action_loop_timestamp[8] = GetTime();
		}
		if(clocks.interval_15_minutes(clock))
		{
			signal_minutes_15();
			action_loop_timestamp[9] = GetTime();
		}
	}

	action_loop_timestamp[10] = GetTime();
	if (action_loop_timestamp[10] - action_loop_timestamp[0] >= GSECOND_1)
	{
		char temp_string[256];
		static char * signal_name[9] = { "ms", "s", "5s", "15s", "m", "h", "d", "5m", "15m" };
		*temp_string = 0;
		DWORD64 last_timestamp = action_loop_timestamp[0];
		for (i = 1; i <= 9; i++)
		{
			if (action_loop_timestamp[i] != 0)
			{
				sprintf(temp_string + strlen(temp_string), "%4s: %5lld", signal_name[i - 1], action_loop_timestamp[i] - last_timestamp);
				last_timestamp = action_loop_timestamp[i];
			}
			else
			{
				sprintf(temp_string + strlen(temp_string), "%4s: %5s", signal_name[i - 1], "---");
			}
		}

		RAPORT("Long Clock() duration! parts: {%s} = %6lld ms",
			temp_string,
			action_loop_timestamp[10] - action_loop_timestamp[0]);
	}
}
/***************************************************************************/
void GServerBase::ConfigureClock()
{
	//current_time=boost::posix_time::second_clock::local_time();
	time_t t;
	time(&t);
	current_s_time=t;

	signal_second.connect(boost::bind(&GRaportInterface::UpdateTime,&raport_interface,boost::ref(clock)));

	signal_seconds_5.connect(boost::bind(&GServerBase::ProcessTimeOut,this));

	signal_minute.connect(boost::bind(&GMemoryManager::UpdateTime,&memory_manager,boost::ref(clock)));

	signal_ms.connect(boost::bind(&GServerBase::ProcessConsoleTasks,this));
}
/***************************************************************************/
//funkcja zwraca info na temat serwisu gdy robimy zapytanie z konsoli
void GServerBase::ServiceInfo(strstream & s)
{
	for_each (socket_internal.begin(), socket_internal.end(),boost::bind(&GTicketInternalClient::ServiceInfo,_1,boost::ref(s)));
}
/***************************************************************************/
//funkcja wymusza reinit serwisu,
void GServerBase::ServiceReinit(strstream & s,SSocketInternalReinit & reinit)
{
	for_each (socket_internal.begin(), socket_internal.end(),boost::bind(&GTicketInternalClient::ServiceReinit,_1,boost::ref(s),boost::ref(reinit)));
}
/***************************************************************************/
//zrwraca informacje na temat zarzadzania pamiecia w serwerze
INT32 GServerBase::CallAction(strstream &s, ECmdConsole cmd, map<string, string> & options)
{
	switch(cmd)
	{
	case CMD_ACTION_MEMORY_INFO:
		{
#define MEM memory_manager.local_stats
			s<<boost::format("Memory allocations          : %|-8d|  deallocations       : %|-8s|\r\n") %MEM.allocation % MEM.deallocation;
			s<<boost::format("Memory real allocations     : %|-8d|  real deallocations  : %|-8s|\r\n") %MEM.real_allocation % MEM.real_deallocation;
			s<<boost::format("Memory cached allocations   : %|-8d|  cached deallocations: %|-8s|\r\n") %MEM.cache_allocation % MEM.cache_deallocation;
			s<<boost::format("Memory reallocations        : %|-8d|\r\n") %MEM.reallocation;
			s<<boost::format("Cache efficiency            : %|-8d|\r\n") %MEM.efficiency;
			s<<boost::format("Memory allocated            : %|-8d|\r\n") %MEM.allocated_memory;
			s<<boost::format("Memory allocated (big size) : %|-8d|\r\n") %MEM.big_size_allocated_memory;
			s<<boost::format("Cache usage     Cached    Used      Cache count   Total\r\n");
			s<<boost::format("%|60T-|\r\n");
			s<<"";
#undef MEM
			int v=K1;
			int a;
			for (a=0;a<KCOUNT;a++)
			{
				int global=(int)memory_manager.GetCachedMemSize(a);
				int used=memory_manager.GetUsedMemSize(a);
				int cached=memory_manager.GetCacheCount(a);
				int total=(global+used)*v;
				s<<boost::format("%|-16d|%|-10d|%|-10d|%|-14d|%|-10d|\r\n") %v%global%used%cached%total;
				v*=2;
			}
		}
		break;
	case CMD_ACTION_SERVICE_INFO:
		{
			GServerBase::ServiceInfo(s);
		}
		break;
	case CMD_ACTION_SERVICE_REINIT:
		{
			SSocketInternalReinit reinit;
			reinit.group = ATOI(options["reinit_group"].c_str());
			reinit.name = options["reinit_name"];
			reinit.addr.sin_addr.s_addr = (DWORD32)ATOI(options["reinit_addr"].c_str());
			reinit.addr.sin_port = (unsigned short)ATOI(options["reinit_port"].c_str());
			GServerBase::ServiceReinit(s, reinit);
		}
		break;
	}
	return 0;
}
/***************************************************************************/
void GServerBase::RaportRestartSQL(const char * status)
{
	boost::recursive_mutex::scoped_lock lock(mtx_sql);
	char command[512];
	sprintf(command, "INSERT INTO `data_server_restarts` (`time`, `game_name`, `status`) "
		"VALUES(UNIX_TIMESTAMP(), '{%d}%s', '%s')", game_instance, game_name.c_str(), status);
	MySQLQuery(&mysql, command);
}
/***************************************************************************/
bool GServerBase::MySQL_Query_Raport(MYSQL * mysql, const char * query, char * qa_filename, int qa_line)
{
	DWORD64 start_time, end_time;

	start_time = GetTime();
	bool result = ::MySQL_Query(mysql, query, qa_filename, qa_line);
	end_time = GetTime();
	action_loop_sql_queries_times += (end_time - start_time);
	action_loop_sql_queries_count++;
	return result;
}
/***************************************************************************/
void GServerBase::InsertConsoleTask(SConsoleTask & task)
{
	boost::mutex::scoped_lock lock(mtx_console_task_queue);
	console_task_queue.push(task);
}
/***************************************************************************/
void GServerBase::ProcessConsoleTasks()
{
	SConsoleTask task;

	for (;;)
	{
		task.type = ECTT_None;
		{
			boost::mutex::scoped_lock lock(mtx_console_task_queue);
			if (console_task_queue.empty())
			{
				break;
			}
			task = console_task_queue.front();
			console_task_queue.pop();
		}
		if (task.type == ECTT_Request)
		{
			GMemory output_buffer;
			output_buffer.Init(&memory_manager);
			output_buffer.Allocate(K512);
			strstream s(output_buffer.End(), output_buffer.Free(), ios_base::out);
			// Wartosc -1 zwrocona przez CallAction() oznacza, ze serwer sam doda odpowiedz do kolejki w pozniejszym terminie.
			if (CallAction(s, task.cmd, task.options) != -1)
			{
				output_buffer.End()[s.pcount()] = 0;
				task.result = s.str();

				task.type = ECTT_Response;
				global_server->InsertConsoleTask(task);
			}
			output_buffer.Deallocate();
		}
	}
}
/***************************************************************************/
GSocket * GServerBase::GetBaseSocketByID(INT32 socketid)
{
	TSocketIdMap::iterator pos = socket_base_map.find(socketid);
	if (pos != socket_base_map.end())
	{
		return pos->second;
	}
	return NULL;
}
/***************************************************************************/
GSocket * GServerBase::GetPlayerSocketByID(INT32 socketid)
{
	TSocketIdMap::iterator pos = socket_game_map.find(socketid);
	if (pos != socket_game_map.end())
	{
		return pos->second;
	}
	return NULL;
}
/***************************************************************************/

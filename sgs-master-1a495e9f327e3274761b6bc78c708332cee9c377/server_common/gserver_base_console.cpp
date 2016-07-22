/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"
#include "gserver_base.h"
#include "gtruerng.h"



/***************************************************************************/
void GServerBaseConsole::AnalizeAcceptConsole(epoll_event * ev)
{
	if(ev->events & (EPOLLERR|EPOLLHUP)) 
	{
		SRAP(ERROR_CONSOLE_ACCEPT_ERROR);
		RAPORT(GSTR(ERROR_CONSOLE_ACCEPT_ERROR));
		return;
	}
	unsigned int			accept_count=0;
	SOCKET					client_sock=0;
	struct	sockaddr_in		client_addr;
	DWORD					addr;
	while (client_sock != -1 && 
		accept_count<global_serverconfig->net.accept_limit && 
		poll_manager.Free())
	{
		socklen_t sin_size=sizeof(struct sockaddr_in);
		GSocketConsole * s=reinterpret_cast<GSocketConsole*>(ev->data.ptr);
		SOCKET fd=s->GetSocket();
		client_sock=accept(fd,(struct sockaddr *)&client_addr,&sin_size);
		if(client_sock==-1) break;
		memcpy(&addr,&client_addr.sin_addr,4);

		accept_count++;
		SRAP(INFO_CONSOLE_ACCEPT_COUNT);

		GSocketConsole* socket=SocketConsoleAdd();
		if(socket)
		{
			GPollManager & pm=poll_manager;
			GMemoryManager & mm=memory_manager;
			socket->Init(raport_interface,client_sock,GetServiceTypeAssociate(s->GetServiceType()),&mm,&pm);
			socket->SetTimeConnection(clock.Get());
			if(socket->RegisterPoll())
			{
				socket->AllocateOut();
				socket->MemoryOut()<<GetServiceName(s->GetServiceType());
				socket->MemoryOut()<<lend;
				socket->Write();
			}
			else
			{
				SRAP(ERROR_CONSOLE_REGISTER_POLL);
				GSTR(ERROR_CONSOLE_REGISTER_POLL);
				SocketConsoleRemove(socket);
			}
		}
		else
		{
			closesocket(client_sock);
		}
	}
	if(client_sock != -1)
	{
		if(accept_count==global_serverconfig->net.accept_limit) 
		{
			SRAP(WARNING_SERVER_ACCEPT_LIMIT_EXCEED);
			RAPORT(GSTR(WARNING_SERVER_ACCEPT_LIMIT_EXCEED));
		}
		if(!poll_manager.Free()) 
		{
			SRAP(WARNING_SERVER_POLL_LIMIT_EXCEED);
			RAPORT(GSTR(WARNING_SERVER_POLL_LIMIT_EXCEED));
		}
	}
}
/***************************************************************************/
void GServerBaseConsole::AnalizeClientConsole(epoll_event *ev)
{
	GSocketConsole* socket=reinterpret_cast<GSocketConsole*>(ev->data.ptr);

	if(ev->events & (EPOLLERR|EPOLLHUP)) 
	{
		socket->Flags().set(ESockExit);
	}
	if((ev->events & EPOLLIN) && !(ev->events & (EPOLLERR|EPOLLHUP)))
	{
		if(socket->Read()) ParseConsole(socket);
		socket->SetTimeLastAction(clock.Get());
	}
	if((ev->events & EPOLLOUT) && !(ev->events & (EPOLLERR|EPOLLHUP)))
	{
		socket->TestWrite();
	}
	if(ev->events & EPOLLPRI) RAPORT("EPOLLPRI");
	if(ev->events & EPOLLRDNORM) RAPORT("EPOLLRDNORM");
	if(ev->events & EPOLLRDBAND) RAPORT("EPOLLRDBAND");
	if(ev->events & EPOLLWRNORM) RAPORT("EPOLLWRNORM");
	if(ev->events & EPOLLWRBAND) RAPORT("EPOLLWRBAND");
	if(ev->events & EPOLLMSG) RAPORT("EPOLLMSG");
	if(ev->events & EPOLLONESHOT) RAPORT("EPOLLONESHOT");
	if(ev->events & EPOLLET) RAPORT("EPOLLET");

	if (socket->Flags()[ESockError])
	{
		socket->Flags().set(ESockExit);
	}
	if (socket->Flags()[ESockExit])
	{
		SocketConsoleRemove(socket);
	}
}
/***************************************************************************/
void GServerBaseConsole::ParseConsole(GSocketConsole* socket)
{
	string cmd;
	cmd.assign(socket->MemoryIn().Start(),0,(DWORD)socket->MemoryIn().Used());
	socket->MemoryIn().IncreaseParsed(socket->MemoryIn().Used());
	socket->MemoryIn().RemoveParsed();
	socket->AllocateOut();
	strstream s(socket->MemoryOut().End(),socket->MemoryOut().Free(),ios_base::out);
	s<<"Console functionality not implemented\r\n";
	socket->MemoryOut().IncreaseUsed(s.pcount());
}
/***************************************************************************/
void GServerBaseConsole::ProcessPoll()
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
			case ESTAcceptBaseConsole:AnalizeAcceptConsole(ev);break;
			case ESTClientBaseConsole:AnalizeClientConsole(ev);break;
			case ESTInternalClient:AnalizeServerClient(ev);break;
			default:
				{SRAP(ERROR_UNKNOWN);RAPORT("%ld - UNKNOWN SERVICE",GSTR(ERROR_UNKNOWN));};
				break;
			}
		}
		poll_manager.ForceWritePoll();
	}
}
/***************************************************************************/
GSocketConsole * GServerBaseConsole::SocketConsoleAdd()
{
	GSocketConsole * socket=NULL;
	try{
		socket=new GSocketConsole();
		SRAP(INFO_CONSOLE_NEW);
	}
	catch(const std::bad_alloc &)
	{
		SRAP(ERROR_CONSOLE_NEW_ALLOCATION_ERROR);
		RAPORT(GSTR(ERROR_CONSOLE_NEW_ALLOCATION_ERROR));
		socket=NULL;
	};
	if (socket)
	{
		socket->socketid = seq_socket_id++;
		socket_console_map.insert(pair<INT32, GSocket *>(socket->socketid, socket));
	}
	return socket;
}
/***************************************************************************/
void GServerBaseConsole::SocketConsoleRemove(GSocketConsole * socket)
{
	socket_console_map.erase(socket->socketid);
	SocketConsoleDestroy(socket);
	SRAP(INFO_CONSOLE_DELETE);
}
/***************************************************************************/
void GServerBaseConsole::SocketConsoleDestroy(GSocketConsole * socket)
{
	socket->Close();
	Delete(socket);
	SRAP(INFO_CONSOLE_DELETE);
}
/***************************************************************************/
void GServerBaseConsole::Destroy()
{
	GServerBase::TSocketIdMap::iterator its;
	for (its = socket_console_map.begin(); its != socket_console_map.end(); its++)
	{
		SocketConsoleDestroy(static_cast<GSocketConsole *>(its->second));
	}
	socket_console_map.clear();
}
/***************************************************************************/
void GServerBaseConsole::ProcessConsoleTasks()
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
		if (task.type == ECTT_Response)
		{
			GServerBase::TSocketIdMap::iterator its;
			its = socket_console_map.find(task.socketid);
			if (its != socket_console_map.end())
			{
				GSocketConsole * socket = static_cast<GSocketConsole *>(its->second);

				socket->MemoryOut().AddString(task.result);
				if (--socket->awaiting_console_tasks_responses_count == 0)
				{
					socket->MemoryOut().AddString("---END---\r\n");
				}
				*socket->MemoryOut().End() = 0;

				while (socket->awaiting_console_tasks_responses_count == 0 && !socket->pending_console_commands.empty())
				{
					string cmd = socket->pending_console_commands.front();
					socket->pending_console_commands.pop();
					ExecuteConsoleCommand(socket, cmd);
				}
				socket->TestWrite();
			}
		}
	}
}
/***************************************************************************/

/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"

/***************************************************************************/
GPollManager::GPollManager():force_write(NET_WRITE_ROOM_LIMIT)
{
	GSocket * empty;
	GSocket * del;
	empty=(GSocket *)0;
	del=(GSocket *)1;
	force_write.set_empty_key(empty);
	force_write.set_deleted_key(del);
	force_write.resize(0);

	maxsize=0;
	events=NULL;
	epfd=0;
	count=0;
	raport = NULL;
}
/***************************************************************************/
bool GPollManager::Init(int maxsize_p)
{
	maxsize=maxsize_p;
	events=new epoll_event[maxsize];
#ifndef EPOLL_EMULATION
	epfd=epoll_create(maxsize);
#else
	epoll_ctl.reserve(maxsize);
#endif
	//ENOMEM - tylko ten blad moze byc zgloszony, wiec nie wnikamy w detale..
	if(epfd==-1) return false;
	return true;
}
/***************************************************************************/
void GPollManager::Destroy()
{
#ifndef EPOLL_EMULATION
	if(epfd!=0)	close(epfd);
	epfd=0;
#endif
	DeleteTbl(events);
}
/***************************************************************************/
GPollManager::~GPollManager()
{
	Destroy();
}
/***************************************************************************/
#ifdef EPOLL_EMULATION
class isSocketEPoll
{
public:
	isSocketEPoll(const GSocket * t){GS=t;};
	bool operator()(epoll_event & c)
	{
		return (c.data.ptr==GS);
	};
private:
	const GSocket * GS;
};
#endif
/***************************************************************************/
bool GPollManager::AddPoll(GSocket * socket)
{
	if(count>=maxsize) 
		return false;
	epoll_event ev;
	memset(&ev,0,sizeof(ev));
	ev.data.ptr=(void*)socket;
	ev.events=EPOLLIN|EPOLLERR|EPOLLHUP;
#ifndef EPOLL_EMULATION
	int ret=epoll_ctl(epfd,EPOLL_CTL_ADD,socket->data.socket,&ev);
	if(ret==-1)
	{
		switch(errno)
		{
		case EBADF:		
			SRAP(ERROR_SOCKET_EPOLL_BADF);
			raport(GSTR(ERROR_SOCKET_EPOLL_BADF));
			break;
		case EPERM:
			SRAP(ERROR_SOCKET_EPOLL_EPERM);
			raport(GSTR(ERROR_SOCKET_EPOLL_EPERM));
			break;
		case EINVAL:
			SRAP(ERROR_SOCKET_EPOLL_EINVAL);
			raport(GSTR(ERROR_SOCKET_EPOLL_EINVAL));
			break;
		case ENOMEM:
			SRAP(ERROR_SOCKET_EPOLL_ENOMEM);
			raport(GSTR(ERROR_SOCKET_EPOLL_ENOMEM));
			break;
		default:
			SRAP(ERROR_SOCKET_EPOLL_UNKNOWN);
			raport(GSTR(ERROR_SOCKET_EPOLL_UNKNOWN));
			break;
		}
		return false;
	}
	count++;
#else
	epoll_ctl.push_back(ev);
	count=(int)epoll_ctl.size();
#endif
	return true;
}
/***************************************************************************/
bool GPollManager::RemovePoll(GSocket * socket)
{
	force_write.erase(socket);
	epoll_event ev;
	memset(&ev,0,sizeof(ev));
	ev.data.ptr=(void*)socket;
	ev.events=0;
#ifndef EPOLL_EMULATION
	int ret=epoll_ctl(epfd,EPOLL_CTL_DEL,socket->data.socket,&ev);
	if(ret==-1)
	{
		switch(errno)
		{
		case EBADF:		
			SRAP(ERROR_SOCKET_EPOLL_BADF);
			raport(GSTR(ERROR_SOCKET_EPOLL_BADF));
			break;
		case EPERM:
			SRAP(ERROR_SOCKET_EPOLL_EPERM);
			raport(GSTR(ERROR_SOCKET_EPOLL_EPERM));
			break;
		case EINVAL:
			SRAP(ERROR_SOCKET_EPOLL_EINVAL);
			raport(GSTR(ERROR_SOCKET_EPOLL_EINVAL));
			break;
		case ENOMEM:
			SRAP(ERROR_SOCKET_EPOLL_ENOMEM);
			raport(GSTR(ERROR_SOCKET_EPOLL_ENOMEM));
			break;
		default:
			SRAP(ERROR_SOCKET_EPOLL_UNKNOWN);
			raport(GSTR(ERROR_SOCKET_EPOLL_UNKNOWN));
			break;
		}
		return false;
	}
	count--;
#else
	vector<epoll_event>::iterator pos;
	pos=find_if(epoll_ctl.begin(),epoll_ctl.end(),isSocketEPoll(socket));
	if(pos!=epoll_ctl.end()) epoll_ctl.erase(pos);
	count=(int)epoll_ctl.size();
#endif
	return true;
}
/***************************************************************************/
bool GPollManager::TestWritePoll(GSocket * socket,bool mode)
{
	epoll_event ev;
	memset(&ev,0,sizeof(ev));
	ev.data.ptr=(void*)socket;
	ev.events=EPOLLIN|EPOLLERR|EPOLLHUP;
	if(mode) ev.events|=EPOLLOUT;
#ifndef EPOLL_EMULATION
	int ret=epoll_ctl(epfd,EPOLL_CTL_MOD,socket->data.socket,&ev);
	if(ret==-1)
	{
		switch(errno)
		{
		case EBADF:		
			SRAP(ERROR_SOCKET_EPOLL_BADF);
			raport(GSTR(ERROR_SOCKET_EPOLL_BADF));
			break;
		case EPERM:
			SRAP(ERROR_SOCKET_EPOLL_EPERM);
			raport(GSTR(ERROR_SOCKET_EPOLL_EPERM));
			break;
		case EINVAL:
			SRAP(ERROR_SOCKET_EPOLL_EINVAL);
			raport(GSTR(ERROR_SOCKET_EPOLL_EINVAL));
			break;
		case ENOMEM:
			SRAP(ERROR_SOCKET_EPOLL_ENOMEM);
			raport(GSTR(ERROR_SOCKET_EPOLL_ENOMEM));
			break;
		}
		return false;
	}
#else
	vector<epoll_event>::iterator pos;
	pos=find_if(epoll_ctl.begin(),epoll_ctl.end(),isSocketEPoll(socket));
	if(pos!=epoll_ctl.end()) epoll_ctl.erase(pos);
	epoll_ctl.push_back(ev);
#endif
	return true;
}
/***************************************************************************/
int GPollManager::Poll()
{
	if(count==0) 
	{
		Sleep(100);
		return 0;
	}
#ifndef EPOLL_EMULATION
	int ret=epoll_wait(epfd,events,maxsize,101);
	if(ret==-1)
	{
		switch(errno)
		{
		case EBADF:		
			SRAP(ERROR_SOCKET_EPOLL_BADF);
			raport(GSTR(ERROR_SOCKET_EPOLL_BADF));
			break;
		case EINVAL:
			SRAP(ERROR_SOCKET_EPOLL_EINVAL);
			raport(GSTR(ERROR_SOCKET_EPOLL_EINVAL));
			break;
		case EFAULT:
			SRAP(ERROR_SOCKET_EPOLL_EFAULT);
			raport(GSTR(ERROR_SOCKET_EPOLL_EFAULT));
			break;
		}
		return -1;
	}
	return ret;
#else
	timeval tv;
	fd_set read_set;
	fd_set write_set;
	fd_set error_set;
	FD_ZERO(&read_set);
	FD_ZERO(&write_set);
	FD_ZERO(&error_set);

	tv.tv_sec=0;
	tv.tv_usec=100;

	unsigned int a;
	for (a=0;a<epoll_ctl.size();a++)
	{
		int ev=epoll_ctl[a].events;
		GSocket * s=reinterpret_cast<GSocket*>(epoll_ctl[a].data.ptr);
		SOCKET fd=s->GetSocket();
#pragma warning( disable : 4127 )
		if(ev&EPOLLIN) 
			FD_SET(fd,&read_set);
		if(ev&(EPOLLERR|EPOLLHUP)) 
			FD_SET(fd,&error_set);
		if(s->Flags()[ESockControlWrite])
			FD_SET(fd,&write_set);
#pragma warning( default: 4127 )
	}
	select(0,&read_set,&write_set,&error_set,&tv);

	int position=0;
	for (a=0;a<epoll_ctl.size();a++)
	{
		epoll_event ev;
		ev.data.fd=0;
		ev.events=0;
		GSocket * s=reinterpret_cast<GSocket*>(epoll_ctl[a].data.ptr);
		SOCKET fd=s->GetSocket();
		if(epoll_ctl[a].events&EPOLLIN) 
			if(FD_ISSET(fd,&read_set)) 
				ev.events|=EPOLLIN;
		if(epoll_ctl[a].events&(EPOLLOUT)) 
			if(FD_ISSET(fd,&write_set)) 
				ev.events|=EPOLLOUT;
		if(epoll_ctl[a].events&(EPOLLERR|EPOLLHUP)) 
			if(FD_ISSET(fd,&error_set)) 
				ev.events|=EPOLLERR;

		if(ev.events)
		{
			ev.data.ptr=epoll_ctl[a].data.ptr;
			events[position++]=ev;
		}
	}
	return position;
#endif
}
/***************************************************************************/
void GPollManager::AddForceWritePoll(GSocket * socket)
{
	force_write.insert(socket);
};
/***************************************************************************/
void GPollManager::RemoveForceWritePoll(GSocket * socket)
{
	TSocketHash::iterator pos;
	if(force_write.find(socket)==force_write.end()) return;
	force_write.erase(socket);
}
/***************************************************************************/
void GPollManager::ForceWritePoll()
{
	if(!force_write.size()) return;
	for_each(force_write.begin(),force_write.end(),boost::bind(&GSocket::TestWrite,_1));
	force_write.clear();
}
/***************************************************************************/







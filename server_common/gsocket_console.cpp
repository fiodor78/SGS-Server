/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"

/***************************************************************************/
GSocketConsole::GSocketConsole() : GSocket()
{
	awaiting_console_tasks_responses_count = 0;
}
/***************************************************************************/
bool GSocketConsole::Read()
{
	if(!memory_in.GetAllocated()) AllocateIn(K4);

	int size=recv(data.socket,memory_in.End(),memory_in.Free(),0);
	SVAL(INFO_NET_IN_CONSOLE,size);
	if (size==SOCKET_ERROR || size==0)
	{
		flags.set(ESockExit);
		return false;
	}
	else
	{
		memory_in.IncreaseUsed(size);
		return true;
	}
}
/***************************************************************************/
void GSocketConsole::Write()
{
	int size=send(data.socket,memory_out.Start(),memory_out.Used(),0);
	SVAL(INFO_NET_OUT_CONSOLE,size);
	if (size==SOCKET_ERROR)
	{
#ifdef LINUX
		if(errno!=EAGAIN && errno!=EWOULDBLOCK) 
#else
		if(GET_SOCKET_ERROR()!=EWOULDBLOCK) 
#endif
			flags.set(ESockExit);
	}
	else
	if(size==0)
	{
		flags.set(ESockExit);
	}
	else
	{
		if(memory_out.Move(size))
		{
			DeallocateOut();
		}
	}
	if(memory_out.Used()) 
	{
		flags.set(ESockControlWrite);
		poll_manager->TestWritePoll(this,true);
	}
	else
	{
		if(flags[ESockControlWrite])
		{
			poll_manager->TestWritePoll(this,false);
			flags.reset(ESockControlWrite);
		}
	}
}
/***************************************************************************/

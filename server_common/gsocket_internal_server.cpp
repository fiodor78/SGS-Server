/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"
#include "gmsg_internal.h"

extern GNetParserBase	msg_base;
/***************************************************************************/
void GSocketInternalServer::Parse(GRaportInterface & raport_interface)
{
	INT32 pre_message=0;
	while(msg.Parse(pre_message))
	{
		DWORD64 time1=GetTime();

		INT32 message=0;
		msg.RI(message);
		if(!msg.GetErrorCode())
		switch(message)
		{
			case IGMI_CONNECT:
				{
					unsigned int host;
					unsigned short port;
					msg.RT(data.game_name).RI(data.game_instance).RUI(host).RUS(port).RI(data.group_id);
					data.global_group_id = GLOBAL_GROUP_ID(host, port, data.group_id);
					msg.WI(IGMI_ACCEPT).A();
					SRAP(INFO_INTERNAL_CONNECTION_ESTABILISHED);
					RAPORT(GSTR(INFO_INTERNAL_CONNECTION_ESTABILISHED));
				}
				break;
			case IGMI_PING:
				{
					msg.WI(IGMI_PING).A();
				}
				break;
			case IGMI_DISCONNECT:
				{
					SRAP(ERROR_INTERNAL_CONNECTION_BROKEN);
					RAPORT(GSTR(ERROR_INTERNAL_CONNECTION_BROKEN));
				}
				break;
			default:
				{
					if(message>IGMI_FIRST && message<IGMI_LAST) if(ProcessInternalMessage(message)) break;
					if(message>IGMIT_FIRST && message<IGMIT_LAST) if(ProcessTargetMessage(message)) break;
					SRAP(ERROR_INTERNAL_MSG_UNKNOWN);
					RAPORT("%s %d",GSTR(ERROR_INTERNAL_MSG_UNKNOWN),message);
					WriteCore();
				}
				break;
		}

		time1=GetTime()-time1;
		if (time1 >= GSECOND_1)
		{
			SNetMsgDesc * format;
			format = msg_base.Get(message, ENetCmdInternal);
			string m;
			if (format)
			{
				m = format->name;
			}

			SRAP(WARNING_INTERNAL_MSG_PARSE_LONG_TIME);
			RAPORT(GSTR(WARNING_INTERNAL_MSG_PARSE_LONG_TIME));
			RAPORT("INTERNAL Message %d/%s Parse Time Is %lld ms",message,m.c_str(),time1);
		}
	}
	if(msg.In()->Used())
	{
		SNetMsgDesc * format;
		int chunk_size;
		format = msg_base.Get(pre_message, ENetCmdInternal);
		string m;
		if (format)
		{
			m = format->name;
		}
		chunk_size = *(int*)(msg.In()->Start());
		// Raportujemy tylko duze chunki, zeby byla informacja w logach o duzych messagach przychodzacych w kawalkach.
		if (chunk_size >= K128)
		{
			RAPORT("INTERNAL Message %d/%s Not Parsed %d, %d/%d, %d", pre_message, m.c_str(), msg.In()->Size(), msg.In()->Used(), chunk_size, msg.GetErrorCode());
		}
	}

	//UWAGA - ta dealokacja tutaj byla bez sensu, po co ciagle to dealokowac skoro pracujemy na 1 buforze?
	if(memory_in.Size()>K128) 
	{
		if(memory_in.Used()==0) 
		{
			DeallocateIn();
			RAPORT("INTERNAL Memory Deallocation");
		}
	}

	TestWrite();

	if(AnalizeMsgError(raport_interface)) flags.set(ESockError);
}
/***************************************************************************/
bool GSocketInternalServer::ProcessInternalMessage(int message)
{
	if(!msg_callback_internal)
	{
		SRAP(ERROR_UNKNOWN);
		RAPORT("ERROR Undefined callback");
		return false;
	}
	else
	{
		if(!msg_callback_internal(this,msg,message))
		{
			SRAP(ERROR_INTERNAL_MSG_UNKNOWN);
			RAPORT("%s %d",GSTR(ERROR_INTERNAL_MSG_UNKNOWN),message);
			return false;
		}
	}
	return true;
}
/***************************************************************************/
bool GSocketInternalServer::ProcessTargetMessage(int message)
{
	GNetTarget target;
	msg.R(target);

	if (message == IGMIT_PROCESSED)
	{
		// Jak wyslalismy do jakiegos serwisu message IGMIT_* nie poprzez mechanizm Ticket() tylko poprzez MsgExt()
		// to mozemy olac odpowiedz IGMIT_PROCESSED.
		return true;
	}
	if(!msg_callback_target)
	{
		SRAP(ERROR_UNKNOWN);
		RAPORT("ERROR Undefined callback");
		return false;
	}
	else
	{
		if(!msg_callback_target(this,msg,message,target))
		{
			SRAP(ERROR_INTERNAL_MSG_UNKNOWN);
			RAPORT("%s %d",GSTR(ERROR_INTERNAL_MSG_UNKNOWN),message);
			return false;
		}
	}
	return true;
}
/***************************************************************************/

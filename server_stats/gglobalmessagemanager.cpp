/***************************************************************************
***************************************************************************
(c) 1999-2006 Ganymede Technologies                 All Rights Reserved
Krakow, Poland                                  www.ganymede.eu
***************************************************************************
***************************************************************************/

#include "headers_all.h"
#include "gconsole.h"
#include "gserver_base.h"
#include "gserver.h"
#include "gtickets.h"
#include "gserver_local.h"
#include "gmsg_internal.h"

extern GServerBase * global_server;

/***************************************************************************/
GGlobalMessageManager::GGlobalMessageManager()
{
	console.SetServer(this);
}
/***************************************************************************/
void GGlobalMessageManager::WakeUpService()
{
	globalSystemRequests.clear();
	signal_seconds_15.connect(boost::bind(&GGlobalMessageManager::ClearExpiredRequests, this));
}
/***************************************************************************/
bool GGlobalMessageManager::ProcessInternalMessage(GSocket * socket, GNetMsg & msg, int message)
{
	switch (message)
	{
		case IGMI_GLOBAL_MESSAGE:
		{
			string command;
			string params;
			bool system_message;
			msg.RT(command).RT(params).Rb(system_message);

			ProcessGlobalMessage(command, params);
		}
		return true;

		case IGMI_SYSTEM_GLOBAL_MESSAGE_RESULT:
		{
			string result;
			msg.RT(result);

			GlobalRequests::iterator it = globalSystemRequests.find(socket->socketid);
			if (it != globalSystemRequests.end())
			{
				SConsoleTask task;
				task.result = result;
				task.socketid = it->second.first;
				task.type = ECTT_Response;
				InsertConsoleTask(task);

				globalSystemRequests.erase(it);
			}
		}
		return true;
	}
	return false;
	
};
/***************************************************************************/
bool GGlobalMessageManager::ProcessTargetMessage(GSocket *socket, GNetMsg & msg, int message, GNetTarget & target)
{
	return false;
};
/***************************************************************************/
void GGlobalMessageManager::ProcessGlobalMessage(const std::string & command, const std::string & params)
{
	for (TSocketIdMap::iterator it = socket_base_map.begin(); it != socket_base_map.end(); ++it)
	{
		GSocket * socket_group = it->second;
		if (socket_group)
		{
			socket_group->MsgExt(IGMI_GLOBAL_MESSAGE).WT(command).WT(params).Wb(false).A();
		}
	}
}
/***************************************************************************/
void GGlobalMessageManager::ProcessSystemGlobalMessage(strstream & s, GSocketConsole * socket_console, const std::string & command, const std::string & params)
{
	INT32 requests_sent = 0;

	for (TSocketIdMap::iterator it = socket_base_map.begin(); it != socket_base_map.end(); ++it)
	{
		GSocket * socket_group = it->second;
		if (socket_group)
		{
			if (!globalSystemRequests.count(socket_group->socketid))
			{
				globalSystemRequests.insert(make_pair(socket_group->socketid, make_pair(socket_console->socketid, CurrentSTime() + 30)));
				socket_group->MsgExt(IGMI_GLOBAL_MESSAGE).WT(command).WT(params).Wb(true).A();
				requests_sent++;
			}
			else
			{
				GSocketInternalServer * server = static_cast<GSocketInternalServer*>(socket_group);
				s << "---- " << GroupInfoToString(server->data.global_group_id) << "----\r\n";
				s << "ERROR: Message not sent, because we are still waiting for a previous response.\r\n";
			}
		}
	}
	socket_console->awaiting_console_tasks_responses_count = requests_sent;
}
/***************************************************************************/
void GGlobalMessageManager::WriteGroupsInfo(strstream& s)
{
	s << "Groups connected: " << socket_base_map.size() << "\r\n";
}
/***************************************************************************/
void GGlobalMessageManager::ClearExpiredRequests()
{
	for (GlobalRequests::iterator it_request = globalSystemRequests.begin(); it_request != globalSystemRequests.end();)
	{
		if (CurrentSTime() > it_request->second.second)
		{
			SConsoleTask task;

			TSocketIdMap::iterator it_socket_group = socket_base_map.find(it_request->first);
			if (it_socket_group != socket_base_map.end())
			{
				GSocketInternalServer * server = static_cast<GSocketInternalServer*>(it_socket_group->second);
				task.result = str(boost::format("ERROR: Task expired: %s\r\n") % GroupInfoToString(server->data.global_group_id));
			}
			else
			{
				task.result = "ERROR: Task expired.\r\n";
			}

			task.socketid = it_request->second.first;
			task.type = ECTT_Response;
			InsertConsoleTask(task);

			globalSystemRequests.erase(it_request++);
		}
		else
		{
			++it_request;
		}
	}
}
/***************************************************************************/
string GGlobalMessageManager::GroupInfoToString(DWORD64 global_group_id)
{
	DWORD32 host = 0;
	INT32 port = 0;
	INT32 group_id = 0;

	group_id = (INT32) (global_group_id & 0xffff);
	global_group_id >>= 16;
	port = (INT32) (global_group_id & 0xffff);
	global_group_id >>= 16;
	host = (DWORD32) (global_group_id & 0xffffffff);

	return str(boost::format("%16s:%-6d Group id = %-3d") % AddrToString(host)() % port % group_id);
}
/***************************************************************************/

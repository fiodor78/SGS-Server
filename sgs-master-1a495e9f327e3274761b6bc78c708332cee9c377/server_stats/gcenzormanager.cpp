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
GCenzorManager::GCenzorManager()
{
	console.SetServer(this);
	single_instance_service = false;
}
/***************************************************************************/
bool GCenzorManager::Init()
{
	if (cenzor.Init(global_serverconfig->vulgar_path))
	{
		return GServerManager::Init();
	}
	return false;
}
/***************************************************************************/
bool GCenzorManager::ProcessInternalMessage(GSocket * socket, GNetMsg & msg, int message)
{
	switch (message)
	{
		case IGMI_CENZOR_MESSAGE:
		{
			string text;
			string payload_1;
			string payload_2;

			msg.RT(text).RT(payload_1).RT(payload_2);
			cenzor.Test(text);
			msg.WI(IGMI_CENZOR_MESSAGE).WT(text).WT(payload_1).WT(payload_2).A();
			return true;
		}
	}
	return false;
};
/***************************************************************************/
bool GCenzorManager::ProcessTargetMessage(GSocket *socket, GNetMsg & msg, int message, GNetTarget & target)
{
	return false;
};
/***************************************************************************/

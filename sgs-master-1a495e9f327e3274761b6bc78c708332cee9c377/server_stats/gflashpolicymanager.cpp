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
#include "utils_conversion.h"
extern GServerBase * global_server;

using namespace boost::spirit;




static char * flash_policy_request = "<policy-file-request/>";

static char * flash_policy_response = 
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
"<!DOCTYPE cross-domain-policy SYSTEM \"http://www.macromedia.com/xml/dtds/cross-domain-policy.dtd\">\r\n"
"<cross-domain-policy>\r\n"
"<allow-access-from domain=\"*\" to-ports=\"*\" />\r\n"
"</cross-domain-policy>\r\n";

/***************************************************************************/
void GSocketFlashPolicyServer::Parse(GRaportInterface & raport_interface)
{
	// Obsluga <policy-file-request/>
	// Moze on przyjsc tylko na samym poczatku polaczenia. Rozpoznajemy to po tym,
	// ze gdy juz sparsowalismy jakis message to msg.CurrentChunk() bedzie nie pusty.
	if (msg.CurrentChunk().size == 0 && !(flags[ESockExit] || flags[ESockDisconnect]))
	{
		unsigned int len = strlen(flash_policy_request) ;
		if (memory_in.Used() >= len &&
			memory_in.ToParse() >= len &&
			strncmp(memory_in.Parse(), flash_policy_request, len) == 0)
		{
			memory_in.IncreaseParsed(len);
			memory_in.RemoveParsed();

			len = strlen(flash_policy_response) + 1;
			if (memory_out.Free() < len)
			{
				memory_out.ReallocateToFit(len);
			}
			memcpy(memory_out.End(), flash_policy_response, len);
			memory_out.IncreaseUsed(len);
		}
	}

	if(memory_in.Used()==0) DeallocateIn();
	TestWrite();

	if(AnalizeMsgError(raport_interface))
	{
		flags.set(ESockError);
	}
}
/***************************************************************************/
GFlashPolicyManager::GFlashPolicyManager()
{
	console.SetServer(this);
	single_instance_service = false;
}
/***************************************************************************/
bool GFlashPolicyManager::ProcessInternalMessage(GSocket *socket,GNetMsg & msg,int message)
{
	return false;
};
/***************************************************************************/
bool GFlashPolicyManager::ProcessTargetMessage(GSocket *socket,GNetMsg & msg,int message,GNetTarget & target)
{
	return false;
};
/***************************************************************************/

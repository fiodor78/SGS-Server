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


/***************************************************************************/
GAccessManager::GAccessManager():sessions(10000),sessions_rev(10000)
{
	sessions.set_empty_key(-1);
	sessions.set_deleted_key(-2);

	sessions_rev.set_empty_key(SSessionRev("empty"));
	sessions_rev.set_deleted_key(SSessionRev("del"));

	instances.clear();

	social_friends_map.clear();
	social_friends_map_timestamp.clear();

	tasks_registration.clear();

	console.SetServer(this);
}
/***************************************************************************/
void GAccessManager::WakeUpService()
{
	GetSessionsFromSQL();
	signal_minute.connect(boost::bind(&GAccessManager::UpdateSession,this));
}
/***************************************************************************/
void GAccessManager::ConfigureClock()
{
	GServerManager::ConfigureClock();
	signal_second.connect(boost::bind(&GAccessManager::ProcessExpiredRegistrationTasks,this));
	signal_minutes_5.connect(boost::bind(&GAccessManager::ClearExpiredSocialFriends,this));
	signal_minutes_15.connect(boost::bind(&GAccessManager::ClearFailedLoginBans,this));
}
/***************************************************************************/

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

#pragma warning(disable: 4239)
#pragma warning (disable: 4996)
/***************************************************************************/
void GAccessManager::GetSessionsFromSQL()
{
	DWORD64 now = CurrentSTime();

	sessions.clear();
	sessions_rev.clear();
	sessions.resize(0);
	sessions_rev.resize(0);

	char command[256];
	sprintf(command,"SELECT `user_id`,`expiration`, `sid` FROM `rtm_sessions`");
	int count=0;
	boost::recursive_mutex::scoped_lock lock(mtx_sql);
	if(MySQLQuery(&mysql,command))
	{
	MYSQL_RES * result=mysql_store_result(&mysql);
	if (result)
	{
		MYSQL_ROW row;
		row=mysql_fetch_row(result);
		while(row)
		{
			SSession s;
			INT32 ID=ATOI(row[0]);
			s.ID = ID;
			s.expiration=ATOI64(row[1]);
			strncpy(s.SID,row[2],MAX_SID);
			s.SID[MAX_SID-1]=0;
			// Dajemy serwerom gry 5 minut na podlaczenie sie do tego accessu i odswiezenie informacji o instancjach graczy.
			s.last_connection = now - 60 * TIME_MAX_SESSION_HASH + 60 * 5;
			sessions.insert(pair<INT32,SSession>(ID,s));
			sessions_rev.insert(pair<SSessionRev,INT32>(SSessionRev(s.SID),ID));
			count++;
			row=mysql_fetch_row(result);
		}
		mysql_free_result(result);
	}
	}

	RL("%d sessions loaded from SQL",count);
}
/***************************************************************************/
void GAccessManager::AddSessionFromSQL(INT32 ID,SSession s)
{
	SRAP(INFO_ACCESS_ADD_SESSION_FROM_SQL);
	s.ID = ID;
	s.last_connection = CurrentSTime();
	sessions[ID] = s;
	sessions_rev[s.SID] = ID;
}
/***************************************************************************/
void GAccessManager::UpdateSession()
{
	sessions.resize(0);
	sessions_rev.resize(0);

	DWORD64 now=CurrentSTime();
	DWORD64 expiration = now + 60 * TIME_MAX_SESSION_SQL;
	DWORD64 remove = now - 60 * TIME_MAX_SESSION_HASH;

	TSessionMap::iterator pos;
	for (pos=sessions.begin();pos!=sessions.end();)
	{
		SSession & session=pos->second;

		if (session.last_connection < remove && instances.count(session.ID) == 0)
		{
			sessions_rev.erase(session.SID);
			sessions.erase(pos++);
		}
        else
		{
			if (session.expiration < now + 60*TIME_MAX_SESSION_SQL/2)
			{
				session.expiration = expiration;
				boost::format txt("INSERT INTO `rtm_sessions` (`user_id`, `expiration`, `sid`) "
									"VALUES ('%d', '%lld', '%s') ON DUPLICATE KEY UPDATE `expiration` = '%lld', `sid` = '%s'");
				txt % pos->first % session.expiration % session.SID % session.expiration % session.SID;
				global_sqlmanager->Add(txt.str().c_str(), EDB_SERVERS);
			}
			++pos;
		}
	}
	boost::format txt("DELETE FROM rtm_sessions WHERE `expiration`<'%lld'");
	txt % now;
	global_sqlmanager->Add(txt.str().c_str(), EDB_SERVERS);
}
/***************************************************************************/

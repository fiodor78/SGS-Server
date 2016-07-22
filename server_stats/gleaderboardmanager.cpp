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

// Uwaga! Ponizsza wartosc nie moze byc wieksza niz rozmiar kolumny `scores`/sizeof(INT32) w tabeli `data_leaderboard_scores`
#define MAX_SCORES_COUNT							12

/***************************************************************************/
GLeaderBoardManager::GLeaderBoardManager()
{
	lb_cache.clear();
	console.SetServer(this);
}
/***************************************************************************/
void GLeaderBoardManager::ConfigureClock()
{
	GServerManager::ConfigureClock();
	signal_minutes_5.connect(boost::bind(&GLeaderBoardManager::ClearExpiredData, this));
}
/***************************************************************************/
bool GLeaderBoardManager::ProcessInternalMessage(GSocket * socket, GNetMsg & msg, int message)
{
	return false;
};	
/***************************************************************************/
bool GLeaderBoardManager::ProcessTargetMessage(GSocket *socket,GNetMsg & msg,int message,GNetTarget & target)
{
	msg.WI(IGMIT_PROCESSED).W(target).A();

	switch (message)
	{
	case IGMITIC_LEADERBOARD_GET:
		{
			string lb_key;
			DWORD64 lb_subkey;
			INT32 user_id, count, a;
			vector<INT32> users_id;
			vector<pair<INT32, INT32> > result;

			msg.RT(lb_key).RBUI(lb_subkey).RI(user_id).RI(count);
			users_id.resize(1 + count);
			users_id[0] = user_id;
			for (a = 1; a <= count; a++)
			{
				msg.RI(users_id[a]);
			}
			bool global_rankings = (lb_key.substr(0, 7) == "GLOBAL:");

			result.resize(MAX_SCORES_COUNT);
			if (!global_rankings)
			{
				LeaderBoardGet(lb_key, lb_subkey, users_id, result);
			}
			else
			{
				string lb_key_stripped = lb_key.substr(7);
				LeaderBoardGetGlobal(lb_key_stripped, lb_subkey, result);
			}

			count = result.size();
			msg.WI(IGMIT_LEADERBOARD_DATA).W(target).WT(lb_key).WBUI(lb_subkey).WI(user_id);
			msg.WI(count);
			for (a = 0; a < count; a++)
			{
				msg.WI(result[a].first).WI(result[a].second);
			}
			msg.A();
		}
		return true;

	case IGMITIC_LEADERBOARD_GET_STANDINGS:
		{
			string lb_key;
			DWORD64 lb_subkey;
			INT32 user_id, standings_index, max_results, count, a;
			vector<INT32> users_id;
			vector<pair<INT32, INT32> > result;

			msg.RT(lb_key).RBUI(lb_subkey).RI(user_id).RI(standings_index).RI(max_results).RI(count);
			users_id.resize(1 + count);
			users_id[0] = user_id;
			for (a = 1; a <= count; a++)
			{
				msg.RI(users_id[a]);
			}
			bool global_rankings = (lb_key.substr(0, 7) == "GLOBAL:");

			result.clear();
			if (standings_index >= 0 && standings_index < MAX_SCORES_COUNT)
			{
				if (!global_rankings)
				{
					LeaderBoardGetStandings(lb_key, lb_subkey, users_id, standings_index, result);
					stable_sort(result.begin(), result.end());
					reverse(result.begin(), result.end());
				}
				else
				{
					string lb_key_stripped = lb_key.substr(7);
					LeaderBoardGetStandingsGlobal(lb_key_stripped, lb_subkey, standings_index, max_results, result);
				}
			}

			count = result.size();
			if (max_results != -1 && count > max_results)
			{
				count = max_results;
			}
			msg.WI(IGMIT_LEADERBOARD_DATA_STANDINGS).W(target).WT(lb_key).WBUI(lb_subkey).WI(user_id).WI(standings_index);
			msg.WI(count);
			for (a = 0; a < count; a++)
			{
				msg.WI(result[a].second).WI(result[a].first);
			}
			msg.A();
		}
		return true;

	case IGMITIC_LEADERBOARD_GET_BATCH_INFO:
		{
			string lb_key;
			DWORD64 lb_subkey;
			INT32 user_id, standings_index, count, a;
			vector<INT32> users_id;
			vector<pair<INT32, INT32> > result;

			msg.RT(lb_key).RBUI(lb_subkey).RI(user_id).RI(standings_index).RI(count);
			users_id.resize(count);
			for (a = 0; a < count; a++)
			{
				msg.RI(users_id[a]);
			}

			result.clear();
			if (standings_index >= 0 && standings_index < MAX_SCORES_COUNT && count > 0)
			{
				LeaderBoardGetStandings(lb_key, lb_subkey, users_id, standings_index, result);
			}

			count = result.size();
			msg.WI(IGMIT_LEADERBOARD_DATA_BATCH_INFO).W(target).WT(lb_key).WBUI(lb_subkey).WI(user_id).WI(standings_index);
			msg.WI(count);
			for (a = 0; a < count; a++)
			{
				msg.WI(result[a].second).WI(result[a].first);
			}
			msg.A();
		}
		return true;

	case IGMITIC_LEADERBOARD_GET_USER_POSITION:
		{
			string lb_key;
			DWORD64 lb_subkey;
			INT32 user_id, count, a;
			vector<INT32> users_id;
			vector<INT32> result;

			msg.RT(lb_key).RBUI(lb_subkey).RI(user_id).RI(count);
			users_id.resize(1 + count);
			users_id[0] = user_id;
			for (a = 1; a <= count; a++)
			{
				msg.RI(users_id[a]);
			}
			bool global_rankings = (lb_key.substr(0, 7) == "GLOBAL:");

			result.resize(MAX_SCORES_COUNT);
			if (!global_rankings)
			{
				LeaderBoardGetUserPosition(lb_key, lb_subkey, users_id, result);
			}
			else
			{
				// Nie zwracamy pozycji gracza w globalnych rankingu.
				result.clear();
			}

			count = result.size();
			msg.WI(IGMIT_LEADERBOARD_DATA_USER_POSITION).W(target).WT(lb_key).WBUI(lb_subkey).WI(user_id);
			msg.WI(count);
			for (a = 0; a < count; a++)
			{
				msg.WI(user_id).WI(result[a]);
			}
			msg.A();
		}
		return true;

	case IGMITIC_LEADERBOARD_SET:
		{
			string lb_key;
			DWORD64 lb_subkey;
			INT32 user_id, count, a;
			vector<INT32> scores;

			msg.RT(lb_key).RBUI(lb_subkey).RI(user_id).RI(count);
			scores.resize(count);
			for (a = 0; a < count; a++)
			{
				msg.RI(scores[a]);
			}
			if (count > MAX_SCORES_COUNT)
			{
				scores.resize(MAX_SCORES_COUNT);
			}

			LeaderBoardSet(lb_key, lb_subkey, user_id, scores);
		}
		return true;
	}

	return false;
};
/***************************************************************************/
void GLeaderBoardManager::LeaderBoardGet(string & lb_key, DWORD64 lb_subkey, vector<INT32> & users_id, vector<pair<INT32, INT32> > & result)
{
	INT32 high_score[MAX_SCORES_COUNT], high_score_user_id[MAX_SCORES_COUNT];
	INT32 a, b, count;

	for (a = 0; a < MAX_SCORES_COUNT; a++)
	{
		high_score[a] = high_score_user_id[a] = 0;
	}

	count = users_id.size();
	for (a = 0; a < count; a++)
	{
		INT32 user_id = users_id[a];

		vector<INT32> * vscores = GetLeaderBoardData(lb_key, lb_subkey, user_id);
		if (vscores)
		{
			INT32 size = min(vscores->size(), MAX_SCORES_COUNT);
			for (b = 0; b < size; b++)
			{
				if ((*vscores)[b] > high_score[b])
				{
					high_score[b] = (*vscores)[b];
					high_score_user_id[b] = user_id;
				}
			}
		}
	}

	count = min(result.size(), MAX_SCORES_COUNT);
	for (a = 0; a < count; a++)
	{
		result[a].first = high_score_user_id[a];
		result[a].second = high_score[a];
	}
}
/***************************************************************************/
void GLeaderBoardManager::LeaderBoardGetStandings(string & lb_key, DWORD64 lb_subkey, vector<INT32> & users_id, INT32 standings_index, vector<pair<INT32, INT32> > & result)
{
	INT32 a, count, scores_count = 0;
	result.resize(users_id.size());

	count = users_id.size();
	for (a = 0; a < count; a++)
	{
		INT32 user_id = users_id[a];

		vector<INT32> * vscores = GetLeaderBoardData(lb_key, lb_subkey, user_id);
		if (vscores)
		{
			INT32 size = min(vscores->size(), MAX_SCORES_COUNT);
			if (standings_index < size)
			{
				result[scores_count].first = (*vscores)[standings_index];
				result[scores_count].second = user_id;
				scores_count++;
			}
		}
	}
	result.resize(scores_count);
}
/***************************************************************************/
void GLeaderBoardManager::LeaderBoardGetUserPosition(string & lb_key, DWORD64 lb_subkey, vector<INT32> & users_id, vector<INT32> & result)
{
	INT32 user_scores[MAX_SCORES_COUNT], user_position[MAX_SCORES_COUNT];
	INT32 a, b, count;

	for (a = 0; a < MAX_SCORES_COUNT; a++)
	{
		user_scores[a] = 0;
		user_position[a] = 0;
	}

	count = users_id.size();
	for (a = 0; a < count; a++)
	{
		INT32 user_id = users_id[a];

		vector<INT32> * vscores = GetLeaderBoardData(lb_key, lb_subkey, user_id);
		if (vscores)
		{
			INT32 size = min(vscores->size(), MAX_SCORES_COUNT);
			for (b = 0; b < size; b++)
			{
				// Pod indeksem 0 w tablicy users_id mamy user_id gracza, ktorego pozycje chcemy wyciagnac.
				if (a == 0)
				{
					user_scores[b] = (*vscores)[b];
					if (user_scores[b] > 0)
					{
						user_position[b] = 1;
					}
				}
				else
				{
					if (user_scores[b] > 0 && (*vscores)[b] > user_scores[b])
					{
						user_position[b]++;
					}
				}
			}
		}
	}

	count = min(result.size(), MAX_SCORES_COUNT);
	for (a = 0; a < count; a++)
	{
		result[a] = user_position[a];
	}
}
/***************************************************************************/
void GLeaderBoardManager::LeaderBoardSet(string & lb_key, DWORD64 lb_subkey, INT32 user_id, vector<INT32> & scores)
{
	char command[1024], scores_string[256], * p;
	INT32 count, a, b;
	static const char * hex_chars = "0123456789ABCDEF";

	p = scores_string;
	count = min(scores.size(), MAX_SCORES_COUNT);
	for (a = 0; a < count; a++)
	{
		DWORD32 score = scores[a];
		for (b = 0; b < 4; b++)
		{
			*p++ = hex_chars[(score & 0xff) >> 4];
			*p++ = hex_chars[score & 0xf];
			score >>= 8;
		}
	}
	*p = 0;

	PutLeaderBoardDataToCache(lb_key, lb_subkey, user_id, scores);

	sprintf(command, "INSERT INTO `data_leaderboard_scores` (`user_id`, `key`, `subkey`, `scores`) "
						"VALUES ('%d', '%s', '%lld', x'%s') ON DUPLICATE KEY UPDATE `scores` = x'%s'",
						user_id, GMySQLReal(&mysql, lb_key.c_str())(), lb_subkey, scores_string, scores_string);
	global_sqlmanager->Add(command, EDB_SERVERS);
}
/***************************************************************************/
vector<INT32> * GLeaderBoardManager::GetLeaderBoardData(string & lb_key, DWORD64 lb_subkey, INT32 user_id)
{
	vector<INT32> scores;
	vector<INT32> * vscores = GetLeaderBoardDataFromCache(lb_key, lb_subkey, user_id);
	if (!vscores)
	{
		char command[256];
		sprintf(command, "SELECT `scores` FROM `data_leaderboard_scores` WHERE `user_id` = '%d' AND `key` = '%s' AND `subkey` = '%lld'",
							user_id, GMySQLReal(&mysql, lb_key.c_str())(), lb_subkey);
		boost::recursive_mutex::scoped_lock lock(mtx_sql);
		if (MySQLQuery(&mysql, command))
		{
			MYSQL_RES * result = mysql_store_result(&mysql);
			if (result)
			{
				MYSQL_ROW row;
				row = mysql_fetch_row(result);
				scores.resize(MAX_SCORES_COUNT);
				if (row && row[0])
				{
					INT32 b, * iscores = (INT32 *)row[0];
					for (b = 0; b < MAX_SCORES_COUNT; b++)
					{
						scores[b] = iscores[b];
					}
				}
				else
				{
					INT32 b;
					for (b = 0; b < MAX_SCORES_COUNT; b++)
					{
						scores[b] = 0;
					}
				}
				PutLeaderBoardDataToCache(lb_key, lb_subkey, user_id, scores);
			}
			mysql_free_result(result);
		}

		vscores = GetLeaderBoardDataFromCache(lb_key, lb_subkey, user_id);
	}

	return vscores;
}
/***************************************************************************/
vector<INT32> * GLeaderBoardManager::GetLeaderBoardDataFromCache(string & lb_key, DWORD64 lb_subkey, INT32 user_id)
{
	char key[128];
	sprintf(key, "%s_%lld", lb_key.c_str(), lb_subkey);

	TLeaderboardsMap::iterator it = lb_cache.find(key);
	if (it == lb_cache.end())
	{
		return NULL;
	}

	TPlayerScoresMap::iterator pos = (it->second).find(user_id);
	if (pos == (it->second).end())
	{
		return NULL;
	}

	return &(pos->second.first);
}
/***************************************************************************/
void GLeaderBoardManager::PutLeaderBoardDataToCache(string & lb_key, DWORD64 lb_subkey, INT32 user_id, vector<INT32> & lb_data)
{
	char key[128];
	sprintf(key, "%s_%lld", lb_key.c_str(), lb_subkey);

	TLeaderboardsMap::iterator it = lb_cache.find(key);
	if (it == lb_cache.end())
	{
		TPlayerScoresMap lb;
		lb_cache.insert(TLeaderboardsMap::value_type(key, lb));
		it = lb_cache.find(key);
		if (it == lb_cache.end())
		{
			return;
		}
	}

	DWORD64 expiration_time = CurrentSTime() + 3600 * 12;
	(it->second)[user_id] = make_pair(lb_data, expiration_time);
}
/***************************************************************************/
void GLeaderBoardManager::ClearExpiredData()
{
	DWORD64 now = CurrentSTime();

	TLeaderboardsMap::iterator it;
	for (it = lb_cache.begin(); it != lb_cache.end(); )
	{
		TPlayerScoresMap::iterator pos;
		for (pos = it->second.begin(); pos != it->second.end(); )
		{
			if (pos->second.second < now)
			{
				it->second.erase(pos++);
				continue;
			}
			pos++;
		}

		if (it->second.size() == 0)
		{
			lb_cache.erase(it++);
			continue;
		}
		it++;
	}
}
/***************************************************************************/
void GLeaderBoardManager::LeaderBoardGetGlobal(string & lb_key, DWORD64 lb_subkey, vector<pair<INT32, INT32> > & result)
{
	boost::recursive_mutex::scoped_lock lock(mtx_sql);

	INT32 a, count = result.size();
	for (a = 0; a < count; a++)
	{
		result[a].first = result[a].second = 0;

		char command[512];
		sprintf(command, "SELECT `user_id`, `scores` FROM `data_leaderboard_scores` WHERE `key` = '%s' AND `subkey` = '%lld' "
							"ORDER BY SUBSTR(HEX(`scores`), %d, 2) DESC, SUBSTR(HEX(`scores`), %d, 2) DESC, SUBSTR(HEX(`scores`), %d, 2) DESC, SUBSTR(HEX(`scores`), %d, 2) DESC "
							"LIMIT %d",
							GMySQLReal(&mysql, lb_key.c_str())(), lb_subkey,
							a * 8 + 7, a * 8 + 5, a * 8 + 3, a * 8 + 1,
							(INT32)1);
		if (MySQLQuery(&mysql, command))
		{
			MYSQL_RES * mysql_result = mysql_store_result(&mysql);
			if (mysql_result)
			{
				MYSQL_ROW row;
				row = mysql_fetch_row(mysql_result);
				if (row && row[0] && row[1])
				{
					INT32 * iscores = (INT32 *)row[1];
					if (iscores[a] > 0)
					{
						result[a].first = iscores[a];
						result[a].second = ATOI(row[0]);
					}
				}
				mysql_free_result(mysql_result);
			}
		}
	}
}
/***************************************************************************/
void GLeaderBoardManager::LeaderBoardGetStandingsGlobal(string & lb_key, DWORD64 lb_subkey, INT32 standings_index, INT32 max_results, vector<pair<INT32, INT32> > & result)
{
	result.clear();
	if (standings_index >= MAX_SCORES_COUNT)
	{
		return;
	}
	if (max_results < 0)
	{
		max_results = 100;
	}

	char command[512];
	sprintf(command, "SELECT `user_id`, `scores` FROM `data_leaderboard_scores` WHERE `key` = '%s' AND `subkey` = '%lld' "
						"ORDER BY SUBSTR(HEX(`scores`), %d, 2) DESC, SUBSTR(HEX(`scores`), %d, 2) DESC, SUBSTR(HEX(`scores`), %d, 2) DESC, SUBSTR(HEX(`scores`), %d, 2) DESC "
						"LIMIT %d",
						GMySQLReal(&mysql, lb_key.c_str())(), lb_subkey,
						standings_index * 8 + 7, standings_index * 8 + 5, standings_index * 8 + 3, standings_index * 8 + 1,
						max_results);
	boost::recursive_mutex::scoped_lock lock(mtx_sql);
	if (MySQLQuery(&mysql, command))
	{
		MYSQL_RES * mysql_result = mysql_store_result(&mysql);
		if (mysql_result)
		{
			INT32 a, count = (INT32)mysql_num_rows(mysql_result);
			result.resize(count);

			for (a = 0; a < count; a++)
			{
				MYSQL_ROW row;
				row = mysql_fetch_row(mysql_result);
				if (!row || !row[0] || !row[1])
				{
					result.resize(a);
					break;
				}

				INT32 * iscores = (INT32 *)row[1];
				if (iscores[standings_index] <= 0)
				{
					result.resize(a);
					break;
				}
				result[a].first = iscores[standings_index];
				result[a].second = ATOI(row[0]);
			}
			mysql_free_result(mysql_result);
		}
	}
}
/***************************************************************************/

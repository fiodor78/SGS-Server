/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/


#include "headers_all.h"
#include "headers_game.h"


extern GServerBase * global_server;

DWORD64 GetTimeUSec();

//dokladamy gracza do grupy
/***************************************************************************/
void GGroup::PlayerAdd(GPlayer * player)
{
	index_rid.insert(make_pair(player->GetRID(),player));
}
/***************************************************************************/
//usuwamy gracza z grupy
void GGroup::PlayerDelete(GPlayer * player,EULogicMode mode)
{
	size_t count=index_rid.erase(player->GetRID());
	if(count==0) 
		WriteCore();
}
/***************************************************************************/
void GGroup::PlayerGameAdd(GPlayerGame* player)
{
	PlayerAdd(player);
	index_target.insert(TPlayerGameMultiMap::value_type(player->player_info.target_location_ID, player));
	
	if (IsLocationAssigned(player->player_info.target_location_ID) && GetScriptingEngine(player->player_info.target_location_ID))
	{
		GetScriptingEngine(player->player_info.target_location_ID)->ProcessConnectToLocation(player->player_info.ID, player->player_info.target_location_ID);
	}

	metrics.CommitCurrentValueChange("clients_connected", +1);
	if (gamestate_ID_from_location_ID(player->player_info.target_location_ID) > 0)
	{
		metrics.CommitCurrentValueChange("clients_connected_to_gamestate", +1);
	}
}
/***************************************************************************/
void GGroup::RemovePlayerFromTargetMap(GPlayerGame *player)
{
	TPlayerGameMultiMap::iterator pos;
	pair<TPlayerGameMultiMap::iterator, TPlayerGameMultiMap::iterator> p=index_target.equal_range(player->player_info.target_location_ID);
	for( pos=p.first;pos!=p.second;++pos)
	{
		GPlayerGame *pg=((*pos).second);
		if(pg==player)
		{
			index_target.erase(pos);
			return;
		}
	}
}
/***************************************************************************/
void GGroup::PlayerGameDelete(GPlayerGame* player,EULogicMode mode)
{
	if (IsLocationAssigned(player->player_info.target_location_ID) && GetScriptingEngine(player->player_info.target_location_ID))
	{
		GetScriptingEngine(player->player_info.target_location_ID)->ProcessDisconnectFromLocation(player->player_info.ID, player->player_info.target_location_ID);
	}
	PlayerDelete(player,mode);
	RemovePlayerFromTargetMap(player);

	metrics.CommitCurrentValueChange("clients_connected", -1);

	INT32 gamestate_ID = gamestate_ID_from_location_ID(player->player_info.target_location_ID);
	if (gamestate_ID > 0)
	{
		metrics.CommitCurrentValueChange("clients_connected_to_gamestate", -1);
	}
}
/***************************************************************************/
TPlayerGameRange GGroup::FindPlayers(const char * target_location_ID)
{
	return index_target.equal_range(target_location_ID);
}
/***************************************************************************/
GPlayerGame* GGroup::FindPlayer(const char * target_location_ID, int ID)
{
	TPlayerGameRange p = FindPlayers(target_location_ID);
	TPlayerGameMultiMap::iterator pos;
	for(pos=p.first;pos!=p.second;++pos)
	{
		GPlayerGame *pg=((*pos).second);
		if(pg->player_info.ID==ID)
		{
			return pg;
		}
	}
	return NULL;
}
/***************************************************************************/
GPlayerGame* GGroup::PlayerDetectOtherInstances(GPlayerGame * pg)
{
	GPlayerGame * player = FindPlayer(pg->player_info.target_location_ID, pg->player_info.ID);
	if (player && player->Valid())
	{
		GNetMsg msg(&Server()->MemoryManager());
		msg.WI(IGM_MESSAGE_DIALOG).WI(DLG_INFO_END).WT("second instance detected, disconnecting").WI(0).A();
		player->MsgExt() += msg;
	}
	return player;
}
/***************************************************************************/
/*
 *  SMetricStats
 */

SMetricStats::SCallContext SMetricStats::BeginDuration()
{
	return GetTimeUSec();
}

void SMetricStats::CommitSampleValue(DWORD64 value, const char * unit_name)
{
	if (*unit_name != 0 && *this->unit_name.c_str() == 0)
	{
		this->unit_name = unit_name;
	}

	if (count == 0 || value < min_value)
	{
		min_value = value;
	}
	if (count == 0 || value > max_value)
	{
		max_value = value;
	}
	sum += value;
	count++;
}

void SMetricStats::CommitEvent(const char * unit_name)
{
	CommitSampleValue(1, unit_name);
}

void SMetricStats::CommitDuration(SCallContext ctx)
{
	DWORD64 current_time = GetTimeUSec();
	CommitSampleValue(current_time >= ctx ? current_time - ctx : 0, "ms");
	denominator = 1000;			// wartosci czasowe wyswietlamy w ms
}

void SMetricStats::CommitCurrentValue(DWORD64 value, const char * unit_name)
{
	if (*unit_name != 0 && *this->unit_name.c_str() == 0)
	{
		this->unit_name = unit_name;
	}

	DWORD64 current_time = GetTimeUSec();

	if (count == 0 || value < min_value)
	{
		min_value = value;
	}
	if (count == 0 || value > max_value)
	{
		max_value = value;
	}
	if (count > 0)
	{
		sum += (current_time - last_value_update_time) * last_value;
		count += (current_time - last_value_update_time);
	}
	else
	{
		count = 1;
	}

	last_value = value;
	last_value_update_time = current_time;
}

void SMetricStats::CommitCurrentValueChange(INT64 change, const char * unit_name)
{
	CommitCurrentValue(last_value + change, unit_name);
}

string SMetricStats::ToString(bool pretty_format)
{
	// min_value;max_value;sum_values;count_values;avg_values

	DWORD64 psum = sum, pcount = count;

	DWORD64 current_time = GetTimeUSec();
	if (last_value_update_time != 0)
	{
		sum += (current_time - last_value_update_time) * last_value;
		count += (current_time - last_value_update_time);
		psum = sum / 1000;
		pcount = count / 1000;
	}

	double avg_value = (count == 0) ? 0.0f : (double)sum / (double)count;

	if (denominator == 0)
	{
		denominator = 1;
	}

	char value_buffer[256];
	if (denominator == 1)
	{
		sprintf(value_buffer, pretty_format ? "%10lld %10lld %10lld %10lld %10.3lf %s" : "%lld;%lld;%lld;%lld;%.3lf;%s",
			min_value, max_value, psum, pcount,
			avg_value,
			unit_name == "" ? "-" : unit_name.c_str());
	}
	else
	{
		sprintf(value_buffer, pretty_format ? "%10.3lf %10.3lf %10.3lf %10lld %10.3lf %s" : "%.3lf;%.3lf;%.3lf;%lld;%.3lf;%s",
			(double)min_value / (double)denominator, (double)max_value / (double)denominator, (double)psum / (double)denominator, pcount,
			(double)avg_value / (double)denominator,
			unit_name == "" ? "-" : unit_name.c_str());
	}

	return value_buffer;
}
/***************************************************************************/
void SMetrics::Zero()
{
	DWORD64 current_time = GetTimeUSec();

	INT32 a;
	for (a = 0; a < 3; a++)
	{
		data[a].interval_seconds = (a == 0) ? 0 : (a == 1) ? 300 : 60;
		data[a].current_metrics_collecting_time_start = 0;

		data[a].previous_metrics.clear();

		map<string, SMetricStats>::iterator pos;
		for (pos = data[a].current_metrics.begin(); pos != data[a].current_metrics.end(); )
		{
			if (pos->second.last_value_update_time != 0)
			{
				pos->second.last_value_update_time = current_time;
				pos->second.min_value = pos->second.last_value;
				pos->second.max_value = pos->second.last_value;
				pos->second.sum = 0;
				pos->second.count = 1;
			}
			else
			{
				data[a].current_metrics.erase(pos++);
				continue;
			}
			pos++;
		}
	}
}

void SMetrics::CheckRotate()
{
	DWORD64 now = GSERVER->CurrentSTime();
	INT32 a;
	for (a = 0; a < 3; a++)
	{
		if (data[a].current_metrics_collecting_time_start == 0)
		{
			data[a].current_metrics_collecting_time_start = now;
			continue;
		}
		if (data[a].interval_seconds == 0)
		{
			continue;
		}

		if (data[a].current_metrics_collecting_time_start / data[a].interval_seconds == now / data[a].interval_seconds)
		{
			continue;
		}

		bool first_rotation = (data[a].current_metrics_collecting_time_start % data[a].interval_seconds) != 0;
		data[a].current_metrics_collecting_time_start = now - now % data[a].interval_seconds;

		DWORD64 current_time = GetTimeUSec();

		map<string, SMetricStats>::iterator pos;
		for (pos = data[a].current_metrics.begin(); pos != data[a].current_metrics.end(); pos++)
		{
			if (pos->second.last_value_update_time != 0)
			{
				pos->second.sum += (current_time - pos->second.last_value_update_time) * pos->second.last_value;
				pos->second.count += (current_time - pos->second.last_value_update_time);
				pos->second.last_value_update_time = current_time;
			}
		}

		if (!first_rotation)
		{
			data[a].previous_metrics = data[a].current_metrics;
		}

		for (pos = data[a].current_metrics.begin(); pos != data[a].current_metrics.end(); pos++)
		{
			if (pos->second.last_value_update_time != 0)
			{
				pos->second.min_value = pos->second.last_value;
				pos->second.max_value = pos->second.last_value;
				pos->second.sum = 0;
				pos->second.count = 1;
			}
			else
			{
				pos->second.Zero(true);
			}
		}
	}
}

bool SMetrics::ValidateMetricName(const char * metric_name)
{
	const char * p = metric_name;
	while (*p != 0)
	{
		char c = *p++;
		if (c < 0x20 || c > 0x7e)
		{
			return false;
		}
	}
	return true;
}

void SMetrics::CommitSampleValue(const char * metric_name, DWORD64 value, const char * unit_name)
{
	if (!ValidateMetricName(metric_name))
	{
		return;
	}

	INT32 a;
	for (a = 0; a < 3; a++)
	{
		data[a].current_metrics[metric_name].CommitSampleValue(value, unit_name);
	}
}

void SMetrics::CommitEvent(const char * metric_name, const char * unit_name)
{
	if (!ValidateMetricName(metric_name))
	{
		return;
	}

	INT32 a;
	for (a = 0; a < 3; a++)
	{
		data[a].current_metrics[metric_name].CommitEvent(unit_name);
	}
}

void SMetrics::CommitDuration(const char * metric_name, SMetricStats::SCallContext ctx)
{
	if (!ValidateMetricName(metric_name))
	{
		return;
	}

	INT32 a;
	for (a = 0; a < 3; a++)
	{
		data[a].current_metrics[metric_name].CommitDuration(ctx);
	}
}

void SMetrics::CommitCurrentValue(const char * metric_name, DWORD64 value, const char * unit_name)
{
	if (!ValidateMetricName(metric_name))
	{
		return;
	}

	INT32 a;
	for (a = 0; a < 3; a++)
	{
		data[a].current_metrics[metric_name].CommitCurrentValue(value, unit_name);
	}
}

void SMetrics::CommitCurrentValueChange(const char * metric_name, INT64 change, const char * unit_name)
{
	if (!ValidateMetricName(metric_name))
	{
		return;
	}

	INT32 a;
	for (a = 0; a < 3; a++)
	{
		data[a].current_metrics[metric_name].CommitCurrentValueChange(change, unit_name);
	}
}

void SMetrics::Print(strstream & s, INT32 interval_seconds, bool pretty_format)
{
	INT32 a;
	for (a = 0; a < 3; a++)
	{
		if (data[a].interval_seconds == interval_seconds)
		{
			if (interval_seconds == 0)
			{
				DWORD64 current_time = GetTimeUSec();

				map<string, SMetricStats>::iterator pos;
				for (pos = data[a].current_metrics.begin(); pos != data[a].current_metrics.end(); pos++)
				{
					if (pos->second.last_value_update_time != 0)
					{
						pos->second.sum += (current_time - pos->second.last_value_update_time) * pos->second.last_value;
						pos->second.count += (current_time - pos->second.last_value_update_time);
						pos->second.last_value_update_time = current_time;
					}
				}

				data[a].previous_metrics = data[a].current_metrics;
			}

			map<string, SMetricStats>::iterator pos;
			s << data[a].previous_metrics.size() << "\r\n";
			for (pos = data[a].previous_metrics.begin(); pos != data[a].previous_metrics.end(); pos++)
			{
				if (!pretty_format)
				{
					s << pos->first << ";" << pos->second.ToString(pretty_format) << "\r\n";
				}
				else
				{
					char metric_name[256];
					sprintf(metric_name, "%-47s ", pos->first.c_str());
					s << metric_name << pos->second.ToString(pretty_format) << "\r\n";
				}
			}
			break;
		}
	}
}
/***************************************************************************/

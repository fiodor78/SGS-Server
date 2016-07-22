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
extern distinct_parser<> keyword_p;
extern distinct_directive<> keyword_d;
distinct_parser<> keyword_eq("=");
distinct_directive<> keyword_d_eq("+-=");

/***************************************************************************/
GSlowSQLManager::GSlowSQLManager()
{
	console.SetServer(this);
}
/***************************************************************************/
bool GSlowSQLManager::ProcessInternalMessage(GSocket *socket,GNetMsg & msg,int message)
{
	return false;
};
/***************************************************************************/
bool GSlowSQLManager::ProcessTargetMessage(GSocket *socket,GNetMsg & msg,int message,GNetTarget & target)
{
	return false;
};
/***************************************************************************/
bool GSlowSQLManager::Init()
{
	if(!GServerManager::Init()) return false;
	return true;
}
/***************************************************************************/
void GSlowSQLManager::ConfigureClock()
{
	GServerManager::ConfigureClock();
	signal_hour.connect(boost::bind(&GSlowSQLManager::UpdateMergedTables,this));
	signal_hour.connect(boost::bind(&GSlowSQLManager::UpdateMostEffective,this));
	signal_hour.connect(boost::bind(&GSlowSQLManager::BalanceCheckpoint,this));
	signal_minutes_5.connect(boost::bind(&GSlowSQLManager::UpdateExperiencePast24hrs,this));
	signal_minutes_5.connect(boost::bind(&GSlowSQLManager::ClearExpiredHeartbeats,this));
}
/***************************************************************************/
void GSlowSQLManager::ClearExpiredHeartbeats()
{
	char command[256];
	boost::recursive_mutex::scoped_lock lock(mtx_sql);

	sprintf(command, "DELETE FROM `rtm_group_heartbeat` WHERE `last_beat` < UNIX_TIMESTAMP() - 1800");
	MySQLQuery(&mysql, command);
	sprintf(command, "DELETE FROM `rtm_service_heartbeat` WHERE `last_beat` < UNIX_TIMESTAMP() - 1800");
	MySQLQuery(&mysql, command);
}
/***************************************************************************/
void GSlowSQLManager::UpdateMergedTables()
{
}
/***************************************************************************/
void GSlowSQLManager::UpdateMostEffective()
{
}
/***************************************************************************/
void GSlowSQLManager::BalanceCheckpoint()
{
}
/***************************************************************************/
// Ponizsza funkcja zostanie wykonana raz i tylko raz na poczatku nowego miesiaca.
void GSlowSQLManager::IntervalMonth()
{
}
/***************************************************************************/
// Ponizsza funkcja zostanie wykonana raz i tylko raz na poczatku nowego tygodnia.
void GSlowSQLManager::IntervalWeek()
{
}
/***************************************************************************/
void GSlowSQLManager::UpdateExperiencePast24hrs()
{
}
/***************************************************************************/
struct cmd_table: public grammar<cmd_table>
{
	cmd_table(vector<string>& tbl_)
		: tbl(tbl_){}
	template <typename ScannerT>
	struct definition
	{
		definition(cmd_table const& self)  
		{ 
			flag1_r=ch_p('`')>>(*(anychar_p-ch_p('`')))[push_back_a(self.tbl)]>>ch_p('`');
			flag2_r=ch_p(',')>>flag1_r;
			flags_r=ch_p('(')>>flag1_r>>(*flag2_r)>>ch_p(')');
			expression = str_p("UNION=")>>flags_r;
		}
		rule<ScannerT> flag1_r,flag2_r,flags_r,expression;
		rule<ScannerT> const& start() const { return expression; }
	};
	vector<string>&	tbl;
};
/***************************************************************************/
bool GSlowSQLManager::UpdateMergedTable(MYSQL * mysql, char * table, int back_to_keep, ETableMergeFrequency merge_mode, bool drop_old_database)
{
	boost::recursive_mutex::scoped_lock lock(::mtx_sql);

	string db_name, table_name;
	char *p;

	if ((p = strchr(table, '.')) == NULL)
	{
		table_name = table;
		db_name = "";
	}
	else
	{
		table_name = p + 1;;
		db_name = table;
		db_name.erase(p - table);
	}

	struct tm *today;
	time_t ltime;
	time( &ltime );
	today = gmtime( &ltime );

	char time[256];

	switch (merge_mode)
	{
	case ETMF_MONTHLY:
		strftime(time, 256, "%Y_%m", today);
		break;
	case ETMF_WEEKLY:
		strftime(time, 256, "%Y_week_%W", today);
		break;
	case ETMF_DAILY:
		strftime(time, 256, "%Y_%m_%d", today);
		break;
	default:
		*time = 0;
		break;
	}

	if (*time == 0)
	{
		return false;
	}

	string new_table_name = table_name;
	new_table_name += "_";
	new_table_name += time;

	char command[256];
	sprintf(command,"show create table %s",table);
	bool from_sql=false;
	string syntax;

	if(MySQLQuery(mysql,command))
	{
		MYSQL_RES * result=mysql_store_result(mysql);
		if (result)
		{
			MYSQL_ROW row;
			row=mysql_fetch_row(result);
			if(row)
			{
				syntax=row[1];
				from_sql=true;
			}
			mysql_free_result(result);
		}
	}
	if(!from_sql)
	{
		return false;
	}

	size_t u=syntax.find("UNION=");
	if(u==string::npos)
	{
		return false;
	}
	syntax.erase(0,u);
	u=syntax.find(")");
	if(u!=string::npos) syntax.erase(++u);

	vector<string> tbl;
	vector<string> tbl_to_drop;
	cmd_table gr(tbl);
	bool ret=parse(syntax.c_str(),gr,space_p).full;

	if(!ret)
	{
		return false;
	}

	vector<string>::iterator it;
	for (it=tbl.begin();it!=tbl.end();it++)
	{
		if((*it)==new_table_name)
		{
			return false;
		}
	}

	string cmd;

	cmd="create table `";
	if (db_name != "")
		cmd += db_name + "`.`";
	cmd+=new_table_name;
	cmd+="` like `";
	if (db_name != "")
		cmd += db_name + "`.`";
	cmd+=*tbl.begin();
	cmd+="`";
	MySQLQuery(mysql, cmd.c_str());


	cmd="alter table `";
	if (db_name != "")
		cmd += db_name + "`.`";
	cmd+=table_name;
	cmd+="` UNION=(";
	
	while ((int)tbl.size() > back_to_keep)
	{
		string to_drop=*tbl.begin();
		tbl_to_drop.push_back(to_drop);
		tbl.erase(tbl.begin());
	}

	for (it=tbl.begin();it!=tbl.end();)
	{
		cmd+="`";
		if (db_name != "")
			cmd += db_name + "`.`";
		cmd+=*it;
		cmd+="`";
		it++;
		if(it!=tbl.end()) cmd+=",";
	}
	cmd+=",`";
	if (db_name != "")
		cmd += db_name + "`.`";
	cmd+=new_table_name;
	cmd+="`)";
	MySQLQuery(mysql, cmd.c_str());

	if (drop_old_database)
	{
		for (it=tbl_to_drop.begin();it!=tbl_to_drop.end();)
		{
			cmd="DROP TABLE `";
			if (db_name != "")
				cmd += db_name + "`.`";
			cmd+=*it;
			cmd+="`";
			cmd+=";";
			MySQLQuery(mysql, cmd.c_str());

			it++;
		}
	}

	return true;
}
/***************************************************************************/
void GSlowSQLManager::InitInternalClients()
{
}
/***************************************************************************/
void GSlowSQLManager::ProcessConnection(GNetMsg & msg)
{
	msg.WI(IGMI_CONNECT).WT("slowsql").WI(game_instance).WUI(0).WUS(0).WI(0);
	msg.A();
}
/***************************************************************************/

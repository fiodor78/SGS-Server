/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"
#include "headers_game.h"
#include "zlib.h"

/***************************************************************************/
bool GLocationDBWorkerThread::InitDatabaseMySQL()
{
	locationdb_initialized = false;

	mysql_init(&mysql_locations);
	int pd=ATOI(global_serverconfig->sql_config.port.c_str());
	if (!mysql_real_connect(&mysql_locations,
		global_serverconfig->sql_config.host.c_str(),
		global_serverconfig->sql_config.user.c_str(),
		global_serverconfig->sql_config.password.c_str(),
		global_serverconfig->sql_config.database.c_str(),
		pd,
		NULL,CLIENT_MULTI_STATEMENTS))
	{
		RAPORT("Failed to connect to database (%s,%s,%s,%s). Error: %s",
			global_serverconfig->sql_config.host.c_str(),
			global_serverconfig->sql_config.user.c_str(),
			global_serverconfig->sql_config.password.c_str(),
			global_serverconfig->sql_config.database.c_str(),
			mysql_error(&mysql_locations));
		return false;
	}
	else
	{
		mysql_set_character_set(&mysql_locations, "utf8");
	}
	mysql_locations.reconnect = 1;

	if (global_serverconfig->gamestatedb_config.type == "mysql")
	{
		mysql_init(&mysql_gamestates);
		int pd = ATOI(global_serverconfig->gamestatedb_config.port.c_str());
		if (!mysql_real_connect(&mysql_gamestates,
			global_serverconfig->gamestatedb_config.host.c_str(),
			global_serverconfig->gamestatedb_config.user.c_str(),
			global_serverconfig->gamestatedb_config.password.c_str(),
			global_serverconfig->gamestatedb_config.database.c_str(),
			pd,
			NULL,CLIENT_MULTI_STATEMENTS))
		{
			RAPORT("Failed to connect to database (%s,%s,%s,%s). Error: %s",
				global_serverconfig->gamestatedb_config.host.c_str(),
				global_serverconfig->gamestatedb_config.user.c_str(),
				global_serverconfig->gamestatedb_config.password.c_str(),
				global_serverconfig->gamestatedb_config.database.c_str(),
				mysql_error(&mysql_gamestates));

			mysql_close(&mysql_locations);
			return false;
		}
		else
		{
			mysql_set_character_set(&mysql_gamestates, "utf8");
		}
		mysql_gamestates.reconnect = 1;
	}

	locationdb_initialized = true;
	return true;
}
/***************************************************************************/
void GLocationDBWorkerThread::DestroyDatabaseMySQL()
{
	if (locationdb_initialized)
	{
		if (global_serverconfig->gamestatedb_config.type == "mysql")
		{
			mysql_close(&mysql_gamestates);
		}
		mysql_close(&mysql_locations);
	}
}
/***************************************************************************/
bool GLocationDBWorkerThread::LoadLocationFromDatabaseMySQL(const char * location_ID, string & location_data)
{
	location_data = "{}";

	INT32 gamestate_ID = gamestate_ID_from_location_ID(location_ID);
	MYSQL * mysql = (gamestate_ID == 0) ? &(this->mysql_locations) : &(this->mysql_gamestates);

	bool location_found = false;
	char command[512];
	if (gamestate_ID == 0)
	{
		sprintf(command, "SELECT `location_data` FROM `data_location_blob` WHERE `location_id` = '%s' AND `game_instance` = '%d'",
			GMySQLReal(mysql, location_ID)(), game_instance);
	}
	else
	{
		sprintf(command, "SELECT `game_state` FROM `data_game_state_blob` WHERE `gamestate_id` = '%d'", gamestate_ID);
	}

	if (MySQLQuery(mysql, command))
	{
		MYSQL_RES * result=mysql_store_result(mysql);
		if (result)
		{
			MYSQL_ROW row=mysql_fetch_row(result);
			if (row)
			{
				location_found = true;
				unsigned long *lengths=mysql_fetch_lengths(result);
				last_compressed_size = lengths[0];
				if (last_compressed_size > 0 && !Uncompress(row[0], lengths[0], location_data))
				{
					string err_str=str(boost::format("Error uncompressing location BLOB (id %1%)") % location_ID);
					GRAPORT(err_str.c_str());
					mysql_free_result(result);
					return false;
				}
			}
			mysql_free_result(result);
		}
	}

	if (location_found)
	{
		boost::mutex::scoped_lock lock(mtx_locationdb_multipart_locations);
		multipart_locations_total_parts[location_ID] = 1;
	}
	return true;
}
/***************************************************************************/
std::string GLocationDBWorkerThread::LoadLocationsIdsFromDatabaseMySQL(const string & pattern)
{
	std::stringstream locations_ids;

	char command[512];
	sprintf(command, "SELECT `location_id` FROM `data_location_blob` WHERE `location_id` LIKE '%%%s%%' AND `game_instance` = '%d'",
		GMySQLReal(&mysql_locations, pattern.c_str())(), game_instance);

	if (MySQLQuery(&mysql_locations, command))
	{
		MYSQL_RES * result = mysql_store_result(&mysql_locations);
		if (result)
		{
			MYSQL_ROW row = mysql_fetch_row(result);
			while (row)
			{
				locations_ids << row[0];

				row = mysql_fetch_row(result);

				if (row)
				{
					locations_ids << ",";
				}
			}
			mysql_free_result(result);
		}
	}
	return locations_ids.str();
}
/***************************************************************************/
bool GLocationDBWorkerThread::WriteLocationToDatabaseMySQL(const char * location_ID, const string & location_data)
{
	INT32 gamestate_ID = gamestate_ID_from_location_ID(location_ID);

	if(gamestate_ID >= MIN_GUEST_GAMESTATE_ID)
	{
		return true;
	}

	MYSQL * mysql = (gamestate_ID == 0) ? &(this->mysql_locations) : &(this->mysql_gamestates);

	unsigned long compressed_size;
	if (!Compress(location_data, &compressed_size))
	{
		return false;
	}
	last_compressed_size = compressed_size;

	INT32 total_parts = 0;
    {
		boost::mutex::scoped_lock lock(mtx_locationdb_multipart_locations);
		total_parts = multipart_locations_total_parts[location_ID];
    }

	mysql_real_escape_string(mysql,query_buffer,(const char*)compress_buffer,compressed_size);
	string command;
	if (total_parts > 0)
	{
		if (gamestate_ID == 0)
		{
			command = str(boost::format("UPDATE `data_location_blob` SET `location_data`='%s' WHERE `location_id`='%s' AND `game_instance` = '%d'")
				% query_buffer % location_ID % game_instance);
		}
		else
		{
			command = str(boost::format("UPDATE `data_game_state_blob` SET `game_state`='%s' WHERE `gamestate_id`='%d'")
				% query_buffer % gamestate_ID);
		}
	}
	else
	{
		if (gamestate_ID == 0)
		{
			command = str(boost::format("INSERT INTO `data_location_blob` (`location_id`, `location_data`, `game_instance`) VALUES ('%s', '%s', '%d')")
				% location_ID % query_buffer % game_instance);
		}
		else
		{
			command = str(boost::format("INSERT INTO `data_game_state_blob` (`gamestate_id`, `game_state`) VALUES ('%d', '%s')")
				% gamestate_ID % query_buffer);
		}
	}
	if(!MySQLQuery(mysql,command.c_str()))
	{
		GRAPORT("Error executing query: %s", command.c_str());
		return false;
	}
	if (total_parts == 0)
	{
		boost::mutex::scoped_lock lock(mtx_locationdb_multipart_locations);
		multipart_locations_total_parts[location_ID] = 1;
	}
	return true;
}
/***************************************************************************/
bool GLocationDBWorkerThread::DeleteLocationFromDatabaseMySQL(const char * location_ID)
{
	INT32 gamestate_ID = gamestate_ID_from_location_ID(location_ID);
	if (gamestate_ID > 0)
	{
		return false;
	}
	MYSQL * mysql = &(this->mysql_locations);

	string command;
	command = str(boost::format("DELETE FROM `data_location_blob` WHERE `location_id` = '%s' AND `game_instance` = '%d'")
		% location_ID % game_instance);
	if(!MySQLQuery(mysql,command.c_str()))
	{
		GRAPORT("Error executing query: %s", command.c_str());
		return false;
	}

	{
		boost::mutex::scoped_lock lock(mtx_locationdb_multipart_locations);
		multipart_locations_total_parts[location_ID] = 0;
	}
	return true;
}
/***************************************************************************/

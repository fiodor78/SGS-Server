/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"
#include "glogic_example.h"

#pragma warning(disable: 4355)
/***************************************************************************/
GTable::GTable()
{
	tableId = 0;
	config_json.Clear();
}

void GTable::Init()
{
	difficulty = 1;
	game_in_progress = false;
}

json::Object & GTable::ConfigJSON()
{
	config_json["difficulty"] = json::Number(difficulty);
	config_json["game_in_progress"] = json::Number(game_in_progress ? 1 : 0);
	return config_json;
}

const char * GTable::GetGlobalSetting(const char * key, char * default_value)
{
	return "";
}

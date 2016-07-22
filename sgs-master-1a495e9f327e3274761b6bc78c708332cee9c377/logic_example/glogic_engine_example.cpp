/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"
#include "glogic_example.h"

#define JSON_INT32(gs, key)					((INT32)json::Number(gs[key]).Value())
#define JSON_INT64(gs, key)					((INT64)json::Number(gs[key]).Value())
#define JSON_ADJUST_INT(gs, key, change)	{ gs[key] = json::Number(json::Number(gs[key]).Value() + (change)); };

/*
 *  GLogicEngineExample
 */

// Uwaga!
// Podczas wywolania tej funkcji nie mozemy jeszcze wolac zadnej funkcji z 'upcalls'.
GLogicEngineExample::GLogicEngineExample(class ILogicUpCalls * upcalls)
{
	this->upcalls = upcalls;

	global_config_json.Clear();
	global_variables.Clear();
	seq_tableId = 1;
}

bool GLogicEngineExample::on_init_global_config(DWORD64 timestamp, const string & global_config)
{
	std::istringstream istr(global_config);
	try
	{
		json::Reader::Read(global_config_json, istr);
	}
	catch (const json::Exception& e)
	{
		std::cout << "Caught json::Exception: " << e.what() << std::endl << std::endl;
		return false;
	}
	return true;
}

void GLogicEngineExample::on_cleanup()
{
	global_config_json.Clear();
}

bool GLogicEngineExample::on_put_gamestate(DWORD64 timestamp, INT32 gamestate_ID, const string & gamestate)
{
	std::istringstream istr(gamestate);
	json::Object gs;

	try
	{
		json::Reader::Read(gs, istr);
	}
	catch (const json::Exception& e)
	{
		std::cout << "Caught json::Exception: " << e.what() << std::endl << std::endl;
		return false;
	}

	gamestates_json[gamestate_ID] = gs;
	GTable * table = new GTable();
	table->Init();
	table->tableId = seq_tableId++;
	tables[table->tableId] = table;
	player2table[gamestate_ID] = table->tableId;
	return true;
}

void GLogicEngineExample::on_get_gamestate(INT32 gamestate_ID, string & gamestate, bool server_closing, bool logic_reload)
{
	json::Object & gs = gamestates_json[gamestate_ID];

	std::stringstream ostr;
	json::Writer::Write(gs, ostr);
	gamestate = ostr.str();

	vector<INT32> scores;
	scores.push_back(JSON_INT32(gs, "xp"));
	upcalls->UpdateLeaderboard(gamestate_ID, "xp", 0, scores);
}

void GLogicEngineExample::on_erase_gamestate(INT32 gamestate_ID)
{
	GTable * table = tables[player2table[gamestate_ID]];
	if (table)
	{
		tables.erase(table->tableId);
		delete table;
	}
	player2table.erase(gamestate_ID);
	gamestates_json.erase(gamestate_ID);
}

void GLogicEngineExample::handle_player_message(DWORD64 timestamp, INT32 player_ID, INT32 gamestate_ID,
													const std::string &message, const std::string &params)
{
	json::Object & gs = gamestates_json[gamestate_ID];
	GTable & table = *tables[player2table[gamestate_ID]];

	json::Object jparams;
	try
	{
		std::istringstream istr(params);
		json::Reader::Read(jparams, istr);
	}
	catch (const json::Exception& e)
	{
		std::cout << "Caught json::Exception: " << e.what() << std::endl << std::endl;
	}

	json::Object response;

	for (;;)
	{
		if (message == "utils/load_config")
		{
			/*
			example config:
			{
				"available_difficulties": [1, 2, 4, 5],
				"special_holiday": "Christmas",
				"points_per_xp": 100
			}
			*/
			response["config"] = json::Object(global_config_json);
			break;
		}

		if (message == "game_state/load")
		{
			response["game_state"]["player"] = gs;
			break;
		}

		if (message == "table/set_difficulty")
		{
			INT32 difficulty = JSON_INT32(jparams, "difficulty");
			if (!table.game_in_progress)
			{
				json::Array & difficulties = global_config_json["available_difficulties"];
				json::Array::const_iterator it;
				for (it = difficulties.Begin(); it != difficulties.End(); it++)
				{
					if (difficulty == json::Number(*it))
					{
						break;
					}
				}
				if (it != difficulties.End())
				{
					table.difficulty = difficulty;
				}
				else
				{
					response["error"] = json::String("Invalid difficulty.");
				}
			}
			else
			{
				response["error"] = json::String("Cannot set difficulty during the game.");
			}
			response["game_state"]["player"] = gs;
			break;
		}

		if (message == "table/game_start")
		{
			if (table.game_in_progress)
			{
				response["error"] = json::String("Game already in progress.");
			}
			else
			{
				table.game_in_progress = true;
			}
			response["game_state"]["player"] = gs;
			break;
		}

		if (message == "table/game_finish")
		{
			if (!table.game_in_progress)
			{
				response["error"] = json::String("Game has not started yet.");
			}
			else
			{
				table.game_in_progress = false;

				INT32 score = JSON_INT32(jparams, "score");
				score *= table.difficulty;

				JSON_ADJUST_INT(gs, "games_completed", +1);

				INT32 points_per_xp = JSON_INT32(global_config_json, "points_per_xp");
				if (points_per_xp > 0)
				{
					JSON_ADJUST_INT(gs, "xp", score / points_per_xp);
				}

				if (score > JSON_INT32(gs, "high_score"))
				{
					gs["high_score"] = json::Number(score);
				}
			}
			response["game_state"]["player"] = gs;
			break;
		}
				
		if (message == "debug/clear_game_state")
		{
			gs.Clear();
			response["game_state"]["player"] = gs;
			break;
		}
		
		if (message == "debug/reset_high_score")
		{
			gs["high_score"] = json::Number(0);
			response["game_state"]["player"] = gs;
			break;
		}

		break;
	}

	if (response.Find("game_state") != response.End())
	{
		response["game_state"]["table"] = table.ConfigJSON();
		response["game_state"]["global_variables"] = json::Object(global_variables);
	}
	std::stringstream ostr;
	json::Writer::Write(response, ostr);
	if (upcalls)
	{
		upcalls->SendMessage(player_ID, gamestate_ID, message, ostr.str());
	}
}

void GLogicEngineExample::handle_system_message(DWORD64 timestamp, INT32 gamestate_ID,
													const std::string &message, const std::string &params, std::string & output)
{
	if (message == "test/go")
	{
		string result = "{ result: 11 }";
		if (upcalls)
		{
			upcalls->SendMessage(gamestate_ID, gamestate_ID, message, result);
		}
		output = "Operation complete.";
	}
}

void GLogicEngineExample::on_print_statistics(strstream & s)
{
	s << "Gamestates in memory:\t\t" << gamestates_json.size() << "\r\n";
}

/***************************************************************************/

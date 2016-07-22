/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#pragma warning(disable: 4511)
#pragma warning(disable: 4355)

#include <sgs_api.h>

#include "json/reader.h"
#include "json/writer.h"
#include "json/elements.h"

#include "gtable.h"

class GLogicEngineExample : public ILogicEngine
{
public:
	GLogicEngineExample(class ILogicUpCalls * upcalls);

	bool					on_init_global_config(DWORD64 timestamp, const string & global_config);
	void					on_cleanup();
	bool					on_put_gamestate(DWORD64 timestamp, INT32 gamestate_ID, const string & gamestate);
	void					on_get_gamestate(INT32 gamestate_ID, string & gamestate, bool server_closing, bool logic_reload);
	void					on_erase_gamestate(INT32 gamestate_ID);
	void					on_print_statistics(strstream & s);

	/*
		Obsluga polecen logiki.
	*/
	void					handle_player_message(DWORD64 timestamp, INT32 player_ID, INT32 gamestate_ID,
													const std::string &message, const std::string &params);
	void					handle_system_message(DWORD64 timestamp, INT32 gamestate_ID,
													const std::string &message, const std::string &params, std::string & output);

	class ILogicUpCalls * 		upcalls;

	INT32						seq_tableId;

	hash_map<INT32, json::Object>	gamestates_json;
	hash_map<INT32, GTable * >	tables;						// tableId -> GTable
	hash_map<INT32, INT32>		player2table;				// gamestate_ID -> tableId
	json::Object				global_config_json;
	json::Object				global_variables;
};

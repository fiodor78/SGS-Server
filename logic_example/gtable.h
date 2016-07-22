/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#pragma warning(disable: 4511)
#pragma warning(disable: 4355)

class GTable
{
public:
	GTable();
	void					Init();
	json::Object &			ConfigJSON();

private:
	json::Object			config_json;

// config & data
public:
	INT32					tableId;

	INT32					difficulty;
	bool					game_in_progress;

	const char *			GetGlobalSetting(const char * key, char * default_value = "");
};

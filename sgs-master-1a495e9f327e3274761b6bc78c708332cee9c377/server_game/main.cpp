/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#pragma warning( disable : 4189 )
#include "headers_all.h"
//common variables
string			game_name = "Game";
char *			game_version = "1.0.0.0";
INT32			game_version_number = 1;

//game_ver powinno byc budowane z 4 czesc, v1>>24+v2>>16+v3>>8+v4, 
//zmiana v1 pociaga za soba koniecznosc reinstalacji softu, 
//zmiana v2 to znaczaca zmiana wersji, 
//zmiana v3 to jakies detale,
//zmiana v4 to inna kompilacja,

INT32			game_instance=0;
			
const char *	analytics_url =	"http://stats-sg.ganymede.eu";


#ifdef _LEAK_DETECTION
void LeakError()
{
	cout<<"leak error\r\n";
}
#endif


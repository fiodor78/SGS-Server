/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

/*!
\brief kasa odpowiedzialna za pracê konsoli z linii komend, tworzy now¹ instacjê serwera który komunikuje siê z poprzedni¹ instancj¹
i wyci¹ga z konsoli to co chcemy, po odebraniu danych zamyka serwer
*/
class GMainConsole
{
public:
	GMainConsole();
	bool	Init();
	bool	Destroy();
	bool	Action(string s);
private:
	SOCKET	client_socket;
	int		port;
};

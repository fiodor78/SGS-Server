/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

/*!
\brief kasa odpowiedzialna za prac� konsoli z linii komend, tworzy now� instacj� serwera kt�ry komunikuje si� z poprzedni� instancj�
i wyci�ga z konsoli to co chcemy, po odebraniu danych zamyka serwer
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

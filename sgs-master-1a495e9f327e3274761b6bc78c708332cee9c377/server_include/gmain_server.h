/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#ifdef USE_SPREAD
#include "sp.h"
#endif
/***************************************************************************/
/*!
\brief g��wna klasa wo�ana przed zainstancjowaniem serwera, instancjuje niezb�dne biblioteki
uzywane przez serwer, takie jak raporty, randomy, sockety itd,
*/
class GMainServer
{
public:
	GMainServer();
	bool			Init();
	bool			Destroy();
	bool			InitSystem();
	bool			InitRaport();
	void			InitRandom();
	bool			InitSocket();
#ifdef USE_SPREAD
	bool			InitSpread();
	bool			DestroySpread();
#endif
	bool			DestroySocket();
	bool			DestroySystem();
	virtual bool	Action();

	GSignals			gsignals;
	GSystem				gsystem;
	GStatistics			gstatistics;
	GSQLManager			gsqlmanager;
	GRaportInterface	raport_interface;
};
/***************************************************************************/



/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include <vector>
#include <strstream>

#define GSTAT(group,type,val,txt)	val,
//#define GSTAT(group,type,val,info,description)	val,
enum EStatEnum{
#include "tbl/gstatistics.tbl"
};
#undef GSTAT

//#define GSTR(val)		L ## #val
#define GSTR(val)		#val


/***************************************************************************/
enum EStatEnumGroup
{
	SWARNING,			//warning z kodu
	SERROR,				//wystapil blad
	SINFO,				//dana statystyczna

	SSERVER,
	SSOCKET,			
	SLOGIC,			
	SCONSOLE,		
	SDATA,
	SPOLL,
	SFIREWALL,
	SNET,
	SINTERNAL,
	SPROCESS,
	SSQL,
	SACCESS,
	SLANGUAGE,
	SCENZOR,
	SRNG,

	SALL,
	SUSED,
};
/***************************************************************************/
struct SStatisticsDesc
{
	int			group;
	int			type;
	int			val;
	char*		val_info;
	//char*		info;
	//char*		descrpiption;
};
/***************************************************************************/
struct SStatistics
{
	SStatistics(INT64 val_p){val=val_p;count=0;};
	INT64		val;
	INT64		count;
};
/***************************************************************************/
#define SRAP(val)	global_statistics->Raport(val);
#define SVAL(val1,val2)	global_statistics->Raport(val1,val2);
/***************************************************************************/
/**	
\brief CStatistics ma za zadanie zbieranie informacji statystycznych o pracy serwera
to co w starych serwerac bylo realizowane przez zmienne globalne tutaj jest realizowane
przez globalna klase CStatistics
klasa jest konfigurowana z pliku tbl - dopisujemy tam nowy EVENT, 
raport odbywa sie poprzez define S
klasa nie ma zadnych semaforow poniewaz jest globalna i elementy sa zawsze typu int, inkrementacja 
inta jest bezpieczna dla watkow wiec nic na nie grozi
*/
class GStatistics
{
public:
	GStatistics();
	~GStatistics();
	void					Init();
	void					Destroy();
	void					Raport(int val);
	void					Raport(int val1,INT64 val2);
	void					Stream(strstream & s,vector<int> & commands);
private:
	vector<SStatistics>		stats;
};
/***************************************************************************/





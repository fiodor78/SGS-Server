/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/
#include <google/dense_hash_map>
#include <google/dense_hash_set>

enum ECmdFireWall
{
CMD_FIREWALL_NONE,
CMD_FIREWALL_ADD,
CMD_FIREWALL_DELETE,
CMD_FIREWALL_EXIST,
CMD_FIREWALL_SIZE,
CMD_FIREWALL_SHOW,
CMD_FIREWALL_LIMITS,
CMD_FIREWALL_RESET,
CMD_FIREWALL_RESET_STATIC,
CMD_FIREWALL_RESET_DYNAMIC,
CMD_FIREWALL_SET_LIMIT,
CMD_FIREWALL_LOAD_SQL,
};

/*
\brief klasa odpowiedzialna za blokadê dostepu do serwera wybranym ip oraz ochronê przed dos'atakami
klasa zawiera 5 1-minutowych struktur przechowuj¹cych info o po³¹czeniach, 12-5 minutowych i jedn¹ strukturê statyczn¹
poprzez analizê wpisanych tam danych mo¿emy wykryæ próbê ataku na serwis i automatycznie odci¹æ atakuj¹c¹ maszynê
*/
class GFireWall: private boost::noncopyable, public boost::signals2::trackable
{
public:
	/*
	konstruktor
	*/
	GFireWall(GRaportInterface & raport_interface_);

	/*
	inicjalizuje firewall'a
	\param firewall - konfiguracja firewalla pochodz¹ca z pliku konfiguracyjnego
	*/
	void			Init(){};
	/**
	g³ówna funkcja testuj¹ca zestawiane po³¹czenia
	\param addr - adres pochodz¹cu z accept'a, identyfikuje po³¹czenie
	*/
	bool			Test(const in_addr addr);
	/**
	postac operatorowa testu
	\param addr - adres pochodz¹cu z accept'a, identyfikuje po³¹czenie
	*/
	bool			operator()(const in_addr addr) {return Test(addr);};
	/**
	cyklicznie wo³ana fukncja update'u czuas, nale¿y wo³aæ ja raz na minutê
	\param clock - referecja na zegar
	*/
	void			UpdateTime(GClock &clock);
	/**
	dodaje adres do firewall'a statycznego
	\param addr - dodawany adres
	*/
	void			Add(const in_addr addr);
	/**
	usuwa adres z firewall'a statycznego
	\param addr - usuwany adres
	*/
	void			Delete(const in_addr addr);
	/**
	sprawdza czy adres jest wpisany w firewall'a
	\param addr - adres do sprawdzenia
	*/
	bool			Exist(const in_addr addr);
	/**
	zwraca rozmiar tablicy statycznego firewall'a
	*/
	int				Size();
	/**
	zwraca listê zablokowanych adresów
	\param s - strumieñ do którego zapisujemy limity
	*/
	void			Stream(strstream &s);
	/**
	zwraca limity firewall'a dynamicznego
	\param s - strumieñ do którego zapisujemy limity
	*/
	void			Limits(strstream &s);
	/**
	ustawia limity firewall'a dynamicznego
	\param id - identyfikator limitu do zmiany
	\param limit - nowa wartoœæ limitu
	*/
	void			SetLimit(int id, int limit);
	/**
	resetuje firewall'a, mo¿na zresetowaæ firewall statyczny, dynamiczny albo oba naraz
	\param cmd - komenda, okreœla któr¹ czêœæ firewall'a nale¿y zresetowaæ
	*/
	void			Reset(ECmdFireWall cmd);
	/**
	pobiera z SQL'a blokowane adresy
	*/
	void			GetFromSQL();

private:
	/**
	pobiera indeks firwalla odpowiadajacego za ostania minute
	*/
	int				GetMinute1Back(int back);
	/**
	pobiera indeks firwalla odpowiadajacego za ostania 5-minutowke
	*/
	int				GetMinutes5Back(int back);
	struct eq_addr
	{
		bool operator()(const in_addr s1, const in_addr s2) const
		{
			return (memcmp(&s1,&s2,4)==0);
		}
	};
	struct hash_addr
	{ 
		size_t operator()(const in_addr x) const { DWORD32 a;memcpy(&a,&x,4);return a;} ;
	};
	//! hash mapa minutowek
	dense_hash_map<const in_addr, int, hash_addr, eq_addr> minute_1[5];
	//! hash mapa 5-minutowke
	dense_hash_map<const in_addr, int, hash_addr, eq_addr> minutes_5[12];
	//! hash mapa statycznego firewall'a
	dense_hash_set<in_addr, hash_addr, eq_addr> wall;
	//! indeks ostatniej minutowki
	int				last_minute_1;
	//! indeks ostatniej 5-minutowki
	int				last_minutes_5;

	GRaportInterface &	raport_interface;
};



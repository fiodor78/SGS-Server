/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

/*!
enum EnetBasicType - s¹ to typy u¿ywane do parsowania message'y, nie musimy nic z nimi robiæ i ich znaæ, powstaj¹ w wyniku preparsingu message'y
i s¹ u¿ywane do przyspieszenia wyszukiwania fraz messagey w czasie pracy serwera
*/
enum ENetBasicType{
	ENB_b,				/*!< dane binarne - bit*/
	ENB_c,				/*!< char*/
	ENB_uc,				/*!< unsigned char*/
	ENB_s,				/*!< short (16 bit)*/
	ENB_us,				/*!< unsigned short*/
	ENB_i,				/*!< int (32 bit)*/
	ENB_ui,				/*!< unsigned int*/
	ENB_f,				/*!< float*/
	ENB_d,				/*!< double*/
	ENB_I,				/*!< int (64 bit)*/
	ENB_UI,				/*!< unsigned int (64 bit)*/
	ENB_t,				/*!< text*/
	ENB_o,				/*!< NPair*/
	ENB_r,				/*!< bitarray*/
	ENB_e,				/*!< */
	ENB_value,				/*!< wartosc*/
	ENB_start,				/*!< start frazy*/
	ENB_end,				/*!< koniec frazy*/
	ENB_mul,				/*!< powtorzenie*/
	ENB_none,				/*!< */
};
/*-----------------------------*/
/**
\brief opisuje format danych message'u, obecnie nie uzywamy juz formatow opisanych stringiem,
sa one tak podawane na wejsciu, po czym sa parsowane, sprawdzana jest ich poprawnosc (np, nawiasy)
i rekodowane sa na vector z opisem message'a, latwiej sie po nim poruszac i jest to znacznie szybsze
*/
struct SNetBasic
{
	//! konstruktor
	SNetBasic()	{Reset();};
	/*-----------------------------*/
	//! konstruktor, definiujemy rodzaj elementu
	SNetBasic(ENetBasicType m){Reset();mode=m;};
	/*-----------------------------*/
	//! reset elementu
	void			Reset()
	{
		mode=ENB_none;
		ilim[0]=ilim[1]=0;
		flim[0]=flim[1]=0;
		ref=0;
		repeat=0;
		use_lim=false;
	}
	/*-----------------------------*/
	//! typ elementu
	ENetBasicType	mode;
	//! czy zdefiniowano limity dla elementow
	bool			use_lim;
	union
	{
		//! zakresy
		INT64			ilim[2];
		double			flim[2];
	};
	union
	{
		//! pozycja wystapienia parujacego nawiasu
		int				ref;
		//! ilosc powtorzen
		int				repeat;
	};
};
/*-----------------------------*/
/**
\brief opisuje format message'a
*/
class GNetParserBase;
struct SNetMsgDesc
{
	//! rozpoczyna budowanie opisu
	void				Build(GNetParserBase * base,ENetCmdType cmd_type);
	/** buduje element opisu
	\mode - definiuje typ
	\f - definiuje opis
	*/
	void				BuildElement(ENetBasicType mode, char*& f);
	//! identyfikator message'a
	int					msgid;
	//! nazwa msg'a
	char *				name;
	//! format opis w postaci znakowe
	char *				format;
	//! opis
	char *				desc;
	//! dobudowywany wektor ktory opisze message
	vector<SNetBasic>	commands;
};
/*-----------------------------*/
/**
\brief netparsebase to klasa globala, tworzymy ja aby miec zrodelko wszystkich message'y w postaci 
przeworzonej, tzn opis bazujacy na wektorze a nie na stringu,
podczas inicjalizacji przetwarza wszystkie dane ze stringa na opis wektorowy
*/
class GNetParserBase
{
public:
	//! konstruktor
	GNetParserBase();
	//! destruktor
	~GNetParserBase();
	//! zwraca wektor opisujacy msg
	SNetMsgDesc	*			Get(int msgid,ENetCmdType cmd_type);
	//! zwraca wektor opisujacy msg
	SNetMsgDesc	*			GetSub(char * msgid,ENetCmdType cmd_type);
private:
	//mapa struktur opisujacych message
	map<int,SNetMsgDesc *>		msg_commands;
	//mapa struktur opisujacych message
	map<int,SNetMsgDesc *>	sub_msg_commands;
#ifdef SERVER
	//mapa struktur opisujacych message
	map<int,SNetMsgDesc *>		internal_msg_commands;
	//mapa struktur opisujacych message
	map<int,SNetMsgDesc *>	sub_internal_msg_commands;
#endif
};
/***************************************************************************/
/*!
funkcja hashujaca string, uzywane do przetworzenia identyfikatora submessage'a na wartosc
*/











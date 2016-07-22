/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

/*
jednym z najwiekszych problemow z klasa obslugi msg byla struktura klas
rozwazalem pare rozwiazan
1. niezalezne chunki funkcjonujace w postaci objektow, mozna by do takiego chunka
zapisywc dane, czytac z niego dane, i skladac z nich rozne rzeczy,
zaleta jest elastycznosc badz mozliwosc parsowania paru czunkow naraz, wada jest natomiat
alokacja dodatkowej pamieci, to co chce osiagnac z pamiecia to jeden bufor wejscia i jeden bufor wyjscia
czyli minimalizacja kwestii alokacji pamieci,
utrudnia sie tez nieco dostep do klas w przypadku duzej liczby roznych klas, 
2. niezalezne objekty GNetMsg, objekt taki dysponuje zarzadza wszystkim co wchodzi i wychodzi z klasy
jest osadzony w obrebie GSocket ale mozna go tez zainstancjowac calkowicie niezaleznie do GSocket 
i wykonywac na nim rozne operacje, 
oba modele sa nieco trudne chociaz ze wzgledu na obsluge pamieci i wyjatkow!
mysle ze zostaniemy przy modelu 2, jest nieco nam blizszy w zwiazku z poprzednimi wersjami engine'u
*/

//#include <boost/any.hpp>

/** 
*/
enum ENetErrorsType{
	ENetErrorNone,
	ENetErrorNotAllocated,				//! nie zaalokowano bufora
	ENetErrorReallocFailed,				//! nie udala sie realokacja pamieci bufora!
	ENetErrorChunkNotInitialized,		//! chunk nie zostal zainicjalizowany, nie mozna go zamknac,
	ENetErrorOutOfData,					//! brakuje danych w buforze ktore powinny tam byc!,
	ENetErrorOutOfLimit,				//! przekroczono limit dlugosci danych w buforze wyjscia albo wejscia
	ENetErrorDataError,					//! bledy danych
	ENetErrorIncorrectFormat,			//! format chunka jest bledny
	ENetErrorLast,
};


/** 
rodzaje bledow obslugi parsowania
te bledy dziela sie na 2 rodzaju, bledy formatu, oraz bledy danych!!!
wszystkie sa rzucane i przejmowane w obrebie funkcji test,
nie trzeba sie wiec nimi specjalnie przejomwac,
*/
enum ENetFormatErrorsType{
	ENetFormatErrorNone,
	//bledy skladni
	ENetFormatErrorIncorrectIterator,	//!itarator ma bledna wartosc
	ENetFormatErrorIncorrectFormat,		//!cos bardzo namieszane s tringu formatujacym!
	ENetFormatErrorOutOfFormat,			//!braklo danych z opisem formatu
	ENetFormatErrorOutOfData,			//!zabraklo danych w buforze
	ENetFormatErrorOutOfLimit,			//!zmienna poza limitem
	ENetFormatLast,
};


/** 
stany zakonczenia czytania 
*/
enum ENetStatusType{
	ENetStatusNone,						//!brak statusu
	ENetStatusFinished,					//!message zostal sparsowany do konca
	ENetStatusSkipped,					//!przeskoczylismy koncowke, nie zostal sparsowany do konca
	ENetStatusLast,						//!ostatni message,
};
/**
flagi GNetMsg
*/
enum ENetFlagsType
{
	ENetTestIn,
	ENetTestOut,
	ENetCryptIn,
	ENetCryptOut,
	ENetLast,
};

enum ENetCmdType
{
	ENetCmdGame,
	ENetCmdInternal,
};

/***************************************************************************/
/**
\brief zglasza blad w poprawnosci pakietu danych, ten wyjatek moze byc zgloszony tylko i wylacznie
podczas parsowania danych, poniewaz jest to oblozone try'em nie ma problemy wywalenie programu przez niego

*/
class GNetTestException
{
public:
	/**
	konstruktor 
	\param error_code_p - kod bledu
	*/
	GNetTestException(ENetFormatErrorsType error_code_p)
	{
		Reset();
		error_code=error_code_p;
	};
	/**
	konstruktor 
	\param error_code_p - kod bledu
	\param pos_p - pozycja do ktorej udalo nam sie doiterowac, jest to pozycja z wektora opisu a nie formatu
	*/
	GNetTestException(ENetFormatErrorsType error_code_p,int pos_p)
	{
		Reset();
		error_code=error_code_p;
		pos=pos_p;
	};
	/**
	konstruktor 
	\param error_code_p - kod bledu
	\param pos_p - pozycja do ktorej udalo nam sie doiterowac, jest to pozycja z wektora opisu a nie formatu
	\param l1 - wartosc minimalna ustawiona dla zmiennej
	\param c - wartosc zmiennej
	\param l1 - wartosc maksymalna ustawiona dla zmiennej
	*/
	GNetTestException(ENetFormatErrorsType error_code_p,int pos_p,boost::any l1,boost::any c,boost::any l2)
	{
		Reset();
		error_code=error_code_p;
		pos=pos_p;
		val[0]=l1;
		val[1]=c;
		val[2]=l2;
	};
	/**
	 reset elementu
	 */
	void			Reset()
	{
		error_code=ENetFormatErrorNone;
		pos=0;
		int a;
		for (a=0;a<3;a++)
		{
			val[a]=0;
		}
	}
	ENetFormatErrorsType		GetError(){return error_code;};
	//! kod bledu 
	ENetFormatErrorsType		error_code;
	//! pozycja bledu
	int							pos;
	//! zakresy danych
	boost::any					val[3];
};
/***************************************************************************/
struct SNetMsgDesc;
/**
\brief - klasa obslugujace komunikacje,
uwaga - swiadomie nie definiuje funkcji in-out poprzez tempate'y
musze miec gwarancje zachowania kolejnosci bitow, dlugosci struktury itd,
definicja poprzez template'a otwiera brak kontroli nad wywalanym w sieci typem 
*/
class GNetMsg: public boost::noncopyable
{
public:
	/**
	 konstruktor
	 \param mgr - jesli netmsg jest tworzony do celu przetworzenia danych bedzie musial sam zaalokowac
	 sobie pamiec, podajemy mu wtedy ptr na memorymanager, jesli natomiat pracuje jako klasa 
	 przetwarzajaca dane na potrzeby GSocket to alokacja odbywa sie poza nim wiec nie potrzeba podawac
	 tego ptr'a
	 */
	GNetMsg(GMemoryManager * mgr=NULL);
	/**
	 dektruktor
	 */
	virtual				~GNetMsg();
	/**
	 \param in_p - pointer na bufor wejsciowy
	 \param out_p - pointer nu bufor wyjsciowy
	 */
	void				Init(ENetCmdType cmd_type_p,GMemory *in_p, GMemory *out_p);
	/**
	\param in_p - pointer na bufor wejsciowy
	\param out_p - pointer na bufor wyjsciowy
	*/
	void				Init(ENetCmdType cmd_type_p,GMemoryManager * mgr);
	/**
	inicjalizuje netmsg, ustawia pointer na memorymanager'a
	\param mgr - pointer na memory manager'a
	 */
	GNetMsg &			Reset();
	/*-----------------------------*/
	/**
	pobiera pointer na bufor do odczytu
	\param size - ilosc bajtow jakie bedziemy chcieli wczytac
	*/
	BYTE *		RPos(DWORD32 size);
	/**
	pobiera pointer na bufor do zapisu
	\param size - ilosc bajtow jakie trzeba zarezerwowac
	*/
	BYTE *				WPos(DWORD32 size);
	/**
	pobiera pointer na bufor do testowania
	\param size - ilosc bajtow jakie trzeba wytestowac
	*/
	BYTE *				TPos(DWORD32 size);
	/*-----------------------------*/
	/**
	flushuje cache agregacji bitow, netmsg pakuje razem bity pochodzace z zapisow typu bool,
	do cachowania uzywa bitset'u bits oraz zmiennej bits_count
	*/
	GNetMsg &			FlushBitsCache()
	{
		if(out_chunk.bits_count)
		{
			out_chunk.bits_count=0;
			WU(out_chunk.bits());
		}
		return * this;
	};
	/*-----------------------------*/

#include "gnet_w.hpp"
#include "gnet_r.hpp"
#include "gnet_t.hpp"

	/*-----------------------------*/
	/**
	zapisuje short'a i zaczyna msg
	\param a - nr message'a
	*/
	GNetMsg &			B(int a){return WI(a);};
	/**
	konczy msg, zapisuje chunka na koncu
	*/
	GNetMsg &			A();
	inline GNetMsg &	operator<<(NEnd){return A();};
	/**
	konczy czytanie danych, wykonuje w tym celu skok do konca czunka
	*/
	GNetMsg &			E();
	inline GNetMsg &	operator>>(NEnd){return E();};
	/*-----------------------------*/
	/**
	jesli mamy zdefiniowany ptr na mgr funkcja otwiera bufor in
	pozwala to na rozne operacje zwiazane z I/O messagey
	*/
	bool				OpenIn()
	{
		if(in) return true;
		if(mgr==NULL) return false;
		in=&in_auto;
		in->Init(mgr);
		in->Allocate(K1);
		if(in->Size()==0) return false;//nie udala sie alokacja
		return true;
	}
	/*-----------------------------*/
	/**
	jesli mamy zdefiniowany ptr na mgr funkcja otwiera bufor in
	pozwala to na rozne operacje, np przygotowanie struktury
	message'a i wyrzucenie go od razu do wiekszej liczby graczy
	*/
	bool				OpenOut()
	{
		if(out) return true;
		if(mgr==NULL) return false;
		out=&out_auto;
		out->Init(mgr);
		out->Allocate(K1);
		if(out->Size()==0) return false;//nie udala sie alokacja
		return true;
	}
	/*-----------------------------*/
	/**
	przypisuje jeden netmsg do drugiego
	przypisanie nie polega na kopiowaniu a ma przekopiowaniu chunkow
	obowiazuja tu dwa zalozenia, jedno to takie ze dane w chunkach 
	srodla sa nie szyfrowane, nie rozszyfrowujemy ich, drugie dane zaczynaja sie 
	i koncza na granicy czunkow
	\param - referencja na GNetMsg
	*/
	GNetMsg &			operator+=(GNetMsg & msg)
	{
		if(!OpenOut()) 
		{
			//throw GNetException(ENetErrorReallocFailed);
			SetErrorCode(ENetErrorReallocFailed,__LINE__);
			return * this;
		}

		DWORD32 free=out->Free();
		if(!msg.OutUsed()) return * this;
		DWORD32 used=msg.out->Used();

		if(free<used)
		{
			if(!out->Reallocate()) 
			{
				//throw GNetException(ENetErrorReallocFailed);
				SetErrorCode(ENetErrorReallocFailed,__LINE__);
				return * this;
			}
		}

		GNetChunk in_chunk;
		DWORD32 copied=0;
		while(copied<used)
		{
			in_chunk.ptr=(BYTE*)&msg.out->Start()[copied];
			in_chunk.size=*((DWORD32 *)in_chunk.ptr);
			if((chunk_limit>0 && in_chunk.size>chunk_limit) || in_chunk.size < 4)
				{
					//throw GNetException(ENetErrorOutOfLimit);
					SetErrorCode(ENetErrorOutOfLimit,__LINE__);
					return * this;
				}

			if((used-copied)>=in_chunk.size) 
			{
				if(out->Free()<(DWORD32)in_chunk.size)
				{
					if(!out->ReallocateToFit((DWORD32)in_chunk.size)) 
					{
						//throw GNetException(ENetErrorReallocFailed);
						SetErrorCode(ENetErrorReallocFailed,__LINE__);
						return * this;
					}
				}

				memcpy(out->End(),in_chunk.ptr,in_chunk.size);
				out_chunk.Init((BYTE*)out->End(),in_chunk.size);
				out_chunk.offset=out_chunk.size;
				if(flags[ENetCryptOut] && crypt_out.GetOffset())
				{
					int len=out_chunk.offset-4;
					len=len%crypt_out.GetOffset();
					if(len) len=crypt_out.GetOffset()-len;
					Fill(len);
				}
				*((DWORD32 *)out_chunk.ptr)=out_chunk.offset;
				out_chunk.size=out_chunk.offset;
				//zwiekszamy obszar uzytej pamieci wyjscia
				out->IncreaseUsed(out_chunk.size);
				last_chunk=out_chunk;
				test_chunk=out_chunk;
				//if(flags[ENetTestOut]) if(!Test(true))
				//{
				//	SetErrorCode(ENetErrorIncorrectFormat,__LINE__);
				//}
				if(flags[ENetCryptOut]) 
					crypt_out<<last_chunk;
				out_chunk.Reset();
			}
			copied+=in_chunk.size;
		}
		return * this;
	}
	/*-----------------------------*/
	/**
	przypisuje chunk z jednego msg do drugiego do drugiego
	\param - referencja do Chunka
	*/
	GNetMsg &			operator+=(const NPair & in_chunk)
	{
		if(!OpenOut()) 
		{
			//throw GNetException(ENetErrorReallocFailed);
			SetErrorCode(ENetErrorReallocFailed,__LINE__);
			return *this;
		}

		DWORD32 free=out->Free();
		DWORD32 used=in_chunk.size;

		if(free<used)
		{
			if(!out->Reallocate())
			{
				//throw GNetException(ENetErrorReallocFailed);
				SetErrorCode(ENetErrorReallocFailed,__LINE__);
				return *this;
			}
		}

		if((chunk_limit>0 && in_chunk.size>chunk_limit) || in_chunk.size < 4)
			{
				//throw GNetException(ENetErrorOutOfLimit);
				SetErrorCode(ENetErrorOutOfLimit,__LINE__);
				return *this;
			}

		if(out->Free()<(DWORD32)in_chunk.size)
		{
			if(!out->ReallocateToFit((DWORD32)in_chunk.size)) 
			{
				//throw GNetException(ENetErrorReallocFailed);
				SetErrorCode(ENetErrorReallocFailed,__LINE__);
				return *this;
			}
		}
		memcpy(out->End(),in_chunk.ptr,in_chunk.size);
		out_chunk.Init((BYTE*)out->End(),in_chunk.size);
		out->IncreaseUsed(out_chunk.size);
		last_chunk=out_chunk;
		test_chunk=out_chunk;
		if(flags[ENetCryptOut]) crypt_out<<last_chunk;
		return * this;
	}
	/*-----------------------------*/
	/**
	iteruje po buforze wejscia
	\return true gdy mamy do sparsowania kolejny chunk, false gdy trzeba przerwac parsowanie
	*/
	bool				Parse(INT32 & pre_message);
	/**
	zwraca status parsowania bufora, tzn czy doczytano do konca czy zakonczono chunk'a skip'em
	\return status
	*/
	ENetStatusType		Status(){return status;};
	/**
	ustawia limit dlugosci chunka, default to 16kb, nie powinno byc dluzszego chunka
	\param limit - limit dlugosci
	*/
	void				SetChunkLimit(DWORD limit_p){chunk_limit=limit_p;};
	/**
	zwraca opis ostatniego chunka, uzywane do szyfrowania
	*/
	const NPair &		LastChunk(){return last_chunk;};
	/**
	zwraca opis nowego chunka ktorego bedziemy zaraz parsowac, uzywane do szyfrowania
	*/
	const NPair &		CurrentChunk(){return current_chunk;};
	/**
	zwraca opis czytanego chunka z pointerami iteracji
	*/
	GNetChunk &			InChunk(){return in_chunk;};
	/**
	zwraca opis zapisywanego chunka z pointerami iteracji
	*/
	GNetChunk &			OutChunk(){return out_chunk;};
	/**
	resetuje chunka wejscia
	*/
	void				InChunkReset(){in_chunk.Reset();};
	/**
	resetuje chunka wyjscia
	*/
	void				OutChunkReset(){out_chunk.Reset();};
	/**
	zwraca opis testowego chunka z pointerami iteracji
	*/
	GNetChunk &			TestChunk(){return test_chunk;};
	/**
	testuje poprawnosc danych w buforze
	*/
	bool				TestMsg(bool out);
	/**
	analizuje format danych,
	*/
	int					ParseFormat(SNetMsgDesc * format,string * parsed,DWORD pos=0);
	/**
	wlacza test wejsica
	*/
	void				SetTestIn(){flags.set(ENetTestIn);};
	/**
	wlacza test wyjscia
	*/
	void				SetTestOut(){flags.set(ENetTestOut);};
	/**
	wlacza kodowanie wejscia
	*/
	void				SetCryptIn(ENetCryptType method){flags.set(ENetCryptIn);crypt_in.SetCrypt(method);};
	/**
	wlacza kodowanie wyjscia
	*/
	void				SetCryptOut(ENetCryptType method){flags.set(ENetCryptOut);crypt_out.SetCrypt(method);};
	/**
	wylacza kodowanie wejscia
	*/
	void				ResetCryptIn(){flags.reset(ENetCryptIn);crypt_in.SetCrypt(ENetCryptNone);};
	/**
	wylacza kodowanie wyjscia
	*/
	void				ResetCryptOut(){flags.reset(ENetCryptOut);crypt_out.SetCrypt(ENetCryptNone);};
	//! sprawdza czy jest blad
	bool				IsError(){return error.code!=ENetErrorNone;};
	//! zwraca kod bledu
	ENetErrorsType		GetErrorCode(){return error.code;};
	//! zwraca linie bledu
	int					GetErrorLine(){return error.line;};
	//! zwraca ostatni msg jaki przyszedl
	int					GetErrorIn(){return error.last_in;};
	int					GetErrorCurrentIn(){return error.current_in;};
	//! zwraca ostatni generowany msg
	int					GetErrorOut(){return error.last_out;};
	//! ustawia kod bledu
	void				SetErrorCode(ENetErrorsType error_p, int line_p){error.code=error_p;error.line=line_p;}

	//! wlacza klucz kodowania
	void				SetKey(void * key)
	{
		crypt_in.SetKey(key);
		crypt_out.SetKey(key);
	}
	DWORD32				InFree()
	{
		if(in==NULL) return 0;
		return in->Free();
	}
	DWORD32				OutFree()
	{
		if(out==NULL) return 0;
		return out->Free();
	}
	DWORD32				InUsed()
	{
		if(in==NULL) return 0;
		return in->Used();
	}
	DWORD32				OutUsed()
	{
		if(out==NULL) return 0;
		return out->Used();
	}
	GMemory * 			InAlloc(DWORD32 size)
	{
		in=&in_auto;
		in->Init(mgr);
		in->Allocate(size);
		return in;
	}
	GMemory *			OutAlloc(DWORD32 size)
	{
		out=&out_auto;
		out->Init(mgr);
		out->Allocate(size);
		return out;
	}

	GMemory *			In()
	{
		return in;
	}
	GMemory *			Out()
	{
		return out;
	}

	//! rozpakowuje dane z hex'a
	bool FromHex(BYTE * hex, int size);
	//! pakuje dane do hex'a
	bool ToHex(BYTE * hex, int size);

	bool Empty(){return OutUsed()==0;};
	/**
	string zawierajacy opis blednie formatowanych danych	
	*/
	string				format_error;
	string				last_format_error;
private:

	//!	chunk wyjscia
	GNetChunk			out_chunk;
	//!	chunk wejscia
	GNetChunk			in_chunk;
	//! chunk testowy
	GNetChunk			test_chunk;

	//! ptr na pamiec wyjscia
	GMemory *			out;
	//! ptr na pamiec wejscia
	GMemory *			in;
	//! bufor ewentualnie alokowanej pamieci wyjscia
	GMemory 			out_auto;
	//! bufor ewentualnie alokowanej pamieci wyjscia
	GMemory 			in_auto;

	//! max dlugosc chunka
	DWORD32				chunk_limit;
	//! ostatni zapisany chynk - mozemy na nim zaaplikowac crypt'a
	NPair				last_chunk;
	//! nastepny chunk ktory bedziemy parsowac - mozemy mu zaaplikowac crypt'a
	NPair				current_chunk;

	//! ptr do memorymnagera
	GMemoryManager *	mgr;
	//! status zakonczenia parsowania
	ENetStatusType		status;
	//! struktura zawiera kod bledu, linie, ostatni msg ktory przyszedl i ostatni ktory wyszedl
	struct SError
	{
		ENetErrorsType		code;
		int					line;
		int					last_in;
		int					current_in;
		int					last_out;
	}error;
	
	//! flagi tesow oraz kryptowania
	bitset<ENetLast>	flags;

	//! klasa dekodujaca wejscie
	GNetCrypt			crypt_in;
	//! klasa kodujace wyjscie
	GNetCrypt			crypt_out;

	ENetCmdType			cmd_type;

	int					last_parsed_message;
};
/***************************************************************************/


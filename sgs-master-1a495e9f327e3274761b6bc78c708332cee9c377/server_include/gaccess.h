/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

//! enum EAccessMode - tryby dostêpu gracza do serwisu
enum EAccessMode
{
	EAccessID,			/*! podano id*/
	EAccessLogin,		/*! podano login*/
	EAccessSession,		/*! podano nr sesji*/
	EAccessPassword,	/*! podano haslo*/
	EAccessLast,		
};


//interpretacja flag zalezy od tego czy mamy okno do wprowadzania danych czy nie,
//gdy go nie ma nalezy dac inny komunikat!!! np. blad inicjalizacji gry
enum EAccessError
{
	EErrorNone,
	EErrorNoIdentification,				//nie podano id,sid, login, login,
	EErrorNoVerification,				//nie podano sesji, logina, hasla
};

/**
\brief klasa odpowiada za kwestie zalogowania sie do serwera, czyli przetworzenie hasel i innych danych 
na formaty zrozumiale dla gry
aby klasa dzia³a³a trzeba wype³niæ poszczególne pola - tzn ID, SID, Login, itd.., nastêpnie wywo³aæ funcjê Parse,
sprawdzi ona poprawnoœæ wpisanych danych, potem mo¿emy wywo³aæ Send, które to wyœle dane do serwera
*/
class GAccessConfig: public GNetStructure
{
public:
	//! konstruktor
	GAccessConfig():GNetStructure(){Init();};
	//! funkcja zapisu danych
	GNetMsg & W(GNetMsg & msg)
	{
		access.W(msg);
		return msg;
	}
	//! funkcja odczytu danych
	GNetMsg & R(GNetMsg & msg)
	{
		access.R(msg);
		return msg;
	}

	//! inicjalizuje dane
	void				Init(){Reset();};
	//! reset danych, s¹ czyszczone
	void				Reset()
	{
		error_code=EErrorNone;
		access.Zero();
		quick_start=false;
	};
	//! ustawia id gracza
	void				SetID(DWORD32 id_p){access.ID=id_p;};
	//! ustawia lid gracza
	void				SetLanguage(const char * lng){strncpy(access.language,lng,MAX_LANGUAGE);};
	//! ustawia login
	void				SetLogin(const char * d){CopyStr(access.login,d,MAX_LOGIN);}
	//! ustawia sid
	void				SetSID(const char * d){CopyStr(access.SID,d,MAX_SID);};
	//! ustawia haslo
	void				SetPassword(const char * d);
	//! ustawia haslo
	void				SetPasswordMD5(const char * d);
	//{CopyStr(access.password,MD5(d)(),MAX_PASSWORD);};
	//! ustawia nr pokoju
	void				SetRoom(int r){access.room_number=r;};
	void				SetQuickStart(){quick_start=true;};
	//! ustawia site
	void				SetSite(const char * d){CopyStr(access.site,d,MAX_SITE);}
	//! ustawia adres hosta serwera
	void				SetHost(const char * host_p)
	{
		host.assign(host_p);
		h=GetAddrFromName(host.c_str());
	}

	void				SetHost(DWORD32 addr)
	{
		h = addr;
	}
	//! ustawiamy nr portow
	void				SetPorts(const char * ports_p)
	{
		boost::char_separator<char> sep(";,.");
		typedef boost::tokenizer<boost::char_separator<char> > TTokenizer;
		string ports_str(ports_p);
		ports.clear();
		TTokenizer tok(ports_str,sep);
		TTokenizer::iterator beg=tok.begin();
		while(beg!=tok.end())
		{
			string key=*beg;
			USHORT16 port=(USHORT16)ATOI(key.c_str());
			ports.push_back(port);
			beg++;
		}

		if(ports.size())
		{
			p=ports[0];
		}
	}

	void				SetPorts(vector<USHORT16> & ports_p)
	{
		ports=ports_p;
		if(ports.size())
		{
			p=ports[0];
		}
	}

	//! wstepnie przetwarza dane klienta, sprawdza ich poprawnosc zanim cos poslemy do serwera
	bool ParseClient()
	{
		bitset<EAccessLast> flags;
		error_code=EErrorNone;
		if(access.ID!=0) flags.set(EAccessID);
		if(strlen(access.login)) flags.set(EAccessLogin);
		if(strlen(access.SID)) flags.set(EAccessSession);
		if(strlen(access.password)) flags.set(EAccessPassword);

		if(!flags[EAccessID] && !flags[EAccessLogin])
		{
			error_code=EErrorNoIdentification;
			return false;
		}
		if(!flags[EAccessSession] && !flags[EAccessPassword])
		{
			error_code=EErrorNoVerification;
			return false;
		}
	}

	//! przetwarza dane po stronie serwera
	bool ParseServer()
	{
		//static const boost::regex e("/^( [a-zA-Z0-9] )+( [a-zA-Z0-9\._-] )*@( [a-zA-Z0-9_-] )+( [a-zA-Z0-9\._-] +)+$/");
		//if(!boost::regex_match(d,e)) return false;
	}
	//!pobiera blad
	EAccessError		GetError(){return error_code;};

	//! struktura przetrzymujaca dane
	GTicketConnection::SAccess	access;
private:
	//! kod bledu
	EAccessError		error_code;

public:
	//! host do ktorego sie podlaczamy
	string				host;
	//! mozliwe porty, na ktore sie podlaczamy
	vector<USHORT16>	ports;
	//! host w postaci binarnej
	DWORD32				h;
	//! biezacy port w postaci binarnej
	USHORT16			p;
	bool				quick_start;
};

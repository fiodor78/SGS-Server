/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include <shared_ptr.h>
extern boost::variate_generator<boost::mt19937, boost::uniform_int<> > rng_byte;

/**
\brief GNetStructure to podstawa kazdej struktury poruszajacej sie po sieci
*/
class GNetMsg;
class GNetStructure
{
public:
	virtual void	Zero(){};
	virtual GNetMsg & W(GNetMsg & );
	virtual GNetMsg & R(GNetMsg & );
	virtual GNetMsg & WE(GNetMsg & );
	virtual GNetMsg & RE(GNetMsg & );
	GNetMsg & operator<<(GNetMsg & );
	GNetMsg & operator>>(GNetMsg & );
	GNetMsg & operator<=(GNetMsg & );
	GNetMsg & operator>=(GNetMsg & );
};
/***************************************************************************/
/*
\brief - to znacznik konca czunka, de facto helper do operacji strumieniowych
*/
struct NEnd
{
	NEnd(){end=true;};
	bool end;
};
//#define nend (NEnd())
/***************************************************************************/
/**
 \brief klasa opisujaca obszar pamieci,
 */
struct NPair
{
	//! konstruktor
	NPair(){Reset();};
	/**
	konstruktor
	\param ptr_p - adres pamieci 
	\param size_p - dlugosc obszaru pamieci
	 */
	NPair(BYTE * ptr_p,int size_p){ptr=ptr_p;size=size_p;};
	/**
	inicjalizuje klase
	\param ptr_p - adres pamieci 
	\param size_p - dlugosc obszaru pamieci
	*/
	void			Init(BYTE * ptr_p,int size_p){ptr=ptr_p;size=size_p;};
	//! reset klasy
	void			Reset(){ptr=NULL;size=0;};
	//!	adres
	BYTE *			ptr;
	//! dlugosc
	DWORD32			size;
};
/***************************************************************************/
/**
\brief klasa opisujaca string,
*/
struct SPair
{
	//! konstruktor
	SPair(){Reset();};
	/**
	konstruktor
	\param ptr_p - adres pamieci 
	\param size_p - dlugosc obszaru pamieci
	*/
	SPair(char * ptr_p,int max_size_p){ptr=ptr_p;max_size=max_size_p;};
	/**
	inicjalizuje klase
	\param ptr_p - adres pamieci 
	\param size_p - dlugosc obszaru pamieci
	*/
	void			Init(char* ptr_p,int max_size_p){ptr=ptr_p;max_size=max_size_p;};
	//! reset klasy
	void			Reset(){ptr=NULL;max_size=0;};
	//!	adres
	char*			ptr;
	//! dlugosc
	DWORD32			max_size;

	SPair & operator()(){return *this;};

};
/***************************************************************************/
/**
 \brief klasa opisuje chunka, czyli obszar pamieci oraz jego stany,
 stany to pozycja ofsetu oraz stany bitowe
 */
struct GNetChunk: public NPair
{
	//! konstruktor
	GNetChunk(){Reset();};
	/**
	inicjalizuje klase
	\param ptr_p - adres pamieci 
	\param size_p - dlugosc obszaru pamieci
	*/
	void			Init(BYTE * ptr_p,int size_p){Reset();ptr=ptr_p;size=size_p;};
	//! resetuje pola opisu i pola stanow
	void				Reset()
	{
		NPair::Reset();
		offset=0;
		bits.UA();
		bits_count=0;
	}
	//! biezacy owset w obrebie chunka,
	DWORD32				offset;
	//!	temp bitowy do agregacji bitow
	GBitArray8			bits;
	//! licznik bitow
	BYTE 				bits_count;
};
/***************************************************************************/
#define TA			-1
#define TN			-2
#define TSYSTEM		-3 
#define TFRIENDS	-4
#define TMAINLOBBY	-5
#define TTABLELOBBY -6
#define TNOTES		-7
#define TDEALER		-8
#define THISTORY	-9
#define TUSERID		-10
class GNetTarget: public GNetStructure
{
public:
	GNetTarget()
	{
		Zero();
	}
	virtual void Zero()
	{
		tg=TN;
		tp=TN;
		tp_id=TN;
		number=0;
		loop=0;
		timestamp=0;
	}
	GNetTarget(INT32 tg,INT32 tp,INT32 tp_id)
	{
		Init(tg,tp,tp_id);
	}
	void Init(INT32 tg_p,INT32 tp_p,INT32 tp_id_p)
	{
		tg=tg_p;tp=tp_p;tp_id=tp_id_p;
	}
	GNetMsg&	W(GNetMsg & msg);
	GNetMsg&	R(GNetMsg & msg);
	BYTE &		Loop(){return loop;};
	bool		OldLoop(){return loop>3;};
	void		ResetPlayer(){tp=TN;};
	void		ResetUserID(){tp_id=TN;};
	void		AllPlayers(){tp=TA;};
public:
	INT32		tg,tp,tp_id;
	DWORD64		number;
	BYTE		loop;
	DWORD64		timestamp;
};
/***************************************************************************/
class GTicket: public GNetStructure
{
public:
	GTicket(){Zero();};
	GTicket(INT32 message_p, GNetTarget & target_p){Zero();message=message_p;target=target_p;}
	virtual void		Init(INT32 message_p){message=message_p;};
	virtual void		Zero()
	{
		message=0;
		target.Zero();
		send_deadline = 0;
	};
	virtual void		Reset(){Zero();};
	virtual GNetMsg	&	W(GNetMsg & msg)
	{
		return msg;
	};
	virtual GNetMsg &	R(GNetMsg & msg)
	{
		return msg;
	};
	GNetTarget &		Target(){return target;};
	bool				OldLoop(){return target.OldLoop();};

	INT32				message;
	GNetTarget			target;
	DWORD64				send_deadline;

	// Tickety o tym samym 'stable_sequence_identifier' zawsze beda nadawane w kolejnosci ich wyslania,
	// ale dopuszczamy mozliwosc, ze czesc zostanie pominieta.
	// Jesli zakolejkowane mamy tickety ABCD o tym samym id, a przyjdzie potwierdzenie odebrania C,
	// to A i B dropujemy zamiast wysylania ich ponownie.
	string				stable_sequence_identifier;
};
typedef boost::shared_ptr<GTicket> GTicketPtr;
/***************************************************************************/
struct SEmpty: public GNetStructure
{
	SEmpty(){};
};
template <class TYPE> class GTicketTmpl: public GTicket
{
public:
	GTicketTmpl(){Zero();};
	GTicketTmpl(INT32 message_p, GNetTarget & target_p){Zero();message=message_p;target=target_p;}
	virtual void		Zero()
	{
		message=0;
		target.Zero();
		data.Zero();
	};
	virtual GNetMsg	&	W(GNetMsg & msg)
	{
		data.W(msg);
		return msg;
	};
	virtual GNetMsg &	R(GNetMsg & msg)
	{
		data.R(msg);
		return msg;
	};
	TYPE				data;
};


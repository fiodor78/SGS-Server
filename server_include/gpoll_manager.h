/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#if !defined(LINUX) || defined(MACOSX)
#define EPOLL_EMULATION
#endif

#ifdef EPOLL_EMULATION
enum EPOLL_EVENTS
{
	EPOLLIN = 0x001,
#define EPOLLIN EPOLLIN
	EPOLLPRI = 0x002,
#define EPOLLPRI EPOLLPRI
	EPOLLOUT = 0x004,
#define EPOLLOUT EPOLLOUT
	EPOLLRDNORM = 0x040,
#define EPOLLRDNORM EPOLLRDNORM
	EPOLLRDBAND = 0x080,
#define EPOLLRDBAND EPOLLRDBAND
	EPOLLWRNORM = 0x100,
#define EPOLLWRNORM EPOLLWRNORM
	EPOLLWRBAND = 0x200,
#define EPOLLWRBAND EPOLLWRBAND
	EPOLLMSG = 0x400,
#define EPOLLMSG EPOLLMSG
	EPOLLERR = 0x008,
#define EPOLLERR EPOLLERR
	EPOLLHUP = 0x010,
#define EPOLLHUP EPOLLHUP
	EPOLLONESHOT = (1 << 30),
#define EPOLLONESHOT EPOLLONESHOT
	EPOLLET = (1 << 31)
#define EPOLLET EPOLLET
};

/* Valid opcodes ( "op" parameter ) to issue to epoll_ctl().  */
#define EPOLL_CTL_ADD 1 /* Add a file decriptor to the interface.  */
#define EPOLL_CTL_DEL 2 /* Remove a file decriptor from the interface.  */
#define EPOLL_CTL_MOD 3 /* Change file decriptor epoll_event structure.  */



typedef union epoll_data {
	void *ptr;
	SOCKET fd;
	__uint32_t u32;
	__uint64_t u64;
} epoll_data_t;

struct epoll_event {
	__uint32_t events;  /* Epoll events */
	epoll_data_t data;  /* User data variable */
};
#endif
/***************************************************************************/
/**
\brief
PollManager to klasa odpowiadajaca za komunikacje ze swiatem
w srodowisku Linux uzywa syspoll'a, w srodowisku Windows uzywa funkcji select
i jest de facto emulowana - rozwiazanie Windowsa nie nadaje sie praktycznie do niczego
poza wolnymi testami
*/
class GPollManager
{
public:
	/**
	konstruktor
	*/
	GPollManager();
	/**
	destruktor
	*/
	virtual ~GPollManager();
	/**
	inicjalizuje manager'a, 
	\param size - okresla wiekosc struktury zaalokowanej na dane wyjsciwe, czyli na informacje o zmienionych socketach
	*/
	bool				Init(int size);
	/**
	kasuje manager'a, usuwa element z tabeli syspoll'a
	*/
	void				Destroy();
	/**
	dodaje nowy element do syspoll'a
	\param socket - wskaznik an GSocket
	\return - true albo false
	*/
	bool				AddPoll(GSocket * socket);
	/**
	usuwa element z syspoll'a
	\param socket - wskaznik na GSocket
	\return - true albo false
	*/
	bool				RemovePoll(GSocket * socket);
	/**
	zmienia flagi parsowania - doklada flage sprawdzania praw zapisu
	\param socket - wskaznik na GSocket
	\param mode - czy sprawdzac ta flage
	\return - true albo false
	*/
	bool				TestWritePoll(GSocket * socket,bool mode);
	/**
	wykonuje funckje poll, zwraca ilosc parametrow ktore 
	sie zmienily
	\return liczba zmienionych elementow
	*/
	int					Poll();
	/**
	wolane zaraz za poll'em
	zwraca Event'y, ile ich jest zwrocil nam poll
	*/
	epoll_event *		GetEvent(int pos){return &events[pos];};

	bool				Free(){return count<maxsize;};
	void				AddForceWritePoll(GSocket * socket);
	void				RemoveForceWritePoll(GSocket * socket);
	void				ForceWritePoll();

	typedef boost::function<void (const char *)> t_raport_fun;
	t_raport_fun		raport;
private:
	/**
	maksymalna ilosc zwracanych ewentow
	*/
	int					maxsize;
	int					count;
	/**
	pointer na tabele eventow, trzeba ja zaalokowac przed uzyciem, kernel ja tylko wypelnia
	*/
	epoll_event *		events;
	/**
	deskryptor na syspoll'a
	*/
	int					epfd;

	struct hash_addr
	{ 
		size_t operator()(GSocket * x) const { return (size_t)x;} ;
	};
	struct eq_addr
	{
		bool operator()(GSocket* a1, GSocket* a2) const
		{
			return (a1==a2);
		}
	};
	typedef dense_hash_set<GSocket*, hash_addr, eq_addr>	TSocketHash;
	//vector<GSocket *>	force_write;
	TSocketHash			force_write;

#ifdef EPOLL_EMULATION
	vector<epoll_event>	epoll_ctl;
#endif
};



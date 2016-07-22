/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

struct SMemDesc
{
	void Reset(){ptr=NULL;size=0;};
	DWORD32		Size(){return size;};
	char *		ptr;				
	DWORD32 	size;				
};
/***************************************************************************/
class GMemoryManager;
/**
\brief bazowa klasa obslugujaca bufory danych, alokuje pamiec, itd..
podstawowym celem jest doprowadzenie do takiej sytuacji, w ktorej bedzie tylko tyle zaalokowanych 
buforow ile jest potrzebne. czyli nie alokujemy buforka dla kazdego elementu gdy go tworzymu,
element GMemory to tylko nosnik informacji o polaczeniu, natomiast bufor jest przydzielany dopiero 
gdy jest do czegos potrzebny, potem jest zwalniany.
nie ma sensu robic super optymalizacji jesli uzyjemy TDMemory z Google, ta klasa ma zarzadzanie pamiecia
miedzy watkami oraz bardzo ladne cachowanie i rozwiazuje wiekszosc naszych problemow,
czyli konceptrujemy sie na wielkosci buforow
*/
class GMemory
{
public:
	/**
	konstruktor, 
	*/
	GMemory();
	/**
	\param mgr_p - adres do memory managera
	*/
	void				Init(GMemoryManager * mgr_p);
	/**
	alokuje pamiec
	*/
	bool				Allocate(DWORD32 size=K1);
	/**
	sprawdza ile zaalokowano pamieci
	*/
	DWORD32				GetAllocated();
	/**
	dealokuje pamieci
	*/
	void				Deallocate();
	/**
	realokuje pamiec pamieci
	\param mul - mnoznik, uile razy powiekszyc bufor
	*/
	bool				Reallocate(DWORD32 mul=2);
	/**
	realokuje pamiec pamieci
	\param size - docelowy rozmiar obszaru
	*/
	bool				ReallocateTo(DWORD32 size=K1);
	bool				ReallocateToFit(DWORD32 size);
	char *				Start();
	char *				End();
	char *				Parse();
	DWORD32				Size();
	DWORD32				Free();
	DWORD32				Used(){return used;};
	DWORD32				Parsed(){return parsed;};
	void				IncreaseUsed(DWORD64 size);
	void				IncreaseParsed(DWORD32 size);
	DWORD32				ToParse();
	bool				RemoveParsed()
	{
		if(parsed==0) return false;
		if(parsed==used) 
		{
			parsed=0;
			used=0;
			return false;
		}
		Move(parsed);
		parsed=0;
		return true;
	}
	char * operator []	(int offset)
	{
		return &md.ptr[offset];
	}
	bool				Move(DWORD32 size)
	{
		if(size>used){used=0;return true;}
		assert(used>=size);
		memmove(md.ptr,&md.ptr[size],used-size);
		used-=size;
		return false;
	}
	void operator<<		(char * str)
	{
		AddString(str);
	}
	void operator<<		(string & str)
	{
		AddString(str);
	}
	void				AddString(char * str);
	void				AddString(string & str);

	void operator=		(GMemory & m)
	{
		Deallocate();
		Allocate(m.md.size);
		if(md.size==m.md.size)
		{
			memcpy(md.ptr,m.md.ptr,m.md.size);
			used=m.used;
		}
	}
private:
	void				Reset();
	/**wskaznik do memory manger'a*/
	GMemoryManager *	mgr;		
	/**deskryptor chunka pamieci*/
	SMemDesc			md;			
	DWORD32				used;
	DWORD32				parsed;
};

/***************************************************************************/
/**
\brief klasa zarzadzajaca buforami danych, koncepcja jest taka:
- mamy bufory podzielone na wiele wielkosci, 1k, 2k, 4k itd az do mb
- server alokuje sobie bufor, wtedy alokuje go normalnie
- alokacja odbywa sie do obszaru stanowiacego jakas tam krotnosc 1024
- gdy serwer zwalnia pamiec nie kasujemy jej tylko przydzielamy do jednego z mem'ow - stosow opisujacych 
zaalokowana pamiec,
- na podstawie access_time mozemy sobie sprawdzic kiedy element byl alokowany, mozemy wtedy ustalic ile nam tego naprawde potrzeba,
i optymalizwac uzycie pamieci
- cala procedura nie dotyczy pamieci alokowanej o niestandardowych rozmiarach. w takim wypadku alokujemy z glownego heap'a i zwracamy do razu tam.
*/
class GMemoryManager: boost::noncopyable 
{
public:
	GMemoryManager();
	~GMemoryManager();
	void				Init();

	void				ResetMD(SMemDesc & md);										//kasuje zawartosc pamieci gdy zamykamy serwer
	void				Reset();
	
	void				Destroy();													//kasuje zaalokowane dane
	SMemDesc			Allocate(DWORD32 size=K1);
	bool				Deallocate(SMemDesc & md);									//zwalnia zaalokowana palieci
	bool				Reallocate(SMemDesc & md,int mul=2);						//realokuje buffor - zamienia go na 2x wiekszy
	
	bool				ReallocateTo(SMemDesc & md,DWORD32 size);					//realokuje buffor do podanego rozmiaru
	bool				CanCache(int pos);											//funkcja steruje cachowanie, 
	void				UpdateTime(GClock &clock);									//funkcja sprawdza czy nie ma jakis buforow do zwolnienia, w razie czego czysci pamiec
	void				StopCache(){cache_memory=false;};							//wy³¹cza cachowanie pamiêci
	void				StartCache(){cache_memory=true;};							//w³¹cza cachowanie pamiêci
	
	size_t				GetCachedMemSize(int pos){return mem_cached[pos].size();};	//zwraca rozmiar chunku na pozycji
	int					GetCacheCount(int pos){return mem_cache_count[pos];};		//zwraca ilosc czunków na pozycji
	int					GetUsedMemSize(int pos){return mem_used[pos];};				//zwraca u¿yt¹ pamiêæ
private:
	int					mem_used[KCOUNT];
	int					mem_cache_count[KCOUNT];
	vector<SMemDesc>	mem_cached[KCOUNT];
	bool				cache_memory;												//okresla czy cachowac pamiec*


	struct				SLimits
	{
		void			Reset(){time=CACHE_LIMIT_60_SEC;used_memory=-1;};
		DWORD64			time;
		INT64			used_memory;
	}limits;

public:
	struct				SLocalStats
	{
		void			Reset(){allocation=deallocation=real_allocation=real_deallocation=cache_allocation=cache_deallocation=reallocation=0;efficiency=0;allocated_memory=big_size_allocated_memory=0;};
		INT64			allocation;
		INT64			deallocation;
		INT64			real_allocation;
		INT64			real_deallocation;
		INT64			cache_allocation;
		INT64			cache_deallocation;
		INT64			reallocation;
		float			efficiency;
		INT64			allocated_memory;
		INT64			big_size_allocated_memory;
	}local_stats;
};




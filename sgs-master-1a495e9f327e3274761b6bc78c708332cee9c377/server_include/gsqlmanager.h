/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

bool AnalizeSQLQuery(int ret,const char * query);
bool MySQL_Query(MYSQL * mysql, const char * query, char * qa_filename, int qa_line);
bool MySQL_Query(MYSQL * mysql, const string & query, char * qa_filename, int qa_line);
bool MySQL_Query(MYSQL * mysql, const boost::format & query, char * qa_filename, int qa_line);

#define MySQLQuery(m, q) MySQL_Query(m, q, __FILE__, __LINE__)

/***************************************************************************/
class GSQLElement
{
public:
	GSQLElement();
	void		Init(const char * txt, EDatabase database);
	DWORD32		R(GMemory * memory);				// odczytuje element z poczatku bufora, zwraca ile bajtow zajmowal element w buforze
	void		W(GMemory * memory);				// zapisuje element na koniec bufora

	string		query;
	EDatabase	database;
};
/***************************************************************************/
class GMySQLReal: boost::noncopyable
{
public:
	GMySQLReal(MYSQL * mysql,const string &str)
	{
		size_t l=str.size();
		buf=new char[l*2+1];
		mysql_real_escape_string(mysql,buf,str.c_str(),(unsigned long)l);
	}
	char * operator()(){return buf;};
	Wrap<char>	buf;
};
/***************************************************************************/
class GSQLManager: private boost::noncopyable, public boost::signals2::trackable
{
public:
	GSQLManager();
	void						Init();
	void						Action();
	void						Destroy();
	bool						Continue(){return !flags[EFClose];};
	void						Close(){flags.set(EFClose);};
	void						Add(const char * query, EDatabase database);
	void						Add(const string & query, EDatabase database);
	void						Add(GMemory * queries, EDatabase database);
	bool						SQL(GSQLElement * el);
protected:
	bitset<numEFlags>			flags;
	boost::thread *				sql_thread;
	boost::mutex				mtx_manager;	
	GRaportInterface			raport_interface;

	GMemoryManager				memory_manager;
	GMemory						sql_new_elements_buffer;
	GMemory						sql_elements_buffer;
};

/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"

#include "mysql.h"
#include "errmsg.h"
extern MYSQL			mysql;

void update_query_stats(char * qa_filename, int qa_line, DWORD64 query_time, const char * query);
static void update_slowest_query(DWORD64 duration_ms);
/***************************************************************************/
bool AnalizeSQLQuery(int ret,const char * query)
{
	//TODO - sprawdzic czy ten graport wogole dziala, usunac te print'f
	switch(ret)
	{
	case CR_COMMANDS_OUT_OF_SYNC:
		SRAP(ERROR_SQL_CR_COMMANDS_OUT_OF_SYNC);
		GRAPORT(GSTR(ERROR_SQL_CR_COMMANDS_OUT_OF_SYNC));
		GRAPORT("Lost SQL Command %s",query);
		return false;
	case CR_UNKNOWN_ERROR:
		SRAP(ERROR_SQL_CR_UNKNOWN_ERROR);
		GRAPORT(GSTR(ERROR_SQL_CR_UNKNOWN_ERROR));
		GRAPORT("Lost SQL Command %s",query);
		return false;
	case CR_SERVER_GONE_ERROR:
		SRAP(ERROR_SQL_CR_SERVER_GONE_ERROR);
		GRAPORT(GSTR(ERROR_SQL_CR_SERVER_GONE_ERROR));
		GRAPORT("Blocked SQL Command %s",query);
		return false;
	case CR_SERVER_LOST:
		SRAP(ERROR_SQL_CR_SERVER_LOST);
		GRAPORT(GSTR(ERROR_SQL_CR_SERVER_LOST));
		GRAPORT("Blocked SQL Command %s",query);
		return false;
	default:
		SRAP(INFO_SQL_QUERIES);
		break;
	};
	return (ret == 0);
}
/***************************************************************************/
bool MySQL_Query(MYSQL * mysql, const char * query, char * qa_filename, int qa_line)
{
	DWORD64 start_time, end_time;

	start_time = GetTime();
	int ret=mysql_query(mysql,query);
	end_time = GetTime();

	if (end_time - start_time >= GSECOND_1)
	{
		SRAP(WARNING_SQL_SLOW_QUERY);
		GRAPORT(GSTR(WARNING_SQL_SLOW_QUERY));
		GRAPORT("Slow query (exec time: %lld ms): %s", end_time - start_time, query);
	}

	update_query_stats(qa_filename, qa_line, end_time - start_time, query);
	update_slowest_query(end_time - start_time);

	return AnalizeSQLQuery(ret,query);
};
/***************************************************************************/
bool MySQL_Query(MYSQL * mysql, const string & query, char * qa_filename, int qa_line)
{
	return MySQL_Query(mysql, query.c_str(), qa_filename, qa_line);
};
/***************************************************************************/
bool MySQL_Query(MYSQL * mysql, const boost::format & query, char * qa_filename, int qa_line)
{
	return MySQL_Query(mysql, query.str().c_str(), qa_filename, qa_line);
};
/***************************************************************************/
GSQLElement::GSQLElement()
{
}
/***************************************************************************/
void GSQLElement::Init(const char * query_p, EDatabase database_p)
{
	query = query_p;
	database = database_p;
}
/***************************************************************************/
DWORD32 GSQLElement::R(GMemory * memory)
{
	DWORD32 len;
	if (memory->ToParse() > 0)
	{
		len = strlen(memory->Parse()) + 1;
		Init(memory->Parse(), *(EDatabase*)(memory->Parse() + len));
		memory->IncreaseParsed(len + sizeof(EDatabase));
		return (len + sizeof(EDatabase));
	}
	return 0;
	// Uwaga! Funkcja nie wola memory->RemoveParsed(). Nalezy to zrobic samemu.
}
/***************************************************************************/
void GSQLElement::W(GMemory * memory)
{
	INT32 len = query.length() + 1;
	memory->ReallocateToFit(memory->Used() + len);
	memcpy(memory->End(), query.c_str(), len);
	memcpy(memory->End() + len, (const char*)&database, sizeof(EDatabase));
	memory->IncreaseUsed(len + sizeof(EDatabase));
}
/***************************************************************************/
GSQLManager::GSQLManager():raport_interface(&raport_manager_global)
{
}
/***************************************************************************/
void GSQLManager::Destroy()
{
	Close();
	sql_thread->join();
	Delete(sql_thread);
	raport_interface.Destroy();
}
/***************************************************************************/
void GSQLManager::Init()
{
	sql_new_elements_buffer.Init(&memory_manager);
	sql_elements_buffer.Init(&memory_manager);

	global_signals->close_fast.connect(boost::bind(&GSQLManager::Close,this));
	sql_thread=new boost::thread(boost::bind(&GSQLManager::Action,this));
}
/***************************************************************************/
void GSQLManager::Action()
{
	while(Continue() || sql_new_elements_buffer.ToParse())
	{
		if (sql_new_elements_buffer.ToParse() == 0 && sql_elements_buffer.ToParse() == 0)
		{
			Sleep(10);
		}
		else
		{
			boost::mutex::scoped_lock lock(mtx_manager);
			DWORD32 data_len = sql_new_elements_buffer.ToParse();
			if (data_len > 0)
			{
				sql_elements_buffer.ReallocateToFit(sql_elements_buffer.Used() + data_len);
				memcpy(sql_elements_buffer.End(), sql_new_elements_buffer.Parse(), data_len);
				sql_elements_buffer.IncreaseUsed(data_len);
				sql_new_elements_buffer.IncreaseParsed(data_len);
				sql_new_elements_buffer.RemoveParsed();
			}
		}
		if (sql_elements_buffer.ToParse() > 0)
		{
			GSQLElement el;
			DWORD64 last_time = GetTime();

			while (sql_elements_buffer.ToParse() > 0)
			{
				DWORD32 element_length = el.R(&sql_elements_buffer);
				if (SQL(&el))
				{
					// Wystapil blad podczas wykonania zapytania SQL - wsadzamy je z powrotem do bufora.
					sql_elements_buffer.IncreaseParsed(-(INT32)element_length);
					break;
				}
			}
			sql_elements_buffer.RemoveParsed();

			DWORD64 time = GetTime();
			if (time - last_time > GSECOND_1)
			{
				RAPORTVERBOSE("SQLManager delay (timeout: %lld ms)", time - last_time);
			}

			// Cos poszlo nie tak. Usypiamy watek, zeby nie zabic serwera.
			if (sql_elements_buffer.ToParse() > 0)
			{
				Sleep(100);
				if(!Continue())
				{
					break;
				}
			}
		}
	}
}
/***************************************************************************/
bool GSQLManager::SQL(GSQLElement * el)
{
	DWORD64 start_time, end_time;
	MYSQL * mysql_current = NULL;
	int ret = 0;

	if (el->database == EDB_SERVERS)
		mysql_current = &mysql;

	if (mysql_current == NULL)
		return false;

	boost::recursive_mutex::scoped_lock lock(mtx_sql);

	start_time = GetTime();
	ret = mysql_query(mysql_current, el->query.c_str());
	end_time = GetTime();

	if (end_time - start_time >= GSECOND_1)
	{
		SRAP(WARNING_SQLMANAGER_SLOW_QUERY);
		RAPORTVERBOSE("%s", GSTR(WARNING_SQLMANAGER_SLOW_QUERY));
		RAPORTVERBOSE("SQLManager slow query (exec time: %lld ms): %s", end_time - start_time, el->query.c_str());
	}

	AnalizeSQLQuery(ret,el->query.c_str());

	if(ret==CR_SERVER_GONE_ERROR || ret==CR_SERVER_LOST) 
	{
		return true;
	}

	// Odbieramy ew. wyniki zapytania, zeby uniknac bledu CR_COMMANDS_OUT_OF_SYNC.
	// Dot. to glownie zapytan wywolujacych stored procedures.
	if (ret == 0)
	{
		MYSQL_RES * result = mysql_store_result(mysql_current);
		if (result)
		{
			mysql_free_result(result);
		}
		else
		{
			while (mysql_next_result(mysql_current) == 0) {};
		}
	}

	return false;
}
/***************************************************************************/
void GSQLManager::Add(const char * query, EDatabase database)
{
	boost::mutex::scoped_lock lock(mtx_manager);
	GSQLElement el;
	el.Init(query, database);
	el.W(&sql_new_elements_buffer);
}
/***************************************************************************/
void GSQLManager::Add(const string & query, EDatabase database)
{
	boost::mutex::scoped_lock lock(mtx_manager);
	GSQLElement el;
	el.Init(query.c_str(), database);
	el.W(&sql_new_elements_buffer);
}
/***************************************************************************/
void GSQLManager::Add(GMemory * queries, EDatabase database)
{
	DWORD32 data_len = queries->ToParse();
	if (data_len > 0)
	{
		boost::mutex::scoped_lock lock(mtx_manager);

		sql_new_elements_buffer.ReallocateToFit(sql_new_elements_buffer.Used() + data_len);
		memcpy(sql_new_elements_buffer.End(), queries->Parse(), data_len);
		sql_new_elements_buffer.IncreaseUsed(data_len);
		queries->IncreaseParsed(data_len);
		queries->RemoveParsed();
	}
}
/***************************************************************************/

// Tam gdzie sa tablice ostatni przedzial to slow query > 1s. 
// Wczesniejsze to proporcjonalnie podzielona 1s.
// Liczba przedzialow 'count_thresholds' musi byc rowna liczbie przedzialow 'time_thresholds'

struct query_stats
{
	string		filename;
	int			line;

	int			count;					// liczba wykonanych query
	DWORD64		total_time;				// sumaryczny czas wykonanych query
	DWORD64		min_time;				// min. czas wykonania query
	DWORD64		max_time;				// max. czas wykonania query

	int			count_thresholds[5];	// liczba wykonanych query (z podzialem na przedzialy)
	DWORD64		time_thresholds[5];		// sumaryczny czas wykonanych query (z podzialem na przedzialy)

	string		slowest_query;

	void Zero()
	{
		filename = "";
		line = 0;
		count = 0;
		total_time = 0;
		min_time = 0;
		max_time = 0;
		int i;
		for (i = 0; i < (int)sizeof(count_thresholds) / sizeof(count_thresholds[0]); i++)
			count_thresholds[i] = 0;
		for (i = 0; i < (int)sizeof(time_thresholds) / sizeof(time_thresholds[0]); i++)
			time_thresholds[i] = 0;
		slowest_query = "";
	}
};

static int						max_base = 0;
static map<string,int>			filename2base;
static map<int,query_stats>		qstats;
static boost::mutex				mtx_qstats;

query_stats * find_query_stats(char * qa_filename, int qa_line)
{
	struct query_stats * qs;
	string fn = qa_filename;
	int query_id = 0;
	boost::mutex::scoped_lock lock(mtx_qstats);

	map<string,int>::iterator it;
	it = filename2base.find(fn);
	if (it != filename2base.end())
	{
		query_id = it->second + qa_line;
	}
	else
	{
		filename2base.insert(pair<string,int>(fn, max_base));
		query_id = max_base + qa_line;
		max_base += 1000000;
	}

	map<int,query_stats>::iterator qit;
	qit = qstats.find(query_id);
	if (qit != qstats.end())
		return &(qit->second);

	qs = new query_stats;
	if (qs != NULL)
	{
		qs->Zero();
		char *p, *s;
		s = qa_filename;
		for (p = qa_filename; *p != 0; p++)
			if (*p == '\\' || *p == '/')
				s = p + 1;
		qs->filename = s;
		qs->line = qa_line;
		qstats.insert(pair<int,query_stats>(query_id, *qs));
	}

	qit = qstats.find(query_id);
	if (qit != qstats.end())
		return &(qit->second);

	return NULL;
}

void update_query_stats(char * qa_filename, int qa_line, DWORD64 query_time, const char * query)
{
	struct query_stats * qs = find_query_stats(qa_filename, qa_line);
	if (qs == NULL)
		return;

	if (qs->count == 0 || query_time < qs->min_time)
		qs->min_time = query_time;
	if (qs->count == 0 || query_time > qs->max_time)
	{
		qs->max_time = query_time;
		qs->slowest_query = query;
		if (qs->slowest_query.size() > 768)
		{
			qs->slowest_query.resize(768);
		}
	}
	qs->count++;
	qs->total_time += query_time;

	int idx, n = (int)sizeof(qs->count_thresholds) / sizeof(qs->count_thresholds[0]);
	if (query_time >= GSECOND_1)
		idx = n - 1;
	else
		idx = (int)((query_time * (n - 1)) / GSECOND_1);

	qs->count_thresholds[idx]++;
	qs->time_thresholds[idx] += query_time;
}
/***************************************************************************/
struct slowest_query_stats
{
	DWORD64		duration_ms;
	time_t		timestamp_finish;

	slowest_query_stats()
	{
		duration_ms = 0;
		timestamp_finish = 0;
	}
} previous_slowest_query, current_slowest_query;

static void rotate_slowest_query_window()
{
	time_t now;
	time(&now);

	const int SLOWEST_QUERY_WINDOW_DURATION_SECONDS = 300;

	if (now - (now % SLOWEST_QUERY_WINDOW_DURATION_SECONDS) !=
		current_slowest_query.timestamp_finish - (current_slowest_query.timestamp_finish % SLOWEST_QUERY_WINDOW_DURATION_SECONDS))
	{
		previous_slowest_query = current_slowest_query;
		current_slowest_query.duration_ms = 0;
		current_slowest_query.timestamp_finish = now;
	}
}

static void update_slowest_query(DWORD64 duration_ms)
{
	rotate_slowest_query_window();
	if (duration_ms > current_slowest_query.duration_ms)
	{
		current_slowest_query.duration_ms = duration_ms;
	}
}

/***************************************************************************/
char * get_query_stats(query_stats * qs)
{
	static char buf[1024], *p;

	p = buf;
	*p = 0;

	sprintf(p, "%s:%d", qs->filename.c_str(), qs->line);
	p = p + strlen(p);
	while (p - buf < 40)
		*p++ = ' ';
	*p = 0;

	sprintf(p, " ||| %7d %7lld.%03d s. ||| ", qs->count, qs->total_time / 1000, (int)(qs->total_time % 1000));
	p = p + strlen(p);
	sprintf(p, "min. %3d.%03d s. , ", (int)(qs->min_time / 1000), (int)(qs->min_time % 1000));
	p = p + strlen(p);
	sprintf(p, "max. %3d.%03d s. , ", (int)(qs->max_time / 1000), (int)(qs->max_time % 1000));
	p = p + strlen(p);
	sprintf(p, "avg. %3d.%03d s. ||| ", (int)((qs->count == 0 ? 0 : qs->total_time / qs->count) / 1000), (int)((qs->count == 0 ? 0 : qs->total_time / qs->count) % 1000));
	p = p + strlen(p);

	int i, n = (int)sizeof(qs->count_thresholds) / sizeof(qs->count_thresholds[0]);
	for (i = 0; i < n; i++)
	{
		sprintf(p, " %7d (%3d.%02d%%) %7lld.%03d s. | ", 
				qs->count_thresholds[i], 
				(int)(qs->count == 0 ? 0 : ((INT64)qs->count_thresholds[i] * 10000) / (INT64)qs->count) / 100, 
				(int)(qs->count == 0 ? 0 : ((INT64)qs->count_thresholds[i] * 10000) / (INT64)qs->count) % 100,
				qs->time_thresholds[i] / 1000, (int)(qs->time_thresholds[i] % 1000));
		p = p + strlen(p);
	}

	return buf;
}

char * get_query_stats_csv(query_stats * qs)
{
	static char buf[1024], *p;

	p = buf;
	*p = 0;

	sprintf(p, "%s:%d;", qs->filename.c_str(), qs->line);
	p = p + strlen(p);

	sprintf(p, "%d;%lld.%03d;", qs->count, qs->total_time / 1000, (int)(qs->total_time % 1000));
	p = p + strlen(p);
	sprintf(p, "%d.%03d;", (int)(qs->min_time / 1000), (int)(qs->min_time % 1000));
	p = p + strlen(p);
	sprintf(p, "%d.%03d;", (int)(qs->max_time / 1000), (int)(qs->max_time % 1000));
	p = p + strlen(p);
	sprintf(p, "%d.%03d;", (int)((qs->count == 0 ? 0 : qs->total_time / qs->count) / 1000), (int)((qs->count == 0 ? 0 : qs->total_time / qs->count) % 1000));
	p = p + strlen(p);

	int i, n = (int)sizeof(qs->count_thresholds) / sizeof(qs->count_thresholds[0]);
	for (i = 0; i < n; i++)
	{
		sprintf(p, "%d;%lld.%03d;", 
				qs->count_thresholds[i], 
				qs->time_thresholds[i] / 1000, (int)(qs->time_thresholds[i] % 1000));
		p = p + strlen(p);
	}

	sprintf(p, "%s", (char*)qs->slowest_query.c_str());

	return buf;
}

void StreamSQLStats(strstream &s, int output_type)
{
	char * p;

	if (output_type == CMD_SQLSTATS_SLOWEST)
	{
		rotate_slowest_query_window();
		s << previous_slowest_query.duration_ms << ";" << previous_slowest_query.timestamp_finish << lend;
		return;
	}

	map<int,query_stats>::iterator qit;
	for (qit = qstats.begin(); qit != qstats.end(); qit++)
	{
		if (output_type == CMD_SQLSTATS_TXT)
		{
			p = get_query_stats(&(qit->second));
			s << p << lend;
		}
		if (output_type == CMD_SQLSTATS_CSV)
		{
			p = get_query_stats_csv(&(qit->second));
			s << p << lend;
		}
	}
}

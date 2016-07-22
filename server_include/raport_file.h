/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#ifndef RAPOT_FILE_H
#define RAPOT_FILE_H

typedef bool (*RAPORT_CALLBACK)(const char * group,const char * msg);
/***************************************************************************/
class GRaportInterface;
class GRaportManager: private boost::noncopyable, public boost::signals2::trackable
{
public:
	GRaportManager();
	~GRaportManager();
	void			Init(const char * raport_path,const char * game_name);
	void			Destroy();
	void			UpdateTime(GClock &clock);
	void			UpdateName(GClock & clock);
	void			ClearModified(){modified=false;};
	bool			TestRaportFile();
	bool			TestOverflow();
	void			Flush();
	void			Raport(const GRaportInterface * int_);
	void			SetRaportCallback(RAPORT_CALLBACK fun){raport_callback=fun;};
	char*			GetFileName(){return file_name;};
	void			SetOverflow(int over){overflow=over;};
	void			AddTimeSignature();
	void			AddLineTimeSignature(bool mode){add_line_time_signature=mode;};
	RAPORT_CALLBACK	raport_callback;
	void  			Raport(const char * format, ...);
	void  			Raport(string format);
	void  			RaportGame(DWORD64 game,const char * format, ...);
	void			RaportLine();
	void			AddBuffer(const char * buffer);
	void			SetRaportMode(ERapMode mode){raport_mode=mode;};

	bool			initialized;

private:
	ERapMode		raport_mode;
	GTimeInterval	time_period;
	GTimeInterval	flush_period;
	struct tm		yesterday;
	char			date[256];
	
	FILE *			raport_file;
	string			raport_path;
	string			game_name;
	char			file_name[256];
	bool			modified;
	bool			wait_for_data;
	boost::mutex	sem_raport;

	char			buffer_arg[K4];
	char			buffer_format[K4];
	size_t			max_len;
	int				overflow;
	int				overflow_lines;
	int				lines;
	bool			add_time_signature;
	bool			add_line_time_signature;
};
/***************************************************************************/
#define MAX_RAPORT_BUFFERS	128
class GRaportInterface: private boost::noncopyable
{
public:
	GRaportInterface(GRaportManager *raport_manager_);
	~GRaportInterface();
	void			Init();
	void			Destroy();
	void  			Raport(const char * format, ...);
	void  			RaportVerbose(const char * format, ...);
	void  			Raport(string format);
	void  			RaportChar(const char * format){Raport(format);};
	void  			RaportStr(string format){Raport(format);};
	void  			RaportGame(DWORD64 game,const char * format, ...);
	void			RaportLine();
	void			AddBuffer(const char * buffer);
	void			Flush();
	void			UpdateTime(GClock &clock);
	void			SetVerboseMode(bool verbose_mode) { this->verbose_mode = verbose_mode; };
	bool			IsVerboseMode() { return verbose_mode; };
	int				final_buffers_size;
	int				lines;
	char			final_buffers[K64];
private:
	char			date[256];
	size_t			max_len;
	char			buffer_arg[K4];
	char			buffer_format[K4];
	GRaportManager *mgr;
	bool			verbose_mode;
};
/***************************************************************************/
class GSimpleRaportManager: private boost::noncopyable, public boost::signals2::trackable
{
public:
	GSimpleRaportManager();
	~GSimpleRaportManager();
	void			Init(const char * raport_path,const char * base_name);
	void			Destroy();
	void			UpdateTime(GClock &clock);
	void			UpdateName(GClock & clock);
	void			ClearModified(){modified=false;};
	bool			TestRaportFile();
	bool			TestOverflow();
	void			Flush();
	char*			GetFileName(){return file_name;};
	void			SetOverflow(int over){overflow=over;};
	void			AddTimeSignature();
	void			AddLineTimeSignature(bool mode){add_line_time_signature=mode;};
	void  			Raport(const char *str);
	void			RaportLine();
	void			SetRaportMode(ERapMode mode){raport_mode=mode;};
private:
	bool			initialized;
	ERapMode		raport_mode;
	GTimeInterval	time_period;
	GTimeInterval	flush_period;
	struct tm		yesterday;
	char			date[256];
	
	FILE *			raport_file;
	string			raport_path;
	string			base_name;
	char			file_name[256];
	bool			modified;
	bool			wait_for_data;

	int				overflow;
	int				overflow_lines;
	int				lines;
	bool			add_time_signature;
	bool			add_line_time_signature;
};




#ifdef SERVICES
#define TAB_NAME	"SS"
#else
#define TAB_NAME	"SG"
#endif


extern GRaportManager raport_manager_global;

#define RAPORT raport_interface.Raport
#define RAPORTGAME raport_interface.RaportGame
#define FLUSH	raport_interface.Flush
#define RAPORT_LINE raport_interface.RaportLine();
#define RL raport_interface_local.Raport
#define RAPORTVERBOSE raport_interface.RaportVerbose
#define RLVERBOSE raport_interface_local.RaportVerbose


#define GRAPORT raport_manager_global.Raport
#define GRAPORTGAME raport_manager_global.RaportGame
#define GFLUSH	raport_manager_global.Flush
#define GRAPORT_LINE raport_manager_global.RaportLine();
#define GRL raport_manager_local.Raport

#endif

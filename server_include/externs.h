/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

//external global variable definitions
extern string			game_name;
extern char *			game_version;
extern INT32			game_instance;
extern INT32			game_version_number;

extern boost::recursive_mutex	mtx_sql;
extern GSocketStack		stack_mgr;
extern GStatistics *	global_statistics;
extern GSignals *		global_signals;
extern GSystem *		global_system;
extern GSQLManager*		global_sqlmanager;
extern GServerConfig*	global_serverconfig;
extern boost::mutex		mtx_global;
extern volatile bool	vol_global;

extern boost::mt19937													rng_alg;      
extern boost::uniform_int<>												rng_dist;
extern boost::variate_generator<boost::mt19937, boost::uniform_int<> >	rng;

extern boost::uniform_int<>												rng_dist_byte;
extern boost::variate_generator<boost::mt19937, boost::uniform_int<> >	rng_byte;

void SigQuit(int);
void SigInt(int);
void SigTerm(int);
void SigTStp(int);
void SigUser1(int);


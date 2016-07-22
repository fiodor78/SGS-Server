/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"
#include "headers_game.h"

/***************************************************************************/
bool GServerGame::Init()
{
	if(!GServer::Init()) return false;

	locationdb_tasks.clear();
	locationdb_tasks_results.clear();
	http_request_tasks.clear();
	http_request_tasks_results.clear();
	seq_http_request_id = 1;

	group.Init(this, &socket_internal_map, 0, EGT_NONE);
	RAPORT_LINE

	if(!InitThreads())
	{
		RAPORT("Threads initialization error");
		return false;
	}
	console.SetServer(this);

	close_gently_timestamp = 0;
	global_signals->close_gently.connect(boost::bind(&GServerGame::PrepareToClose,this));
	return true;
}
/***************************************************************************/
void GServerGame::PrepareToClose()
{
	// Jesli dostalismy sygnal po raz drugi to olewamy czekanie 'close_gently()' i przechodzimy od razu do sygnalu 'close_slow()'.
	if (close_gently_timestamp != 0)
	{
		flags.set(EFPrepareToClose, 0);
		global_signals->close_slow();
		return;
	}

	close_gently_timestamp = CurrentSTime();
	flags.set(EFPrepareToClose);
	for_each(groups.begin(),groups.end(),boost::bind(&GGroupManager::PrepareToClose,_1));
}
/***************************************************************************/
void GServerGame::CheckGentleClose()
{
	if (flags[EFPrepareToClose])
	{
		vector<GGroupManager *>::iterator it;

		for (it = groups.begin(); it != groups.end(); it++)
		{
			if ((*it)->Continue())
			{
				break;
			}
		}

		// Wszystkie watki group managerow zglosily gotowosc zamkniecia, mozemy wolac sygnal 'close_slow'.
		// Watki maja deadline na zamkniecie sie rowny 30 minut - potem przestajemy czekac.
		if (it == groups.end() || CurrentSTime() > close_gently_timestamp + 60 * 30)
		{
			// Z glownego watku staramy sie wyslac jeszcze odpowiedzi 'zly adres'.
			group.ProcessLogicMessagesTransferQueue();

			flags.set(EFPrepareToClose, 0);
			global_signals->close_slow();
		}
	}
}
/***************************************************************************/
void GServerGame::Destroy()
{
	global_signals->close_slow();
	for_each(group_threads.begin(), group_threads.end(), boost::bind(&boost::thread::join,_1));
	for_each(group_threads.begin(), group_threads.end(), &Delete<boost::thread>);
	// Nie usuwamy GroupManagerow przez Delete(), bo dopoki sie gasza to chcemy, zeby istnial o nich wpis w groups.
	// Potrzebne jest to do poprawnego dzialania funkcji GetGroupWriteLocationsDeadline().
	// Delete() natomiast najpierw ustawia wskaznik NULL, a dopiero potem wola destruktor.
	{
		vector<GGroupManager *>::iterator pos;
		for (pos = groups.begin(); pos != groups.end(); pos++)
		{
			delete (*pos);
			*pos = NULL;
		}
		groups.clear();
	}
	for_each(locationdb_worker_threads.begin(), locationdb_worker_threads.end(), &Delete<GLocationDBWorkerThread>);
	for_each(http_request_worker_threads.begin(), http_request_worker_threads.end(), &Delete<GHTTPRequestWorkerThread>);
	for_each(http_fire_and_forget_request_worker_threads.begin(), http_fire_and_forget_request_worker_threads.end(), &Delete<GHTTPRequestWorkerThread>);
	
	GServer::Destroy();
}
/***************************************************************************/
void GServerGame::InitInternalClients()
{
	InitInternalClient(i_access,ESTClientAccess,0);
	i_access->msg_callback_target=boost::bind(&GGroup::ProcessGroupTargetMessage,&group,_1,_2,_3,_4);
	i_access->msg_callback_internal=boost::bind(&GGroup::ProcessInternalMessage,&group,_1,_2,_3);
	InitInternalClient(i_nodebalancer,ESTClientNodeBalancer,0);
	i_nodebalancer->msg_callback_target=boost::bind(&GGroup::ProcessGroupTargetMessage,&group,_1,_2,_3,_4);
	i_nodebalancer->msg_callback_internal=boost::bind(&GGroup::ProcessInternalMessage,&group,_1,_2,_3);
}
/***************************************************************************/
bool GServerGame::InitThreads()
{
	string threads_config = global_serverconfig->misc.threads;
	if (threads_config == "")
	{
		threads_config = "CPU;dbworker:CPU;http:CPU;httpfaf:CPU";
	}

	INT32 CPU_cores = -1;

	map<string, int> threads_count;
	// threads_config -> threads_count
	{
		boost::char_separator<char> sep(";");
		typedef boost::tokenizer<boost::char_separator<char> > TTokenizer;
		TTokenizer tok(threads_config, sep);
		TTokenizer::iterator it;
		for (it = tok.begin(); it != tok.end(); it++)
		{
			string option_name;
			const char * p, * value;
			if ((p = strchr(it->c_str(), ':')) == NULL)
			{
				option_name = "gamestate";
				value = it->c_str();
			}
			else
			{
				option_name.assign(it->c_str(), p);
				value = p + 1;
			}

			INT32 value_int = ATOI(value);
			if (strcmp(value, "CPU") == 0)
			{
				if (CPU_cores == -1)
				{
					CPU_cores = get_CPU_cores_count();
				}
				value_int = CPU_cores;
			}
			threads_count[option_name] = Clamp(value_int, 0, MAX_SERVER_THREADS);
		}
	}

	// Odpalamy pozadana liczbe watkow danego typu.
	
	int i, tcount;

	tcount = threads_count["dbworker"];
	for (i = 0; i < max(tcount, 1); i++)
	{
		GLocationDBWorkerThread * locationdb_worker_thread = new GLocationDBWorkerThread();
		if (!locationdb_worker_thread->Init())
		{
			return false;
		}
		locationdb_worker_threads.push_back(locationdb_worker_thread);
	}
	global_metrics.CommitCurrentValue("free_locationdb_worker_threads", locationdb_worker_threads.size());

	tcount = threads_count["http"];
	for (i = 0; i < max(tcount, 1); i++)
	{
		GHTTPRequestWorkerThread * http_request_worker_thread = new GHTTPRequestWorkerThread(http_request_tasks, mtx_http_request_tasks, false, "http");
		if (!http_request_worker_thread->Init())
		{
			return false;
		}
		http_request_worker_threads.push_back(http_request_worker_thread);
	}
	global_metrics.CommitCurrentValue("free_http_request_worker_threads", http_request_worker_threads.size());

	tcount = threads_count["httpfaf"];
	for (i = 0; i < max(tcount, 1); i++)
	{
		GHTTPRequestWorkerThread * http_fire_and_forget_request_worker_thread = new GHTTPRequestWorkerThread(http_fire_and_forget_request_tasks, mtx_http_fire_and_forget_request_tasks, true, "http_faf");
		if (!http_fire_and_forget_request_worker_thread->Init())
		{
			return false;
		}
		http_fire_and_forget_request_worker_threads.push_back(http_fire_and_forget_request_worker_thread);
	}
	global_metrics.CommitCurrentValue("free_http_faf_request_worker_threads", http_fire_and_forget_request_worker_threads.size());

	int group_id = 1;
	tcount = threads_count["gamestate"];
	for (i = 0; i < tcount; i++)
	{
		if (!InitThread(group_id++, EGT_GAMESTATE))
		{
			return false;
		}
	}
	tcount = threads_count["location"];
	for (i = 0; i < tcount; i++)
	{
		if (!InitThread(group_id++, EGT_LOCATION))
		{
			return false;
		}
	}
	tcount = threads_count["lobby"];
	for (i = 0; i < tcount; i++)
	{
		if (!InitThread(group_id++, EGT_LOBBY))
		{
			return false;
		}
	}
	tcount = threads_count["leaderboard"];
	for (i = 0; i < tcount; i++)
	{
		if (!InitThread(group_id++, EGT_LEADERBOARD))
		{
			return false;
		}
	}

	RAPORT("Threads initialized");
	return true;
}
/***************************************************************************/
bool GServerGame::InitThread(int group_number, EGroupType group_type)
{
	// Dodajemy losowa pauze przed startem watku, zeby IntervalSecond() w poszczegolnych room groupach
	// wykonywal sie w miare mozliwosci w roznych momentach.
	Sleep(truerng.Rnd(1000));

	GGroupManager * group_manager=new GGroupManager(service_manager.services[0]);
	groups.push_back(group_manager);
	boost::thread * group_thread=NULL;
	if (service_manager.services.size() > 0 && group_manager->Init(group_number, group_type)) 
	{
		group_thread = new boost::thread(boost::bind(&GGroupManager::Action,group_manager));
		group_threads.push_back(group_thread);
		assert(group_thread);
	}
	return true;
}
/***************************************************************************/
bool GServerGame::RegisterLogic(GSocket * socket)
{
	if (socket->GetServiceType() == ESTClientBase)
	{
		GSocketInternalServer * server = static_cast<GSocketInternalServer *>(socket);
		server->msg_callback_internal = boost::bind(&GGroup::ProcessInternalMessage, &group, _1,_2,_3);
		server->msg_callback_target = boost::bind(&GGroup::ProcessTargetMessageGroupConnection, &group, _1,_2,_3,_4);
		return true;
	}

	GPlayerAccess * logic=NULL;
	logic=new GPlayerAccess(&group,socket,GenerateClientRID());
	socket->LinkLogic(logic);
	group.PlayerAdd(logic);
	logic->Register();
	SRAP(INFO_LOGIC_ACCEPT_NEW);
	return true;
}
/***************************************************************************/
void GServerGame::UnregisterLogic(GSocket * socket,EUSocketMode mode,bool del)
{
	if (socket->GetServiceType() == ESTClientBase)
	{
		GSocketInternalServer * server = static_cast<GSocketInternalServer *>(socket);
		server->msg_callback_internal = NULL;
		server->msg_callback_target = NULL;
		return;
	}

	GPlayerAccess* logic=static_cast<GPlayerAccess*>(socket->Logic());
	if(logic==NULL) return;
	logic->Unregister();
	group.PlayerDelete(logic,ULR_PLAYER_DISCONNECT);
	socket->UnlinkLogic();
	if(del) 
	{
		Delete(logic);
		SRAP(INFO_LOGIC_ACCEPT_DELETE);
	}
}
/***************************************************************************/
INT32 GServerGame::CallAction(strstream & s, ECmdConsole cmd, map<string, string> & options)
{
	SConsoleTask task;
	task.socketid = ATOI(options["socketid"].c_str());
	task.type = ECTT_Request;
	task.cmd = cmd;
	task.options = options;

	INT32 awaiting_responses_count = 0;

	switch(cmd)
	{
		case CMD_ACTION_SERVICE_INFO:
			{
				s<<boost::format("%|-8s|%|-12s|%|-20s|%|-12s|%|-22s|%|-10s|%|-10s|%|-11s|%|-10s|\r\n")% "Group" % "Name" %"Server:Port" % "Status" % "Date & Time" % "Processed" % "Queue Out"% "Queue Wait"% "TimeOut";
				s<<boost::format("%|115T-|\r\n");
				GServer::CallAction(s, cmd, options);
				for_each(groups.begin(),groups.end(),boost::bind(&GGroupManager::InsertConsoleTask,_1, boost::ref(task)));
				awaiting_responses_count = groups.size();
			}
			break;
		case CMD_ACTION_SERVICE_REINIT:
			{
				s<<boost::format("%|s|") % "Services reinitialization\r\n";
				s<<boost::format("%|-8s|%|-12s|%|-20s|%|-12s|%|-22s|%|-10s|%|-10s|%|-11s|%|-10s|\r\n")% "Group" % "Name" %"Server:Port" % "Status" % "Date & Time" % "Processed" % "Queue Out"% "Queue Wait"% "TimeOut";
				s<<boost::format("%|115T-|\r\n");
				GServer::CallAction(s, cmd, options);
				for_each(groups.begin(),groups.end(),boost::bind(&GGroupManager::InsertConsoleTask,_1, boost::ref(task)));
				awaiting_responses_count = groups.size();
			}
			break;
		case CMD_ACTION_MEMORY_INFO:
			s<<boost::format("-------------------- Main Thread --------------------\r\n");
			GServerBase::CallAction(s, cmd, options);
			for_each(groups.begin(),groups.end(),boost::bind(&GGroupManager::InsertConsoleTask,_1, boost::ref(task)));
			awaiting_responses_count = groups.size();
			break;
		case CMD_LOGIC_RELOAD:
			for_each(groups.begin(),groups.end(),boost::bind(&GGroupManager::InsertConsoleTask,_1, boost::ref(task)));
			awaiting_responses_count = groups.size();
			break;
		case CMD_LOGIC_STATS:
			s<<boost::format("-------------------- Main Thread --------------------\r\n");
			{
				INT32 interval_seconds = ATOI(options["interval"].c_str());
				if (interval_seconds <= 0)
				{
					interval_seconds = 0;
				}
				{
					boost::mutex::scoped_lock lock(mtx_global_metrics);
					s << group.GetID() << ";" << interval_seconds << "\r\n";
					global_metrics.Print(s, interval_seconds, ATOI(options["csv"].c_str()) != 1);
				}
				{
					s << group.GetID() << ";" << interval_seconds << "\r\n";
					group.metrics.Print(s, interval_seconds, ATOI(options["csv"].c_str()) != 1);
				}
			}
			for_each(groups.begin(),groups.end(),boost::bind(&GGroupManager::InsertConsoleTask,_1, boost::ref(task)));
			awaiting_responses_count = groups.size();
			break;
		case CMD_LOGIC_CLEARSTATS:
			for_each(groups.begin(),groups.end(),boost::bind(&GGroupManager::InsertConsoleTask,_1, boost::ref(task)));
			awaiting_responses_count = groups.size();
			break;
		case CMD_LOGIC_MESSAGE:
			{
				if(options.find("group")==options.end())
				{
					s << "Error: no group id provided\r\n";
					return 0;
				}
				int group_id=atoi(options["group"].c_str());
				if (group_id <= 0 || group_id > (int)groups.size())
				{
					s << "Error: invalid group id\r\n";
					return 0;
				}
				groups[group_id - 1]->InsertConsoleTask(task);
				awaiting_responses_count = 1;
			}
			break;
		case CMD_LOGIC_GLOBAL_MESSAGE:
			
			if (options.find("command") == options.end())
			{
				s << "ERROR: no command id provided\r\n";
				break;
			}
			if (options.find("params") == options.end())
			{
				s << "ERROR: no parameters provided\r\n";
				break;
			}

			for_each(groups.begin(), groups.end(), boost::bind(&GGroupManager::InsertConsoleTask, _1, boost::ref(task)));
			awaiting_responses_count = groups.size();
			break;
	}

	return awaiting_responses_count;
}
/***************************************************************************/
GGroupManager* GServerGame::FindGroupManager(int group)
{
	if(group==0) return NULL;
	vector<GGroupManager *>::iterator pos;
	for (pos = groups.begin(); pos != groups.end(); pos++)
	{
		if ((*pos) != NULL && (*pos)->Group().GetID() == group)
		{
			return *pos;
		}
	}
	return NULL;
}
/***************************************************************************/
//to jest wolane co 15 sek, jest wpiete w sygnaly
void GServerGame::ProcessTimeOut()
{
	if(!socket_game.size()) return;

	//sprawdza timeouty dla socketow, i usuwamy nie zestawione w max. czasie polaczenia
	for_each(socket_game.begin(), socket_game.end(),boost::bind(&GServerGame::TestTimeOut,this,_1));

	socket_game.resize(0);
}
/***************************************************************************/
//wolane z ProcessTimeOut, sprawdza czy polaczenie nie wisi martwe dluzej niz TIME_MAX_CONNECTON_ESTABILISH, 
//jesli tak to trzeba go zerwac odrejestrowuje element, 
bool GServerGame::TestTimeOut(GSocket * socket)
{
	if (socket->GetServiceType() == ESTClientBaseGame)
	{
		//ma rozpiac polaczenie jesli przez 60 sek nic sie nie dzieje
		if(clock.Get()-socket->GetTimeConnection()>TIME_MAX_CONNECTION_ESTABILISH)
		{
			SocketRemove(socket,USR_TIMEOUT_CONNECTION);
			return true;
		}
	}
	return false;
}
/***************************************************************************/
void GServerGame::IntervalSecond()
{
	// Metryki w glownym watku nie sa na tyle precyzyjne, zeby wymagac CheckRotate() co ms.
	boost::mutex::scoped_lock lock(mtx_global_metrics);
	global_metrics.CheckRotate();
}
/***************************************************************************/
void GServerGame::ConfigureClock()
{
	GServer::ConfigureClock();

	signal_second.connect(boost::bind(&GServerGame::IntervalSecond,this));
	signal_second.connect(boost::bind(&GRaportManager::UpdateTime,&raport_manager_global,boost::ref(clock)));
	signal_seconds_15.connect(boost::bind(&GServerGame::ProcessTimeOut,this));
	signal_second.connect(boost::bind(&GServerGame::CheckGentleClose,this));
	signal_minutes_5.connect(boost::bind(&GServerGame::RaportCurrentStatus,this));
}
/***************************************************************************/
bool GServerGame::InitSQL()
{
	if(!GServer::InitSQL()) return false;

	return true;
}
/***************************************************************************/
GSocket * GServerGame::SocketNew(EServiceTypes type)
{
	GSocket * s;
	if (type == ESTAcceptBase)
	{
		s = new GSocketInternalServer(raport_interface);
	}
	else
	{
		s = new GSocketAccess();
	}
	return s;
}
/***************************************************************************/
DWORD64 GServerGame::GetGroupWriteLocationsDeadline(INT32 group_id)
{
	GGroupManager * group = FindGroupManager(group_id);
	if (group)
	{
		return group->Group().write_locations_deadline;
	}
	return 0;
}
/***************************************************************************/
void GServerGame::AddLocationDBTask(const char * location_ID, SLocationDBTask & task)
{
	boost::mutex::scoped_lock lock(mtx_locationdb_tasks);

	SLocationDBTasks & tasks = locationdb_tasks[location_ID];
	if (task.direction == ELDBTD_Read || task.direction == ELDBTD_Read_Locations_Ids)
	{
		tasks.tasks_read.push_back(task);
		tasks.read_execution_deadline = (tasks.read_execution_deadline == 0) ? task.execution_deadline : min(tasks.read_execution_deadline, task.execution_deadline);
		if (task.has_priority)
		{
			tasks.priority_read_execution_deadline = (tasks.priority_read_execution_deadline == 0) ? task.execution_deadline : min(tasks.priority_read_execution_deadline, task.execution_deadline);
		}
	}
	else
	{
		tasks.tasks_write.push_back(task);
		tasks.write_execution_deadline = (tasks.write_execution_deadline == 0) ? task.execution_deadline : min(tasks.write_execution_deadline, task.execution_deadline);
		if (task.has_priority)
		{
			tasks.priority_write_execution_deadline = (tasks.priority_write_execution_deadline == 0) ? task.execution_deadline : min(tasks.priority_write_execution_deadline, task.execution_deadline);
		}
	}
}
/***************************************************************************/
void GServerGame::AddLocationDBTaskResult(INT32 group_id, SLocationDBTaskResult & task_result)
{
	boost::mutex::scoped_lock lock(mtx_locationdb_tasks_results);

	vector<SLocationDBTaskResult> & task_results = locationdb_tasks_results[group_id];
	task_results.push_back(task_result);
	if (task_result.status == ELDBTS_OK)
	{
		INT32 a, count = task_results.size();
		for (a = 0; a < count - 1; a++)
		{
			if (task_results[a].location_ID == task_result.location_ID &&
				task_results[a].direction == task_result.direction && 
				task_results[a].status != ELDBTS_OK)
			{
				task_results[a].status = ELDBTS_OK;
				task_results[a].location_data = task_result.location_data;
			}
		}
	}
}
/***************************************************************************/
void GServerGame::AddHTTPFireAndForgetRequestTask(SHTTPRequestTask & task)
{
	boost::mutex::scoped_lock lock(mtx_http_fire_and_forget_request_tasks);
	http_fire_and_forget_request_tasks.push_back(task);
}
/***************************************************************************/
INT32 GServerGame::AddHTTPRequestTask(SHTTPRequestTask & task)
{
	boost::mutex::scoped_lock lock(mtx_http_request_tasks);
	task.http_request_id = seq_http_request_id++;
	http_request_tasks.push_back(task);
	return task.http_request_id;
}
/***************************************************************************/
void GServerGame::AddHTTPRequestTaskResult(INT32 group_id, SHTTPRequestTask & task_result)
{
	boost::mutex::scoped_lock lock(mtx_http_request_tasks_results);
	vector<SHTTPRequestTask> & task_results = http_request_tasks_results[group_id];
	task_results.push_back(task_result);
}
/***************************************************************************/
void GServerGame::RaportCurrentStatus()
{
	// Polaczenie z baza danych.
	GRAPORT("[GSTATUS %2d] mysql_status: %s", (int)0, mysql_ping(&mysql) == 0 ? "OK" : "Error");

	// Polaczenia incoming od graczy (zanim zostana przepieci do wlasciwej grupy).
	GRAPORT("[GSTATUS %2d] incoming_player_connections = %d", (int)0, socket_game_map.size());

	// Polaczenia incoming od innych grup.
	GRAPORT("[GSTATUS %2d] incoming_group_connections = %d", (int)0, socket_base_map.size());
}
/***************************************************************************/

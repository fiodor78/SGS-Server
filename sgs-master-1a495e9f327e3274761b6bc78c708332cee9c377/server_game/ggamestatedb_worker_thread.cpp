/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"
#include "headers_game.h"
#include "zlib.h"

extern GServerBase * global_server;

boost::mutex			GLocationDBWorkerThread::mtx_locationdb_multipart_locations;
map<string, INT32>		GLocationDBWorkerThread::multipart_locations_total_parts;
map<string, INT32>		GLocationDBWorkerThread::multipart_locations_second_partID;

/***************************************************************************/
GLocationDBWorkerThread::GLocationDBWorkerThread() : 
	compress_buffer(NULL),
	uncompress_buffer(NULL),
	raport_interface(&raport_manager_global)
{
	locationdb_initialized = false;
	compress_buffer_size = 0;
	uncompress_buffer_size = 0;
}
/***************************************************************************/
bool GLocationDBWorkerThread::Init()
{
	if (!InitLocationDB())
	{
		return false;
	}

	compress_buffer_size = K64;
	uncompress_buffer_size = K256;
	compress_buffer = new unsigned char[compress_buffer_size];
	uncompress_buffer = new unsigned char[uncompress_buffer_size];
	query_buffer = new char[compress_buffer_size * 4 + K4];

	global_signals->close_fast.connect(boost::bind(&GLocationDBWorkerThread::Close, this));
	worker_thread = new boost::thread(boost::bind(&GLocationDBWorkerThread::Action, this));

	return true;
}
/***************************************************************************/
void GLocationDBWorkerThread::Action()
{
	while (Continue())
	{
		for (;;)
		{
			// Wykrywamy sytuacje, ze glowny watek serwera gry juz sie zgasil.
			// Odswiezamy wtedy jego zegar, bo od jego globalnej wartosci zaleza rozne interakcje w watkach bazodanowych.
			{
				time_t t;
				time(&t);
				if (GSERVER->CurrentSTime() + 3 < (DWORD64)t)
				{
					GSERVER->SetCurrentSTime();
					GSERVER->GetClock().Probe();
				}
			}

			ClearExpiredDBTasks();

			SLocationDBTask current_task = GetLocationDBTaskToRun();
			if (current_task.direction == ELDBTD_None)
			{
				break;
			}
			ProcessLocationDBTask(current_task);
		}
		
		Sleep(10 + truerng.Rnd(500));
	}
}
/***************************************************************************/
void GLocationDBWorkerThread::Destroy()
{
	Close();
	worker_thread->join();
	Delete(worker_thread);
	raport_interface.Destroy();

	delete [] compress_buffer;
	delete [] uncompress_buffer;
	delete [] query_buffer;
	compress_buffer = NULL;
	uncompress_buffer = NULL;
	query_buffer = NULL;

	DestroyLocationDB();
}
/***************************************************************************/
SLocationDBTask GLocationDBWorkerThread::GetLocationDBTaskToRun()
{
	boost::mutex::scoped_lock lock(GSERVER->mtx_locationdb_tasks);

	SLocationDBTask current_task;
	current_task.direction = ELDBTD_None;

	DWORD64 cur_time = GSERVER->CurrentSTime();

	INT32 pass;
	string best_location_ID = "";
	// Priorytety taskow (od najwiekszego):
	// 0: priority write
	// 1: priority read
	// 2: read
	// 3: write
	for (pass = 0; pass < 4; pass++)
	{
		DWORD64 min_deadline = 0;
		TLocationDBTasksMap::iterator pos;
		for (pos = GSERVER->locationdb_tasks.begin(); pos != GSERVER->locationdb_tasks.end(); pos++)
		{
			SLocationDBTasks & tasks = pos->second;

			if (tasks.read_task_running_taskid != 0 || tasks.write_task_running_taskid != 0)
			{
				continue;
			}

			DWORD64 compare_value = 0;
			DWORD64 min_success_delay = 0;
			DWORD64 min_failure_delay = 0;

			switch (pass)
			{
			case 0:
				min_success_delay = 2;
				min_failure_delay = min(1 + tasks.last_write_fails_count * 2, (INT32)20);
				break;
			case 1:
				min_success_delay = 2;
				min_failure_delay = min(1 + tasks.last_read_fails_count * 2, (INT32)20);
				break;
			case 2:
				min_success_delay = 3;
				min_failure_delay = min(1 + tasks.last_read_fails_count * 2, (INT32)30);
				break;
			case 3:
				min_success_delay = 10;
				min_failure_delay = min(9 + tasks.last_write_fails_count * tasks.last_write_fails_count, (INT32)300);
				break;
			}

			if (tasks.random_interval_change == 0)
			{
				tasks.random_interval_change = 50 + truerng.Rnd(101);
			}
			min_success_delay = max((DWORD64)1, (min_success_delay * tasks.random_interval_change) / 100);
			min_failure_delay = max((DWORD64)1, (min_failure_delay * tasks.random_interval_change) / 100);

			switch (pass)
			{
			case 0:
			case 3:
				if ((tasks.last_write_operation_success == 0 || tasks.last_write_operation_success + min_success_delay <= cur_time) &&
					(tasks.last_write_operation_fail == 0 || tasks.last_write_operation_fail + min_failure_delay <= cur_time))
				{
					compare_value = (pass == 0) ? tasks.priority_write_execution_deadline : tasks.write_execution_deadline;
				}
				break;
			case 1:
			case 2:
				if ((tasks.last_read_operation_success == 0 || tasks.last_read_operation_success + min_success_delay <= cur_time) &&
					(tasks.last_read_operation_fail == 0 || tasks.last_read_operation_fail + min_failure_delay <= cur_time))
				{
					compare_value = (pass == 1) ? tasks.priority_read_execution_deadline : tasks.read_execution_deadline;
				}
				break;
			}

			if (compare_value != 0)
			{
				if (min_deadline == 0 || compare_value < min_deadline)
				{
					min_deadline = compare_value;
					best_location_ID = pos->first;
				}
			}
		}

		if (best_location_ID != "")
		{
			break;
		}
	}

	if (best_location_ID == "")
	{
		// Brak zadan do wykonania w kolejce.
		return current_task;
	}

	// Mamy wybrane location_ID, wybieramy teraz task.
	SLocationDBTasks & tasks = GSERVER->locationdb_tasks[best_location_ID];

	switch (pass)
	{
	case 0:
	case 3:
		{
			INT32 a, count = tasks.tasks_write.size();
			INT32 best_task_idx = -1;
			for (a = 0; a < count; a++)
			{
				if (pass == 0 && !tasks.tasks_write[a].has_priority)
				{
					continue;
				}
				if (best_task_idx == -1 || tasks.tasks_write[a].taskid > tasks.tasks_write[best_task_idx].taskid)
				{
					best_task_idx = a;
				}
			}
			if (best_task_idx != -1)
			{
				current_task = tasks.tasks_write[best_task_idx];
			}
		}
		break;
	case 1:
	case 2:
		{
			INT32 a, count = tasks.tasks_read.size();
			INT32 best_task_idx = -1;
			for (a = 0; a < count; a++)
			{
				if (pass == 1 && !tasks.tasks_read[a].has_priority)
				{
					continue;
				}
				if (best_task_idx == -1 || tasks.tasks_read[a].taskid > tasks.tasks_read[best_task_idx].taskid)
				{
					best_task_idx = a;
				}
			}
			if (best_task_idx != -1)
			{
				current_task = tasks.tasks_read[best_task_idx];
			}
		}
		break;
	}

	if (current_task.direction == ELDBTD_None)
	{
		return current_task;
	}

	// Wykonujemy 'current_task'
	if (current_task.direction == ELDBTD_Read || current_task.direction == ELDBTD_Read_Locations_Ids)
	{
		tasks.read_task_running_taskid = current_task.taskid;
	}
	else
	{
		tasks.write_task_running_taskid = current_task.taskid;
	}

	return current_task;
}
/***************************************************************************/
void GLocationDBWorkerThread::ProcessLocationDBTask(SLocationDBTask & current_task)
{
	ELocationDBTaskStatus status = ELDBTS_None;

	last_compressed_size = -1;
	SMetricStats::SCallContext call_ctx = SMetricStats::BeginDuration();
	{
		boost::mutex::scoped_lock lock(GSERVER->mtx_global_metrics);
		GSERVER->global_metrics.CommitCurrentValueChange("busy_locationdb_worker_threads", +1);
		GSERVER->global_metrics.CommitCurrentValueChange("free_locationdb_worker_threads", -1);
	}

	if (current_task.direction == ELDBTD_Read)
	{
		status = ELDBTS_ReadFailed;
		if (global_serverconfig->gamestatedb_config.type == "mysql")
		{
			status = LoadLocationFromDatabaseMySQL(current_task.location_ID.c_str(), current_task.location_data) ? ELDBTS_OK : ELDBTS_ReadFailed;
		}
		if (global_serverconfig->gamestatedb_config.type == "dynamodb")
		{
			status = LoadLocationFromDatabaseDynamoDB(current_task.location_ID.c_str(), current_task.location_data) ? ELDBTS_OK : ELDBTS_ReadFailed;
		}
	}
	else if (current_task.direction == ELDBTD_Read_Locations_Ids)
	{
		current_task.location_data = LoadLocationsIdsFromDatabaseMySQL(current_task.location_data);
		status = ELDBTS_OK;
	}	
	else
	{
		DWORD64 cur_time = GSERVER->CurrentSTime();
		DWORD64 write_locations_deadline = GSERVER->GetGroupWriteLocationsDeadline(current_task.group_id);
		if (cur_time >= write_locations_deadline)
		{
			string err_str = str(boost::format("WARNING: abandoning location write (%s). location_ID: %s") %
									(write_locations_deadline == 0 ? "write forbidden" : "deadline reached") % current_task.location_ID.c_str());
			GRAPORT(err_str.c_str());
			status = ELDBTS_WriteDeadlineReached;
		}
		else
		{
			status = ELDBTS_WriteFailed;
			bool duplicate_insert = false;

			if (current_task.direction == ELDBTD_Insert)
			{
				boost::mutex::scoped_lock lock(mtx_locationdb_multipart_locations);
				if (multipart_locations_total_parts[current_task.location_ID] > 0)
				{
					duplicate_insert = true;
				}
				else
				{
					multipart_locations_total_parts[current_task.location_ID] = 0;
					multipart_locations_second_partID[current_task.location_ID] = 0;
				}
			}

			if (current_task.direction == ELDBTD_Delete)
			{
				if (global_serverconfig->gamestatedb_config.type == "mysql")
				{
					status = DeleteLocationFromDatabaseMySQL(current_task.location_ID.c_str()) ? ELDBTS_OK : ELDBTS_WriteFailed;
				}
				if (global_serverconfig->gamestatedb_config.type == "dynamodb")
				{
					status = DeleteLocationFromDatabaseDynamoDB(current_task.location_ID.c_str()) ? ELDBTS_OK : ELDBTS_WriteFailed;
				}
			}
			else if (!duplicate_insert)
			{
				if (global_serverconfig->gamestatedb_config.type == "mysql")
				{
					status = WriteLocationToDatabaseMySQL(current_task.location_ID.c_str(), current_task.location_data) ? ELDBTS_OK : ELDBTS_WriteFailed;
				}
				if (global_serverconfig->gamestatedb_config.type == "dynamodb")
				{
					status = WriteLocationToDatabaseDynamoDB(current_task.location_ID.c_str(), current_task.location_data) ? ELDBTS_OK : ELDBTS_WriteFailed;
				}
			}
		}
	}

	{
		boost::mutex::scoped_lock lock(GSERVER->mtx_global_metrics);
		GSERVER->global_metrics.CommitCurrentValueChange("busy_locationdb_worker_threads", -1);
		GSERVER->global_metrics.CommitCurrentValueChange("free_locationdb_worker_threads", +1);

		if (last_compressed_size >= 0 && gamestate_ID_from_location_ID(current_task.location_ID.c_str()) > 0)
		{
			if (current_task.direction == ELDBTD_Read)
			{
				GSERVER->global_metrics.CommitDuration("load_gamestate_from_gamestatedb_time", call_ctx);
				GSERVER->global_metrics.CommitSampleValue("load_gamestate_from_gamestatedb_size", last_compressed_size, "b");
			}
			if (current_task.direction == ELDBTD_Write)
			{
				GSERVER->global_metrics.CommitDuration("write_gamestate_to_gamestatedb_time", call_ctx);
				GSERVER->global_metrics.CommitSampleValue("write_gamestate_to_gamestatedb_size", last_compressed_size, "b");
			}
		}
	}

	ProcessLocationDBTaskResult(current_task, status);
}
/***************************************************************************/
void GLocationDBWorkerThread::ProcessLocationDBTaskResult(SLocationDBTask & current_task, ELocationDBTaskStatus status)
{
	boost::mutex::scoped_lock lock(GSERVER->mtx_locationdb_tasks);

	DWORD64 cur_time = GSERVER->CurrentSTime();
	SLocationDBTasks & tasks = GSERVER->locationdb_tasks[current_task.location_ID];

	bool result = (status == ELDBTS_OK);
	tasks.random_interval_change = 50 + truerng.Rnd(101);

	if (current_task.direction == ELDBTD_Read || current_task.direction == ELDBTD_Read_Locations_Ids)
	{
		if (result)
		{
			tasks.last_read_operation_success = cur_time;
			tasks.last_read_operation_fail = 0;
			tasks.last_read_fails_count = 0;
		}
		else
		{
			tasks.last_read_operation_fail = cur_time;
			tasks.last_read_fails_count++;
			tasks.last_read_operation_success = 0;
		}

		tasks.read_execution_deadline = 0;
		tasks.priority_read_execution_deadline = 0;

		// Przerzucamy wszystkie taski read do kolejki out.
		INT32 a, count = tasks.tasks_read.size();
		for (a = 0; a < count; a++)
		{
			if (result)
			{
				SLocationDBTaskResult task_result;
				task_result.location_ID = current_task.location_ID;
				task_result.direction = current_task.direction;
				task_result.taskid = tasks.tasks_read[a].taskid;
				task_result.status = status;
				task_result.location_data = current_task.location_data;
				GSERVER->AddLocationDBTaskResult(tasks.tasks_read[a].group_id, task_result);

				tasks.tasks_read.erase(tasks.tasks_read.begin() + a);
				a--;
				count--;
				continue;
			}
			tasks.tasks_read[a].task_executed = true;

			if (tasks.read_execution_deadline == 0 || tasks.tasks_read[a].execution_deadline < tasks.read_execution_deadline)
			{
				tasks.read_execution_deadline = tasks.tasks_read[a].execution_deadline;
			}
			if (tasks.tasks_read[a].has_priority)
			{
				if (tasks.priority_read_execution_deadline == 0 || tasks.tasks_read[a].execution_deadline < tasks.priority_read_execution_deadline)
				{
					tasks.priority_read_execution_deadline = tasks.tasks_read[a].execution_deadline;
				}
			}
		}

		tasks.read_task_running_taskid = 0;
	}
	else
	{
		if (result)
		{
			tasks.last_write_operation_success = cur_time;
			tasks.last_write_operation_fail = 0;
			tasks.last_write_fails_count = 0;
		}
		else
		{
			tasks.last_write_operation_fail = cur_time;
			tasks.last_write_fails_count++;
			tasks.last_write_operation_success = 0;
		}

		tasks.write_execution_deadline = 0;
		tasks.priority_write_execution_deadline = 0;

		INT32 a, count = tasks.tasks_write.size();
		for (a = 0; a < count; a++)
		{
			if (tasks.tasks_write[a].taskid <= current_task.taskid &&
				(tasks.tasks_write[a].direction == current_task.direction ||
				(tasks.tasks_write[a].direction == ELDBTD_Insert && current_task.direction == ELDBTD_Write)))
			{
				if (result)
				{
					SLocationDBTaskResult task_result;
					task_result.location_ID = current_task.location_ID;
					task_result.direction = current_task.direction;
					task_result.taskid = tasks.tasks_write[a].taskid;
					task_result.status = status;
					task_result.location_data = current_task.location_data;
					GSERVER->AddLocationDBTaskResult(tasks.tasks_write[a].group_id, task_result);

					tasks.tasks_write.erase(tasks.tasks_write.begin() + a);
					a--;
					count--;
					continue;
				}
				tasks.tasks_write[a].task_executed = true;
			}

			if (tasks.write_execution_deadline == 0 || tasks.tasks_write[a].execution_deadline < tasks.write_execution_deadline)
			{
				tasks.write_execution_deadline = tasks.tasks_write[a].execution_deadline;
			}
			if (tasks.tasks_write[a].has_priority)
			{
				if (tasks.priority_write_execution_deadline == 0 || tasks.tasks_write[a].execution_deadline < tasks.priority_write_execution_deadline)
				{
					tasks.priority_write_execution_deadline = tasks.tasks_write[a].execution_deadline;
				}
			}
		}

		tasks.write_task_running_taskid = 0;
	}
}
/***************************************************************************/
void GLocationDBWorkerThread::ClearExpiredDBTasks()
{
	boost::mutex::scoped_lock lock(GSERVER->mtx_locationdb_tasks);

	DWORD64 cur_time = GSERVER->CurrentSTime();

	TLocationDBTasksMap::iterator pos;
	for (pos = GSERVER->locationdb_tasks.begin(); pos != GSERVER->locationdb_tasks.end(); )
	{
		SLocationDBTasks & tasks = pos->second;

		tasks.read_execution_deadline = 0;
		tasks.priority_read_execution_deadline = 0;

		INT32 a, count;
		count = tasks.tasks_read.size();
		for (a = 0; a < count; a++)
		{
			if (tasks.read_task_running_taskid == 0 &&
				tasks.tasks_read[a].execution_deadline <= cur_time)
			{
				SLocationDBTaskResult task_result;
				task_result.location_ID = tasks.tasks_read[a].location_ID;
				task_result.direction = tasks.tasks_read[a].direction;
				task_result.taskid = tasks.tasks_read[a].taskid;
				task_result.status = (tasks.tasks_read[a].task_executed) ? ELDBTS_ReadFailed : ELDBTS_ExecutionDeadline;
				task_result.location_data = "";
				GSERVER->AddLocationDBTaskResult(tasks.tasks_read[a].group_id, task_result);

				tasks.tasks_read.erase(tasks.tasks_read.begin() + a);
				a--;
				count--;
				continue;
			}

			if (tasks.read_execution_deadline == 0 || tasks.tasks_read[a].execution_deadline < tasks.read_execution_deadline)
			{
				tasks.read_execution_deadline = tasks.tasks_read[a].execution_deadline;
			}
			if (tasks.tasks_read[a].has_priority)
			{
				if (tasks.priority_read_execution_deadline == 0 || tasks.tasks_read[a].execution_deadline < tasks.priority_read_execution_deadline)
				{
					tasks.priority_read_execution_deadline = tasks.tasks_read[a].execution_deadline;
				}
			}
		}

		tasks.write_execution_deadline = 0;
		tasks.priority_write_execution_deadline = 0;

		count = tasks.tasks_write.size();
		for (a = 0; a < count; a++)
		{
			if (tasks.write_task_running_taskid < tasks.tasks_write[a].taskid &&
				tasks.tasks_write[a].execution_deadline <= cur_time)
			{
				SLocationDBTaskResult task_result;
				task_result.location_ID = tasks.tasks_write[a].location_ID;
				task_result.direction = tasks.tasks_write[a].direction;
				task_result.taskid = tasks.tasks_write[a].taskid;
				task_result.status = (tasks.tasks_write[a].task_executed) ? ELDBTS_WriteFailed : ELDBTS_ExecutionDeadline;
				task_result.location_data = tasks.tasks_write[a].location_data;
				GSERVER->AddLocationDBTaskResult(tasks.tasks_write[a].group_id, task_result);

				tasks.tasks_write.erase(tasks.tasks_write.begin() + a);
				a--;
				count--;
				continue;
			}

			if (tasks.write_execution_deadline == 0 || tasks.tasks_write[a].execution_deadline < tasks.write_execution_deadline)
			{
				tasks.write_execution_deadline = tasks.tasks_write[a].execution_deadline;
			}
			if (tasks.tasks_write[a].has_priority)
			{
				if (tasks.priority_write_execution_deadline == 0 || tasks.tasks_write[a].execution_deadline < tasks.priority_write_execution_deadline)
				{
					tasks.priority_write_execution_deadline = tasks.tasks_write[a].execution_deadline;
				}
			}
		}

		// Po 10 min. nieuzywania usuwamy strukture dot. danego location_ID.
		if (tasks.tasks_read.size() == 0 && tasks.tasks_write.size() == 0 &&
			tasks.last_read_operation_success + 600 < cur_time &&
			tasks.last_read_operation_fail + 600 < cur_time &&
			tasks.last_write_operation_success + 600 < cur_time &&
			tasks.last_write_operation_fail + 600 < cur_time)
		{
			GSERVER->locationdb_tasks.erase(pos++);
			continue;
		}
		pos++;
	}
}
/***************************************************************************/
bool GLocationDBWorkerThread::InitLocationDB()
{
	if (!InitDatabaseMySQL())
	{
		return false;
	}
	if (global_serverconfig->gamestatedb_config.type == "dynamodb")
	{
		if (!InitDatabaseDynamoDB())
		{
			DestroyDatabaseMySQL();
			locationdb_initialized = false;
			return false;
		}
	}

	return true;
}
/***************************************************************************/
void GLocationDBWorkerThread::DestroyLocationDB()
{
	if (locationdb_initialized)
	{
		if (global_serverconfig->gamestatedb_config.type == "dynamodb")
		{
			DestroyDatabaseDynamoDB();
		}
		DestroyDatabaseMySQL();
	}
}
/***************************************************************************/
bool GLocationDBWorkerThread::Compress(const string & str, unsigned long * compressed_size)
{
	for (;;)
	{
		*compressed_size = compress_buffer_size;
		int ret = compress(compress_buffer, compressed_size, (const unsigned char *)str.c_str(), str.size() + 1);
		if (ret == Z_OK)
		{
			return true;
		}
		if (ret == Z_BUF_ERROR)
		{
			if (ReallocateCompressBuffers(compress_buffer_size * 2, 0))
			{
				continue;
			}
		}
		break;
	}
	GRAPORT("ERROR: unknown error when compressing string");
	return false;
}
/***************************************************************************/
bool GLocationDBWorkerThread::Uncompress(const char * compressed_data, unsigned long compressed_size, string & uncompressed_data)
{
	for (;;)
	{
		unsigned long buf_size = uncompress_buffer_size - 1;
		int ret = uncompress(uncompress_buffer, &buf_size, (const unsigned char *)compressed_data, compressed_size);
		if (ret == Z_OK && (long)buf_size < uncompress_buffer_size - 5)
		{
			//gamestate should be compressed with null terminator included, but better to be safe than sorry
			uncompress_buffer[buf_size] = 0;
			uncompressed_data = (char *)uncompress_buffer;
			return true;
		}
		if (ret == Z_BUF_ERROR || ret == Z_OK)
		{
			if (ReallocateCompressBuffers(0, uncompress_buffer_size * 2))
			{
				continue;
			}
		}
		break;
	}
	GRAPORT("ERROR: unknown error when uncompressing string");
	return false;
}
/***************************************************************************/
bool GLocationDBWorkerThread::ReallocateCompressBuffers(INT32 new_compress_buffer_size, INT32 new_uncompress_buffer_size)
{
	if (new_compress_buffer_size != 0 && new_compress_buffer_size > compress_buffer_size)
	{
		while (new_compress_buffer_size > compress_buffer_size)
		{
			compress_buffer_size <<= 1;
		}
		if (new_compress_buffer_size > K1024 * 4)
		{
			return false;
		}
		delete compress_buffer;
		delete query_buffer;
		compress_buffer = new unsigned char[compress_buffer_size];
		query_buffer = new char[compress_buffer_size * 4 + K4];
	}

	if (new_uncompress_buffer_size != 0 && new_uncompress_buffer_size > uncompress_buffer_size)
	{
		while (new_uncompress_buffer_size > uncompress_buffer_size)
		{
			uncompress_buffer_size <<= 1;
		}
		if (new_uncompress_buffer_size > K1024 * 16)
		{
			return false;
		}
		delete uncompress_buffer;
		uncompress_buffer = new unsigned char[uncompress_buffer_size];
	}
	return true;
}
/***************************************************************************/

/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"
#include "headers_game.h"
#include "utils_conversion.h"

extern GServerBase * global_server;

/***************************************************************************/
GHTTPRequestWorkerThread::GHTTPRequestWorkerThread(vector<SHTTPRequestTask> & http_request_tasks,
	boost::mutex & mtx_http_request_tasks,
	bool fire_and_forget,
	const string & metrics_prefix) :
	raport_interface(&raport_manager_global),
	http_request_tasks(http_request_tasks),
	mtx_http_request_tasks(mtx_http_request_tasks),
	fire_and_forget(fire_and_forget),
	metrics_prefix(metrics_prefix)
{
	curl = NULL;

	last_submit_operation_success = 0;
	last_submit_operation_fail = 0;
	last_submit_fails_count = 0;
	random_interval_change = 0;
}
/***************************************************************************/
bool GHTTPRequestWorkerThread::Init()
{
	if (curl)
	{
		curl_easy_cleanup(curl);
		curl = NULL;
	}

	curl = curl_easy_init();
	if (!curl)
	{
		return false;
	}
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, GHTTPRequestWorkerThread::receive_data);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

	global_signals->close_fast.connect(boost::bind(&GHTTPRequestWorkerThread::Close, this));
	worker_thread = new boost::thread(boost::bind(&GHTTPRequestWorkerThread::Action, this));

	return true;
}
/***************************************************************************/
void GHTTPRequestWorkerThread::Action()
{
	while (Continue())
	{
		for (;;)
		{
			ClearExpiredHTTPRequestTasks();

			SHTTPRequestTask current_task;
			if (!GetHTTPRequestTaskToRun(current_task))
			{
				break;
			}
			ProcessHTTPRequestTask(current_task);
		}

		Sleep(10 + truerng.Rnd(50));
	}
}
/***************************************************************************/
void GHTTPRequestWorkerThread::Destroy()
{
	Close();
	worker_thread->join();
	Delete(worker_thread);
	raport_interface.Destroy();

	if (curl)
	{
		curl_easy_cleanup(curl);
		curl = NULL;
	}
}
/***************************************************************************/
bool GHTTPRequestWorkerThread::GetHTTPRequestTaskToRun(SHTTPRequestTask & current_task)
{
	boost::mutex::scoped_lock lock(mtx_http_request_tasks);

	DWORD64 now = GSERVER->GetClock().Get();
	DWORD64 min_success_delay_ms = 25;
	DWORD64 min_failure_delay_ms = min(50 + last_submit_fails_count * last_submit_fails_count * 5, (DWORD64)GSECONDS_15);

	if (random_interval_change == 0)
	{
		random_interval_change = 50 + truerng.Rnd(101);
	}
	min_success_delay_ms = max((DWORD64)25, (min_success_delay_ms * random_interval_change) / 100);
	min_failure_delay_ms = max((DWORD64)50, (min_failure_delay_ms * random_interval_change) / 100);

	if (last_submit_operation_success + min_success_delay_ms > now ||
		last_submit_operation_fail + min_failure_delay_ms > now)
	{
		return false;
	}

	// Wybieramy task z najwczesniejszym deadlinem.
	vector<SHTTPRequestTask>::iterator pos;
	vector<SHTTPRequestTask>::iterator pos_best = http_request_tasks.end();
	for (pos = http_request_tasks.begin(); pos != http_request_tasks.end(); pos++)
	{
		if (pos_best == http_request_tasks.end() || pos->execution_deadline < pos_best->execution_deadline)
		{
			pos_best = pos;
		}
	}

	if (pos_best != http_request_tasks.end())
	{
		current_task = *pos_best;
		http_request_tasks.erase(pos_best);
		return true;
	}
	return false;
}
/***************************************************************************/
void GHTTPRequestWorkerThread::ProcessHTTPRequestTask(SHTTPRequestTask & current_task)
{
	SMetricStats::SCallContext call_ctx = SMetricStats::BeginDuration();
	{
		boost::mutex::scoped_lock lock(GSERVER->mtx_global_metrics);
		GSERVER->global_metrics.CommitCurrentValueChange(str(boost::format("busy_%1%_request_worker_threads") % metrics_prefix).c_str(), +1);
		GSERVER->global_metrics.CommitCurrentValueChange(str(boost::format("free_%1%_request_worker_threads") % metrics_prefix).c_str(), -1);
	}

	current_task.response_code = 0;

	if (STRICMP(current_task.request_method.c_str(), "GET") != 0 && !current_task.payload.empty() && !current_task.content_type.empty())
	{
		current_task.response_code = PerformHttpCustomRequest(current_task.url.c_str(), current_task.request_method.c_str(), current_task.payload.c_str(), current_task.content_type.c_str(), current_task.secret_key.c_str(), current_task.response);
	}
	else
	{
		string params_urlencoded = "";
		map<string, string>::iterator pos;
		for (pos = current_task.params.begin(); pos != current_task.params.end(); pos++)
		{
			if (pos != current_task.params.begin())
			{
				params_urlencoded += "&";
			}
			params_urlencoded += pos->first;
			params_urlencoded += "=";
			params_urlencoded += URLEncode(pos->second.c_str())();
		}

		if (STRICMP(current_task.request_method.c_str(), "GET") == 0)
		{
			string url = current_task.url;
			if (url.find('?') == string::npos && params_urlencoded != "")
			{
				url += "?";
			}
			url += params_urlencoded;
			current_task.response_code = PerformHttpGetRequest(url.c_str(), current_task.secret_key.c_str(), current_task.response);
		}
		else
		{
			current_task.response_code = PerformHttpCustomRequest(current_task.url.c_str(), current_task.request_method.c_str(), params_urlencoded.c_str(), NULL, current_task.secret_key.c_str(), current_task.response);
		}
	}

	{
		boost::mutex::scoped_lock lock(GSERVER->mtx_global_metrics);
		GSERVER->global_metrics.CommitCurrentValueChange(str(boost::format("busy_%1%_request_worker_threads") % metrics_prefix).c_str(), -1);
		GSERVER->global_metrics.CommitCurrentValueChange(str(boost::format("free_%1%_request_worker_threads") % metrics_prefix).c_str(), +1);
		GSERVER->global_metrics.CommitEvent(current_task.response_code > 0 ?
			str(boost::format("%1%_request_task_success") % metrics_prefix).c_str() : str(boost::format("%1%_request_task_failure") % metrics_prefix).c_str());
	}

	ProcessHTTPRequestTaskResult(current_task, current_task.response_code > 0);
}
/***************************************************************************/
void GHTTPRequestWorkerThread::ProcessHTTPRequestTaskResult(SHTTPRequestTask & current_task, bool success)
{
	DWORD64 now = GSERVER->GetClock().Get();

	random_interval_change = 50 + truerng.Rnd(101);

	if (success)
	{
		last_submit_operation_success = now;
		last_submit_operation_fail = 0;
		last_submit_fails_count = 0;
	}
	else
	{
		last_submit_operation_success = 0;
		last_submit_operation_fail = now;
		last_submit_fails_count++;
	}

	if (!fire_and_forget)
	{
		GSERVER->AddHTTPRequestTaskResult(current_task.group_id, current_task);
	}
}
/***************************************************************************/
void GHTTPRequestWorkerThread::ClearExpiredHTTPRequestTasks()
{
	boost::mutex::scoped_lock lock(mtx_http_request_tasks);
	boost::mutex::scoped_lock lock_metrics(GSERVER->mtx_global_metrics);

	DWORD64 current_time = GSERVER->CurrentSTime();

	INT32 a, count = http_request_tasks.size();
	for (a = count - 1; a >= 0; a--)
	{
		if (http_request_tasks[a].execution_deadline <= current_time)
		{
			http_request_tasks.erase(http_request_tasks.begin() + a);
			GSERVER->global_metrics.CommitEvent(str(boost::format("%1%_request_task_failure") % metrics_prefix).c_str());
		}
	}
}
/***************************************************************************/
size_t GHTTPRequestWorkerThread::receive_data(void * buffer, size_t size, size_t nmemb, void * string_buffer)
{
    string * s = (string *)string_buffer;
	s->append((char *)buffer, size * nmemb);
    return size * nmemb;
}
/***************************************************************************/
INT32 GHTTPRequestWorkerThread::PerformHttpGetRequest(const char * url, const char * secret_key, string & result)
{
	result = "";

	struct curl_slist * headers = NULL;
	headers = curl_slist_append(headers, "Connection: Keep-Alive");
	if (secret_key && secret_key[0])
	{
		headers = curl_slist_append(headers, (string("X-REST-Auth: ") + secret_key).c_str());
	}

	bool request_success = false;
	DWORD64 response_code = 0;
	response = "";

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_HTTPGET, true);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);

	CURLcode res = curl_easy_perform(curl);
	if (res == CURLE_OK)
	{
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
		result = response;
	}
	curl_slist_free_all(headers);

	return (INT32)response_code;
}
/***************************************************************************/
INT32 GHTTPRequestWorkerThread::PerformHttpCustomRequest(const char * url, const char * request_method, const char * post_payload, const char * content_type, const char * secret_key, string & result)
{
	result = "";

	struct curl_slist * headers = NULL;
	if (content_type && content_type[0])
	{
		string content_type_string("Content-Type: ");
		content_type_string += content_type;
		headers = curl_slist_append(headers, content_type_string.c_str());
	}
	else
	{
		headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
	}
	headers = curl_slist_append(headers, "Connection: Keep-Alive");
	if (secret_key && secret_key[0])
	{
		headers = curl_slist_append(headers, (string("X-REST-Auth: ") + secret_key).c_str());
	}

	bool request_success = false;
	DWORD64 response_code = 0;
	response = "";

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_payload);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request_method);

	CURLcode res = curl_easy_perform(curl);
	if (res == CURLE_OK)
	{
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
		result = response;
	}
	curl_slist_free_all(headers);

	return (INT32)response_code;
}

/***************************************************************************/


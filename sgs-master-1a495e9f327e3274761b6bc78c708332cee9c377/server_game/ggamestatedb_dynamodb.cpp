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

#define MAX_DYNAMODB_GAMESTATE_PART_SIZE								60000

boost::mutex	DynamoDBConnection::mtx_aws_session;
struct			DynamoDBConnection::SAWSSession DynamoDBConnection::aws_session;
/***************************************************************************/
DynamoDBConnection::DynamoDBConnection()
{
	curl = NULL;
}
/***************************************************************************/
DynamoDBConnection::~DynamoDBConnection()
{
	if (curl)
	{
		curl_easy_cleanup(curl);
		curl = NULL;
	}
}
/***************************************************************************/
bool DynamoDBConnection::Init()
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
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, DynamoDBConnection::receive_data);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);

	return true;
}
/***************************************************************************/
#define SHA256_DIGEST_SIZE ( 256 / 8)

void sha256(const unsigned char *message, unsigned int len, unsigned char *digest);
void hmac_sha256(const unsigned char *key, unsigned int key_size,
					const unsigned char *message, unsigned int message_len,
					unsigned char *mac, unsigned mac_size);
char * base64_encode(const char *data, int input_length, char * encoding_buffer);
int base64_decode(const char * data, int input_length, char * decode_buffer);
/***************************************************************************/
size_t DynamoDBConnection::receive_data(void * buffer, size_t size, size_t nmemb, void * string_buffer)
{
    string * s = (string *)string_buffer;
	s->append((char *)buffer, size * nmemb);
    return size * nmemb;
}
/***************************************************************************/
bool DynamoDBConnection::RefreshSessionToken()
{
	DWORD64 cur_time = GSERVER->CurrentSTime();
	// Nie robimy zapytania o session token czesciej niz raz na 5 sekund.
	{
		boost::mutex::scoped_lock lock(mtx_aws_session);

		if (aws_session.last_session_request != 0 && aws_session.last_session_request + 5 > cur_time)
		{
			return false;
		}
		aws_session.last_session_request = cur_time;
	}

	if (!Init())
	{
		return false;
	}

	struct tm * now;
	time_t ltime;
	time(&ltime);
	now = gmtime(&ltime);
	char current_timestamp[128];
	strftime(current_timestamp, 64, "%Y-%m-%dT%H%M%SZ", now);

	DWORD64 aws_session_time = 3600;

	SSQLConfig & cfg = global_serverconfig->gamestatedb_config;

	string to_sign = str(boost::format("GET\n" "sts.amazonaws.com\n" "/\n"
										"AWSAccessKeyId=%1%&Action=GetSessionToken&DurationSeconds=%2%&SignatureMethod=HmacSHA256&SignatureVersion=2&"
										"Timestamp=%3%&Version=2011-06-15") %
										cfg.user % aws_session_time % current_timestamp);

	char mac[SHA256_DIGEST_SIZE + 10], mac_base64[SHA256_DIGEST_SIZE * 2 + 10];
	hmac_sha256((unsigned char *)cfg.password.c_str(), cfg.password.size(), (unsigned char *)to_sign.c_str(), to_sign.size(), (unsigned char *)mac, SHA256_DIGEST_SIZE);
	base64_encode(mac, SHA256_DIGEST_SIZE, mac_base64);
	char * mac_base64_urlencoded = curl_easy_escape(curl, mac_base64, strlen(mac_base64));

	string url = str(boost::format("https://sts.amazonaws.com?"
										"AWSAccessKeyId=%1%&Action=GetSessionToken&DurationSeconds=%2%&SignatureMethod=HmacSHA256&SignatureVersion=2&"
										"Timestamp=%3%&Version=2011-06-15&Signature=%4%") %
										cfg.user % aws_session_time % current_timestamp % mac_base64_urlencoded);
	curl_free(mac_base64_urlencoded);

	response = "";
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	if (curl_easy_perform(curl) != CURLE_OK)
	{
		return false;
	}

	DWORD64 response_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
	if (response_code != 200)
	{
		return false;
	}

	{
		boost::mutex::scoped_lock lock(mtx_aws_session);

		boost::char_separator<char> sep("<> \n\r");
		typedef boost::tokenizer<boost::char_separator<char> > TTokenizer;
		TTokenizer tok(response, sep);

		string prev_value = "";
		TTokenizer::iterator it;
		for (it = tok.begin(); it != tok.end(); it++)
		{
			if (prev_value == "SessionToken")
			{
				aws_session.session_token = it->c_str();
			}
			if (prev_value == "AccessKeyId")
			{
				aws_session.access_key = it->c_str();
			}
			if (prev_value == "SecretAccessKey")
			{
				aws_session.secret_key = it->c_str();
			}
			prev_value = it->c_str();
		}

		if (aws_session.session_token == "" ||
			aws_session.access_key == "" ||
			aws_session.secret_key == "")
		{
			return false;
		}

		aws_session.expiration_time = cur_time + aws_session_time;
	}
	return true;
}
/***************************************************************************/
bool DynamoDBConnection::PerformRequest(const char * action, const char * params, string & result)
{
	result = "";

	DWORD64 cur_time = GSERVER->CurrentSTime();
	if (cur_time + 180 > aws_session.expiration_time)
	{
		bool refresh_sts_result = RefreshSessionToken();
		if (!refresh_sts_result && cur_time + 60 > aws_session.expiration_time)
		{
			return false;
		}
	}

	DynamoDBConnection::SAWSSession aws_session_copy;
	{
		boost::mutex::scoped_lock lock(mtx_aws_session);
		aws_session_copy = aws_session;
	}

	struct tm * now;
	time_t ltime;
	time(&ltime);
	now = gmtime(&ltime);
	char current_timestamp[128];
	strcpy(current_timestamp, asctime(now));
	while (*current_timestamp && (current_timestamp[strlen(current_timestamp) - 1] == '\n' || current_timestamp[strlen(current_timestamp) - 1] == '\r'))
	{
		current_timestamp[strlen(current_timestamp) - 1] = 0;
	}
	// Dodajemy leading 0 przy datach z jednocyfrowym dniem - tak wymaga AWS.
	{
		char * pp;
		for (pp = current_timestamp; *pp != 0; pp++)
		{
			if (pp[0] == ' ' && pp[1] == ' ' && (pp[2] >= '1' && pp[2] <= '9'))
			{
				pp[1] = '0';
				break;
			}
		}
	}

	SSQLConfig & cfg = global_serverconfig->gamestatedb_config;

	string to_sign = str(boost::format("POST\n" "/\n" "\n"
										"host:%1%\n" "x-amz-date:%2%\n" "x-amz-security-token:%3%\n" "x-amz-target:DynamoDB_20111205.%4%\n" "\n" "%5%") %
										cfg.host % current_timestamp % aws_session_copy.session_token % action % params);

	char sha256_to_sign[SHA256_DIGEST_SIZE + 10], mac[SHA256_DIGEST_SIZE + 10], mac_base64[SHA256_DIGEST_SIZE * 2 + 10];
	sha256((unsigned char *)to_sign.c_str(), to_sign.size(), (unsigned char *)sha256_to_sign);
	hmac_sha256((unsigned char *)aws_session_copy.secret_key.c_str(), aws_session_copy.secret_key.size(), (unsigned char *)sha256_to_sign, SHA256_DIGEST_SIZE, (unsigned char *)mac, SHA256_DIGEST_SIZE);
	base64_encode(mac, SHA256_DIGEST_SIZE, mac_base64);

	string url = str(boost::format("http://%1%") % cfg.host);

	struct curl_slist * headers = NULL;
	headers = curl_slist_append(headers, str(boost::format("x-amz-date: %1%") % current_timestamp).c_str());
	headers = curl_slist_append(headers, str(boost::format("x-amzn-authorization: AWS3 AWSAccessKeyId=%1%,Algorithm=HmacSHA256,"
															"SignedHeaders=host;x-amz-date;x-amz-security-token;x-amz-target,Signature=%2%") %
															aws_session_copy.access_key % mac_base64).c_str());
	headers = curl_slist_append(headers, str(boost::format("x-amz-target: DynamoDB_20111205.%1%") % action).c_str());
	headers = curl_slist_append(headers, str(boost::format("x-amz-security-token: %1%") % aws_session_copy.session_token).c_str());
	headers = curl_slist_append(headers, "content-type: application/x-amz-json-1.0");
	headers = curl_slist_append(headers, "connection: Keep-Alive");
	headers = curl_slist_append(headers, "user-agent: perl");

	// Do kiedy bedziemy probowac wykonac request w przypadku throttlingu.
	time_t request_deadline = ltime + 25;
	bool request_success = false;
	for (;;)
	{
		response = "";
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, params);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		
		CURLcode res = curl_easy_perform(curl);
		if (res != CURLE_OK)
		{
			break;
		}
	
		DWORD64 response_code = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
		if (response_code == 200)
		{
			request_success = true;
			break;
		}
		if (response_code == 400)
		{
			// Throttling. Bedziemy probowac wykonac request ponownie.
			Sleep(1000 + truerng.Rnd(3000));
			time_t ltime;
			time(&ltime);
			if (ltime < request_deadline)
			{
				continue;
			}
		}
		break;
	}
	curl_slist_free_all(headers); 

	if (request_success)
	{
		result = response;
	}
	return request_success;
}
/***************************************************************************/
bool GLocationDBWorkerThread::InitDatabaseDynamoDB()
{
	if (!dynamodb_gamestates.Init())
	{
		return false;
	}
	return true;
}
/***************************************************************************/
void GLocationDBWorkerThread::DestroyDatabaseDynamoDB()
{
}
/***************************************************************************/
bool GLocationDBWorkerThread::LoadLocationFromDatabaseDynamoDB(const char * location_ID, string & location_data)
{
	boost::char_separator<char> json_sep("\":{}, \n\r");
	typedef boost::tokenizer<boost::char_separator<char> > TTokenizer;

	location_data = "{}";

	INT32 gamestate_ID = gamestate_ID_from_location_ID(location_ID);
	if (gamestate_ID == 0)
	{
		return LoadLocationFromDatabaseMySQL(location_ID, location_data);
	}

	string gamestate_data = "";
	INT32 total_parts = 0, second_partID = 0;

	string cmdparams = str(boost::format("{ \"TableName\": \"%1%\", "
											"\"Key\": { "
												"\"HashKeyElement\": { \"N\": \"%2%\"}, "
												"\"RangeKeyElement\": { \"N\": \"%3%\" } "
											"} "
										"}") %
										global_serverconfig->gamestatedb_config.database %
										gamestate_ID % (DWORD32)1);

	string response;
	if (!dynamodb_gamestates.PerformRequest("GetItem", cmdparams.c_str(), response))
	{
		return false;
	}

	{
		TTokenizer tok(response, json_sep);

		string prev_value[2] = {"", ""};
		TTokenizer::iterator it;
		INT32 idx = 0;
		for (it = tok.begin(); it != tok.end(); it++, idx++)
		{
			string & prev1 = prev_value[idx & 1];
			string & prev2 = prev_value[(idx & 1) ^ 1];

			if (prev1 == "S" && prev2 == "gamestate_data")
			{
				gamestate_data = it->c_str();
			}
			if (prev1 == "N" && prev2 == "total_parts")
			{
				total_parts = ATOI(it->c_str());
			}
			if (prev1 == "N" && prev2 == "second_partID")
			{
				second_partID = ATOI(it->c_str());
			}
			prev2 = it->c_str();
		}
		// Zachowujemy kompatybilnosc wstecz, gdy jeszcze nie korzystalismy z second_partID.
		if (total_parts > 1 && second_partID == 0)
		{
			second_partID = 2;
		}
	}

	{
		boost::mutex::scoped_lock lock(mtx_locationdb_multipart_locations);
		multipart_locations_total_parts[location_ID] = total_parts;
		multipart_locations_second_partID[location_ID] = second_partID;
	}

	if (total_parts > 1)
	{
		cmdparams = str(boost::format("{ \"TableName\": \"%1%\", "
										"\"Limit\": %2%, "
										"\"HashKeyValue\": { \"N\": \"%3%\"}, "
												"\"ExclusiveStartKey\": { "
													"\"HashKeyElement\": { \"N\": \"%4%\"}, "
													"\"RangeKeyElement\": { \"N\": \"%5%\" } "
												"} "
											"}") %
											global_serverconfig->gamestatedb_config.database %
											(DWORD32)(total_parts - 1) % gamestate_ID % gamestate_ID % (DWORD32)(second_partID - 1));

		string response;
		if (!dynamodb_gamestates.PerformRequest("Query", cmdparams.c_str(), response))
		{
			return false;
		}

		TTokenizer tok(response, json_sep);

		string prev_value[2] = {"", ""};
		TTokenizer::iterator it;
		INT32 idx = 0;
		for (it = tok.begin(); it != tok.end(); it++, idx++)
		{
			string & prev1 = prev_value[idx & 1];
			string & prev2 = prev_value[(idx & 1) ^ 1];

			if (prev1 == "S" && prev2 == "gamestate_data")
			{
				gamestate_data += it->c_str();
			}
			prev2 = it->c_str();
		}
	}

	if (gamestate_data == "")
	{
		return true;
	}

	if ((int)gamestate_data.size() > compress_buffer_size)
	{
		if (!ReallocateCompressBuffers(gamestate_data.size(), 0))
		{
			return false;
		}
	}
	last_compressed_size = gamestate_data.size();
	INT32 decoded_size = base64_decode(gamestate_data.c_str(), gamestate_data.size(), query_buffer);
	if (!Uncompress(query_buffer, decoded_size, location_data))
	{
		string err_str = str(boost::format("Error uncompressing gamestate BLOB (id %1%)") % gamestate_ID);
		GRAPORT(err_str.c_str());
		return false;
	}
	return true;
}
/***************************************************************************/
bool GLocationDBWorkerThread::WriteLocationToDatabaseDynamoDB(const char * location_ID, const string & location_data)
{
	INT32 gamestate_ID = gamestate_ID_from_location_ID(location_ID);
	if(gamestate_ID >= MIN_GUEST_GAMESTATE_ID)
	{
		return true;
	}
	if (gamestate_ID == 0)
	{
		return WriteLocationToDatabaseMySQL(location_ID, location_data);
	}

	unsigned long compressed_size;
	if (!Compress(location_data, &compressed_size))
	{
		return false;
	}
	base64_encode((const char*)compress_buffer, compressed_size, query_buffer);

	INT32 gamestate_data_size = strlen(query_buffer);
	last_compressed_size = gamestate_data_size;

	INT32 total_parts_old, total_parts, second_partID_old, second_partID = 0;
	{
		boost::mutex::scoped_lock lock(mtx_locationdb_multipart_locations);
		total_parts_old = multipart_locations_total_parts[location_ID];
		second_partID_old = multipart_locations_second_partID[location_ID];
	}
	total_parts = (gamestate_data_size + MAX_DYNAMODB_GAMESTATE_PART_SIZE - 1) / MAX_DYNAMODB_GAMESTATE_PART_SIZE;
	if (total_parts > 1)
	{
		if (second_partID_old == 0 || second_partID_old > total_parts)
		{
			second_partID = 2;
		}
		else
		{
			second_partID = second_partID_old + total_parts_old - 1;
		}
	}

	INT32 a;
	for (a = total_parts - 1; a >= 0; a--)
	{
		string gamestate_part;
		gamestate_part.assign(query_buffer + a * MAX_DYNAMODB_GAMESTATE_PART_SIZE, min(gamestate_data_size - a * MAX_DYNAMODB_GAMESTATE_PART_SIZE, MAX_DYNAMODB_GAMESTATE_PART_SIZE));

		string cmdparams = str(boost::format("{ \"TableName\": \"%1%\", "
												"\"Item\": { "
													"\"playerID\": { \"N\": \"%2%\"}, "
													"\"partID\": { \"N\": \"%3%\" }, "
													"\"second_partID\": { \"N\": \"%4%\" }, "
													"\"total_parts\": { \"N\": \"%5%\" }, "
													"\"gamestate_data\": { \"S\": \"%6%\" } "
												"} "
											"}") %
											global_serverconfig->gamestatedb_config.database % gamestate_ID %
											(DWORD32)(a == 0 ? 1 : second_partID + a - 1) % (DWORD32)second_partID % (DWORD32)total_parts % gamestate_part);
		string response;
		if (!dynamodb_gamestates.PerformRequest("PutItem", cmdparams.c_str(), response))
		{
			return false;
		}
	}
	for (a = second_partID_old; a < second_partID_old + total_parts_old - 1; a++)
	{
		string cmdparams = str(boost::format("{ \"TableName\": \"%1%\", "
												"\"Key\": { "
													"\"HashKeyElement\": { \"N\": \"%2%\"}, "
													"\"RangeKeyElement\": { \"N\": \"%3%\" } "
												"} "
											"}") %
											global_serverconfig->gamestatedb_config.database %
											gamestate_ID % (DWORD32)a);
		string response;
		if (!dynamodb_gamestates.PerformRequest("DeleteItem", cmdparams.c_str(), response))
		{
			return false;
		}
	}

	{
		boost::mutex::scoped_lock lock(mtx_locationdb_multipart_locations);
		multipart_locations_total_parts[location_ID] = total_parts;
		multipart_locations_second_partID[location_ID] = second_partID;
	}
	return true;
}
/***************************************************************************/
bool GLocationDBWorkerThread::DeleteLocationFromDatabaseDynamoDB(const char * location_ID)
{
	INT32 gamestate_ID = gamestate_ID_from_location_ID(location_ID);
	if (gamestate_ID > 0)
	{
		return false;
	}
	return DeleteLocationFromDatabaseMySQL(location_ID);
}
/***************************************************************************/

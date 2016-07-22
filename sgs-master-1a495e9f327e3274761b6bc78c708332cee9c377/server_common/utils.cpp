/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"
#include "gtruerng.h"

/***************************************************************************/
#ifdef LINUX

inline struct in_addr *make_addr(const char *host)
{
	struct hostent *hp;
	static struct in_addr addr;
	char myname[MAX_HOSTNAME + 1];

	if(host == NULL || strlen(host) == 0)
	{
		gethostname(myname,MAX_HOSTNAME);
		hp = gethostbyname(myname);
		if(hp != NULL)
		{
			return (struct in_addr *) *hp->h_addr_list;
		}
	}
	else
	{
		addr.s_addr = inet_addr(host);
		if(addr.s_addr != -1)
		{
			return &addr;
		}
		hp = gethostbyname(host);
		if(hp != NULL)
		{
			return (struct in_addr *) *hp->h_addr_list;
		}
	}
	return NULL;
}
#endif
/***************************************************************************/
#ifdef LINUX
int make_netsocket(const char *host,int port)
#else
int make_netsocket(const char *,int port)
#endif
{
	SOCKET s;
	struct sockaddr_in sa;
	int socket_type;
	unsigned long l=1;

#ifdef LINUX
	bzero((void *)&sa,sizeof(struct sockaddr_in));
#else
	memset(&sa,0,sizeof(sockaddr_in));
#endif

	socket_type = SOCK_STREAM;
	if((s = socket(AF_INET,socket_type,0)) < 0) return(-1);
	ioctl(s,FIONBIO,&l);

	sa.sin_family = AF_INET;
	sa.sin_port = htons((u_short)port);

	sa.sin_addr.s_addr = INADDR_ANY;
#ifdef LINUX
	if (host != NULL)
	{
		struct in_addr * saddr;
		saddr = make_addr(host);
		if (saddr == NULL) return(-1);
		sa.sin_addr.s_addr = saddr->s_addr;
	}
#endif

	if(::bind(s,(struct sockaddr*)&sa,sizeof sa) < 0) 
	{
		closesocket(s);
		return(-1);
	}
	return((int)s);
}
/***************************************************************************/
// host: * = INADDR_ANY, IP = konkretny IP
// port: * = dowolny port, X = konkretny port, A-B = losowy port z zakresu [A, B], A- = losowy port >= A
// Zwracamy: >= 0 - OK - nr socketu, -1 - blad, assigned_port - port na ktory sie ostatecznie zbindowalismy
int make_netsocket_from_range(const char * host, const char * port, int & assigned_port)
{
	assigned_port = -1;
	if (host != NULL && (*host == 0 || strcmp(host, "*") == 0))
	{
		host = NULL;
	}

	int min_port = 0, max_port = 0xffff;
	if (port != NULL && *port && strcmp(port, "*") != 0)
	{
		min_port = ATOI(port);
		if (min_port < 0 || min_port > 0xffff)
		{
			min_port = 0;
		}
		const char * p = strchr(port, '-');
		if (p == NULL)
		{
			max_port = min_port;
		}
		else
		{
			max_port = ATOI(p + 1);
			if (max_port < 0 || max_port > 0xffff)
			{
				max_port = 0xffff;
			}
		}
	}
	if (min_port > max_port)
	{
		return -1;
	}

	set<int> checked_ports;
	while ((int)checked_ports.size() < max_port - min_port + 1)
	{
		INT32 port = min_port + truerng.Rnd(max_port - min_port + 1);
		if (checked_ports.find(port) != checked_ports.end())
		{
			continue;
		}
		int socket = make_netsocket(host, port);
		if (socket >= 0)
		{
			assigned_port = port;
			return socket;
		}
		checked_ports.insert(port);
	}
	return -1;
}
/***************************************************************************/
bool is_listening_socket(int fd)
{
	if (fd <= 0)
	{
		return false;
	}
	int val;
	socklen_t len = sizeof(val);
	if (getsockopt(fd, SOL_SOCKET, SO_ACCEPTCONN, (char *)&val, &len) == -1 || !val)
	{
		return false;
	}
	return true;
}
/***************************************************************************/
DWORD32 StringToAddr (char *string)
{
	//ta drobna zmiana sprawi ze w przypadku IPV6 po stronie klienta dostaniemy jakas wartosc zwrotna
	//a nie wartosc losowa, rezultat - jesli ktos uzyje ipv6 do rozrabiania to ban zadziala na cala jego podsieci lokalna 
	//uzywajaca ipv6
	int ha1=1, ha2=2, ha3=3, ha4=4;
	DWORD32 ipaddr;
	sscanf(string, "%d.%d.%d.%d", &ha1, &ha2, &ha3, &ha4);
	ipaddr = (ha1 << 24) | (ha2 << 16) | (ha3 << 8) | ha4;
	return ipaddr;
}
/***************************************************************************/
DWORD32 IPToAddr (BYTE p0,BYTE p1,BYTE p2, BYTE p3)
{
	DWORD32 ipaddr;
	ipaddr = (p3<< 24) | (p2<< 16) | (p1<< 8) | p0;
	return ipaddr;
}
/***************************************************************************/
DWORD32 GetAddrFromName(const char * name)
{
	unsigned long addr;
	addr=0;
	if (name[0] >= '0' && name[0] <= '9')
	{
		addr=inet_addr(name);
	}
	else
	{
		struct hostent * host_server;
		host_server = gethostbyname(name);
		if (host_server)
		{
			memcpy(&addr,host_server->h_addr_list[0],4);
		}
		else
		{
			addr=0;
		}
	}
	return addr;
}
/***************************************************************************/
void CopyStr(char * dest, const char * src, int len)
{
	strncpy(dest,src,len);
	dest[len-1]=0;
}
/***************************************************************************/
DWORD64 GetTime()
{
	DWORD64		time;
#ifdef LINUX
	timeval tv;
	gettimeofday(&tv,NULL);
	time=tv.tv_sec;
	time*=1000;
	time+=(tv.tv_usec/1000);
#else
	time=timeGetTime();
#endif
	return time;
};
/***************************************************************************/
DWORD64 GetTimeUSec()
{
	DWORD64		time;
#ifdef LINUX
	timeval tv;
	gettimeofday(&tv,NULL);
	time=tv.tv_sec;
	time*=1000000;
	time+=tv.tv_usec;
#else
	time=(DWORD64)timeGetTime()*1000;
#endif
	return time;
};
/***************************************************************************/
#ifdef LINUX
void _i64toa(INT64 val, char * buf, int)
{
	sprintf(buf,"%lld",val);
};
void _ui64toa(INT64 val, char * buf, int)
{
	sprintf(buf,"%lld",val);
};
void _gcvt(float val, int len,char * buf)
{
	char format[10];
	sprintf(format,"%%.%df",len);
	sprintf(buf,format,val);
}
#endif
/***************************************************************************/
wstring CodedToUTF(char * str)
{
	string text=str;

	wstring decoded;

	while(text.size())
	{
		if(text[0]=='&' && text.size()>=3 && text[1]=='#') 
		{
			text.erase(0,2);
			string digit;
			while(text.size() && text[0]>='0' && text[0]<='9')
			{
				digit.append(1,text[0]);
				text.erase(0,1);
			}
			int d=ATOI(digit.c_str());
			wchar_t c=(wchar_t)d;
			decoded.append(1,c);
			text.erase(0,1);
		}
		else
		{
			decoded.append(1,text[0]);
			text.erase(0,1);
		}
	}
	return decoded;
}

/***************************************************************************/
string& ShortString2LowerCase(string& text)
{
	char temp_string[256], *p;

	strncpy(temp_string, text.c_str(), sizeof(temp_string));
	temp_string[sizeof(temp_string) - 1] = 0;
	for (p = temp_string; *p != 0; p++)
		*p = (char)tolower(*p);

	text = temp_string;
	return text;
}

/***************************************************************************/
string escape_json_string(const string & input)
{
	std::ostringstream ss;
	std::string::const_iterator iter;
	for (iter = input.begin(); iter != input.end(); iter++)
	{
		switch (*iter)
		{
			case '\\': ss << "\\\\"; break;
			case '"': ss << "\\\""; break;
			case '/': ss << "\\/"; break;
			case '\b': ss << "\\b"; break;
			case '\f': ss << "\\f"; break;
			case '\n': ss << "\\n"; break;
			case '\r': ss << "\\r"; break;
			case '\t': ss << "\\t"; break;
			default: ss << *iter; break;
		}
	}
	return ss.str();
}
/***************************************************************************/
// 'interface_name' to np. "eth0", "eth1:1" itp. Default to pierwsze z brzegu.
string get_local_IP(const char * interface_name)
{
	string result = "127.0.0.1";
#ifndef LINUX
	char hostName[MAX_HOSTNAME];
	int ret = gethostname(hostName, MAX_HOSTNAME);
	if (ret != SOCKET_ERROR)
	{
		struct hostent * hostIP = NULL;
		hostIP = gethostbyname(hostName);
		if (hostIP != NULL)
		{
			u_long address;
			u_long address_temp;
			address = (hostIP) ? *(int *)hostIP->h_addr_list[0] : 0;
			address_temp = ntohl(address);
			char hostId[MAX_HOSTNAME];
			sprintf(hostId, "%d.%d.%d.%d", (address_temp>> 24) & 0xff, (address_temp>> 16) & 0xff, (address_temp>> 8) & 0xff, address_temp& 0xff);
			return hostId;
		}
	}
#else
	char command_name[128];
	SNPRINTF(command_name, sizeof(command_name), "/sbin/ifconfig %s", interface_name);
	command_name[sizeof(command_name) - 1] = 0;
	FILE * fp = popen(command_name, "r");
	if (fp)
	{
		char * pline = NULL, * p, * e; size_t n;
		while ((getline(&pline, &n, fp) > 0) && pline)
		{
			if (p = strstr(pline, "inet addr:"))
			{
				p += 10;
				if (e = strchr(p, ' '))
				{
					*e = '\0';
					if (strncmp(p, "10.", 3) == 0 || strncmp(p, "192.168.", 8) == 0)
					{
						result = p;
						break;
					}
				}
			}
		}
		if (pline)
		{
			free(pline);
		}
		pclose(fp);
	}
#endif
	return result;
}
/***************************************************************************/
string get_EC2_public_IP()
{
	string result = "";
#ifdef LINUX
	FILE * fp = popen("curl --max-time 5 http://169.254.169.254/latest/meta-data/public-ipv4 2>/dev/null", "r");
	if (fp)
	{
		char * pline = NULL, * e; size_t n;
		while ((getline(&pline, &n, fp) > 0) && pline)
		{
			e = pline;
			while (*e && *e != ' ' && *e != '\r' && *e != '\n')
			{
				e++;
			}
			*e = 0;
			if (*pline)
			{
				result = pline;
				break;
			}
		}
		if (pline)
		{
			free(pline);
		}
		pclose(fp);
	}
#endif
	if (result == "")
	{
		result = get_local_IP();
	}
	return result;
}
/***************************************************************************/
int get_CPU_cores_count()
{
	string result = "";
#ifdef LINUX
	FILE * fp = popen("grep -c ^processor /proc/cpuinfo 2>/dev/null", "r");
	if (fp)
	{
		char * pline = NULL, * e; size_t n;
		while ((getline(&pline, &n, fp) > 0) && pline)
		{
			e = pline;
			while (*e && *e != ' ' && *e != '\r' && *e != '\n')
			{
				e++;
			}
			*e = 0;
			if (*pline)
			{
				result = pline;
				break;
			}
		}
		if (pline)
		{
			free(pline);
		}
		pclose(fp);
	}
#endif
	int CPU_count = ATOI(result.c_str());
	return (CPU_count > 0) ? CPU_count : 1;
}
/***************************************************************************/

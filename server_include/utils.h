/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

DWORD32 GetAddrFromName(const char * name);
DWORD32 IPToAddr (BYTE p0,BYTE p1,BYTE p2, BYTE p3);
int make_netsocket(const char *host,int port);
int make_netsocket_from_range(const char * host, const char * port, int & assigned_port);
bool is_listening_socket(int fd);
string get_local_IP(const char * interface_name = "");
string get_EC2_public_IP();
int get_CPU_cores_count();
/***************************************************************************/
class AddrToString: boost::noncopyable
{
public:
	AddrToString(const sockaddr_in & addr_p)
	{
		addr=addr_p;
		only_ip=false;
	}
	AddrToString(const DWORD32 & addr_p)
	{
		(&addr)->sin_addr.s_addr=addr_p;
		only_ip=true;
	}
	char * operator()()
	{
		DWORD32 haddr = ntohl((&addr)->sin_addr.s_addr);
		if(only_ip)
			sprintf(buffer, "%d.%d.%d.%d", (haddr >> 24) & 0xff, (haddr >> 16) & 0xff, (haddr >> 8) & 0xff, haddr & 0xff);
		else
			sprintf(buffer, "%d.%d.%d.%d:%d", (haddr >> 24) & 0xff, (haddr >> 16) & 0xff, (haddr >> 8) & 0xff, haddr & 0xff, ntohs(((sockaddr_in *)&addr)->sin_port));
		return buffer;
	}
private:
	sockaddr_in addr;
	char buffer[100];
	bool only_ip;
};
/***************************************************************************/
#ifdef LINUX
/***************************************************************************/
inline void Sleep(DWORD time)
{
	usleep(time*1000);
};
#endif
/***************************************************************************/
DWORD64 GetTime();
/***************************************************************************/
class GClock: public boost::noncopyable
{
public:
	GClock()	{Probe();};
	DWORD64		Get()
	{
		return	time;
	}
	void		Probe()
	{
		time=GetTime();
	}
	bool		IsAfter(DWORD64 start, DWORD64 period)
	{
		return (start+period>time);
	}
	DWORD64 operator()(){time=GetTime();return time;};
private:
	DWORD64		time;
};
/***************************************************************************/
#define GSECOND_1	1000
#define GSECONDS_5	5000
#define GSECONDS_15	15000
#define GMINUTE_1	60000
#define GMINUTES_5	300000
#define GMINUTES_15	900000
#define GHOUR_1		3600000
#define GDAY_1		86400000
#define GDAY_7		604800000

class GTimeInterval
{
public:
	GTimeInterval(){time_start=time_end=time_update=interval=0;};
	GTimeInterval(GClock & clock)
	{
		time_start=clock.Get();
		time_end=time_start;
		time_update=time_start;
		interval=0;
	}
	GTimeInterval(DWORD64 interval_p)
	{
		time_start=0;
		time_end=0;
		time_update=0;
		interval=interval_p;
	}
	GTimeInterval(GClock & clock, DWORD64 interval_p)
	{
		time_start=clock.Get();
		time_end=time_start+interval;
		time_update=time_start;
		interval=interval_p;
	}
	void	Init(GClock & clock, DWORD64 interval_p)
	{
		time_start=clock.Get();
		time_end=time_start+interval;
		time_update=time_start;
		interval=interval_p;
	}
	void	Init(DWORD64 interval_p)
	{
		time_start=0;
		time_end=0;
		time_update=0;
		interval=interval_p;
	}
	void	Init()
	{
		time_start=0;
		time_end=0;
		time_update=0;
	}
	void	SetInterval(DWORD64 interval_p)
	{
		interval=interval_p;
	}
	bool	operator()(GClock & clock)
	{
		if(time_update>clock.Get())
		{
			DWORD64 diff=time_update-clock.Get();
			if(time_start>diff) time_start-=diff; else time_start=0;
			if(time_end>diff) time_end-=diff; else time_end=0;
			if(time_update>diff) time_update-=diff; else time_update=0;
		}
		time_update=clock.Get();
		if(time_end==0) 
		{
			time_start=time_update;
			time_end=time_update+interval;
		}
		if(time_update>time_end)
		{
			time_start=time_end;
			time_end+=interval;
			return true;
		}
		return false;
	}
	bool	Test(GClock & clock)
	{
		if(time_update>clock.Get())
		{
			DWORD64 diff=time_update-clock.Get();
			if(time_start>diff) time_start-=diff; else time_start=0;
			if(time_end>diff) time_end-=diff; else time_end=0;
			if(time_update>diff) time_update-=diff; else time_update=0;
		}
		time_update=clock.Get();
		if(time_end==0) 
		{
			time_start=time_update;
			time_end=time_update+interval;
		}
		if(time_update>time_end)
		{
			time_start=time_end;
			//time_end+=interval;
			return true;
		}
		return false;
	}
	void	Update(GClock & clock, bool back=false)
	{
		time_update=clock.Get();
		time_start=time_update;
		time_end=time_update+interval;
		if(back) time_end=time_update-interval-1;
	}
	bool	operator+=(DWORD64 val)
	{
		interval+=val;
		time_end+=val;
	}
	bool	operator==(DWORD64 val)
	{
		interval=val;
		time_end=time_start+interval;
	}
private:
	DWORD64					time_start;
	DWORD64					time_end;
	DWORD64					time_update;
	DWORD64					interval;
};
/***************************************************************************/
class GClocks
{
public:
	GClocks():interval_ms(100),interval_second(GSECOND_1),interval_5_seconds(GSECONDS_5),interval_15_seconds(GSECONDS_15),interval_minute(GMINUTE_1),interval_5_minutes(GMINUTES_5),interval_15_minutes(GMINUTES_15),interval_hour(GHOUR_1),interval_day(GDAY_1){};
	GTimeInterval		interval_ms;
	GTimeInterval		interval_second;
	GTimeInterval		interval_5_seconds;
	GTimeInterval		interval_15_seconds;
	GTimeInterval		interval_minute;
	GTimeInterval		interval_5_minutes;
	GTimeInterval		interval_15_minutes;
	GTimeInterval		interval_hour;
	GTimeInterval		interval_day;
};
/***************************************************************************/
void CopyStr(char * dest, const char * src, int len);
/***************************************************************************/
struct GBitArray8
{
	GBitArray8(){tbl=0;}
	void			Zero(){tbl=0;};
	GBitArray8 &	MA(){tbl=255;return *this;}
	GBitArray8 &	UA(){tbl=0;return *this;}
	bool			IM(int n){int bit = 1 << n;return (tbl&bit) == bit;}
	GBitArray8 &	M(int n){int bit = 1 << n;tbl|= bit;return *this;}
	GBitArray8 &	U(int n){int bit = 1 << n;tbl&= ~bit;return *this;}
	GBitArray8 &	I(int n){int bit = 1 << n;if(tbl&bit) tbl&= ~bit;else tbl|= bit;return *this;}
	GBitArray8 &	S(int n, bool v){if (v) M(n); else U(n); return *this;}
	bool			operator[](int n) const {int bit = 1 << n;return (tbl&bit) == bit;}
	void			operator=(const GBitArray8 & ba){tbl=ba.tbl;}
	void			operator=(BYTE b){tbl=b;};
	BYTE			operator()(){return tbl;};

	int				Size()
	{
		int count = 0;
		for (int a = 0; a < 8; a++)
		{
			if (IM(a)) count++;
		}
		return count;
	}

	int				GetPosition(int bit)
	{
		int p = 0;
		for (int a = 0; a < 8; a++)
		{
			if (IM(a))
			{
				if (bit == p) return a;
				p++;
			}
		}
		return -1;
	}
	bool			operator!=(const GBitArray8 & ba)
	{
		return (tbl != ba.tbl);
	}
	void			Clear() { tbl = 0; };
	GBitArray8 & MI(GBitArray8 & r, GBitArray8 & m)
	{
		Clear();
		for (int a=0;a<8;a++)
		{
			if(m.IM(a)) if(!r.IM(a)) M(a);
		}
		return *this;
	}
	GBitArray8 & U(GBitArray8 & r)
	{
		for (int a=0;a<8;a++) if(r.IM(a)) U(a);
		return *this;
	}

	BYTE			tbl;
};
/***************************************************************************/
#ifdef LINUX
void _i64toa(INT64 val, char * buf, int);
void _ui64toa(INT64 val, char * buf, int);
void _gcvt(float val, int len,char * buf);
#endif

/***************************************************************************/
#ifdef SERVER
void WriteCore(bool force_coredump = false);
#endif

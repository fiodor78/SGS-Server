/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#ifdef LINUX
//funkcje do konwersji Ascii2Wide oraz Wide2Unicode UTF-8
//wersja z zewnetrznym i wewnetrznym buforem,
int a2w(const char * src,wchar_t * dest,size_t dest_len=512);
//wchar_t * A2W(const char * src);
int w2u(const wchar_t * src,char * dest,size_t dest_len=512);
//char * W2U(const wchar_t * src);
#else
int a2w(const char * src, wchar_t * dest, size_t dest_len);
int w2u(const wchar_t * src, char * dest, size_t dest_len);
#endif
/***************************************************************************/
//klasa zwracajaca wynik konwersji
class W2Us : boost::noncopyable
{
	char buf[512];
public:
	W2Us(const wstring & src)
	{
		w2u(src.c_str(), buf, 512);
	}
	W2Us(const wchar_t * src)
	{
		w2u(src, buf, 512);
	}
	operator const char*()
	{
		return buf;
	}
	const char * operator()()
	{
		return buf;
	}
};
/***************************************************************************/
//klasa zwracajaca wynik konwersji
class A2Ws : boost::noncopyable
{
	wchar_t buf[512];
public:
	A2Ws(const char * src)
	{
		a2w(src, buf, 512);
	}
	A2Ws(const std::string & src)
	{
		a2w(src.c_str(), buf, 512);
	}
	const wchar_t * operator()() const { return buf; }
	operator const wchar_t*() const { return buf; }
	operator wstring() const { return wstring(buf); }
};
/***************************************************************************/
class S2I: boost::noncopyable
{
public:
	S2I(const char * val)
	{
		value=0;
		value=atoi(val);
	}
	S2I(const string val)
	{
		value=0;
		value=atoi(val.c_str());
	}
	int operator()(){return value;};
	int value;
};
/***************************************************************************/
class I2S: boost::noncopyable
{
public:
	I2S(const int val)
	{
		sprintf(buf,"%d",val);
	}
	char * operator()(){return buf;};
	char buf[16];
};
/***************************************************************************/
class I2SW: boost::noncopyable
{
public:
	I2SW(const int val)
	{
		swprintf(buf,15,L"%d",val);
	}
	wchar_t * operator()(){return buf;};
	wchar_t buf[16];
};
/***************************************************************************/
class URLEncode: boost::noncopyable
{
	char buf[8192];
public:
	URLEncode(const char * src)
	{
		char * dst;
		for (dst = buf; dst < buf + sizeof(buf) - 5 && *src != 0; src++)
		{
			if (*src >= 0x30 && *src < 0x7f)
			{
				*dst++ = *src;
			}
			else
			{
				sprintf(dst, "%%%02X", (unsigned char)*src);
				dst += 3;
			}
		}
		*dst = 0;
	}
	operator const char*()
	{
		return buf;
	}
	const char * operator()()
	{
		return buf;
	}
};
/***************************************************************************/
class URLDecode: boost::noncopyable
{
	char buf[8192];
public:
	URLDecode(const char * src)
	{
		char * dst;
		for (dst = buf; dst < buf + sizeof(buf) - 5 && *src != 0; )
		{
			if (*src != '%')
			{
				*dst++ = *src++;
				continue;
			}

			if (src[1] == 0 || src[2] == 0)
			{
				break;
			}
			int value;
			value = (src[1] >= '0' && src[1] <= '9') ? (src[1] - '0') : (src[1] >= 'A' && src[1] <= 'F') ? (src[1] - 'A' + 10) : 0;
			value <<= 4;
			value += (src[2] >= '0' && src[2] <= '9') ? (src[2] - '0') : (src[2] >= 'A' && src[2] <= 'F') ? (src[2] - 'A' + 10) : 0;
			*dst++ = (char)value;
			src += 3;
		}
		*dst = 0;
	}
	operator const char*()
	{
		return buf;
	}
	const char * operator()()
	{
		return buf;
	}
};
/***************************************************************************/
#undef max
class ToTime: boost::noncopyable
{
public:
	ToTime(const boost::posix_time::ptime t)
	{
		strcpy(buf,"UNDEFINED");
		try{
		boost::gregorian::date today = t.date();
		int y=today.year();
		int m=today.month();
		int d=today.day();
		boost::posix_time::time_duration time=t.time_of_day();
		int hour=time.hours();
		int min=time.minutes();
		int sec=time.seconds();
		sprintf(buf,"%04d-%02d-%02d %02d:%02d:%02d",y,m,d,hour,min,sec);
		}
		catch(...){};
	};
	ToTime(DWORD64 t_p)
	{
		boost::posix_time::ptime t;
		t=boost::posix_time::from_time_t(t_p);

		strcpy(buf,"UNDEFINED");
		try{
			boost::gregorian::date today = t.date();
			int y=today.year();
			int m=today.month();
			int d=today.day();
			boost::posix_time::time_duration time=t.time_of_day();
			int hour=time.hours();
			int min=time.minutes();
			int sec=time.seconds();
			sprintf(buf,"%04d-%02d-%02d %02d:%02d:%02d",y,m,d,hour,min,sec);
		}
		catch(...){};
	};
	const char * operator()() const {return buf;}
	operator const char*() const {return buf;}
	char buf[32];
};
class ToTimeHour: boost::noncopyable
{
public:
	ToTimeHour(const boost::posix_time::time_duration time)
	{
		strcpy(buf,"UNDEFINED");
		try{
			int hour=time.hours();
			int min=time.minutes();
			int sec=time.seconds();
			if(hour)
				sprintf(buf,"%02d:%02d:%02d",hour,min,sec);
			else
				sprintf(buf,"%02d:%02d",min,sec);
		}
		catch(...){};
	};
	ToTimeHour(const DWORD64 time)
	{
		strcpy(buf,"UNDEFINED");
		DWORD64 seconds=time/1000;
		int hour=(int)(seconds/3600);
		int min=(int)((seconds%3600)/60);
		int sec=(int)((seconds%60));
		if(hour)
			sprintf(buf,"%02d:%02d:%02d",hour,min,sec);
		else
			sprintf(buf,"%02d:%02d",min,sec);
	};
	const char * operator()() const {return buf;}
	operator const char*() const {return buf;}
	char buf[32];
};
class ToTimeSec: boost::noncopyable
{
public:
	ToTimeSec(const boost::posix_time::time_duration time)
	{
		strcpy(buf,"0");
		try{
		int hour=time.hours();
		int min=time.minutes();
		int sec=time.seconds();
		sprintf(buf,"%d",hour*60*60+min*60+sec);
		}
		catch(...){};
	};
	const char * operator()() const {return buf;}
	operator const char*() const {return buf;}
	char buf[32];
};
class ToTimeMSec: boost::noncopyable
{
public:
	ToTimeMSec(const DWORD64 time)
	{
		strcpy(buf,"");
		DWORD64 seconds=time/1000;
		int hour=(int)(seconds/3600);
		int min=(int)((seconds%3600)/60);
		int sec=(int)((seconds%60));
		int ms=(int)(time%1000);
		ms/=10;
		if(hour)
			sprintf(buf,"%02d:%02d:%02d.%02d",hour,min,sec,ms);
		else
			sprintf(buf,"%02d:%02d.%02d",min,sec,ms);
	};
	const char * operator()() const {return buf;}
	operator const char*() const {return buf;}
	char buf[32];
};
/***************************************************************************/
class ToMoney: boost::noncopyable
{
public:
	ToMoney(INT64 val_,const char *currency_)
	{
		val=val_;
		strncpy(currency,currency_,7);
	}
	char * operator()()
	{
		buf[0]=0;
		bool start=true;
		int step=1;
		INT64 t=val/100;
		while(t/1000) 
		{
			t/=1000;
			step++;
		}

		if(val)
		{
			INT64 v=val/100;
			while(v || step)
			{
				INT64 m=1;
				INT64 t=v;
				int a;
				INT64 s=1;
				for(a=1;a<step;a++)
				{
					s=s*1000;
				}
				t/=s;
				m=s*t;
				if(!start)
				{
#ifdef LINUX
					sprintf(&buf[strlen(buf)],"%03lld",t);
#else
					sprintf(&buf[strlen(buf)],"%03I64d",t);
#endif
				}
				else
				{
					if(currency[0]=='$')
					{
						sprintf(&buf[strlen(buf)],"%s",currency);
					}
#ifdef LINUX
					sprintf(&buf[strlen(buf)],"%lld",t);
#else
					sprintf(&buf[strlen(buf)],"%I64d",t);
#endif
				}
				if(v>1000)
				{
					strcat(buf,",");
				}
				v-=m;
				step--;
				start=false;
			}
			v=val%100;
			if(strlen(buf)==0) sprintf(buf,"0");
#ifdef LINUX
			sprintf(&buf[strlen(buf)],".%02lld",val%100);
#else
			sprintf(&buf[strlen(buf)],".%02I64d",val%100);
#endif
			if(currency[0]!='$')
			{
				sprintf(&buf[strlen(buf)],"%s",currency);
			}
		}
		else
		{
			if(currency[0]=='$')
			{
				sprintf(&buf[strlen(buf)],"%s",currency);
			}
			sprintf(&buf[strlen(buf)],"0.00");
			if(currency[0]!='$')
			{
				sprintf(&buf[strlen(buf)],"%s",currency);
			}
		}
		return buf;
	}
	INT64 val;
	char currency[8];
	char buf[32];
};
/***************************************************************************/
class ToGameChips: boost::noncopyable
{
public:
	ToGameChips(INT64 val_,bool base_correct_=false,const char* sep_=NULL)
	{
		val=val_;
		base_correct=base_correct_;
		sep[0]=sep_ ? sep_[0] : L',';
		sep[1]=0;
	}
	char * operator()()
	{
		buf[0]=0;
		bool start=true;
		int step=1;
		INT64 base=1;
		if(base_correct)
		{
			if(val%1000000==0) base=1000000;
			else
				if(val%1000==0) base=1000;
		}
		INT64 val_correct=val/base;
		INT64 t=val_correct;
		while(t/1000) 
		{
			t/=1000;
			step++;
		}

		if(val_correct)
		{
			INT64 v=val_correct;
			while(v || step)
			{
				INT64 m=1;
				INT64 t=v;
				int a;
				INT64 s=1;
				for(a=1;a<step;a++)
				{
					s=s*1000;
				}
				t/=s;
				m=s*t;
				if(!start)
#ifdef LINUX
					sprintf(&buf[strlen(buf)],"%03lld",t);
#else
					sprintf(&buf[strlen(buf)],"%03I64d",t);
#endif
				else
#ifdef LINUX
					sprintf(&buf[strlen(buf)],"%lld",t);
#else
					sprintf(&buf[strlen(buf)],"%I64d",t);
#endif
				if(m>1000 || step>1) strcat(buf,sep);
				v-=m;
				start=false;
				step--;
			}
			v=val_correct%100;
			if(strlen(buf)==0) sprintf(buf,"0");
		}
		else
		{
			sprintf(buf,"0");
		}
		if(base==1000) strcat(buf,"K");
		if(base==1000000) strcat(buf,"M");
		return buf;
	}
	INT64 val;
	bool base_correct;
	char sep[2];
	char buf[32];
};
/***************************************************************************/
class ToGameChipsW: boost::noncopyable
{
public:
	ToGameChipsW(INT64 val_,bool base_correct_=false,const wchar_t* sep_=NULL)
	{
		val=val_;
		base_correct=base_correct_;
		sep[0]=sep_ ? sep_[0] : L',';
		sep[1]=0;
	}
	wchar_t * operator()()
	{
		buf[0]=0;
		bool start=true;
		int step=1;
		INT64 base=1;
		if(base_correct)
		{
			if(val%1000000==0) base=1000000;
			else
				if(val%1000==0) base=1000;
		}
		INT64 val_correct=val/base;
		INT64 t=val_correct;
		while(t/1000) 
		{
			t/=1000;
			step++;
		}

		if(val_correct)
		{
			INT64 v=val_correct;
			while(v || step)
			{
				INT64 m=1;
				INT64 t=v;
				int a;
				INT64 s=1;
				for(a=1;a<step;a++)
				{
					s=s*1000;
				}
				t/=s;
				m=s*t;
				if(!start)
#ifdef LINUX
					swprintf(&buf[wcslen(buf)],31,L"%03lld",t);
#else
					swprintf(&buf[wcslen(buf)],31,L"%03I64d",t);
#endif
				else
#ifdef LINUX
					swprintf(&buf[wcslen(buf)],31,L"%lld",t);
#else
					swprintf(&buf[wcslen(buf)],31,L"%I64d",t);
#endif
				if(m>1000 || step>1) wcscat(buf,sep);
				v-=m;
				start=false;
				step--;
			}
			v=val_correct%100;
			if(wcslen(buf)==0) swprintf(buf,31,L"0");
		}
		else
		{
			swprintf(buf,31,L"0");
		}
		if(base==1000) wcscat(buf,L"K");
		if(base==1000000) wcscat(buf,L"M");
		return buf;
	}
	INT64 val;
	bool base_correct;
	wchar_t sep[2];
	wchar_t buf[32];
};
/***************************************************************************/
class ToGameChipsApproxW: boost::noncopyable
{
public:
	ToGameChipsApproxW(INT64 val_,const wchar_t* sep_=NULL)
	{
		val=val_;
		sep[0]=sep_ ? sep_[0] : L',';
		sep[1]=0;
	}
	wchar_t * operator()()
	{
		buf[0]=0;
		bool start=true;
		int step=1;
		INT64 base=1;
		if(val>=1000000)
		{
			base=1000000;
		}
		else if(val>=1000)
		{
			base=1000;
		}
		INT64 val_correct=val/base;
		INT64 fraction=(10*val/base)%10;
		INT64 t=val_correct;
		while(t/1000) 
		{
			t/=1000;
			step++;
		}

		if(val_correct)
		{
			INT64 v=val_correct;
			while(v || step)
			{
				INT64 m=1;
				INT64 t=v;
				int a;
				INT64 s=1;
				for(a=1;a<step;a++)
				{
					s=s*1000;
				}
				t/=s;
				m=s*t;
				if(!start)
#ifdef LINUX
					swprintf(&buf[wcslen(buf)],31,L"%03lld",t);
#else
					swprintf(&buf[wcslen(buf)],31,L"%03I64d",t);
#endif
				else
#ifdef LINUX
					swprintf(&buf[wcslen(buf)],31,L"%lld",t);
#else
					swprintf(&buf[wcslen(buf)],31,L"%I64d",t);
#endif
				if(m>1000 || step>1) wcscat(buf,sep);
				v-=m;
				start=false;
				step--;
			}
			v=val_correct%100;
			if(wcslen(buf)==0) swprintf(buf,31,L"0");
		}
		else
		{
			swprintf(buf,31,L"0");
		}
		if(fraction)
		{
#ifdef LINUX
			swprintf(&buf[wcslen(buf)],31,L".%lld",fraction);
#else
			swprintf(&buf[wcslen(buf)],31,L".%I64d",fraction);
#endif
		}
		if(base==1000) wcscat(buf,L"K");
		if(base==1000000) wcscat(buf,L"M");
		return buf;
	}
	INT64 val;
	wchar_t sep[2];
	wchar_t buf[32];
};
/***************************************************************************/
















class ToTimeW: boost::noncopyable
{
public:
	ToTimeW(const boost::posix_time::ptime t)
	{
		wcscpy(buf,L"UNDEFINED");
		try{
			boost::gregorian::date today = t.date();
			int y=today.year();
			int m=today.month();
			int d=today.day();
			boost::posix_time::time_duration time=t.time_of_day();
			int hour=time.hours();
			int min=time.minutes();
			int sec=time.seconds();
			swprintf(buf,31,L"%04d-%02d-%02d %02d:%02d:%02d",y,m,d,hour,min,sec);
		}
		catch(...){};
	};
	ToTimeW(DWORD64 t_p)
	{
		boost::posix_time::ptime t;
		t=boost::posix_time::from_time_t(t_p);

		wcscpy(buf,L"UNDEFINED");
		try{
			boost::gregorian::date today = t.date();
			int y=today.year();
			int m=today.month();
			int d=today.day();
			boost::posix_time::time_duration time=t.time_of_day();
			int hour=time.hours();
			int min=time.minutes();
			int sec=time.seconds();
			swprintf(buf,31,L"%04d-%02d-%02d %02d:%02d:%02d",y,m,d,hour,min,sec);
		}
		catch(...){};
	};
	const wchar_t * operator()() const {return buf;}
	operator const wchar_t*() const {return buf;}
	wchar_t buf[32];
};

class ToTimeHourW: boost::noncopyable
{
public:
	ToTimeHourW(const boost::posix_time::time_duration time)
	{
		wcscpy(buf,L"UNDEFINED");
		try{
			int hour=time.hours();
			int min=time.minutes();
			int sec=time.seconds();
			if(hour)
				swprintf(buf,31,L"%02d:%02d:%02d",hour,min,sec);
			else
				swprintf(buf,31,L"%02d:%02d",min,sec);
		}
		catch(...){};
	};
	ToTimeHourW(const DWORD64 time)
	{
		wcscpy(buf,L"UNDEFINED");
		DWORD64 seconds=time/1000;
		int hour=(int)(seconds/3600);
		int min=(int)((seconds%3600)/60);
		int sec=(int)((seconds%60));
		if(hour)
			swprintf(buf,31,L"%02d:%02d:%02d",hour,min,sec);
		else
			swprintf(buf,31,L"%02d:%02d",min,sec);
	};
	const wchar_t * operator()() const {return buf;}
	operator const wchar_t*() const {return buf;}
	wchar_t buf[32];
};

class ToTimeMinutesW: boost::noncopyable
{
public:
	ToTimeMinutesW(const boost::posix_time::time_duration time)
	{
		wcscpy(buf,L"UNDEFINED");
		try{
			int hour=time.hours();
			int min=time.minutes();
			swprintf(buf,31,L"%02d:%02d",hour,min);
		}
		catch(...){};
	};
	ToTimeMinutesW(const DWORD64 time)
	{
		wcscpy(buf,L"UNDEFINED");
		DWORD64 seconds=time/1000;
		int hour=(int)(seconds/3600);
		int min=(int)((seconds%3600)/60);
		swprintf(buf,31,L"%02d:%02d",hour,min);
	};
	const wchar_t * operator()() const {return buf;}
	operator const wchar_t*() const {return buf;}
	wchar_t buf[32];
};

class ToTimeSecW: boost::noncopyable
{
public:
	ToTimeSecW(const boost::posix_time::time_duration time)
	{
		wcscpy(buf,L"0");
		try{
			int hour=time.hours();
			int min=time.minutes();
			int sec=time.seconds();
			swprintf(buf,31,L"%d",hour*60*60+min*60+sec);
		}
		catch(...){};
	};
	const wchar_t * operator()() const {return buf;}
	operator const wchar_t*() const {return buf;}
	wchar_t buf[32];
};
class ToTimeMSecW: boost::noncopyable
{
public:
	ToTimeMSecW(const DWORD64 time)
	{
		wcscpy(buf,L"");
		DWORD64 seconds=time/1000;
		int hour=(int)(seconds/3600);
		int min=(int)((seconds%3600)/60);
		int sec=(int)((seconds%60));
		int ms=(int)(time%1000);
		ms/=10;
		if(hour)
			swprintf(buf,31,L"%02d:%02d:%02d.%02d",hour,min,sec,ms);
		else
			swprintf(buf,31,L"%02d:%02d.%02d",min,sec,ms);
	};
	const wchar_t * operator()() const {return buf;}
	operator const wchar_t*() const {return buf;}
	wchar_t buf[32];
};
class ToTimeMSecShortW: boost::noncopyable
{
public:
	ToTimeMSecShortW(const DWORD64 time)
	{
		wcscpy(buf,L"");
		DWORD64 seconds=time/1000;
		int sec=(int)((seconds%60));
		int ms=(int)(time%1000);
		swprintf(buf,31,L"%d.%02d",sec,ms);
	};
	const wchar_t * operator()() const {return buf;}
	operator const wchar_t*() const {return buf;}
	wchar_t buf[32];
};
/***************************************************************************/
class ToMoneyW: boost::noncopyable
{
public:
	ToMoneyW(INT64 val_,const wchar_t *currency_)
	{
		val=val_;
		wcsncpy(currency,currency_,7);
	}
	wchar_t * operator()()
	{
		const wchar_t usd[]={L'$',0};
		const wchar_t eur[]={0x20AC,0};

		buf[0]=0;
		bool start=true;
		int step=1;
		INT64 t=val/100;
		while(t/1000) 
		{
			t/=1000;
			step++;
		}

		if(val)
		{
			INT64 v=val/100;
			while(v || step)
			{
				INT64 m=1;
				INT64 t=v;
				int a;
				INT64 s=1;
				for(a=1;a<step;a++)
				{
					s=s*1000;
				}
				t/=s;
				m=s*t;
				if(!start)
				{
#ifdef LINUX
					swprintf(&buf[wcslen(buf)],31,L"%03lld",t);
#else
					swprintf(&buf[wcslen(buf)],31,L"%03I64d",t);
#endif
				}
				else
				{
					if(wcscmp(currency,L"USD")==0)
					{
						swprintf(&buf[wcslen(buf)],31,L"%s",usd);
					}
#ifdef LINUX
					swprintf(&buf[wcslen(buf)],31,L"%lld",t);
#else
					swprintf(&buf[wcslen(buf)],31,L"%I64d",t);
#endif
				}
				if(v>1000) wcscat(buf,L",");
				v-=m;
				step--;
				start=false;
			}
			v=val%100;
			if(wcslen(buf)==0) swprintf(buf,31,L"0");
#ifdef LINUX
			swprintf(&buf[wcslen(buf)],31,L".%02lld",val%100);
#else
			swprintf(&buf[wcslen(buf)],31,L".%02I64d",val%100);
#endif
			if(wcscmp(currency,L"USD")!=0)
			{
				if(wcscmp(currency,L"EUR")==0)
				{
					swprintf(&buf[wcslen(buf)],31,L"%s",eur);
				}
				else
				{
					swprintf(&buf[wcslen(buf)],31,L"%s",currency);
				}
			}
		}
		else
		{
			if(wcscmp(currency,L"USD")==0)
			{
				swprintf(&buf[wcslen(buf)],31,L"%s",usd);
			}
			swprintf(&buf[wcslen(buf)],31,L"0.00");
			if(wcscmp(currency,L"USD")!=0)
			{
				if(wcscmp(currency,L"EUR")==0)
				{
					swprintf(&buf[wcslen(buf)],31,L"%s",eur);
				}
				else
				{
					swprintf(&buf[wcslen(buf)],31,L"%s",currency);
				}
			}
		}
		return buf;
	}
	INT64 val;
	wchar_t currency[8];
	wchar_t buf[32];
};
/***************************************************************************/
/***************************************************************************/
string& ShortString2LowerCase(string& text);
string escape_json_string(const string & input);

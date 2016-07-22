/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "md5.h"
/***************************************************************************/
//klasa zwracajaca wynik konwersji
class MD5: boost::noncopyable
{
public:
	MD5(const BYTE * in,int len)
	{
		md5_hex(in,len);
	}
	MD5(const char * in)
	{
		md5_hex(in);
	}
	char buf[33];
	BYTE hex[16];
	char * operator()(){return buf;};

	void md5(const char *in)
	{
		MD5_CTX context;
		unsigned int len=(unsigned int)strlen(in);
		MD5Init(&context);
		MD5Update(&context,(BYTE*)in,len);
		MD5Final(hex, &context);
	}
	void md5_hex(const char *in)
	{
		md5(in);
		int i=0;
		char *tmp=buf;
		for(;i<16;i++)
		{
			sprintf(tmp,"%02x",hex[i]);
			tmp+=2;
		}
		buf[32]=0;
	}
	void md5(const BYTE *in,int len)
	{
		MD5_CTX context;
		MD5Init(&context);
		MD5Update(&context,(BYTE*)in,len);
		MD5Final(hex, &context);
	}
	void md5_hex(const BYTE *in,int len)
	{
		md5(in,len);
		int i=0;
		char *tmp=buf;
		for(;i<16;i++)
		{
			sprintf(tmp,"%02x",hex[i]);
			tmp+=2;
		}
		buf[32]=0;
	}
};
/***************************************************************************/

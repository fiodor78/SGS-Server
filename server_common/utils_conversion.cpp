/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/


#include "headers_all.h"

#ifdef LINUX
#include "iconv.h"
#include "boost/thread/mutex.hpp"

iconv_t	i_a2w=iconv_open("UCS-4LE","UTF-8");
iconv_t	i_w2u=iconv_open("UTF-8","UCS-4LE");

boost::mutex  mtx_iconv;
#endif

#ifndef LINUX
#include <windows.h>
#include "utils_conversion.h"
#endif

/***************************************************************************/
#ifdef LINUX
int a2w(const char * src,wchar_t * dest,size_t dest_len)
{
	dest_len*=sizeof(wchar_t);
	dest[0]=0;
	size_t len;
	char * srcp=const_cast<char*>(src);
	char * destp=(char*)dest;
	size_t src_size=strlen(src)+1;
	size_t dest_size=dest_len;
	{
		boost::mutex::scoped_lock lock(mtx_iconv);
		iconv(i_a2w,&srcp,&src_size,&destp,&dest_size);
	}
	len=dest_len-dest_size;
	len/=4;
	return (int)len;
}
#else
int a2w(const char * src,wchar_t * dest,size_t dest_len)
{
	*dest = '\0';
	int ret = MultiByteToWideChar(CP_UTF8, 0, src, -1, dest, dest_len);
	return ret;
}
#endif
/***************************************************************************/
#ifdef LINUX
int w2u(const wchar_t * src,char * dest,size_t dest_len)
{
	dest[0]=0;
	size_t src_size=wcslen(src)+1;
	src_size*=4;
	wchar_t * srcwp=const_cast<wchar_t*>(src);
	char * srcp=reinterpret_cast<char*>(srcwp);
	char * destp=dest;
	size_t dest_size=dest_len;
	{
		boost::mutex::scoped_lock lock(mtx_iconv);
		iconv(i_w2u,&srcp,&src_size,&destp,&dest_size);
	}
	size_t len=dest_len-dest_size;
	len-=1;
	if(len<0) len=0;
	return (int)len;
}
#else
int w2u(const wchar_t * src,char * dest,size_t dest_len)
{
	*dest = '\0';
	int ret = WideCharToMultiByte(CP_UTF8, 0, src, -1, dest, dest_len, NULL, NULL);
	return ret;
}
#endif
/***************************************************************************/

/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

/***************************************************************************/
//
//		main header
//
/***************************************************************************/

#ifndef LINUX
#define LINUX
#endif

#ifdef WIN32
#undef LINUX
#endif

// include_libraries.h
#include <bitset>
#include <iostream>
#include <fstream>
#include <string>


#include <vector>
#include <list>
#include <queue>
#include <algorithm>
#include <functional>
#include <strstream>
#include <stack>
#include <set>
#include <map>
#include <string>

#ifdef LINUX
#include "stdarg.h"
#include <fcntl.h>
#endif

#pragma warning (disable: 4512)

using namespace std;

#ifdef LINUX

#include <ext/hash_map>
using namespace __gnu_cxx;
#else
#include <hash_map>
using namespace stdext;
#endif

// types.h
typedef unsigned char				BYTE;

typedef short						SHORT16;
typedef unsigned short				USHORT16;

typedef int							INT32;
typedef unsigned int				UINT32;
typedef unsigned int				DWORD32;


typedef long long int				INT64;
typedef unsigned long long int		UINT64;
typedef unsigned long long int		DWORD64;

#ifdef LINUX
typedef unsigned int				DWORD;
typedef int							SOCKET;
#else
typedef unsigned long				DWORD;
typedef	int							__uint32_t;
typedef long long int				__uint64_t;
#endif

// headers_all.h

#pragma warning (disable: 4267)
#pragma warning (disable: 4100)


#ifndef LINUX
#define ATOI64                  _atoi64
#define ATOI                    atoi
#define ATOF                    (float)atof
#else
#define ATOI64                  atoll
#define ATOI                    atol
#define ATOF                    (float)atof
#endif

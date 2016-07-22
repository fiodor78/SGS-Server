/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

/***************************************/
/***************************************/
//DATA TYPES
/***************************************/
/***************************************/
//#define __int64						!stop!
//#define __UCHAR						!stop!
//#define DWORD							!stop!
//#define SHORT							!stop!
//#define USHORT						!stop!
//#define strcpy						!stop!

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



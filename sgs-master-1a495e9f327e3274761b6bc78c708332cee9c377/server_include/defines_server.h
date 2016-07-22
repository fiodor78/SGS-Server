/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#define SERVER

#ifdef LINUX
//#define USE_SPREAD
#endif

#define _LEAK_DETECTION

#ifdef _LEAK_DETECTION
extern void LeakDetection();
#endif


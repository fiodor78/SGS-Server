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

#pragma warning (disable: 4127)

#include "includes_system.h"
#include "include_libraries.h"
#include "include_ssl.h"

#pragma warning (disable: 4267)
#pragma warning (disable: 4100)

#include "types.h"
#include "enums.h"

#include "templates.h"

#include "define_game.h"
#include "defines_server.h"
#include "defines.h"
#include "includes.h"
#include "externs.h"


#ifdef _DEBUG
#include <assert.h>
#endif




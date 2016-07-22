/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#ifndef LINUX
#undef FD_SETSIZE
#define FD_SETSIZE	1024
#endif


/***************************************/
/***************************************/
//INCLUDES
/***************************************/
/***************************************/

#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "stdarg.h"
#include <time.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include <sys/stat.h>
#include "ctype.h"
#include "math.h"


#ifndef LINUX
#include "crtdbg.h" //----
#include "windows.h"
#include <io.h>
#include <conio.h>
#else
#include <syslog.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#ifndef MACOSX
#include <sys/epoll.h>
#endif
#include <malloc.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <semaphore.h>
#include <pthread.h>
#include <netdb.h>
#include <utmp.h>
#include <grp.h>
#include <pwd.h>
#include <wchar.h>
#endif

#include "mysql.h"




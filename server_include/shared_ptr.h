/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

//wylaczamy mutex'y w pointerach

#undef BOOST_HAS_THREADS
#include <boost/shared_ptr.hpp>
#define BOOST_HAS_THREADS

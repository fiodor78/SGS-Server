/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

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

#pragma warning (disable: 4512)

#include <boost/random.hpp>
#include <boost/bind.hpp>
#include <boost/utility.hpp>
#include <boost/function.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/any.hpp>
#include <boost/thread.hpp>
#include <boost/version.hpp>
#define BOOST_SPIRIT_THREADSAFE
#if BOOST_VERSION >= 103800
	#define BOOST_SPIRIT_USE_OLD_NAMESPACE
	#include <boost/spirit/include/classic.hpp>
	#include <boost/spirit/include/classic_insert_at_actor.hpp>
#else
	#include <boost/spirit.hpp>
	#include <boost/spirit/actor/insert_at_actor.hpp>
#endif
#include <boost/tokenizer.hpp>
#include <boost/regex.hpp>
#include <boost/multi_array.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/functional/hash.hpp>
#include <boost/date_time/date.hpp>
#include <boost/date_time/time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/scoped_array.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/indexed_by.hpp>

#include <google/dense_hash_set>
		
#ifndef SERVICES
#include <curl/curl.h>
#endif

#undef loop

using namespace boost;
using namespace std;
using namespace google;

#ifdef LINUX

#include <ext/hash_map>
using namespace __gnu_cxx;
#else
#include <hash_map>
using namespace stdext;
#endif

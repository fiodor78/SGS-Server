/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include <sgs_api.h>

#include "gconsole.h"
#include "gserver_base.h"
#include "gserver.h"
#include "gtickets.h"
#include "gtickets_client.h"
#include "gtruerng.h"
#include "glogic.h"
#include "glogic_engine.h"
#include "gserver_local.h"
#include "gmsg_internal.h"
#include "gmsg_game.h"
#include "utils_md5.h"

struct eq_INT32
{
    bool operator()(const INT32 s1, const INT32 s2) const
    {
        return (s1==s2);
    }
};
struct hash_INT32
{
    size_t operator()(const INT32 x) const { return x;} ;
};

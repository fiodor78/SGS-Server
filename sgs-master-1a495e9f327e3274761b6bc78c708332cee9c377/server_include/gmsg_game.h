#define GNET(msgid,format,desc)		msgid,
enum EMsgGameEnum{
IGM_MSG_EMPTY,
#include "gmsg_game.tbl"
IGM_MSG_LAST,
};
#undef GNET



#define GSUBNET(msgid,format,desc)		msgid,
enum EMsgGameSumEnum{
IGS_MSG_EMPTY,
#include "gmsg_game_sub.tbl"
IGS_MSG_LAST,
};
#undef GSUBNET




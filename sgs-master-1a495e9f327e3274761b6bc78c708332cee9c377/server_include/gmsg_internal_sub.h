#define GSUBNET(msgid,format,desc)		msgid,
enum EMsgInternalSubEnum{
IGSI_MSG_EMPTY,
#include "tbl/gmsg_internal_sub.tbl"
IGSI_MSG_LAST,
};
#undef GSUBNET

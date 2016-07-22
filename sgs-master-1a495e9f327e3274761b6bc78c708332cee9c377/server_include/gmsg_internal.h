#define GNET(msgid,format,desc)		msgid,
enum EMsgInternalEnum{
IGMI_MSG_EMPTY,
#include "tbl/gmsg_internal.tbl"
IGMI_MSG_LAST,
};
#undef GNET



#define IGMI_ACHIEVEMENT_EVENT				IGMI_SOCIAL_EVENT

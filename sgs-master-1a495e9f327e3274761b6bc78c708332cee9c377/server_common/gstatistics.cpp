/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"

/***************************************************************************/
#define GSTAT(group,type,val,txt)		{group,type,val,#val},
//#define GSTAT(group,type,val,info,description)		{group,type,val,#val,info,description},
SStatisticsDesc stats_default[]={
#include "tbl/gstatistics.tbl"
};

int stats_count=(sizeof(stats_default)/sizeof(stats_default[0]));

#undef GSTAT
/***************************************************************************/
GStatistics::GStatistics()
{
}
/***************************************************************************/
GStatistics::~GStatistics()
{
}
/***************************************************************************/
void GStatistics::Init()
{
	int a;
	for (a=0;a<stats_count;a++)
	{
		stats.push_back(SStatistics(stats_default[a].val));
	}
}
/***************************************************************************/
void GStatistics::Stream(strstream &s,vector<int> & commands)
{
	int a;

	vector<int>::iterator pos;

	//size_t max_info=0;
	//size_t max_description=0;
	size_t max_val_info=0;
	for (a=0;a<stats_count;a++)
	{
		//size_t li=strlen(stats_default[a].info);
		//size_t ld=strlen(stats_default[a].descrpiption);
		size_t lv=strlen(stats_default[a].val_info);
		//max_info=max(max_info,li);
		//max_description=max(max_description,ld);
		max_val_info=max(max_val_info,lv);
	}
	char buf_format[128];
	char buf[1024];
	for (a=0;a<stats_count;a++)
	{

		bool show=false;
		bool show_type=false;
		bool show_group=false;
		bool is_type=false;
		bool is_group=false;
		bool all=true;

		int b;
		for (b=SWARNING;b<SSOCKET;b++)
		{
			pos=find_if(commands.begin(),commands.end(),isCommand(b));
			if(pos!=commands.end()) if(stats_default[a].type==b) show_type=true;
			if(pos!=commands.end()) {all=false;is_type=true;};
		}
		for (b=SSOCKET;b<SALL;b++)
		{
			pos=find_if(commands.begin(),commands.end(),isCommand(b));
			if(pos!=commands.end()) if(stats_default[a].group==b) show_group=true;
			if(pos!=commands.end()) {all=false;is_group=true;};
		}
		if(is_type && is_group) if(show_type && show_group) show=true;
		if(is_type && !is_group) show=show_type;
		if(!is_type && is_group) show=show_group;

		pos=find_if(commands.begin(),commands.end(),isCommand(SALL));
		if(pos!=commands.end()) show=true;

		if(all) show=true;

		pos=find_if(commands.begin(),commands.end(),isCommand(SUSED));
		if(pos!=commands.end()) if(stats[a].count==0) show=false;

		if(show)
		{
			size_t l=strlen(stats_default[a].val_info);
			sprintf(buf_format,"%%s %%-%ds: %%lld",(int)(max_val_info+5-l));
			sprintf(buf,buf_format,stats_default[a].val_info,"",stats[a].count);
			s<<buf<<lend;
		}
	}
}
/***************************************************************************/
void GStatistics::Destroy()
{
	int a;
	//size_t max_info=0;
	//size_t max_description=0;
	size_t max_val_info=0;
	for (a=0;a<stats_count;a++)
	{
		//size_t li=strlen(stats_default[a].info);
		//size_t ld=strlen(stats_default[a].descrpiption);
		size_t lv=strlen(stats_default[a].val_info);
		//max_info=max(max_info,li);
		//max_description=max(max_description,ld);
		max_val_info=max(max_val_info,lv);
	}
	char buf_format[128];
	char buf[1024];

	GRAPORT("Server statistics:");
	for (a=0;a<stats_count;a++)
	{
		size_t l=strlen(stats_default[a].val_info);
		sprintf(buf_format,"%%s %%-%ds: %%d",(int)(max_val_info+5-l));
		sprintf(buf,buf_format,stats_default[a].val_info,"",stats[a].count);
		GRAPORT(buf);
	}
	GRAPORT("Statistics end");
	GRAPORT_LINE
}
/***************************************************************************/
void GStatistics::Raport(int id)
{
	stats[id].count++;
}
/***************************************************************************/
void GStatistics::Raport(int id,INT64 val)
{
	stats[id].count+=val;
}
/***************************************************************************/




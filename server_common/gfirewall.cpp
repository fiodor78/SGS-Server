/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"

/***************************************************************************/
GFireWall::GFireWall(GRaportInterface & raport_interface_):wall(1024),raport_interface(raport_interface_)
{
	in_addr empty,del;
	INT32 a;
	a=0;
	memcpy(&empty,&a,4);
	a=1;
	memcpy(&del,&a,4);
	for (a=0;a<5;a++)
	{
		minute_1[a].set_empty_key(empty);
		minute_1[a].set_deleted_key(del);
	}
	for (a=0;a<12;a++)
	{
		minutes_5[a].set_empty_key(empty);
		minutes_5[a].set_deleted_key(del);
	}

	last_minute_1=0;
	last_minutes_5=0;

	wall.set_empty_key(empty);
	wall.set_deleted_key(del);

	wall.resize(0);
}
/***************************************************************************/
int GFireWall::GetMinute1Back(int back)
{
	int ret=last_minute_1;
	while(back)
	{
		ret--;
		if(ret==-1) ret=4;
		back--;
	}
	return ret;
}
/***************************************************************************/
int GFireWall::GetMinutes5Back(int back)
{
	int ret=last_minutes_5;
	while(back)
	{
		ret--;
		if(ret==-1) ret=11;
		back--;
	}
	return ret;
}
/***************************************************************************/
void GFireWall::UpdateTime(GClock &)
{
	last_minute_1++;
	if(last_minute_1==5) last_minute_1=0;
	minute_1[last_minute_1].clear();
	minute_1[last_minute_1].resize(0);

	if(last_minute_1==0) 
	{
		last_minutes_5++;
		if(last_minutes_5==12) last_minutes_5=0;
		minutes_5[last_minutes_5].clear();
		minutes_5[last_minutes_5].resize(0);
	}
}
/***************************************************************************/
bool GFireWall::Test(const in_addr addr)
{
	INT32 a;
	a=0;
	memcpy(&a,&addr,4);
	if(a==0 || a==1) 
	{
		SRAP(INFO_FIREWALL_BAD_ADDR);
		RAPORT(GSTR(INFO_FIREWALL_BAD_ADDR));
		return false;
	}

	if(global_serverconfig->firewall.use)
	{
		dense_hash_set<in_addr, hash_addr, eq_addr>::iterator it;
		it=wall.find(addr);
		if(it!=wall.end()) return false;
		return true;
	}

	if(global_serverconfig->firewall.use_limits)
	{
		dense_hash_map<const in_addr, int, hash_addr, eq_addr>::iterator it;

		it=minute_1[last_minute_1].find(addr);
		if(it!=minute_1[last_minute_1].end())
		{
			minute_1[last_minute_1][addr]++;
		}
		else
		{
			minute_1[last_minute_1].insert(pair<in_addr,int>(addr,1));
		}

		it=minutes_5[last_minutes_5].find(addr);
		if(it!=minutes_5[last_minutes_5].end())
		{
			minutes_5[last_minutes_5][addr]++;
		}
		else
		{
			minutes_5[last_minutes_5].insert(pair<in_addr,int>(addr,1));
		}


		u_int hit_minute_1=0;
		u_int hit_minutes_5=0;
		u_int hit_minutes_15=0;
		u_int hit_minutes_60=0;

		hit_minute_1=minute_1[GetMinute1Back(0)][addr];
		int a;
		for (a=0;a<5;a++) hit_minutes_5+=minute_1[GetMinute1Back(a)][addr];
		for (a=0;a<3;a++) hit_minutes_15+=minutes_5[GetMinutes5Back(a)][addr];
		for (a=0;a<12;a++) hit_minutes_60+=minutes_5[GetMinutes5Back(a)][addr];

		if(global_serverconfig->firewall.limit1)
		if(hit_minute_1>global_serverconfig->firewall.limit1) 
		{
			SRAP(INFO_FIREWALL_1);
			RAPORT("%s IP:%s, TRY: %d",GSTR(INFO_FIREWALL_1),inet_ntoa(addr),hit_minute_1);
			return false;
		}
		if(global_serverconfig->firewall.limit5)
		if(hit_minutes_5>global_serverconfig->firewall.limit5) 
		{
			SRAP(INFO_FIREWALL_5);
			RAPORT("%s IP:%s, TRY: %d",GSTR(INFO_FIREWALL_5),inet_ntoa(addr),hit_minutes_5);
			return false;
		}
		if(global_serverconfig->firewall.limit15)
		if(hit_minutes_15>global_serverconfig->firewall.limit15) 
		{
			SRAP(INFO_FIREWALL_15);
			RAPORT("%s IP:%s, TRY: %d",GSTR(INFO_FIREWALL_15),inet_ntoa(addr),hit_minutes_15);
			return false;
		}
		if(global_serverconfig->firewall.limit60)
		if(hit_minutes_60>global_serverconfig->firewall.limit60) 
		{
			SRAP(INFO_FIREWALL_60);
			RAPORT("%s IP:%s, TRY: %d",GSTR(INFO_FIREWALL_60),inet_ntoa(addr),hit_minutes_60);
			return false;
		}
	}
	return true;
}
/***************************************************************************/
void GFireWall::Add(const in_addr addr)
{
	wall.insert(addr);
}
/***************************************************************************/
void GFireWall::Delete(const in_addr addr)
{
	wall.erase(addr);
	wall.resize(0);
}
/***************************************************************************/
bool GFireWall::Exist(const in_addr addr)
{
	dense_hash_set< in_addr, hash_addr, eq_addr>::iterator it;
	it=wall.find(addr);
	if(it!=wall.end()) return true;
	return false;
}
/***************************************************************************/
int GFireWall::Size()
{
	return (int)wall.size();
}
/***************************************************************************/
void GFireWall::Stream(strstream &s)
{
	if(wall.begin()==wall.end())
	{
		s<<"empty\r\n";
	}
	for (dense_hash_set< in_addr, hash_addr, eq_addr>::iterator it = wall.begin();it != wall.end(); ++it) 
	{
		in_addr in;
		in=*it;
		char * str=inet_ntoa(in);
		s<<str<<lend;
	}
}
/***************************************************************************/
void GFireWall::Limits(strstream &s)
{
	s<<"limit1  "<<global_serverconfig->firewall.limit1<<lend;
	s<<"limit5  "<<global_serverconfig->firewall.limit5<<lend;
	s<<"limit15 "<<global_serverconfig->firewall.limit15<<lend;
	s<<"limit60 "<<global_serverconfig->firewall.limit60<<lend;
}
/***************************************************************************/
void GFireWall::Reset(ECmdFireWall cmd)
{
	switch(cmd)
	{
	case CMD_FIREWALL_RESET:
		wall.clear();
		wall.resize(0);
	case CMD_FIREWALL_RESET_DYNAMIC:
		{
			int a;
			for (a=0;a<5;a++)
			{
				minute_1[a].clear();
				minute_1[a].resize(0);
			}
			for (a=0;a<12;a++)
			{
				minutes_5[a].clear();
				minutes_5[a].resize(0);
			}
		}
		break;
	case CMD_FIREWALL_RESET_STATIC:
		wall.clear();
		wall.resize(0);
		break;
	}
}
/***************************************************************************/
void GFireWall::SetLimit(int id, int limit)
{
	switch(id)
	{
	case 1:		global_serverconfig->firewall.limit1=limit;break;
	case 5:		global_serverconfig->firewall.limit5=limit;break;
	case 15:	global_serverconfig->firewall.limit15=limit;break;
	case 60:	global_serverconfig->firewall.limit60=limit;break;
	}
}
/***************************************************************************/
void GFireWall::GetFromSQL()
{

}
/***************************************************************************/


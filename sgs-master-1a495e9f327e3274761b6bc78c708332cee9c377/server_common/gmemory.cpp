/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"

/***************************************************************************/
GMemory::GMemory()
{
	mgr=NULL;
	Reset();
}
/***************************************************************************/
void GMemory::Init(GMemoryManager * mgr_p)
{
	mgr=mgr_p;
}
/***************************************************************************/
void GMemory::Reset()
{
	md.Reset();
	used=0;
	parsed=0;
}
/***************************************************************************/
bool GMemory::Allocate(DWORD32 size)
{
	assert(mgr!=NULL);
	if(md.size==size) return md.size!=0;
	if(md.size) mgr->Deallocate(md);

	Reset();
	md=mgr->Allocate(size);
	return md.size!=0;
}
/***************************************************************************/
DWORD32 GMemory::GetAllocated()
{
	return md.size;
}
/***************************************************************************/
void GMemory::Deallocate()
{
	if(md.size==0) return;
	if(mgr) mgr->Deallocate(md);
	used=0;
	parsed=0;
}
/***************************************************************************/
bool GMemory::ReallocateTo(DWORD32 size)
{
	assert(mgr!=NULL);
	if(md.size>=size) return true;
	return mgr->ReallocateTo(md,size);
}
/***************************************************************************/
bool GMemory::ReallocateToFit(DWORD32 size)
{
	size+=used;
	return ReallocateTo(size);
}
/***************************************************************************/
bool GMemory::Reallocate(DWORD32 mul)
{
	assert(mgr!=NULL);
	return mgr->Reallocate(md,mul);
}
/***************************************************************************/
void GMemory::AddString(char * str)
{
	size_t len=strlen(str);
	if (len > Free())
	{
		if (!ReallocateToFit(len))
		{
			return;
		}
	}
	strncpy(End(),str,len);
	IncreaseUsed((DWORD32)len);
}
/***************************************************************************/
void GMemory::AddString(string & str)
{
	size_t len=str.length();
	if (len > Free())
	{
		if (!ReallocateToFit(len))
		{
			return;
		}
	}
	strncpy(End(),str.c_str(),len);
	IncreaseUsed((DWORD32)len);
}
/***************************************************************************/
char * GMemory::Start()
{
	assert(md.ptr!=NULL);
	return md.ptr;
};
/***************************************************************************/
char * GMemory::End()
{
	assert(md.ptr!=NULL);
	return &md.ptr[used];
};
/***************************************************************************/
char * GMemory::Parse()
{
	assert(md.ptr!=NULL);
	return &md.ptr[parsed];
};
/***************************************************************************/
DWORD32	GMemory::Size()
{
	assert(md.ptr!=NULL);
	return md.size;
};
/***************************************************************************/
DWORD32	GMemory::Free()
{
	assert(md.size>=used);
	return md.size-used;
};
/***************************************************************************/
void GMemory::IncreaseUsed(DWORD64 size)
{
	used+=(DWORD32)size;
	assert(used<=md.size);
};
/***************************************************************************/
void GMemory::IncreaseParsed(DWORD32 size)
{
	parsed+=size;
	assert(parsed<=md.size);
};
/***************************************************************************/
DWORD32 GMemory::ToParse()
{
	assert(used>=parsed);
	return used-parsed;
};
/***************************************************************************/



















/***************************************************************************/
GMemoryManager::GMemoryManager()
{
	Init();
};
/***************************************************************************/
GMemoryManager::~GMemoryManager()
{
	//Destroy();
};
/***************************************************************************/
void GMemoryManager::Init()
{
	int a;
	for (a=0;a<KCOUNT;a++)
	{
		mem_cached[a].reserve(MEMORY_RESERVE);
		mem_cache_count[a]=0;;
		mem_used[a]=0;
	}
	local_stats.Reset();
	cache_memory=true;
}
/***************************************************************************/
void GMemoryManager::Destroy()
{
	Reset();
}
/***************************************************************************/
void GMemoryManager::ResetMD(SMemDesc & md)
{
	local_stats.real_deallocation++;
	local_stats.allocated_memory-=md.size;
	if(md.size>K1024) local_stats.big_size_allocated_memory-=md.size;
	DeleteTbl(md.ptr);
	md.Reset();
}
/***************************************************************************/
void GMemoryManager::Reset()
{
	int a;
	for (a=0;a<KCOUNT;a++)
	{
		for_each (mem_cached[a].begin(), mem_cached[a].end(),boost::bind(&GMemoryManager::ResetMD,this,_1));
		mem_cache_count[a]=0;
		mem_used[a]=0;
	}
}
/***************************************************************************/
SMemDesc GMemoryManager::Allocate(DWORD32 size)
{
	local_stats.allocation++;
	int pos=0;
	DWORD32 tmp=1024;
	while(size>tmp)
	{
		tmp=tmp<<1;
		pos++;
	}
	SMemDesc md;

	DWORD32 real_alloc=0;
	if(pos>=KCOUNT) 
	{
		local_stats.real_allocation++;
		real_alloc=size;
		//sprawdzamy czy udala sie alokacja
		try{
			md.ptr=new char[real_alloc];
			md.size=real_alloc;
			local_stats.allocated_memory+=md.size;
			local_stats.big_size_allocated_memory+=md.size;
		}
		//osluga wyjatkow
		catch(const std::bad_alloc &)
		{
			md.Reset();
		};
	}
	else
	{
		if(mem_cached[pos].size())
		{
			local_stats.cache_allocation++;
			mem_used[pos]++;
			mem_cache_count[pos]++;
			md=mem_cached[pos].back();
			mem_cached[pos].pop_back();

		}
		else
		{
			real_alloc=1024<<pos;
			//sprawdzamy czy udala sie alokacja
			try{						
				md.ptr=new char[real_alloc];
				md.size=real_alloc;
				local_stats.real_allocation++;
				local_stats.allocated_memory+=md.size;
				mem_used[pos]++;
			}
			//obsluga wyjatkow
			catch(const std::bad_alloc &)
			{
				md.Reset();
			}
		}
	}
	return md;
}
/***************************************************************************/
bool GMemoryManager::Deallocate(SMemDesc & md)
{
	if(md.size==0) return false;
	local_stats.deallocation++;
	int pos=0;
	DWORD32 tmp=1024;
	while(md.size>tmp)
	{
		tmp=tmp<<1;
		pos++;
	}
	if(pos>=KCOUNT) 
	{
		local_stats.real_deallocation++;
		local_stats.allocated_memory-=md.size;
		local_stats.big_size_allocated_memory-=md.size;
		DeleteTbl(md.ptr);
		md.Reset();
		return true;
	}
	else
	{
		if(CanCache(pos))
		{
			local_stats.cache_deallocation++;
			mem_used[pos]--;
			mem_cached[pos].push_back(md);
			md.Reset();
			return false;
		}
		else
		{
			local_stats.real_deallocation++;
			mem_used[pos]--;
			local_stats.allocated_memory-=md.size;
			DeleteTbl(md.ptr);
			md.Reset();
			return true;
		}
	}
}
/***************************************************************************/
bool GMemoryManager::ReallocateTo(SMemDesc & md,DWORD32 size)
{
	if(md.size>=size) return true;
	local_stats.reallocation++;
	SMemDesc md_tmp=Allocate(size);
	if(md_tmp.size==0) return false;		//nie powiodla sie alokacja
	if(md.ptr!=NULL) memcpy(md_tmp.ptr,md.ptr,md.size);
	Deallocate(md);
	md=md_tmp;
	return true;
}
/***************************************************************************/
bool GMemoryManager::Reallocate(SMemDesc & md,int mul)
{
	local_stats.reallocation++;
	int pos=0;
	DWORD32 tmp=1024;
	while(md.size>tmp)
	{
		tmp=tmp<<1;
		pos++;
	}
	if(pos>=KCOUNT) return false;
	DWORD size=md.size*mul;
	if(size==0) size=K1;
	SMemDesc md_tmp=Allocate(size);
	if(md_tmp.size==0) return false;		//nie powiodla sie alokacja
	if(md.ptr!=NULL) memcpy(md_tmp.ptr,md.ptr,md.size);
	Deallocate(md);
	md=md_tmp;
	return true;
}
/***************************************************************************/
bool GMemoryManager::CanCache(int pos)
{
	if(!cache_memory) return false;
	local_stats.efficiency=(float)local_stats.cache_allocation*100.0f/(float)local_stats.allocation;

	//nie cachujemy obszarow ponad 1 MB
	if(pos>=KCOUNT) return false;

	if(mem_cached[pos].size())
	{
		//Bug 427 - tu by wartalo dolozyc jakies ograniczniki 
	}
	return true;
}
/***************************************************************************/
void GMemoryManager::UpdateTime(GClock &)
{
	//Bug 428 to jest bardzo prymitywne, trzeba zrobic jakas wartosc srednia uzycia za jakis czas
	int pos;
	for (pos=0;pos<KCOUNT;pos++)
	{
		int cached=(int)mem_cached[pos].size();
		int used=mem_used[pos];
		if(cached>used*2)
		{
			int diff=cached-(used*2);

			int a;
			for (a=0;a<diff;a++)
			{
				SMemDesc md;
				//mem_cache_count[pos]--;
				md=mem_cached[pos].back();
				mem_cached[pos].pop_back();
				local_stats.real_deallocation++;
				local_stats.allocated_memory-=md.size;
				DeleteTbl(md.ptr);
			}
		}
	}
}

/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

template<class TYPE> void
Delete(TYPE*& p)
{
	if(p)
	{
		TYPE * tmp = p;
		p = NULL;
		delete tmp;
	}

}
template<class TYPE> void
DeleteTbl(TYPE*& p)
{
	if(p)
	{
		TYPE * tmp = p;
		p = NULL;
		delete [] tmp;
	}

}
template<class TYPE1, class TYPE2> void
DeletePair(std::pair<TYPE1,TYPE2> p)
{
	if(p.second) delete p.second;
}

template<class TYPE> void
New(TYPE*& p)
{
	if(p!=NULL) return;
	p=new TYPE;
}
template<class TYPE> void
NewTbl(TYPE*& p,int size)
{
	if(p!=NULL) return;
	p=new TYPE[size];
}


template<class TYPE> class Wrap
{
	TYPE * ptr;
public:
	Wrap(TYPE * t)
	{
		ptr = t;
	}
	Wrap()
	{
		ptr = NULL;
	}
	~Wrap()
	{
		DeleteTbl(ptr);
	}
	TYPE * operator()()
	{
		return ptr;
	}
	TYPE & operator[](int i)
	{
		return ptr[i];
	}
	operator TYPE*()
	{
		return ptr;
	}
	
	void operator=(TYPE *t)
	{
		if(t != ptr)
		{
			DeleteTbl(ptr);
			ptr = t;
		}
	}
	void Set(TYPE *t)
	{
		ptr = t;
	}
	TYPE & operator*()
	{
		return ptr[0];
	}
};
///////////////////////////////////////////////////////////////////////////
template<class TYPE> void
Zero(TYPE& v)
{
	memset((void*)&v, 0, sizeof(TYPE));
}
/////////////////////////////////////////////////////////////////////////
template <class TYPE>inline TYPE
Clamp(TYPE var,TYPE min,TYPE max)
{
	if(var < min)
		return min;
	else if(var > max)
		return max;
	else
		return var;
}
/////////////////////////////////////////////////////////////////////////
template <class TYPE1, class TYPE2>inline TYPE1
Clamp(TYPE1 p,TYPE2 r)
{
	TYPE1 pt;
	if(r.x0<r.x1)
	{
		if(p.x< r.x0) pt.x=r.x0; else if(p.x > r.x1) pt.x=r.x1; else pt.x=p.x;
	}
	else
	{
		if(p.x> r.x0) pt.x=r.x0; else if(p.x < r.x1) pt.x=r.x1; else pt.x=p.x;
	}

	if(r.y0<r.y1)
	{
		if(p.y< r.y0) pt.y=r.y0; else if(p.y > r.y1) pt.y=r.y1; else pt.y=p.y;
	}
	else
	{
		if(p.y> r.y0) pt.y=r.y0; else if(p.y < r.y1) pt.y=r.y1; else pt.y=p.y;
	}
	return pt;
}
/////////////////////////////////////////////////////////////////////////
template <class TYPE>inline TYPE
Abs(TYPE i)
{
	return (TYPE)(i >= 0 ? i: -i);
}
/////////////////////////////////////////////////////////////////////////
template <class TYPE> inline TYPE
Dist(TYPE x,TYPE y)
{
	return Abs((TYPE)(x-y));
}
/////////////////////////////////////////////////////////////////////////
template <class TYPE> inline void
Swap(TYPE & x, TYPE & y)
{
	TYPE tmp;
	
	tmp = x;
	x = y;
	y = tmp;
}
/***************************************************************************/
template <class TYPE>
class IsEqual
{
public:
	IsEqual(TYPE mode,TYPE p1){ret=(mode==p1);}
	IsEqual(TYPE mode,TYPE p1,TYPE p2){ret=(mode==p1 || mode==p2);}
	IsEqual(TYPE mode,TYPE p1,TYPE p2, TYPE p3){ret=(mode==p1 || mode==p2 || mode==p3);}
	IsEqual(TYPE mode,TYPE p1, TYPE p2, TYPE p3, TYPE p4){ret=(mode==p1 || mode==p2 || mode==p3 || mode==p4);}
	IsEqual(TYPE mode,TYPE p1, TYPE p2, TYPE p3, TYPE p4, TYPE p5){ret=(mode==p1 || mode==p2 || mode==p3 || mode==p4 || mode==p5);}
	IsEqual(TYPE mode,TYPE p1, TYPE p2, TYPE p3, TYPE p4, TYPE p5, TYPE p6){ret=(mode==p1 || mode==p2 || mode==p3 || mode==p4 || mode==p5 || mode==p6);}
	IsEqual(TYPE mode,TYPE p1, TYPE p2, TYPE p3, TYPE p4, TYPE p5, TYPE p6, TYPE p7){ret=(mode==p1 || mode==p2 || mode==p3 || mode==p4 || mode==p5 || mode==p6 || mode==p7);}
	IsEqual(TYPE mode,TYPE p1, TYPE p2, TYPE p3, TYPE p4, TYPE p5, TYPE p6, TYPE p7, TYPE p8){ret=(mode==p1 || mode==p2 || mode==p3 || mode==p4 || mode==p5 || mode==p6 || mode==p7 || mode==p8);}
	IsEqual(TYPE mode,TYPE p1, TYPE p2, TYPE p3, TYPE p4, TYPE p5, TYPE p6, TYPE p7, TYPE p8, TYPE p9){ret=(mode==p1 || mode==p2 || mode==p3 || mode==p4 || mode==p5 || mode==p6 || mode==p7 || mode==p8 || mode==p9);}
	IsEqual(TYPE mode,TYPE p1, TYPE p2, TYPE p3, TYPE p4, TYPE p5, TYPE p6, TYPE p7, TYPE p8, TYPE p9, TYPE p10){ret=(mode==p1 || mode==p2 || mode==p3 || mode==p4 || mode==p5 || mode==p6 || mode==p7 || mode==p8 || mode==p9 || mode==p10);}
	const bool operator()(){return ret;};
	operator const bool(){return ret;};
private:
	bool	ret;
};
/***************************************************************************/
template <class TYPE>
class IsDiff
{
public:
	IsDiff(TYPE mode,TYPE p1){ret=(mode!=p1);}
	IsDiff(TYPE mode,TYPE p1,TYPE p2){ret=(mode!=p1 && mode!=p2);}
	IsDiff(TYPE mode,TYPE p1,TYPE p2, TYPE p3){ret=(mode!=p1 && mode!=p2 && mode!=p3);}
	IsDiff(TYPE mode,TYPE p1, TYPE p2, TYPE p3, TYPE p4){ret=(mode!=p1 && mode!=p2 && mode!=p3 && mode!=p4);}
	IsDiff(TYPE mode,TYPE p1, TYPE p2, TYPE p3, TYPE p4, TYPE p5){ret=(mode!=p1 && mode!=p2 && mode!=p3 && mode!=p4 && mode!=p5);}
	IsDiff(TYPE mode,TYPE p1, TYPE p2, TYPE p3, TYPE p4, TYPE p5, TYPE p6){ret=(mode!=p1 && mode!=p2 && mode!=p3 && mode!=p4 && mode!=p5 && mode!=p6);}
	IsDiff(TYPE mode,TYPE p1, TYPE p2, TYPE p3, TYPE p4, TYPE p5, TYPE p6, TYPE p7){ret=(mode!=p1 && mode!=p2 && mode!=p3 && mode!=p4 && mode!=p5 && mode!=p6 && mode!=p7);}
	IsDiff(TYPE mode,TYPE p1, TYPE p2, TYPE p3, TYPE p4, TYPE p5, TYPE p6, TYPE p7, TYPE p8){ret=(mode!=p1 && mode!=p2 && mode!=p3 && mode!=p4 && mode!=p5 && mode!=p6 && mode!=p7 && mode!=p8);}
	IsDiff(TYPE mode,TYPE p1, TYPE p2, TYPE p3, TYPE p4, TYPE p5, TYPE p6, TYPE p7, TYPE p8, TYPE p9){ret=(mode!=p1 && mode!=p2 && mode!=p3 && mode!=p4 && mode!=p5 && mode!=p6 && mode!=p7 && mode!=p8 && mode!=p9);}
	IsDiff(TYPE mode,TYPE p1, TYPE p2, TYPE p3, TYPE p4, TYPE p5, TYPE p6, TYPE p7, TYPE p8, TYPE p9,TYPE p10){ret=(mode!=p1 && mode!=p2 && mode!=p3 && mode!=p4 && mode!=p5 && mode!=p6 && mode!=p7 && mode!=p8 && mode!=p9 && mode!=p10);}
	const bool operator()(){return ret;};
	operator const bool(){return ret;};
private:
	bool	ret;
};
/***************************************************************************/
template <class TYPE>
bool IsInside(TYPE left,TYPE center, TYPE right){return !(center<=left || center>=right);};
/***************************************************************************/
template <int N>
struct GBitArrayBase
{
	DWORD32		tbl[(N+31)/32];

	GBitArrayBase()
	{
		UA();
	}
	GBitArrayBase(int n)
	{
		UA().M(n);
	}
	GBitArrayBase<N> & MA()
	{
		memset(tbl,255,((N+31)/32)*4);
		return * this;
	}
	GBitArrayBase<N> & UA()
	{
		memset(tbl,0,((N+31)/32)*4);
		return *this;
	}
	GBitArrayBase<N> & Clear()
	{
		UA();
		return *this;
	}
	GBitArrayBase<N> & Zero()
	{
		UA();
		return *this;
	}
	bool		IM(int n) const
	{
		n=Correct(n);
		int off = n >> 5;
		int pos = n & 31;
		DWORD32 bit = 1<<pos;
		return (tbl[off]&bit) == bit;
	}
	GBitArrayBase<N> & MI(GBitArrayBase  & r, GBitArrayBase  & m)
	{
		Clear();
		for (int a=0;a<N;a++)
		{
			if(m.IM(a)) if(!r.IM(a)) M(a);
		}
		return *this;
	}
	GBitArrayBase<N> & Mask(GBitArrayBase  & m)
	{
		for (int a=0;a<N;a++)
		{
			if(!m.IM(a)) U(a);
		}
		return *this;
	}
	GBitArrayBase<N> & M(int n)
	{
		n=Correct(n);
		int off = n >> 5;
		int pos = n & 31;
		DWORD bit = 1<<pos;
		tbl[off]|= bit;
		return *this;
	}
	GBitArrayBase<N> & Mnc(int n)
	{
		int m=Correct(n);
		if(m!=n) return * this;
		int off = n >> 5;
		int pos = n & 31;
		DWORD bit = 1<<pos;
		tbl[off]|= bit;
		return *this;
	}
	GBitArrayBase<N> & MM(int first, ...)
	{
		int i;
		i=first;
		va_list args;
		va_start(args, first) ;
		while(i!=-1)
		{
			M(i);
			i=va_arg(args,int);
		}
		va_end(args);
		return *this;
	}
	GBitArrayBase<N> & MU(int first, ...)
	{
		int i;
		i=first;
		va_list args;
		va_start(args, first) ;
		while(i!=-1)
		{
			U(i);
			i=va_arg(args,int);
		}
		va_end(args);
		return *this;
	}
	GBitArrayBase<N> & U(int n)
	{
		n=Correct(n);
		int off = n >> 5;
		int pos = n & 31;
		DWORD bit = 1<<pos;
		tbl[off]&= ~bit;
		return *this;
	}
	GBitArrayBase<N> & Unc(int n)
	{
		int m=Correct(n);
		if(m!=n) return *this;
		int off = n >> 5;
		int pos = n & 31;
		DWORD bit = 1<<pos;
		tbl[off]&= ~bit;
		return *this;
	}
	GBitArrayBase<N> & I(int n)
	{
		n=Correct(n);
		if(IM(n)) U(n); else M(n);
		return *this;
	}
	GBitArrayBase<N> & Inc(int n)
	{
		int m=Correct(n);
		if(m!=n) return * this;
		if(IM(n)) U(n); else M(n);
		return *this;
	}
	GBitArrayBase<N> & S(int n,bool s)
	{
		if(s) M(n); 
		else U(n);
		return *this;
	}
	bool Set(int n,bool s)
	{
		bool state=IM(n);
		if(s) M(n); 
		else U(n);
		return state!=IM(n);
	}
	int			Size()
	{
		int count=0;
		for (int a=0;a<N;a++)
		{
			if(IM(a)) count++;
		}
		return count;
	}
	int			Bytes()
	{
		return ((N+31)/32)*4;
	}
	BYTE*		Data()
	{
		return	(BYTE*)&tbl;
	}
	void		operator=(const GBitArrayBase<N> & ba)
	{
		memcpy(tbl,ba.tbl,((N+31)/32)*4);
	}
	bool		operator==(const GBitArrayBase<N> & ba) const
	{

		return !(memcmp(tbl,ba.tbl,((N+31)/32)*4));
	}
	bool		operator!=(const GBitArrayBase<N> & ba) const
	{

		return !!(memcmp(tbl,ba.tbl,((N+31)/32)*4));
	}
	bool		operator[](int n) const
	{
		n=Correct(n);
		return IM(n);
	}
	void		operator+=(GBitArrayBase<N> & ba)
	{
		int a;
		for (a=0;a<N;a++)
		{
			if(ba[a]) 
			{
				M(a);
			}
		}
	}
	void		operator-=(GBitArrayBase<N> & ba)
	{
		int a;
		for (a=0;a<N;a++)
		{
			if(ba[a]) U(a);
		}
	}
	int			Correct(int n) const
	{
		do {
			if(n<0) n+=N;
			if(n>=N) n-=N;
		} while(n<0 || n>=N);
		return n;
	}
	int			operator()()
	{
		return Size();
	}
	int			GetPosition(int bit)
	{
		int p=0;
		for (int a=0;a<N;a++)
		{
			if(IM(a))
			{
				if(bit==p) return a;
				p++;
			}
		}
		return -1;
	}
	char * 		GetMap(char * buf,int s=N)
	{
		int a;
		for(a=0;a<s;a++)
		{
			buf[a]=IM(a)?'1':'0';
		}
		buf[s]=0;
		return buf;
	}
	void		SetMap(const char * buf,int s=N)
	{
		UA();
		int a;
		for(a=0;a<s;a++)
		{
			S(a,buf[a]=='1');
		}
	}
	char * 		GetMapLong(char * buf,int s=N)
	{
		int a;
		int pos=0;
		for(a=0;a<s;a++)
		{
			buf[pos++]=IM(a)?'1':'0';
			if(a%10==9) buf[pos++]=' ';
		}
		buf[pos]=0;
		return buf;
	}
};
template <int N>
struct GBitArrayBaseMap: public GBitArrayBase<N>
{
public:
	char * 		Map(int s=N)
	{
		int a;
		for(a=0;a<s;a++)
		{
			map[a]=GBitArrayBase<N>::IM(a)?'1':'0';
		}
		map[s]=0;
		return map;
	}
	void		operator=(const GBitArrayBaseMap<N> & ba)
	{
		memcpy(GBitArrayBase<N>::tbl,ba.tbl,((N+31)/32)*4);
		memcpy(map,ba.map,N+1);
	}
	void		operator=(const GBitArrayBase<N> & ba)
	{
		memcpy(GBitArrayBase<N>::tbl,ba.tbl,((N+31)/32)*4);
		Map();
	}
private:
	char map[N+1];
};

typedef GBitArrayBase<32>	BA;
typedef GBitArrayBase<32>	GBitArray;
typedef GBitArrayBase<32>	GBitArray32;
typedef GBitArrayBase<64>	GBitArray64;
typedef GBitArrayBase<128>	GBitArray128;
typedef GBitArrayBaseMap<32>	GBitArrayMap;
typedef GBitArrayBaseMap<32>	GBitArray32Map;
typedef GBitArrayBaseMap<64>	GBitArray64Map;
typedef GBitArrayBaseMap<128>	GBitArray128Map;
/////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
template <class TYPE> inline TYPE
Max2(TYPE x, TYPE y)
{
	if(x>=y)
		return x;
	else
		return y;
}
/////////////////////////////////////////////////////////////////////////
template <class TYPE> inline void
SetMax2(TYPE & x, TYPE y)
{
	if(y>x) x=y;
}
/////////////////////////////////////////////////////////////////////////
template <class TYPE> inline TYPE
Max3(TYPE x, TYPE y,TYPE z)
{
	return Max2(x,Max2(y,z));
}
/////////////////////////////////////////////////////////////////////////
template <class TYPE> inline TYPE
Min2(TYPE x, TYPE y)
{
	if(x<=y)
		return x;
	else
		return y;
}
/////////////////////////////////////////////////////////////////////////
template <class TYPE> inline TYPE
Min3(TYPE x, TYPE y,TYPE z)
{
	return Min2(x,Min2(y,z));
}
/////////////////////////////////////////////////////////////////////////
template <class TYPE> inline TYPE
IsRange(TYPE x, TYPE y,TYPE z)
{
	if(y<x || y>z) return false;
	return true;
}

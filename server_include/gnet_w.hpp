	/*-----------------------------*/
	GNetMsg &			Wb(const bool a)
	{
		if(a) out_chunk.bits.M(out_chunk.bits_count++); else out_chunk.bits.U(out_chunk.bits_count++);
		if(out_chunk.bits_count==8) FlushBitsCache();
		return *this;
	};
	//inline GNetMsg &	Wb(const bool a){return W(a);};
	inline GNetMsg &	Wb(const int a){return Wb(a?true:false);};
	//inline GNetMsg &	operator<<(const bool a){return W(a);};
	/*-----------------------------*/
	GNetMsg &			WC(const char a)
	{
		FlushBitsCache();
		BYTE * buf=WPos(1);
		if(buf==NULL) return *this;
		memcpy(buf,&a,1);
		return *this;
	};
	//inline GNetMsg &	WC(const char a){return W(a);};
	inline GNetMsg &	WC(const int a){return WC((char)a);};
	//inline GNetMsg &	operator<<(const char a){return W(a);};
	/*-----------------------------*/
	GNetMsg &			WU(const BYTE a)
	{
		FlushBitsCache();
		BYTE * buf=WPos(1);
		if(buf==NULL) return *this;
		memcpy(buf,&a,1);
		return *this;
	};
	//inline GNetMsg &	WU(const BYTE a){return W(a);};
	inline GNetMsg &	WU(const int a){return WU((BYTE)a);};
	//inline GNetMsg &	operator<<(const BYTE a){return W(a);};
	/*-----------------------------*/
	GNetMsg &			WS(const SHORT16 a)
	{
		FlushBitsCache();
		BYTE * buf=WPos(2);
		if(buf==NULL) return *this;
		memcpy(buf,&a,2);
		return *this;
	};
	//inline GNetMsg &	WS(const SHORT16 a){return W(a);};
	inline GNetMsg &	WS(const int a){return WS((SHORT16)a);};
	//inline GNetMsg &	operator<<(const SHORT16 a){return W(a);};
	/*-----------------------------*/
	GNetMsg &			WUS(const USHORT16 a)
	{
		FlushBitsCache();
		BYTE * buf=WPos(2);
		if(buf==NULL) return *this;
		memcpy(buf,&a,2);
		return *this;
	};
	//inline GNetMsg &	WUS(const USHORT16 a){return W(a);};
	inline GNetMsg &	WUS(const int a){return WUS((USHORT16)a);};
	//inline GNetMsg &	operator<<(const USHORT16 a){return W(a);};
	/*-----------------------------*/
	GNetMsg &			WI(const INT32 a)
	{
		if(out_chunk.ptr==NULL) error.last_out=a;
		FlushBitsCache();
		BYTE * buf=WPos(4);
		if(buf==NULL) return *this;
		memcpy(buf,&a,4);
		return *this;
	};
	//inline GNetMsg &	WI(const INT32 a){return W(a);};
	//inline GNetMsg &	operator<<(const INT32 a){return W(a);};
	/*-----------------------------*/
	GNetMsg &			WUI(const DWORD32 a)
	{
		FlushBitsCache();
		BYTE * buf=WPos(4);
		if(buf==NULL) return *this;
		memcpy(buf,&a,4);
		return *this;
	};
	//inline GNetMsg &	WUI(const DWORD32 a){return W(a);};
	//inline GNetMsg &	operator<<(const DWORD32 a){return W(a);};
	/*-----------------------------*/
	GNetMsg &			WF(const float a)
	{
		FlushBitsCache();
		BYTE * buf=WPos(4);
		if(buf==NULL) return *this;
		memcpy(buf,&a,4);
		return *this;
	};
	//inline GNetMsg &	WF(const float a){return W(a);};
	//inline GNetMsg &	operator<<(const float a){return W(a);};
	/*-----------------------------*/
	GNetMsg &			WD(const double a)
	{
		FlushBitsCache();
		BYTE * buf=WPos(8);
		if(buf==NULL) return *this;
		memcpy(buf,&a,8);
		return *this;
	};
	//inline GNetMsg &	WD(const double a){return W(a);};
	//inline GNetMsg &	operator<<(const double a){return W(a);};
	/*-----------------------------*/
	GNetMsg &			WBI(const INT64 a)
	{
		FlushBitsCache();
		BYTE * buf=WPos(8);
		if(buf==NULL) return *this;
		memcpy(buf,&a,8);
		return *this;
	};
	//inline GNetMsg &	WBI(const INT64 a){return W(a);};
	//inline GNetMsg &	operator<<(const INT64 a){return W(a);};
	/*-----------------------------*/
	GNetMsg &			WBUI(const DWORD64 a)
	{
		FlushBitsCache();
		BYTE * buf=WPos(8);
		if(buf==NULL) return *this;
		memcpy(buf,&a,8);
		return *this;
	};
	//inline GNetMsg &	WBUI(const DWORD64 a){return W(a);};
	//inline GNetMsg &	operator<<(const DWORD64 a){return W(a);};
	/*-----------------------------*/
	GNetMsg &			WT(const char *s)
	{
		FlushBitsCache();
		if (s==NULL || strlen(s)==0) WI(0);
		else
		{
			int size=(int)strlen(s)+1;
			WI(size);
			BYTE * buf=WPos(size);
			if(buf==NULL) return *this;
			strncpy((char*)buf,s,size);
		}
		return *this;
	};
	//inline GNetMsg &	WT(const char * a){return W(a);};
	inline GNetMsg &	WT(const string &a){return WT(a.c_str());};
	inline GNetMsg &	WT(const boost::format & a){return WT(a.str().c_str());};
	//inline GNetMsg &	operator<<(const char * a){return W(a);};
	//inline GNetMsg &	operator<<(const string a){return W(a.c_str());};
	//inline GNetMsg &	operator<<(const boost::format & a){return W(a.str().c_str());};
	/*-----------------------------*/
	//template<class U>	GNetMsg & W(U & a){a.W(*this);return *this;}
	//template<class U>	inline GNetMsg & operator<<(U & str){return W(str);};
	/*-----------------------------*/
	GNetMsg &			W(const NPair & a)
	{
		FlushBitsCache();
		WI((int)a.size);
		BYTE * buf=WPos(a.size);
		if(buf==NULL) return *this;
		memcpy(buf,a.ptr,a.size);
		return *this;
	};
	//inline GNetMsg &	operator<<(const NPair & a){return W(a);};
	/*-----------------------------*/
	template<size_t N>	GNetMsg & W(bitset<N> &a)
	{
		size_t c=a.size();
		size_t b=0;
		for (b=0;b<c;b++)
		{
			bool r=a[b];
			Wb(r);
		}
		return *this;
	}
	//template<size_t N> inline GNetMsg &	operator<<(bitset<N> &a){return W(a);};
	/*-----------------------------*/
	GNetMsg & W(GNetStructure & str)
	{
		str.W(*this);
		return *this;
	}
	GNetMsg & WE(GNetStructure & str)
	{
		str.WE(*this);
		return *this;
	}
	//inline GNetMsg & operator<<(GNetStructure & str){return W(str);};
	/*-----------------------------*/
	GNetMsg &			Fill(const int len)
	{
		FlushBitsCache();
		BYTE * buf=WPos(len);
		if(buf==NULL) return *this;
		memset(buf,0,len);
		return *this;
	};
	/*-----------------------------*/
	template <int N>
	GNetMsg & W(GBitArrayBase<N> & bit)
	{
		FlushBitsCache();
		DWORD32 bytes=bit.Bytes();
		BYTE * buf=WPos(bytes);
		if(buf==NULL) return *this;
		memcpy(buf,bit.Data(),bytes);
		return *this;
	}
	/*-----------------------------*/
	GNetMsg & WMsg(GNetMsg & msg)
	{
		FlushBitsCache();
		INT32 size=msg.in_chunk.size;
		INT32 offset=msg.in_chunk.offset;
		if(size>offset)
		{
			BYTE * bufw=WPos(size-offset);
			BYTE * bufr=msg.RPos(size-offset);
			if (bufw == NULL || bufr == NULL) return *this;
			memcpy(bufw,bufr,size-offset);
		}
		return *this;
	}
	GNetMsg & W(GNetMsg & msg)
	{
		return WMsg(msg);
	}
	//inline GNetMsg &	operator<<(GNetMsg & a){return WMsg(a);};
	/*-----------------------------*/
	//template <int N>
	//inline GNetMsg & operator<<(GBitArrayBase<N> & bit){return W(bit);};


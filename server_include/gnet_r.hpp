	/*-----------------------------*/
	bool				Rb()
	{
		if(in_chunk.bits_count==0)
		{
			in_chunk.bits=RU();
			in_chunk.bits_count=8;
		}
		return in_chunk.bits.IM(8-in_chunk.bits_count--);
	};
	GNetMsg &			Rb(bool & a)
	{
		a=Rb();
		return *this;
	};
	//inline GNetMsg &	Rb(bool & a){return R(a);};
	//inline GNetMsg &	operator>>(bool & a){return R(a);};
	/*-----------------------------*/
	char				RC()
	{
		in_chunk.bits_count=0;
		char c;
		const BYTE * buf=RPos(1);
		if(buf==NULL) return 0;
		memcpy(&c,buf,1);
		return c;
	};
	GNetMsg &			RC(char & a)
	{
		a=RC();
		return *this;
	};
	//inline GNetMsg &	RC(char & a){return RC(a);};
	//inline GNetMsg &	operator>>(char & a){return R(a);};
	/*-----------------------------*/
	BYTE				RU()
	{
		in_chunk.bits_count=0;
		BYTE c;
		const BYTE * buf=RPos(1);
		if(buf==NULL) return 0;
		memcpy(&c,buf,1);
		return c;
	};
	GNetMsg &			RU(BYTE & a)
	{
		a=RU();
		return *this;
	};
	//inline GNetMsg &	RU(BYTE & a){return R(a);};
	//inline GNetMsg &	operator>>(BYTE & a){return R(a);};
	/*-----------------------------*/
	SHORT16				RS()
	{
		in_chunk.bits_count=0;
		SHORT16 c;
		const BYTE * buf=RPos(2);
		if(buf==NULL) return 0;
		memcpy(&c,buf,2);
		return c;
	};
	GNetMsg &			RS(SHORT16 & a)
	{
		a=RS();
		return *this;
	};
	//inline GNetMsg &	RS(SHORT16 & a){return R(a);};
	//inline GNetMsg &	operator>>(SHORT16 & a){return R(a);};
	/*-----------------------------*/
	USHORT16			RUS()
	{
		in_chunk.bits_count=0;
		USHORT16 c;
		const BYTE * buf=RPos(2);
		if(buf==NULL) return 0;
		memcpy(&c,buf,2);
		return c;
	};
	GNetMsg &			RUS(USHORT16 & a)
	{
		a=RUS();
		return *this;
	};
	//inline GNetMsg &	RUS(USHORT16 & a){return R(a);};
	//inline GNetMsg &	operator>>(USHORT16 & a){return R(a);};
	/*-----------------------------*/
	INT32				RI()
	{
		in_chunk.bits_count=0;
		INT32 c;
		const BYTE * buf=RPos(4);
		if(buf==NULL) return 0;
		memcpy(&c,buf,4);
		return c;
	};
	GNetMsg &			RI(INT32 & a)
	{
		a=RI();
		return *this;
	};
	//inline GNetMsg &	RI(INT32 & a){return R(a);};
	//inline GNetMsg &	operator>>(INT32 & a){return R(a);};
	/*-----------------------------*/
	DWORD32				RUI()
	{
		in_chunk.bits_count=0;
		DWORD32 c;
		const BYTE * buf=RPos(4);
		if(buf==NULL) return 0;
		memcpy(&c,buf,4);
		return c;
	};
	GNetMsg &			RUI(DWORD32 & a)
	{
		a=RUI();
		return *this;
	};
	//inline GNetMsg &	RUI(DWORD32 & a){return R(a);};
	//inline GNetMsg &	operator>>(DWORD32 & a){return R(a);};
	/*-----------------------------*/
	float				RF()
	{
		in_chunk.bits_count=0;
		float c;
		const BYTE * buf=RPos(4);
		if(buf==NULL) return 0;
		memcpy(&c,buf,4);
		return c;
	};
	GNetMsg &			RF(float & a)
	{
		a=RF();
		return *this;
	};
	//inline GNetMsg &	RF(float & a){return R(a);};
	//inline GNetMsg &	operator>>(float & a){return R(a);};
	/*-----------------------------*/
	double				RD()
	{
		in_chunk.bits_count=0;
		double c;
		const BYTE * buf=RPos(8);
		if(buf==NULL) return 0;
		memcpy(&c,buf,8);
		return c;
	};
	GNetMsg &			RD(double & a)
	{
		a=RD();
		return *this;
	};
	//inline GNetMsg &	RD(double & a){return R(a);};
	//inline GNetMsg &	operator>>(double & a){return R(a);};
	/*-----------------------------*/
	INT64				RBI()
	{
		in_chunk.bits_count=0;
		INT64 c;
		const BYTE * buf=RPos(8);
		if(buf==NULL) return 0;
		memcpy(&c,buf,8);
		return c;
	};
	GNetMsg &			RBI(INT64 & a)
	{
		a=RBI();
		return *this;
	};
	//inline GNetMsg &	RBI(INT64 & a){return R(a);};
	//inline GNetMsg &	operator>>(INT64 & a){return R(a);};
	/*-----------------------------*/
	DWORD64				RBUI()
	{
		in_chunk.bits_count=0;
		DWORD64 c;
		const BYTE * buf=RPos(8);
		if(buf==NULL) return 0;
		memcpy(&c,buf,8);
		return c;
	};
	GNetMsg &			RBUI(DWORD64 & a)
	{
		a=RBUI();
		return *this;
	};
	//inline GNetMsg &	RBUI(DWORD64 & a){return R(a);};
	//inline GNetMsg &	operator>>(DWORD64 & a){return R(a);};
	/*-----------------------------*/
	char *				RT()
	{
		int size=RI();
		if(size==0) return "";
		if (size<0) 
		{
			//throw GNetException(ENetErrorDataError);
			SetErrorCode(ENetErrorDataError,__LINE__);
			return "";
		}
		else
		{
			const BYTE * buf=RPos(size);
			if(buf==NULL) return "";
			return (char *)buf;
		}
	};
	GNetMsg &			RT(SPair & a)
	{
		strncpy(a.ptr,RT(),a.max_size);
		return *this;
	};
	//inline GNetMsg &	RT(SPair & a){return R(a);};
	GNetMsg &			RT(string & a)
	{
		char * str=RT();
		a.assign(str);
		return *this;
	};
	//inline GNetMsg &	RT(string & a){return R(a);};
	//inline GNetMsg &	operator>>(SPair & a){return R(a);};
	//inline GNetMsg &	operator>>(string & a){return R(a);};
	/*-----------------------------*/
	//template<class U>	GNetMsg & R(U & a){a.R(*this);return * this;}
	//template<class U>	inline GNetMsg & operator>>(U & a){return R(a);};
	/*-----------------------------*/
	template<size_t N>	GNetMsg & R(bitset<N> &a)
	{
		size_t c=a.size();
		size_t b=0;
		for (b=0;b<c;b++)
		{
			a[b]=Rb();
		}
		return *this;
	}
	//template<size_t N>	inline GNetMsg & operator>>(bitset<N> & a){return R(a);};
	/*-----------------------------*/
	GNetMsg & R(GNetStructure & str)
	{
		str.R(*this);
		return *this;
	}
	GNetMsg & RE(GNetStructure & str)
	{
		str.RE(*this);
		return *this;
	}
	//inline GNetMsg & operator>>(GNetStructure & str){return R(str);};
	/*-----------------------------*/
	GNetMsg &			R(const NPair & a)
	{
		DWORD32 size=RI();
		if(size!=a.size) 
		{
			SetErrorCode(ENetErrorDataError,__LINE__);
			return *this;
		}
		const BYTE * buf=RPos(size);
		if(buf==NULL) return *this;
		memcpy(a.ptr,buf,a.size);
		return *this;
	};
	//inline GNetMsg &	operator>>(const NPair & a){return R(a);};
	/*-----------------------------*/
	template <int N>
	GNetMsg & R(GBitArrayBase<N> & bit)
	{
		DWORD32 bytes=bit.Bytes();
		const BYTE * buf=RPos(bytes);
		if(buf==NULL) return *this;
		memcpy(bit.Data(),buf,bytes);
		return *this;
	}
	//template <int N>
	//inline GNetMsg & operator>>(GBitArrayBase<N> & bit){return R(bit);};

	//pojawil sie problem jak testowac wychodzace dane
	//teoretycznie zapis powinien byc spojny i uzywac funkcji z grupy R
	//tyle ze pociaga to za soba koniecznosc podania kontekstu w ktorym pracujemy
	//wiec musialby wszedzie dodac kolejny parametr z opisem tego kontekstu
	//poniewaz jednak takie funkcje jak RPos musza byc zmienione i np. nie automatyzowac
	//zamykania czunka zdecydowalem sie zrobic kopie funkcji R do T
	//i w ten sposob obejsc problem, 
	//alternatywa podawania kontekstu nie pasuje mi ze wzgledu na zapis i wydajnosc
	//a skopiowanie ewentualnych zmian nie robi problemu
	//w sumie niech zyje wydajnosc!!

	/*-----------------------------*/
	bool				Tb()
	{
		if(test_chunk.bits_count==0)
		{
			test_chunk.bits=TU();
			test_chunk.bits_count=8;
		}
		return test_chunk.bits.IM(8-test_chunk.bits_count--);
	};
	GNetMsg &			T(bool & a)
	{
		a=Tb();
		return *this;
	};
	inline GNetMsg &	Tb(bool & a){return T(a);};
	/*-----------------------------*/
	GNetMsg &			T(char & a)
	{
		a=TC();
		return *this;
	};
	char				TC()
	{
		test_chunk.bits_count=0;
		char c;
		BYTE * buf=TPos(1);
		if(buf==0) return 0;
		memcpy(&c,buf,1);
		return c;
	};
	inline GNetMsg &	TC(char & a){return TC(a);};
	/*-----------------------------*/
	GNetMsg &			T(BYTE & a)
	{
		a=TU();
		return *this;
	};
	BYTE				TU()
	{
		test_chunk.bits_count=0;
		BYTE c;
		BYTE * buf=TPos(1);
		if(buf==NULL) return 0;
		memcpy(&c,buf,1);
		return c;
	};
	inline GNetMsg &	TU(BYTE & a){return T(a);};
	/*-----------------------------*/
	SHORT16				TS()
	{
		test_chunk.bits_count=0;
		SHORT16 c;
		BYTE * buf=TPos(2);
		if(buf==NULL) return 0;
		memcpy(&c,buf,2);
		return c;
	};
	GNetMsg &			T(SHORT16 & a)
	{
		a=TS();
		return *this;
	};
	inline GNetMsg &	TS(SHORT16 & a){return T(a);};
	/*-----------------------------*/
	USHORT16			TUS()
	{
		test_chunk.bits_count=0;
		USHORT16 c;
		BYTE * buf=TPos(2);
		if(buf==NULL) return 0;
		memcpy(&c,buf,2);
		return c;
	};
	GNetMsg &			T(USHORT16 & a)
	{
		a=TUS();
		return *this;
	};
	inline GNetMsg &	TUS(USHORT16 & a){return T(a);};
	/*-----------------------------*/
	INT32				TI()
	{
		test_chunk.bits_count=0;
		INT32 c;
		BYTE * buf=TPos(4);
		if(buf==NULL) return 0;
		memcpy(&c,buf,4);
		return c;
	};
	GNetMsg &			T(INT32 & a)
	{
		a=TI();
		return *this;
	};
	inline GNetMsg &	TI(INT32 & a){return T(a);};
	/*-----------------------------*/
	DWORD32				TUI()
	{
		test_chunk.bits_count=0;
		DWORD32 c;
		BYTE * buf=TPos(4);
		if(buf==NULL) return 0;
		memcpy(&c,buf,4);
		return c;
	};
	GNetMsg &			T(DWORD32 & a)
	{
		a=TUI();
		return *this;
	};
	inline GNetMsg &	TUI(DWORD32 & a){return T(a);};
	/*-----------------------------*/
	float				TF()
	{
		test_chunk.bits_count=0;
		float c;
		BYTE * buf=TPos(4);
		if(buf==NULL) return 0;
		memcpy(&c,buf,4);
		return c;
	};
	GNetMsg &			T(float & a)
	{
		a=TF();
		return *this;
	};
	inline GNetMsg &	TF(float & a){return T(a);};
	/*-----------------------------*/
	double				TD()
	{
		test_chunk.bits_count=0;
		double c;
		BYTE * buf=TPos(8);
		if(buf==NULL) return 0;
		memcpy(&c,buf,8);
		return c;
	};
	GNetMsg &			T(double & a)
	{
		a=TD();
		return *this;
	};
	inline GNetMsg &	TD(double & a){return T(a);};
	/*-----------------------------*/
	INT64				TBI()
	{
		test_chunk.bits_count=0;
		INT64 c;
		BYTE * buf=TPos(8);
		if(buf==NULL) return 0;
		memcpy(&c,buf,8);
		return c;
	};
	GNetMsg &			T(INT64 & a)
	{
		a=TBI();
		return *this;
	};
	inline GNetMsg &	TBI(INT64 & a){return T(a);};
	/*-----------------------------*/
	DWORD64				TBUI()
	{
		test_chunk.bits_count=0;
		DWORD64 c;
		BYTE * buf=TPos(8);
		if(buf==NULL) return 0;
		memcpy(&c,buf,8);
		return c;
	};
	GNetMsg &			T(DWORD64 & a)
	{
		a=TBUI();
		return *this;
	};
	inline GNetMsg &	TBUI(DWORD64 & a){return T(a);};
	/*-----------------------------*/
	char *				TT()
	{
		int size=TS();
		if(size==0) return "";
		if (size<0) 
		{
			//throw GNetException(ENetErrorDataError);
			SetErrorCode(ENetErrorDataError,__LINE__);
			return "";
		}
		else
		{
			BYTE * buf=TPos(size);
			if(buf==NULL) return "";
			return (char *)buf;
		}
	};	
	GNetMsg &			T(char * a)
	{
		strcpy(a,TT());
		return *this;
	};
	inline GNetMsg &	TT(char * a){return T(a);};
	GNetMsg &			T(string & a)
	{
		a.assign(TT());
		return *this;
	};
	inline GNetMsg &	TT(string & a){return T(a);};
	/*-----------------------------*/
	//template<class U>	GNetMsg & T(U & a){a.T(*this);return * this;}
	/*-----------------------------*/
	GNetMsg &			Te(int count)
	{
		int b=0;
		for (b=0;b<count;b++)
		{
			Tb();
		}
		return *this;
	}
	/*-----------------------------*/
	GNetMsg &			T(NPair & a)
	{
		a.size=TI();
		BYTE * buf=TPos(a.size);
		if(buf==NULL) return *this;
		a.ptr=buf;
		//memcpy(a.ptr,buf,a.size);
		return *this;
	};
	/*-----------------------------*/
	GNetMsg & Tr(int size)
	{
		BYTE * buf=TPos(size*4);
		if(buf==NULL) return *this;
		return *this;
	}

/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

/**
typy szyfrowania
*/
enum ENetCryptType{
	ENetCryptXOR,						//!xor z wartoscia
	ENetCryptReverse18,					//!zamiana bitow
	ENetCryptReverse27,					//!zamiana bitow
	ENetCryptReverse36,					//!zamiana bitow
	ENetCryptReverse45,					//!zamiana bitow
	ENetCryptReverse15,					//!zamiana bitow
	ENetCryptReverse26,					//!zamiana bitow
	ENetCryptReverse37,					//!zamiana bitow
	ENetCryptReverse48,					//!zamiana bitow
	ENetCryptShift1,					//!przesuniecie o 1 bit
	ENetCryptShift2,					//!przesuniecie o 2 bity
	ENetCryptShift3,					//!przesuniecie o 3 bity
	ENetCryptShift4,					//!przesuniecie o 4 bity
	ENetCryptShift5,					//!przesuniecie o 5 bitow
	ENetCryptShift6,					//!przesuniecie o 6 bitow
	ENetCryptShift7,					//!przesuniecie o 7 bitow
	ENetCryptLast,
	ENetCryptSpecial,					//klucze specjalne
	ENetCryptAES,						//!AES
	ENetCryptBF,						//!BF
	ENetCryptDES,						//!DES
	ENetCryptDES3,						//!DES3
	ENetCryptNone,						//!brak szyfrowania
};

/**
struktura opisujaca dane kluczy AES
*/
struct SAESKey
{
	void Init()
	{
		int a;
		for (a=0;a<32;a++) {key[a]=0;};
		for (a=0;a<16;a++) {iv_e[a]=0;iv_d[a]=0;};
		MEMRESET(aes_ks_e);
		MEMRESET(aes_ks_d);
	}
	void Rnd()
	{
		Init();
		int a;
		for (a=0;a<32;a++) key[a]=(BYTE)rng_byte();
		SetKey();
	}
	void Buf(BYTE * b)
	{
		Init();
		memcpy(key,b,32);
		SetKey();
	}
	void SetKey()
	{
		AES_set_encrypt_key(key,256,&aes_ks_e);
		AES_set_decrypt_key(key,256,&aes_ks_d);
	}
	BYTE				key[32];
	BYTE				iv_e[16];
	BYTE				iv_d[16];
	AES_KEY				aes_ks_e;
	AES_KEY				aes_ks_d;
};
/**
struktura opisujaca dane kluczy BF
*/
struct SBFKey
{
	void Init()
	{
		int a;
		for (a=0;a<16;a++) {key[a]=0;};
		for (a=0;a<8;a++) {iv_e[a]=0;iv_d[a]=0;};
		MEMRESET(bf_ks);
	}
	void Rnd()
	{
		Init();
		int a;
		for (a=0;a<16;a++) key[a]=(BYTE)rng_byte();
		SetKey();
	}
	void Buf(BYTE * b)
	{
		Init();
		memcpy(key,b,16);
		SetKey();
	}
	void SetKey()
	{
		BF_set_key(&bf_ks,16,key);
	}
	BYTE				key[16];
	BYTE				iv_e[8];
	BYTE				iv_d[8];
	BF_KEY				bf_ks;
};
/**
struktura opisujaca dane kluczy DES
*/
struct SDESKey
{
	void Init()
	{
		memset(des_ive,0,sizeof(DES_cblock));
		memset(des_ivd,0,sizeof(DES_cblock));
		MEMRESET(key1);
		MEMRESET(key2);
		MEMRESET(key3);
		MEMRESET(sch1);
		MEMRESET(sch2);
		MEMRESET(sch3);
	}
	void Rnd()
	{
		Init();
		int c=0;
		do{
			RAND_bytes(key1,8);
			DES_set_odd_parity(&key1);
			c++;
		}
		while(DES_set_key_checked(&key1,&sch1)!=0);
		do{
			RAND_bytes(key2,8);
			DES_set_odd_parity(&key2);
			c++;
		}
		while(DES_set_key_checked(&key2,&sch2)!=0);
		do{
			RAND_bytes(key3,8);
			DES_set_odd_parity(&key3);
			c++;
		}
		while(DES_set_key_checked(&key3,&sch3)!=0);
	}
	void Buf(BYTE * b)
	{
		Init();
		memcpy(key1,&b[0],8);
		memcpy(key2,&b[8],8);
		memcpy(key3,&b[16],8);
		DES_set_key_checked(&key1,&sch1);
		DES_set_key_checked(&key2,&sch2);
		DES_set_key_checked(&key3,&sch3);
	}
	void FillBuf(BYTE * b)
	{
		memcpy(&b[0],key1,8);
		memcpy(&b[8],key2,8);
		memcpy(&b[16],key3,8);
	}
	DES_cblock			key1;
	DES_cblock			key2;
	DES_cblock			key3;
	DES_key_schedule	sch1;
	DES_key_schedule	sch2;
	DES_key_schedule	sch3;
	DES_cblock			des_ive;
	DES_cblock			des_ivd;
};
/**
struktura opisujaca dane kluczy XOR
*/
struct SXORKey
{
	void Init()
	{
		xor_base=0;
	}
	void Rnd()
	{
		Init();
		RAND_bytes((BYTE*)&xor_base,4);
	}
	void Buf(BYTE * b)
	{
		Init();
		memcpy(&xor_base,b,4);
	}
	void FillBuf(BYTE * b)
	{
		memcpy(&b[0],&xor_base,4);
	}
	DWORD32				xor_base;
};
/***************************************************************************/
/**
\brief klasa odpowiada za szyfrowanie danych, podajemy obszar do szyfrowania
a na podstawie ustawionych w klasie parametrow obszar ten jest rekodowany
*/
class GNetCrypt
{
public:
	/*-----------------------------*/
	//! konstruktow
	GNetCrypt()
	{
		Reset();
	}
	/*-----------------------------*/
	void Reset()
	{
		crytp_method=ENetCryptNone;
		code=true;
		key=NULL;
	}
	/*-----------------------------*/
	//! szyfruje dane opisane przez p
	void operator<<(const NPair & p)
	{
		code=true;
		Code(p);
	}
	/*-----------------------------*/
	//! deszyfruje dane opisane przez p
	void operator>>(const NPair & p)
	{
		code=false;
		Code(p);
	}
	/*-----------------------------*/
	//! ustawia algorytm szyfrowania
	void				SetCrypt(ENetCryptType method)
	{
		crytp_method=method;
	};
	/*-----------------------------*/
	//! pobiera katualny alogrym szyfrowania
	ENetCryptType		GetCrypt(){return crytp_method;};
	/*-----------------------------*/
	int					GetOffset(){
		switch(crytp_method)
		{
		case ENetCryptAES: return 16;
		case ENetCryptBF: return 8;
		case ENetCryptDES: return 8;
		case ENetCryptDES3: return 8;
		};
		return 0;
	}
	/*-----------------------------*/
	void				SetKey(void * key_p)
	{
		key=key_p;
	}
private:
	/*-----------------------------*/
	//! koduje dane
	void				Code(const NPair & p)
	{
		NPair p1=p;
		p1.size-=4;
		p1.ptr=p.ptr+4;
		switch(crytp_method)
		{
		case ENetCryptXOR: CodeXOR(p1);return;
		case ENetCryptReverse18: CodeReverse(p1,0,7);return;
		case ENetCryptReverse27: CodeReverse(p1,1,6);return;
		case ENetCryptReverse36: CodeReverse(p1,2,5);return;
		case ENetCryptReverse45: CodeReverse(p1,3,4);return;
		case ENetCryptReverse15: CodeReverse(p1,0,4);return;
		case ENetCryptReverse26: CodeReverse(p1,1,5);return;
		case ENetCryptReverse37: CodeReverse(p1,2,6);return;
		case ENetCryptReverse48: CodeReverse(p1,3,7);return;
		case ENetCryptShift1: CodeShift(p1,1);return;
		case ENetCryptShift2: CodeShift(p1,2);return;
		case ENetCryptShift3: CodeShift(p1,3);return;
		case ENetCryptShift4: CodeShift(p1,4);return;
		case ENetCryptShift5: CodeShift(p1,5);return;
		case ENetCryptShift6: CodeShift(p1,6);return;
		case ENetCryptShift7: CodeShift(p1,7);return;
		case ENetCryptAES:	CodeAES(p1);return;
		case ENetCryptBF:	CodeBF(p1);return;
		case ENetCryptDES:	CodeDES(p1);return;
		case ENetCryptDES3:	CodeDES3(p1);return;
		case ENetCryptNone: return;
		default: return;
		}
	}
	/*-----------------------------*/
	void				CodeXOR(const NPair & p)
	{
		if(key==NULL) return;
		DWORD32 xor_base=((SXORKey*)key)->xor_base;
		DWORD32 a;
		BYTE * d=(BYTE*)p.ptr;
		BYTE * b=(BYTE*)&xor_base;
		int bp=0;
		for (a=0;a<p.size;a++)
		{
			d[a]=d[a]^b[bp++];
			if(bp==4) bp=0;
		}
	}
	/*-----------------------------*/
	void				CodeReverse(const NPair & p,BYTE ba, BYTE bb)
	{
		BYTE * d=(BYTE*)p.ptr;
		DWORD32 a;
		for (a=0;a<p.size;a++)
		{
			BYTE b1 = 1 << ba;
			BYTE b2 = 1 << bb;
			bool bs1 = ((d[a]&b1)==b1);
			bool bs2 = ((d[a]&b2)==b2);
			if(bs1) d[a]|=b2; else d[a]&=~b2;
			if(bs2) d[a]|=b1; else d[a]&=~b1;
		}
	}
	/*-----------------------------*/
	void				CodeShift(const NPair & p,BYTE ba)
	{
		DWORD32 a;
		BYTE * d=(BYTE*)p.ptr;
		if(code)
		{
			for (a=0;a<p.size;a++)
			{
				BYTE b1=d[a]>>ba;
				BYTE b2=d[a]<<(8-ba);
				d[a]=b1|b2;
			}
		}
		else
		{
			for (a=0;a<p.size;a++)
			{
				BYTE b1=d[a]<<ba;
				BYTE b2=d[a]>>(8-ba);
				d[a]=b1|b2;
			}
		}
	}
	/*-----------------------------*/
	void				CodeAES(const NPair & p)
	{
		if(key==NULL) return;
		BYTE * d=(BYTE*)p.ptr;
		if(code)
		{
			AES_cbc_encrypt(d,d,(unsigned long)p.size,&((SAESKey*)key)->aes_ks_e,((SAESKey*)key)->iv_e,AES_ENCRYPT);
		}
		else
		{
			AES_cbc_encrypt(d,d,(unsigned long)p.size,&((SAESKey*)key)->aes_ks_d,((SAESKey*)key)->iv_d,AES_DECRYPT);
		}
	}
	/*-----------------------------*/
	void				CodeBF(const NPair & p)
	{
		if(key==NULL) return;
		BYTE * d=(BYTE*)p.ptr;
		if(code)
		{
			BF_cbc_encrypt(d,d,(unsigned long)p.size,&((SBFKey*)key)->bf_ks,((SBFKey*)key)->iv_e,BF_ENCRYPT);
		}
		else
		{
			BF_cbc_encrypt(d,d,(unsigned long)p.size,&((SBFKey*)key)->bf_ks,((SBFKey*)key)->iv_d,BF_DECRYPT);
		}
	}
	/*-----------------------------*/
	void				CodeDES(const NPair & p)
	{
		if(key==NULL) return;
		BYTE * d=(BYTE*)p.ptr;
		if(code)
		{
			DES_ncbc_encrypt(d,d,(unsigned long)p.size,&((SDESKey*)key)->sch1,&((SDESKey*)key)->des_ive,DES_ENCRYPT);
		}
		else
		{
			DES_ncbc_encrypt(d,d,(unsigned long)p.size,&((SDESKey*)key)->sch1,&((SDESKey*)key)->des_ivd,DES_DECRYPT);
		}
	}
	/*-----------------------------*/
	void				CodeDES3(const NPair & p)
	{
		if(key==NULL) return;
		BYTE * d=(BYTE*)p.ptr;
		if(code)
		{
			DES_ede3_cbc_encrypt(d,d,(unsigned long)p.size,&((SDESKey*)key)->sch1,&((SDESKey*)key)->sch2,&((SDESKey*)key)->sch3,&((SDESKey*)key)->des_ive,DES_ENCRYPT);
		}
		else
		{
			DES_ede3_cbc_encrypt(d,d,(unsigned long)p.size,&((SDESKey*)key)->sch1,&((SDESKey*)key)->sch2,&((SDESKey*)key)->sch3,&((SDESKey*)key)->des_ivd,DES_DECRYPT);
		}
	}
	/*-----------------------------*/

private:
	ENetCryptType		crytp_method;
	bool				code;
	void *				key;
};
/***************************************************************************/

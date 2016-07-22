/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"
#include "gmsg_game.h"
#include "gmsg_game_sub.h"
#include "gmsg_internal.h"
#include "gmsg_internal_sub.h"
#include "utils_conversion.h"
#include <stack>
#include <stdlib.h>
boost::hash<std::string> hash_str;
boost::hash<std::wstring> hash_wstr;

static const BYTE vecLower[] = "0123456789ABCDEF";

/***************************************************************************/
#define GNET(msgid,format,desc)		{msgid,#msgid,format,desc},
SNetMsgDesc msg_game_default[]={
#include "gmsg_game.tbl"
};
int msg_game_count=(sizeof(msg_game_default)/sizeof(msg_game_default[0]));
#undef GNET

#define GSUBNET(msgid,format,desc)		{msgid,#msgid,format,desc},
SNetMsgDesc msg_game_sub_default[]={
#include "gmsg_game_sub.tbl"
};
int msg_game_sub_count=(sizeof(msg_game_sub_default)/sizeof(msg_game_sub_default[0]));
#undef GSUBNET

#ifdef SERVER
#define GNET(msgid,format,desc)		{msgid,#msgid,format,desc},
SNetMsgDesc msg_internal_default[]={
#include "tbl/gmsg_internal.tbl"
};
int msg_internal_count=(sizeof(msg_internal_default)/sizeof(msg_internal_default[0]));
#undef GNET

#define GSUBNET(msgid,format,desc)		{msgid,#msgid,format,desc},
SNetMsgDesc msg_internal_sub_default[]={
#include "tbl/gmsg_internal_sub.tbl"
};
int msg_internal_sub_count=(sizeof(msg_internal_sub_default)/sizeof(msg_internal_sub_default[0]));
#undef GSUBNET
#endif



GNetParserBase	msg_base;

void FormatError(string s)
{
	cout<<"format error\r\n";
	cout<<s.c_str();
	cout<<"\r\n";
}
/***************************************************************************/
GNetMsg::GNetMsg(GMemoryManager * mgr_p)
{
	in=out=NULL;
	chunk_limit=K128;
	Reset();
	mgr=mgr_p;
	cmd_type=ENetCmdGame;
	last_parsed_message=0;
}
/***************************************************************************/
GNetMsg::~GNetMsg()
{
	Reset();
	in=out=NULL;
}
/***************************************************************************/
void GNetMsg::Init(ENetCmdType cmd_type_p,GMemoryManager * mgr_p)
{
	cmd_type=cmd_type_p;
	chunk_limit=K128;
	Reset();
	mgr=mgr_p;
}
/***************************************************************************/
void GNetMsg::Init(ENetCmdType cmd_type_p,GMemory * in_p, GMemory * out_p)
{
	cmd_type=cmd_type_p;
	chunk_limit=K128;
	Reset();
	in=in_p;
	out=out_p;
}
/***************************************************************************/
GNetMsg & GNetMsg::Reset()
{
	in_auto.Deallocate();
	out_auto.Deallocate();
	out_chunk.Reset();
	in_chunk.Reset();
	test_chunk.Reset();

	status=ENetStatusNone;
	error.code=ENetErrorNone;
	error.line=0;
	error.last_in=0;
	error.last_out=0;
	last_chunk.Reset();
	current_chunk.Reset();

	format_error.clear();

	crypt_in.Reset();
	crypt_out.Reset();
	return *this;
}
/***************************************************************************/
BYTE * GNetMsg::WPos(DWORD32 size)
{
	//sprawdzamy czy da sie otworzyc bufor,
	if(out==NULL) if(!OpenOut()) 
	{
		SetErrorCode(ENetErrorNotAllocated,__LINE__);
		return NULL;
	}

	//sprawdzamy czy jest otwarty chunk,
	if(out_chunk.ptr==NULL) 
	{
		//patrzymy czy jest gdzie pisac
		if(out->Free()<4)
		{
			//nie ma - realokujemy bufor
			//if(!out->ReallocateTo(out->Size()+512))
			if(!out->Reallocate()) 
			{
				SetErrorCode(ENetErrorReallocFailed,__LINE__);
				return NULL;
			}
		}
		//zostawiamy 4 bajty na chunka
		out_chunk.ptr=(BYTE*)out->End();

		if(out_chunk.ptr==NULL)
		{
			SetErrorCode(ENetErrorNotAllocated,__LINE__);
			return NULL;
		}

		*((DWORD32*)out_chunk.ptr)=0;
		out_chunk.offset=4;
	}

	//patrzymy czy jest gdzie pisac
	if(out->Free()<size+out_chunk.offset)
	{
		//if(!out->ReallocateTo(out->Size()+512))
		if(!out->ReallocateToFit(out_chunk.offset+size)) 
		{
			SetErrorCode(ENetErrorReallocFailed,__LINE__);
			return NULL;
		}
		out_chunk.ptr=(BYTE*)out->End();
	}

	//przygotowujemy ptr na dane
	BYTE * ret=(BYTE*)&(out->End()[out_chunk.offset]);
	//i zwiekszamy odpowiednio rozmar chunka
	out_chunk.offset+=size;
	return ret;
}
/***************************************************************************/
bool GNetMsg::Parse(INT32 & pre_message)
{
	pre_message=0;
	//sprawdzamy czy bufor in jest otwarty
	if(in==NULL) if(!OpenIn()) 
	{
		SetErrorCode(ENetErrorNotAllocated,__LINE__);
		return false;
	}
	
	//jesli mamy otwary jakis chunk przesuwamy ptr parsowania na jego koniec
	if(in_chunk.ptr && in_chunk.size) in->IncreaseParsed(in_chunk.size);

	//patrzymy czy jest w buforze dosc danych
	if(in->Used()>=4) 
	{
		//patrzymy czy sa 4 bajty z dlugoscia chunka 
		if(in->ToParse()>=4)
		{
			in_chunk.ptr=(BYTE*)in->Parse();
			in_chunk.size=*((DWORD32*)in_chunk.ptr);
			if(in->ToParse()>=8)
			{
				pre_message=*((DWORD32*)(in_chunk.ptr+4));
			}
			if((chunk_limit>0 && in_chunk.size>chunk_limit) || in_chunk.size < 4)
			{
				SetErrorCode(ENetErrorOutOfLimit, __LINE__);
				in_chunk.Reset();
				return false;
			}
			if((int)in->ToParse()>=in_chunk.size) 
			{
				current_chunk.Init(in_chunk.ptr,in_chunk.size);
				test_chunk=in_chunk;
				if(flags[ENetCryptIn]) 
					crypt_in>>current_chunk;
				if(flags[ENetTestIn]) if(!TestMsg(false)) 
				{
					SetErrorCode(ENetErrorIncorrectFormat,__LINE__);
					return false;
				}
				in_chunk.offset=4;
				return true;
			}
			if(in_chunk.size<chunk_limit && in_chunk.size>in->Size())
			{
				in->ReallocateToFit(in_chunk.size);
			}
		}
	}
	//nie ma juz zadnych danych do parsowania, kasujemy dane z bufora in
    in->RemoveParsed();
	in_chunk.Reset();
	return false;
}
/***************************************************************************/
BYTE * GNetMsg::RPos(DWORD32 size)
{
	//sprawdzamy czy bufor wejscia istnieje, ewentualnie go alokujemy
	if(in==NULL) if(!OpenIn()) 
	{
		SetErrorCode(ENetErrorNotAllocated,__LINE__);
		return NULL;
	}

	//sprawdzamy czy mamy otwarty chunk
	if(in_chunk.ptr==NULL) 
	{
		//jesli nie to sprawdzamy czy da sie parsowac dane 
		//if(!Parse())
		{
			SetErrorCode(ENetErrorOutOfData,__LINE__);
			return NULL;
		}
	}

	//jest za malo danych
	if(in->ToParse()<in_chunk.offset+size)
	{
		SetErrorCode(ENetErrorOutOfData,__LINE__);
		return NULL;
	}

	//przygotowujemy ptr na dane
	BYTE * ret=(BYTE*)&(in_chunk.ptr[in_chunk.offset]);
	//zwiekszamy rozmiar chunka
	in_chunk.offset+=size;
	//jesli doszlismy do konca czunka to zamykamy go
	if(in_chunk.offset==in_chunk.size) E();
	return ret;
}
/***************************************************************************/
BYTE * GNetMsg::TPos(DWORD32 size)
{
	//test nie moze nic alokowac, wiec jesli nie ma zdefiniwanego chunka to cos jest nie tak
	if(test_chunk.ptr==NULL) 
		throw GNetTestException(ENetFormatErrorOutOfData);
	//przygotowujemy dane i zwracamy wartosc
	if(test_chunk.offset+size>test_chunk.size) 
		throw GNetTestException(ENetFormatErrorOutOfData);
	BYTE * ret=(BYTE*)&(test_chunk.ptr[test_chunk.offset]);
	test_chunk.offset+=size;
	return ret;
}
/***************************************************************************/
GNetMsg & GNetMsg::A()
{
	if(IsError()) return *this;
	FlushBitsCache();
	//jesli chunk nie jest otwarty to cos jest skopane
	if(out_chunk.ptr==NULL)
	{
		SetErrorCode(ENetErrorChunkNotInitialized,__LINE__);
		return *this;
	}

	if(flags[ENetCryptOut] && crypt_out.GetOffset())
	{
		int len=out_chunk.offset-4;
		len=len%crypt_out.GetOffset();
		if(len) len=crypt_out.GetOffset()-len;
		Fill(len);
	}
	//ustawiamy rozmiar chunka
	*((DWORD32 *)out_chunk.ptr)=out_chunk.offset;
	out_chunk.size=out_chunk.offset;
	last_chunk=out_chunk;
	test_chunk=out_chunk;
	bool error=false;
	if(flags[ENetTestOut]) if(!TestMsg(true))
	{
		SetErrorCode(ENetErrorIncorrectFormat,__LINE__);
		error=true;
	}
	if(!error)
	{
		//zwiekszamy obszar uzytej pamieci wyjscia
		out->IncreaseUsed(out_chunk.size);
		if(flags[ENetCryptOut]) crypt_out<<last_chunk;
	}
	out_chunk.Reset();
	return *this;
}
/***************************************************************************/
GNetMsg & GNetMsg::E()
{
	//funkcjonalnosc parse jest nieco wieksza od tej funkcji

	//sprawdza czy jest otwarty chunk
	if(in_chunk.ptr==NULL) return *this;
	//jestesmy na koncu, ustawia status
	if(in_chunk.size==in_chunk.offset) status=ENetStatusFinished;
	else status=ENetStatusSkipped;
	//zwieksza limit wczytanych danych 
	in->IncreaseParsed(in_chunk.size);
	//resetuje chunka
	in_chunk.Reset();
	return * this;
}
/***************************************************************************/
bool GNetMsg::TestMsg(bool /*out*/)
{
	//sprawdzamy czy jest ustawiony chunk testowy
	if(test_chunk.ptr==NULL) return false;

	test_chunk.offset=0;
	test_chunk.bits.UA();
	test_chunk.bits_count=0;
	test_chunk.offset=4;

	int msg=TI();
	//if(msg<=0 || msg>=IGM_MSG_LAST) 
	//{
	//	FormatError();
	//	return false;
	//}
	error.current_in=msg;
	SNetMsgDesc * format;
	format=msg_base.Get(msg,cmd_type);
	if(format==NULL) 
	{
		format_error="Format string not found";

		if(test_chunk.ptr!=NULL)
		{
			string buffer;
			char buf[64];
			sprintf(buf," Size: %d ",test_chunk.size);
			buffer+=buf;
			buffer+=" Data: ";
			int a;
			int v=test_chunk.size;
			if(v>100) v=100;
			for (a=0;a<v;a++)
			{
				BYTE b=(BYTE)(test_chunk.ptr[a]);
				char buf[10];
				sprintf(buf,"%03d,",b);
				buffer+=buf;
			}
			format_error+=buffer;
		}

		FormatError(format_error);
		return false;
	}
	error.last_in=msg;

	string * p=NULL;
	try{
		//staramy sie go sparsowac
		ParseFormat(format,p);
		//if(test_chunk.offset!=test_chunk.size) throw(GNetTestException(ENetFormatErrorOutOfFormat,test_chunk.offset));
	}
	catch(GNetTestException &)
	{
		//jesli cos jeblo to robimy jeszcze raz parsowanie i budujemy string z opisem,
		FormatError("");
		test_chunk.offset=0;
		test_chunk.bits.UA();
		test_chunk.bits_count=0;
		test_chunk.offset=8;
		string parsed;
		try{
			ParseFormat(format,&parsed);
			if(test_chunk.offset!=test_chunk.size) throw(GNetTestException(ENetFormatErrorOutOfFormat,test_chunk.offset));
		}
		catch(GNetTestException &a)
		{
			switch(a.error_code)
			{
			case ENetFormatErrorIncorrectIterator:format_error="ENetFormatErrorIncorrectIterator  ";break;
			case ENetFormatErrorIncorrectFormat:format_error="ENetFormatErrorIncorrectFormat  ";break;
			case ENetFormatErrorOutOfFormat:
				{
					format_error="ENetFormatErrorOutOfFormat  ";
					char buf[32];
					sprintf(buf,"(%d/%d)  ",test_chunk.offset,test_chunk.size);
					format_error+=buf;
				}
				break;
			case ENetFormatErrorOutOfData:format_error="ENetFormatErrorOutOfData  ";break;
			case ENetFormatErrorOutOfLimit:format_error="ENetFormatErrorOutOfLimit  ";break;
			default: format_error="ENetFormatErrorNone"; break;
			}
		};
		format_error+=format->name;
		format_error+="   ";
		format_error+=parsed;

		if(test_chunk.ptr!=NULL)
		{
			string buffer;
			char buf[64];
			sprintf(buf," Size: %d ",test_chunk.size);
			buffer+=buf;
			buffer+=" Data: ";
			int a;
			int v=test_chunk.size;
			if(v>100) v=100;
			for (a=0;a<v;a++)
			{
				BYTE b=(BYTE)(test_chunk.ptr[a]);
				char buf[10];
				sprintf(buf,"%03d,",b);
				buffer+=buf;
			}
			format_error+=buffer;
		}
		last_format_error=format_error;
		FormatError(format_error);

		return false;
	}
	last_parsed_message=msg;
	return true;
}
/***************************************************************************/
#define POS_INC			pos++;if(pos>fl) throw(GNetTestException(ENetFormatErrorOutOfData,pos))
#define POS_O			format->commands[pos]
#define POS_L			format->commands.size()
//iterujemy przez chunka, sprawdzajac zakresy i poprawnosc danych
int GNetMsg::ParseFormat(SNetMsgDesc * format,string * parsed,DWORD pos)
{
	DWORD begin=pos;
	int last_possible_mul=0;
	bool fast_up=true;
	if(pos==0) fast_up=false;
	while(pos<POS_L)
	{
		switch(POS_O.mode)
		{
		case ENB_start:
			{
				if(parsed) *parsed+='(';
				fast_up=false;
			}
			break;
		case ENB_mul:
			{
				int c;
				int repeat=last_possible_mul;
				if(parsed) *parsed+='*';
				if(POS_O.repeat!=-1) repeat=POS_O.repeat;
				for (c=0;c<repeat;) 
				{
					int ret=ParseFormat(format,parsed,pos+1);
					c++;
					if(c==repeat) pos+=ret;
				}
				if(repeat==0)
				{
					int level = 0;				// poziom zagniezdzenia nawiasow
					if(pos<POS_L+1)
					if(format->commands[pos+1].mode==ENB_start)
					{
						pos++;
						while(pos<POS_L)
						{
							if (POS_O.mode==ENB_start)
								level++;
							if (POS_O.mode==ENB_end && --level == 0)
							{
								break;
							}
							pos++;
						}
					}
					if(parsed) *parsed+="()";
				}
			}
			break;
		case ENB_b:
			{
				if(parsed) *parsed+='b';
				Tb();
			}
			break;
		case ENB_c:
			{
				char c=TC();last_possible_mul=c;
				if(parsed) *parsed+='c';
				if(POS_O.use_lim) 
				{
					if(parsed) 
					{
						char buffer[65];
						*parsed+='[';
						_i64toa(POS_O.ilim[0],buffer,10);
						*parsed+=buffer;
						*parsed+=',';
						*parsed+='<';
						_i64toa(c,buffer,10);
						*parsed+=buffer;
						*parsed+='>';
						*parsed+=',';
						_i64toa(POS_O.ilim[1],buffer,10);
						*parsed+=buffer;
						*parsed+=']';
					}

					if(c<POS_O.ilim[0] || c>POS_O.ilim[1]) throw(GNetTestException(ENetFormatErrorOutOfLimit,pos));
				}
			}
			break;
		case ENB_uc:
			{
				BYTE c=TU();last_possible_mul=c;
				if(parsed) *parsed+='u';
				if(parsed) *parsed+='c';
				if(POS_O.use_lim) 
				{
					if(parsed)
					{
						*parsed+='[';
						char buffer[65];
						_i64toa(POS_O.ilim[0],buffer,10);
						*parsed+=buffer;
						*parsed+=',';
						*parsed+='<';
						_i64toa(c,buffer,10);
						*parsed+=buffer;
						*parsed+='>';
						*parsed+=',';
						_i64toa(POS_O.ilim[1],buffer,10);
						*parsed+=buffer;
						*parsed+=']';
					}
					if(c<POS_O.ilim[0] || c>POS_O.ilim[1]) throw(GNetTestException(ENetFormatErrorOutOfLimit,pos));
				}
			}
			break;
		case ENB_s:
			{
				SHORT16 c=TS();last_possible_mul=c;
				if(parsed) *parsed+='s';
				if(POS_O.use_lim) 
				{
					if(parsed) 
					{
						*parsed+='[';
						char buffer[65];
						_i64toa(POS_O.ilim[0],buffer,10);
						*parsed+=buffer;
						*parsed+=',';
						*parsed+='<';
						_i64toa(c,buffer,10);
						*parsed+=buffer;
						*parsed+='>';
						*parsed+=',';
						_i64toa(POS_O.ilim[1],buffer,10);
						*parsed+=buffer;
						*parsed+=']';
					}

					if(c<POS_O.ilim[0] || c>POS_O.ilim[1]) throw(GNetTestException(ENetFormatErrorOutOfLimit,pos));
				}
			}
			break;
		case ENB_us:
			{
				USHORT16 c=TUS();last_possible_mul=c;
				if(parsed) *parsed+='u';
				if(parsed) *parsed+='s';
				if(POS_O.use_lim) 
				{
					if(parsed) 
					{
						*parsed+='[';
						char buffer[65];
						_i64toa(POS_O.ilim[0],buffer,10);
						*parsed+=buffer;
						*parsed+=',';
						*parsed+='<';
						_i64toa(c,buffer,10);
						*parsed+=buffer;
						*parsed+='>';
						*parsed+=',';
						_i64toa(POS_O.ilim[1],buffer,10);
						*parsed+=buffer;
						*parsed+=']';
					}

					if(c<POS_O.ilim[0] || c>POS_O.ilim[1]) throw(GNetTestException(ENetFormatErrorOutOfLimit,pos));
				}
			}
			break;
		case ENB_i:
			{
				INT32 c=TI();last_possible_mul=c;
				if(parsed) *parsed+='i';
				if(POS_O.use_lim) 
				{
					if(parsed) 
					{
						*parsed+='[';
						char buffer[65];
						_i64toa(POS_O.ilim[0],buffer,10);
						*parsed+=buffer;
						*parsed+=',';
						*parsed+='<';
						_i64toa(c,buffer,10);
						*parsed+=buffer;
						*parsed+='>';
						*parsed+=',';
						_i64toa(POS_O.ilim[1],buffer,10);
						*parsed+=buffer;
						*parsed+=']';
					}

					if(c<POS_O.ilim[0] || c>POS_O.ilim[1]) throw(GNetTestException(ENetFormatErrorOutOfLimit,pos));
				}
			}
			break;
		case ENB_ui:
			{
				DWORD c=TUI();last_possible_mul=c;
				if(parsed) *parsed+='u';
				if(parsed) *parsed+='i';
				if(POS_O.use_lim) 
				{
					if(parsed) 
					{
						*parsed+='[';
						char buffer[65];
						_i64toa(POS_O.ilim[0],buffer,10);
						*parsed+=buffer;
						*parsed+=',';
						*parsed+='<';
						_i64toa(c,buffer,10);
						*parsed+=buffer;
						*parsed+='>';
						*parsed+=',';
						_i64toa(POS_O.ilim[1],buffer,10);
						*parsed+=buffer;
						*parsed+=']';
					}

					if(c<POS_O.ilim[0] || c>POS_O.ilim[1]) throw(GNetTestException(ENetFormatErrorOutOfLimit,pos));
				}
			}
			break;
		case ENB_f:
			{
				float c=TF();
				if(parsed) *parsed+='f';
				if(POS_O.use_lim) 
				{
					if(parsed) 
					{
						*parsed+='[';
						char buffer[65];
						_gcvt(POS_O.flim[0],5,buffer);
						*parsed+=buffer;
						*parsed+=',';
						*parsed+='<';
						_gcvt(c,5,buffer);
						*parsed+=buffer;
						*parsed+='>';
						*parsed+=',';
						_gcvt(POS_O.flim[1],5,buffer);
						*parsed+=buffer;
						*parsed+=']';
					}

					if(c<POS_O.flim[0] || c>POS_O.flim[1]) throw(GNetTestException(ENetFormatErrorOutOfLimit,pos));
				}
			}
			break;
		case ENB_d:
			{
				double c=TD();
				if(parsed) *parsed+='d';
				if(POS_O.use_lim) 
				{
					if(parsed) 
					{
						*parsed+='[';
						char buffer[65];
						_gcvt(POS_O.flim[0],5,buffer);
						*parsed+=buffer;
						*parsed+=',';
						*parsed+='<';
						_gcvt(c,5,buffer);
						*parsed+=buffer;
						*parsed+='>';
						*parsed+=',';
						_gcvt(POS_O.flim[1],5,buffer);
						*parsed+=buffer;
						*parsed+=']';
					}

					if(c<POS_O.flim[0] || c>POS_O.flim[1]) throw(GNetTestException(ENetFormatErrorOutOfLimit,pos));
				}
			}
			break;
		case ENB_I:
			{
				INT64 c=TBI();last_possible_mul=(int)c;
				if(parsed) *parsed+='I';
				if(POS_O.use_lim) 
				{
					if(parsed) 
					{
						*parsed+='[';
						char buffer[65];
						_i64toa(POS_O.ilim[0],buffer,10);
						*parsed+=buffer;
						*parsed+=',';
						*parsed+='<';
						_i64toa(c,buffer,10);
						*parsed+=buffer;
						*parsed+='>';
						*parsed+=',';
						_i64toa(POS_O.ilim[1],buffer,10);
						*parsed+=buffer;
						*parsed+=']';
					}

					if(c<POS_O.ilim[0] || c>POS_O.ilim[1]) throw(GNetTestException(ENetFormatErrorOutOfLimit,pos));
				}
			}
			break;
		case ENB_UI:
			{
				DWORD64 c=TBUI();last_possible_mul=(int)c;
				if(parsed) *parsed+='U';
				if(parsed) *parsed+='I';
				if(POS_O.use_lim) 
				{
					if(parsed) 
					{
						*parsed+='[';
						char buffer[65];
						_ui64toa(POS_O.ilim[0],buffer,10);
						*parsed+=buffer;
						*parsed+=',';
						*parsed+='<';
						_ui64toa(c,buffer,10);
						*parsed+=buffer;
						*parsed+='>';
						*parsed+=',';
						_ui64toa(POS_O.ilim[1],buffer,10);
						*parsed+=buffer;
						*parsed+=']';
					}

					if((INT64)c<POS_O.ilim[0] || (INT64)c>POS_O.ilim[1]) throw(GNetTestException(ENetFormatErrorOutOfLimit,pos));
				}
			}
			break;
		case ENB_o:
			{
				NPair p;
				T(p);
				if(parsed) *parsed+='o';
				if(POS_O.use_lim) 
				{
					if(parsed)
					{
						*parsed+='[';
						char buffer[65];
						_ui64toa(POS_O.ilim[0],buffer,10);
						*parsed+=buffer;
						*parsed+=',';
						*parsed+='<';
						_ui64toa(p.size,buffer,10);
						*parsed+=buffer;
						*parsed+='>';
						*parsed+=']';
					}
					if(p.size>POS_O.ilim[0]) throw(GNetTestException(ENetFormatErrorOutOfLimit,pos));
				}
			}
			break;
		case ENB_r:
			{
				Tr((int)POS_O.ilim[0]);
				if(parsed) 
				{
					*parsed+='r';
					*parsed+='[';
					char buffer[65];
					_ui64toa(POS_O.ilim[0],buffer,10);
					*parsed+=buffer;
					*parsed+=']';
				}
			}
			break;
		case ENB_e:
			{
				Te((int)POS_O.ilim[0]);
				if(parsed) 
				{
					*parsed+='e';
					*parsed+='[';
					char buffer[65];
					_ui64toa(POS_O.ilim[0],buffer,10);
					*parsed+=buffer;
					*parsed+=']';
				};
			}
			break;
		case ENB_end:
			{
				if(parsed) *parsed+=')';
				pos++;
				return pos-begin;
			}
			break;
		case ENB_t:
			{
				INT32 c=TI();last_possible_mul=c;
				if(parsed) *parsed+='t';
				if(POS_O.use_lim) 
				{
					if(parsed) 
					{
						*parsed+='[';
						char buffer[65];
						_i64toa(POS_O.ilim[0],buffer,10);
						*parsed+=buffer;
						*parsed+=',';
						*parsed+='<';
						_i64toa(c,buffer,10);
						*parsed+=buffer;
						*parsed+='>';
						*parsed+=']';
					}
					if(c<0 || c>POS_O.ilim[0]) throw(GNetTestException(ENetFormatErrorOutOfLimit,pos));
				}
				TPos(c);
			}
			break;
		//case ENB_r,
		//case ENB_value,
		//case ENB_start,
		//case ENB_end,
		//case ENB_mul,
		//case ENB_none,
			default:throw(GNetTestException(ENetFormatErrorIncorrectFormat,pos));
		}
		pos++;
		if(fast_up) return pos-begin;
	}//while 
	return (pos-begin);
}
/***************************************************************************/
bool GNetMsg::FromHex(BYTE * hex, int size)
{
	GMemory * in=InAlloc(size);
	if(in==NULL) return false;

	INT32 a;
	BYTE * buf=hex;
	BYTE * hbuf=(BYTE*)in->Start();
	int pos=0;
	for (a=0;a<size;a++)
	{
		BYTE v1=(buf[pos]<'A')?(buf[pos]-'0'):(buf[pos]-'A'+10);
		pos++;
		BYTE v2=(buf[pos]<'A')?(buf[pos]-'0'):(buf[pos]-'A'+10);
		pos++;
		BYTE v=v1<<4|v2;
		hbuf[a]=v;
	}
	in->IncreaseUsed(size);
	return true;
}
/***************************************************************************/
bool GNetMsg::ToHex(BYTE * hex, int size)
{
	int code_pos=0;
	DWORD a;
	BYTE * hptr=(BYTE*)out->Start();
	for (a=0;a<out->Used();a++)
	{
		hex[code_pos++]=vecLower[hptr[a]>>4];
		if(code_pos==size) return false;
		hex[code_pos++]=vecLower[hptr[a]&0x0F];
		if(code_pos==size) return false;
		hex[code_pos]=0;
	}
	return true;
}
/***************************************************************************/




















/***************************************************************************/
//parsujemy string opisujacy format msg'a i zamieniany go na wektor
//przeprowadzamy wstepny parsing formatu, sprawdzamy takie detale jak parowanie nawiasow np.
void SNetMsgDesc::Build(GNetParserBase * base,ENetCmdType cmd_type)
{
	stack<int> brackets;
	char * f=format;
	while(f[0])
	{
		if (f[0]>='1' && f[0]<='9')
		{
			char len[16];
			memset(len,0,16);
			int count=0;
			while(f[0]>='0'&& f[0]<='9')
			{
				len[count++]=f[0];
				f++;
			}
			int repeat=ATOI(len);
			commands.push_back(SNetBasic(ENB_mul));
			commands[commands.size()-1].repeat=repeat;
		}
		else
		switch(f[0])
		{
		case ' ':
		case '-':
		case '_':
		case '+':
			f++;
			break;
		case '/':
			{
				f++;
				char * end_comment=strchr(f,'/');
				assert(end_comment!=NULL);
				f=end_comment;
				f++;
			}
			break;
		case '<':
			{
				f++;
				char * end_sub=strchr(f,'>');
				assert(end_sub!=NULL);
				size_t len=(end_sub-f);
				char sub[128];
				strncpy(sub,f,len);
				sub[len]=0;
				f=end_sub;
				f++;

				SNetMsgDesc * s=base->GetSub(sub,cmd_type);
				assert(s!=NULL);
				assert(s->commands.size());

				DWORD32 pos=0;
				while(pos<s->commands.size()) commands.push_back(s->commands[pos++]);
			}
			break;
		case '(':
			f++;
			commands.push_back(SNetBasic(ENB_start));
            brackets.push((int)commands.size()-1);
			break;
		case ')':
			f++;
			commands.push_back(SNetBasic(ENB_end));
			assert(brackets.size()>0);
			commands[brackets.top()].ref=(int)commands.size();
			commands[commands.size()-1].ref=brackets.top();
			brackets.pop();
			break;
		case '*':
		case 'x':
			f++;
			commands.push_back(SNetBasic(ENB_mul));
			commands[commands.size()-1].repeat=-1;
			break;
		case 'u':
		case 'U':
			{
				f++;
				int c=f[0];
				switch(c)
				{
					case 'c':BuildElement(ENB_uc,f);break;
					case 's':BuildElement(ENB_us,f);break;
					case 'i':BuildElement(ENB_ui,f);break;
					case 'I':BuildElement(ENB_UI,f);break;
					default: assert(0);
				}
			}
			break;
		case 'b':BuildElement(ENB_b,f);break;
		case 'c':BuildElement(ENB_c,f);break;
		case 's':BuildElement(ENB_s,f);break;
		case 'i':BuildElement(ENB_i,f);break;
		case 'f':BuildElement(ENB_f,f);break;
		case 'd':BuildElement(ENB_d,f);break;
		case 'I':BuildElement(ENB_I,f);break;
		case 't':BuildElement(ENB_t,f);break;
		case 'o':BuildElement(ENB_o,f);break;
		case 'e':BuildElement(ENB_e,f);break;
		case 'r':BuildElement(ENB_r,f);break;
		default:assert(0);
		}
	}
	assert(brackets.size()==0);
}
/***************************************************************************/
void SNetMsgDesc::BuildElement(ENetBasicType mode, char*& f)
{
	f++;       
	switch(mode)
	{
		case ENB_b:
		case ENB_c:
		case ENB_s:
		case ENB_i:
		case ENB_I:
		case ENB_uc:
		case ENB_us:
		case ENB_ui:
		case ENB_UI:
			{
				commands.push_back(SNetBasic(mode));
				if(f[0]=='[')
				{
					f++;
					char len[16];
					memset(len,0,16);
					int count=0;
					while(f[0]>='0'&& f[0]<='9')
					{
						len[count++]=f[0];
						f++;
					}
					INT64 lim1=ATOI64(len);

					assert(f[0]==',');
					f++;

					assert(mode==ENB_uc && lim1<0);
					assert(mode==ENB_us && lim1<0);
					assert(mode==ENB_ui && lim1<0);
					assert(mode==ENB_UI && lim1<0);

					memset(len,0,16);
					count=0;
					while(f[0]>='0'&& f[0]<='9')
					{
						len[count++]=f[0];
						f++;
					}
					INT64 lim2=ATOI64(len);

					assert(f[0]==']');
					f++;
					commands[commands.size()-1].use_lim=true;
					commands[commands.size()-1].ilim[0]=lim1;
					commands[commands.size()-1].ilim[1]=lim2;
				}
			}
			break;
		case ENB_f:
		case ENB_d:
			{
				commands.push_back(SNetBasic(mode));
				if(f[0]=='[')
				{
					f++;
					char len[16];
					memset(len,0,16);
					int count=0;
					while(f[0]>='0'&& f[0]<='9')
					{
						len[count++]=f[0];
						f++;
					}
					double lim1=atof(len);

					assert(f[0]==',');
					f++;

					memset(len,0,16);
					count=0;
					while(f[0]>='0'&& f[0]<='9')
					{
						len[count++]=f[0];
						f++;
					}
					double lim2=atof(len);

					assert(f[0]==']');
					f++;
					commands[commands.size()-1].use_lim=true;
					commands[commands.size()-1].flim[0]=lim1;
					commands[commands.size()-1].flim[1]=lim2;
				}
			}
			break;

		case ENB_t:
		case ENB_o:
		case ENB_e:
			{
				commands.push_back(SNetBasic(mode));
				if(mode==ENB_e)
				{
					assert(f[0]=='[');
				}
				if(f[0]=='[')
				{
					f++;
					char len[16];
					memset(len,0,16);
					int count=0;
					while(f[0]>='0'&& f[0]<='9')
					{
						len[count++]=f[0];
						f++;
					}
					int lim1=ATOI(len);

					assert(f[0]==']');
					f++;
					commands[commands.size()-1].use_lim=true;
					commands[commands.size()-1].ilim[0]=lim1;
				}
			}
			break;
		case ENB_r:
			{
				commands.push_back(SNetBasic(mode));
				if(f[0]=='[')
				{
					f++;
					char len[16];
					memset(len,0,16);
					int count=0;
					while(f[0]>='0'&& f[0]<='9')
					{
						len[count++]=f[0];
						f++;
					}
					int lim1=ATOI(len);

					assert(f[0]==']');
					f++;
					commands[commands.size()-1].use_lim=true;
					commands[commands.size()-1].ilim[0]=lim1;
				}
				else
				{
					commands[commands.size()-1].use_lim=true;
					commands[commands.size()-1].ilim[0]=1;
				}
			}
			break;
	}
}


















/***************************************************************************/
GNetParserBase::GNetParserBase()
{
	int a;
	for (a=0;a<msg_game_sub_count;a++)
	{
		msg_game_sub_default[a].Build(this,ENetCmdGame);
		sub_msg_commands.insert(make_pair((int)hash_str(msg_game_sub_default[a].name),&msg_game_sub_default[a]));
	}

	for (a=0;a<msg_game_count;a++)
	{
		msg_game_default[a].Build(this,ENetCmdGame);
		msg_commands.insert(make_pair(msg_game_default[a].msgid,&msg_game_default[a]));
	}

#ifdef SERVER
	for (a=0;a<msg_internal_sub_count;a++)
	{
		msg_internal_sub_default[a].Build(this,ENetCmdInternal);
		sub_internal_msg_commands.insert(make_pair((int)hash_str(msg_internal_sub_default[a].name),&msg_internal_sub_default[a]));
	}
	for (a=0;a<msg_internal_count;a++)
	{
		msg_internal_default[a].Build(this,ENetCmdInternal);
		internal_msg_commands.insert(make_pair(msg_internal_default[a].msgid,&msg_internal_default[a]));
	}
#endif
}
/***************************************************************************/
GNetParserBase::~GNetParserBase()
{
}
/***************************************************************************/
#ifdef SERVER
SNetMsgDesc * GNetParserBase::Get(int msgid,ENetCmdType cmd_type)
#else
SNetMsgDesc * GNetParserBase::Get(int msgid,ENetCmdType)
#endif
{
#ifdef SERVER
	if(cmd_type==ENetCmdInternal) return internal_msg_commands[msgid];
#endif
	return msg_commands[msgid];
}
/***************************************************************************/
#ifdef SERVER
SNetMsgDesc * GNetParserBase::GetSub(char * msgid,ENetCmdType cmd_type)
#else
SNetMsgDesc * GNetParserBase::GetSub(char * msgid,ENetCmdType )
#endif
{
	map<int,SNetMsgDesc*>::iterator pos;

#ifdef SERVER
	if(cmd_type==ENetCmdInternal) 
	{
		pos=sub_internal_msg_commands.find((int)hash_str(msgid));
		if(pos==sub_internal_msg_commands.end()) return NULL;
		return sub_internal_msg_commands[(int)hash_str(msgid)];
	}
	else
#endif
	{
		pos=sub_msg_commands.find((int)hash_str(msgid));
		if(pos==sub_msg_commands.end()) return NULL;
		return sub_msg_commands[(int)hash_str(msgid)];
	}
}
/***************************************************************************/
GNetMsg& GNetTarget::W(GNetMsg & msg)
{
	msg.WBUI(number).WBUI(timestamp).WI(tg).WI(tp).WI(tp_id);
	return msg;
};
/***************************************************************************/
GNetMsg& GNetTarget::R(GNetMsg & msg)
{
	msg.RBUI(number).RBUI(timestamp).RI(tg).RI(tp).RI(tp_id);
	return msg;
};
/***************************************************************************/
GNetMsg & GNetStructure::W(GNetMsg & msg)
{
	return msg;
};
/***************************************************************************/
GNetMsg & GNetStructure::R(GNetMsg & msg)
{
	return msg;
};
/***************************************************************************/
GNetMsg & GNetStructure::WE(GNetMsg & msg)
{
	return W(msg);
};
/***************************************************************************/
GNetMsg & GNetStructure::RE(GNetMsg & msg)
{
	return R(msg);
};
/***************************************************************************/
GNetMsg & GNetStructure::operator<<(GNetMsg & msg)
{
	W(msg);
	return msg;
}
/***************************************************************************/
GNetMsg & GNetStructure::operator>>(GNetMsg & msg)
{
	R(msg);
	return msg;
}
/***************************************************************************/


/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"
#include "headers_game.h"
#include "utils_conversion.h"
extern GServerBase * global_server;

static char * flash_policy_request = "<policy-file-request/>";

static char * flash_policy_response = 
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
	"<!DOCTYPE cross-domain-policy SYSTEM \"http://www.macromedia.com/xml/dtds/cross-domain-policy.dtd\">\r\n"
	"<cross-domain-policy>\r\n"
	"<allow-access-from domain=\"*\" to-ports=\"*\" />\r\n"
	"</cross-domain-policy>\r\n";

#include "include_ssl.h"
/***************************************************************************/
void GSocketAccess::Parse(GRaportInterface & raport_interface)
{
	INT32 pre_message=0;

	// Obsluga <policy-file-request/>
	// Moze on przyjsc tylko na samym poczatku polaczenia. Rozpoznajemy to po tym,
	// ze gdy juz sparsowalismy jakis message to msg.CurrentChunk() bedzie nie pusty.
	if (msg.CurrentChunk().size == 0 && !(flags[ESockExit] || flags[ESockDisconnect]))
	{
		unsigned int len = strlen(flash_policy_request) + 1;
		if (memory_in.Used() >= len &&
			memory_in.ToParse() >= len &&
			strncmp(memory_in.Parse(), flash_policy_request, len) == 0)
		{
			memory_in.IncreaseParsed(len);
			memory_in.RemoveParsed();

			len = strlen(flash_policy_response) + 1;
			if (memory_out.Free() < len)
			{
				memory_out.ReallocateToFit(len);
			}
			memcpy(memory_out.End(), flash_policy_response, len);
			memory_out.IncreaseUsed(len);
		}
	}

	if (!GSERVER->Continue() || GSERVER->flags[EFPrepareToClose])
	{
		flags.set(ESockDisconnect);
	}

	while(msg.Parse(pre_message) && !(flags[ESockExit] || flags[ESockDisconnect]) )
	{
		INT32 message=0;
		msg.RI(message);
		if(!msg.GetErrorCode())
			switch(message)
		{
			case IGM_CONNECT:
				{
					timestamp_igm_connect = GSERVER->GetClock().Get();

					INT32 gv;
					INT32 LID;
					INT32 gr;
					msg.RI(gr).RI(gv).RI(LID);
					if(game_version_number>gv) 
					{
						msg.WI(IGM_MESSAGE_DIALOG).WI(DLG_ERROR_VERSION_END).WT("old protocol version").WI(game_version_number).A();
						flags.set(ESockError);
					}
					else
					msg.WI(IGM_ACCEPT).W(NPair(GSERVER->RSA_bits,32)).W(NPair(GSERVER->RSA_crypt,RSA_BYTES)).A();
				}
				break;
			case IGM_ACCEPT_ERROR:
				{
					BYTE RSA_bits[32];
					BYTE RSA_crypt[RSA_BYTES];
					msg.R(NPair(RSA_bits,32)).R(NPair(RSA_crypt,RSA_BYTES));
					flags.set(ESockError);
					SRAP(ERROR_DATA_CLIENT_MSG_RSA);
					RAPORT(GSTR(ERROR_DATA_CLIENT_MSG_RSA));
				}
				break;
			case IGM_SECRET:
				{
					BYTE RSA_decrypt[RSA_BYTES];
					BYTE RSA_decrypt_tmp[RSA_BYTES];
					msg.R(NPair(RSA_decrypt,RSA_BYTES));
					int size = RSA_private_decrypt(RSA_BYTES,RSA_decrypt,RSA_decrypt_tmp,GSERVER->priv_key,RSA_PKCS1_PADDING);
					if(size!=36) 
					{
						SRAP(ERROR_DATA_MSG_RSA);
						RAPORT(GSTR(ERROR_DATA_MSG_RSA));
					}
					else
					{
						ENetCryptType	crypt_mode;
						memcpy(&crypt_mode,RSA_decrypt_tmp,4);
						if(crypt_mode>=ENetCryptNone)
						{
							SRAP(ERROR_DATA_MSG_RSA);
							RAPORT(GSTR(ERROR_DATA_MSG_RSA));
						}
						else
						switch(crypt_mode)
						{
							case ENetCryptXOR:data.keys.XOR_key.Buf(&RSA_decrypt_tmp[4]);break;
							case ENetCryptAES:data.keys.AES_key.Buf(&RSA_decrypt_tmp[4]);break;
							case ENetCryptDES:data.keys.DES_key.Buf(&RSA_decrypt_tmp[4]);break;
							case ENetCryptDES3:data.keys.DES_key.Buf(&RSA_decrypt_tmp[4]);break;
						}

						SetCryptMode(crypt_mode);
					}
				}
				break;
			case IGM_PING:
				{
					msg.WI(IGM_PING).WI(0).WI(0).A();
				}
				break;
			case IGM_DISCONNECT:
				{
					flags.set(ESockDisconnect);
				}
				break;
			default:
				{
					GLogic* logic=Logic();
					GPlayerAccess * player=static_cast<GPlayerAccess*>(logic);
					player->ProcessMessage(msg,message);
				}
				break;
		}
	}
	if(memory_in.Used()==0) DeallocateIn();
	TestWrite();

	if(AnalizeMsgError(raport_interface))
	{
		msg.SetCryptOut(ENetCryptNone);
		flags.set(ESockError);
	}
}


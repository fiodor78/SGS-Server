/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/


/***************************************************************************/
class GTicketConnectionClient: public GTicketConnection
{
public:
	GTicketConnectionClient(){Zero();};
	GTicketConnectionClient(INT32 /*message_p*/, GNetTarget & /*target_p*/):GTicketConnection(){Zero();};
	GNetMsg& W(GNetMsg & msg)
	{
		GTicketConnection::W(msg);
		return msg;
	}
	GNetMsg& R(GNetMsg & msg)
	{
		GTicketConnection::RE(msg);
		return msg;
	}
	void Zero()
	{
		GTicketConnection::Zero();
	}
};
typedef boost::shared_ptr<GTicketConnectionClient> GTicketConnectionClientPtr;
/***************************************************************************/
inline void Coerce(GTicketConnection &t,GTicketConnectionClient & tc)
{
	t.client_host = tc.client_host;
	t.server_host = tc.server_host;
	t.access = tc.access;
}
/***************************************************************************/

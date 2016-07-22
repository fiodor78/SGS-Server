/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"
#include "headers_game.h"

extern GServerBase * global_server;

//#include "headers_all.h"
#ifdef SERVICES
#include "gmsg_internal.h"
#endif

extern GServerBase * global_server;
extern GNetParserBase	msg_base;
/***************************************************************************/
GTicketInternalClient::GTicketInternalClient(GRaportInterface & raport_interface_):GSocketInternalClient(raport_interface_)
{
	sequence_number=1;
	last_latency = 0;
	last_connection_activity = GSERVER->GetClock().Get();
	connection_established = false;
}
/***************************************************************************/
void GTicketInternalClient::Ticket(GTicketPtr ticket)
{
	last_connection_activity = GSERVER->GetClock().Get();
	ticket->target.number=sequence_number++;
	out.push_back(ticket);
}
/***************************************************************************/
bool GTicketInternalClient::FlushTicket(GTicketPtr ticket)
{
	if(IsConnection() && memory_out.Free()>K1)
	{
		if(ticket->message>IGMI_FIRST && ticket->message<IGMI_LAST)
		{
			SRAP(INFO_LOGIC_MSG_OUT);
			Msg().WI(ticket->message).W(*ticket).A();
		}
		else
		if(ticket->message>IGMIT_FIRST && ticket->message<IGMIT_LAST)
		{
			SRAP(INFO_LOGIC_TICKET_OUT);
			if (ticket->target.Loop()++ == 0)
			{
				ticket->target.timestamp = GSERVER->GetClock().Get();
			}
			Msg().WI(ticket->message).W(ticket->target).W(*ticket).A();
			wait.push_back(ticket);
		}
		else
		{
			WriteCore();
		}
		return false;
	}
	return true;
}
/***************************************************************************/
void GTicketInternalClient::FlushTickets()
{
	list<GTicketPtr>::iterator pos;

	if(out.size()==0) return;
	TestConnection();
	if (IsConnection())
	{
		pos=find_if(out.begin(),out.end(),boost::bind(&GTicketInternalClient::FlushTicket,this,_1));
		out.erase(out.begin(),pos);
		Write();
	}

	DWORD64 now = GSERVER->CurrentSTime();
	for (pos = out.begin(); pos != out.end(); )
	{
		if ((*pos)->send_deadline != 0 && (*pos)->send_deadline <= now)
		{
			if (dropped_ticket_callback)
			{
				dropped_ticket_callback(**pos, ETDR_DeadlineReached);
			}
			pos = out.erase(pos);
		}
		else
		{
			pos++;
		}
	}
}
/***************************************************************************/
class isTarget
{
public:
	isTarget(const DWORD64 number_p){number=number_p;};
	bool operator()(GTicketPtr ticket){return ticket->target.number==number;};
private:
	DWORD64 number;
};
/***************************************************************************/
bool GTicketInternalClient::RollbackTicket(GTicketPtr ticket, DWORD64 number, string & stable_sequence_identifier)
{
	if (ticket->target.number < number)
	{
		if (ticket->send_deadline != 0 && ticket->send_deadline <= GSERVER->CurrentSTime())
		{
			if (dropped_ticket_callback)
			{
				dropped_ticket_callback(*ticket, ETDR_DeadlineReached);
			}
		}
		else
		{
			bool repush_ticket = true;
			if (ticket->stable_sequence_identifier != "")
			{
				if (ticket->stable_sequence_identifier == stable_sequence_identifier)
				{
					repush_ticket = false;
				}

				list<GTicketPtr>::iterator it;
				for (it = out.begin(); repush_ticket && it != out.end(); it++)
				{
					if (ticket->stable_sequence_identifier == (*it)->stable_sequence_identifier)
					{
						repush_ticket = false;
					}
				}
				for (it = wait.begin(); repush_ticket && it != wait.end(); it++)
				{
					if (ticket->target.number < (*it)->target.number &&
						ticket->stable_sequence_identifier == (*it)->stable_sequence_identifier)
					{
						repush_ticket = false;
					}
				}
			}

			if (repush_ticket)
			{
				ticket->target.number = sequence_number++;
				out.push_back(ticket);
			}
			else
			{
				if (dropped_ticket_callback)
				{
					dropped_ticket_callback(*ticket, ETDR_StableSequenceMiss);
				}
			}
		}
		return false;
	}
	return true;
}
/***************************************************************************/
void GTicketInternalClient::DropTickets()
{
	list<GTicketPtr>::iterator pos;
	for (pos = out.begin(); pos != out.end(); pos++)
	{
		if (dropped_ticket_callback)
		{
			dropped_ticket_callback(**pos, ETDR_ForcedDrop);
		}
	}
	out.clear();
}
/***************************************************************************/
void GTicketInternalClient::PreProcessMessage(int message, GNetTarget & target)
{
	if(target.number==0) return;

	last_connection_activity = GSERVER->GetClock().Get();

	list<GTicketPtr>::iterator pos;
	pos=find_if(wait.begin(),wait.end(),isTarget(target.number));
	if(pos!=wait.end())
	{
		if (target.timestamp > 0)
		{
			last_latency = GSERVER->GetClock().Get() - target.timestamp;
			if (last_latency >= GSECOND_1)
			{
				SNetMsgDesc * format;
				format = msg_base.Get((*pos)->message, ENetCmdInternal);
				string m;
				if (format)
				{
					m = format->name;
				}

				RAPORT("Long Ticket acknowledgment latency: %5lld ms. %s:%s%s",
					last_latency, GetServiceName(client_service_type), m.c_str(), pos == wait.begin() ? "" : " (not in order)");
			}
		}

		if(pos!=wait.begin())
		{
			string stable_sequence_identifier = (*pos)->stable_sequence_identifier;
			wait.erase(pos);
			list<GTicketPtr>::iterator it;
			for (it = wait.begin(); it != wait.end(); )
			{
				if ((*it)->OldLoop())
				{
					if (dropped_ticket_callback)
					{
						dropped_ticket_callback(**it, ETDR_TooManyRetries);
					}
					it = wait.erase(it);
				}
				else
				{
					it++;
				}
			}
			wait.erase(wait.begin(),find_if(wait.begin(),wait.end(),boost::bind(&GTicketInternalClient::RollbackTicket,this,_1,target.number,stable_sequence_identifier)));
			SRAP(WARNING_LOGIC_ROLLBACK);
		}
		else
		{
			wait.erase(pos);
		}
	}
	else
	{
		list<GTicketPtr>::iterator pos;
		pos=find_if(out.begin(),out.end(),isTarget(target.number));
		if(pos!=out.end())
		{
			out.erase(pos);
			SRAP(WARNING_LOGIC_OUT_ERASE);
		}
	}
}
/***************************************************************************/
void GTicketInternalClient::Communicate()
{
	FlushTickets();
	GSocketInternalClient::Communicate();
	if (!connection_established && connection_mode == EConnectionEstabilished)
	{
		// Moment gdy nawiazalismy polaczenie.
		if ((int)out.size() >= global_serverconfig->net.ticket_queue_busy_limit - 10 ||
			(int)wait.size() >= global_serverconfig->net.ticket_queue_busy_limit - 10)
		{
			global_serverconfig->net.ticket_queue_busy_limit += 10;
		}
		last_connection_activity = GSERVER->GetClock().Get();
	}
	connection_established = (connection_mode == EConnectionEstabilished);
}
/***************************************************************************/
void GTicketInternalClient::ServiceInfo(strstream & s)
{
	boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
	string t;
	IsConnection()?t=to_simple_string(connected):t=to_simple_string(disconnected);
	s<<boost::format("%|-8d|%|-12s|%|-20s|%|-12s|%|-22s|%|-10d|%|-10d|%|-11d|%|-10d|")%
		client_service_group % GetServiceName(client_service_type) % AddrToString(addr)() % (IsConnection()?"Connected":"Error") %t % (sequence_number-1) % (int)out.size() % (int)wait.size() % (int)(last_latency / GSECOND_1);
	s<<lend;
}
/***************************************************************************/
void GTicketInternalClient::Close()
{
	list<GTicketPtr>::iterator pos;
	for (pos = wait.begin(); pos != wait.end(); pos++)
	{
		if (dropped_ticket_callback)
		{
			dropped_ticket_callback(**pos, ETDR_NoProcessedConfirmation);
		}
	}
	wait.clear();

	GSocketInternalClient::Close();
}
/***************************************************************************/

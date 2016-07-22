/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#pragma warning (disable: 4511)

enum ETicketDropReason
{
	ETDR_None,
	ETDR_DeadlineReached,
	ETDR_TooManyRetries,
	ETDR_StableSequenceMiss,
	ETDR_ForcedDrop,
	ETDR_NoProcessedConfirmation,
	ETDR_Custom1,
};

/*!
\brief klasa przetwarzajaca ticekety, czyli socketinternalclient z umiejetnoscia obslugi ticketow
*/
class GTicketInternalClient: public GSocketInternalClient
{
public:
	//! konstruktor
	GTicketInternalClient(GRaportInterface & raport_interface);
	//! glowna funkcja do komunikacji
	void					Communicate();
	/*! wrzuca ticket do kolejki
	\param ticket - pointer na ticket
	*/
	void					Ticket(GTicketPtr ticket);
	/*! usuwa ticket z kolejki
	\param ticket - pointer na ticket
	*/
	bool					FlushTicket(GTicketPtr);
	/*!
	usuwa wszystkie tickety z kolejki
	*/
	void					FlushTickets();
	/*!
	odbiera dane z sieci, sprawdza czy przyszla odpowiedz na ticket, jesli tak to go usuwa
	*/
	void					PreProcessMessage(int message, GNetTarget & target);
	void					Close();
	/*!
	jesli nie przyszla odpowiedz na ticket to ponownie przesuwa go do kolejki do wyslania
	*/
	bool					RollbackTicket(GTicketPtr ticket, DWORD64 number, string & stable_sequence_identifier);
	virtual void			ServiceInfo(strstream & s);

	DWORD32					GetOutQueueSize() { return out.size(); }
	DWORD32					GetWaitQueueSize() { return wait.size(); }

	void					DropTickets();

	boost::function<void (GTicket &, ETicketDropReason)> dropped_ticket_callback;

	DWORD64					last_connection_activity;

private:
	//! lista wychodzaca
	list<GTicketPtr>		out;
	//! lista oczekujaca
	list<GTicketPtr>		wait;
	//! numer sekwencji
	DWORD64					sequence_number;
	//! ostatni czas oczekiwania na odpowiedz na ticket
	DWORD64					last_latency;

	bool					connection_established;
};
#pragma warning (default: 4511)
/***************************************************************************/

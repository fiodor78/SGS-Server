/***************************************************************************
 ***************************************************************************
  (c) 2011 Ganymede Sp. z o.o.                       All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

struct SPlayerInfo: public GNetStructure
{
	SPlayerInfo(){Zero();};
	void Zero()
	{
		RID=ID=0;
		target_gamestate_ID = 0;
		target_location_ID[0] = 0;
		SID[0]=0;
		unused = 0;
		game_instance = 0;
		global_group_id = 0;
		access=EGS_UNKNOWN;
		info.Zero();
	};
	GNetMsg& W(GNetMsg & msg)
	{
		msg.WI(RID).WI(ID).WI(target_gamestate_ID).WT(target_location_ID).WT(SID);
		msg.WI(unused).WI(game_instance).WBUI(global_group_id).WI(access).WUI(info.addr_detected_by_server);
		return msg;
	}
	GNetMsg& R(GNetMsg & msg)
	{
		msg.RI(RID).RI(ID).RI(target_gamestate_ID).RT(SPair(target_location_ID, MAX_LOCATION)()).RT(SPair(SID,MAX_SID)());
		msg.RI(unused).RI(game_instance).RBUI(global_group_id).RI(access).RUI(info.addr_detected_by_server);
		return msg;
	}
	GNetMsg& WE(GNetMsg & msg)
	{
		msg.WI(RID).WI(ID).WI(target_gamestate_ID).WT(target_location_ID).WT(SID);
		msg.WI(unused).WI(game_instance).WI(access).WUI(info.addr_detected_by_server);
		return msg;
	}
	GNetMsg& RE(GNetMsg & msg)
	{
		msg.RI(RID).RI(ID).RI(target_gamestate_ID).RT(SPair(target_location_ID, MAX_LOCATION)()).RT(SPair(SID,MAX_SID)());
		msg.RI(unused).RI(game_instance).RI(access).RUI(info.addr_detected_by_server);
		return msg;
	}

	void				clear_private()
	{
		SID[0]=0;
	}
	INT32				RID;						//random id, czyli takie co sie zawsze zmienia
	INT32				ID;							//id gracza, czyli jego nr w bazie danych
	INT32				target_gamestate_ID;		//id gamestate, do którego gracz jest pod³¹czony (albo 0 jesli podlaczenie jest do lokacji)
	char				target_location_ID[MAX_LOCATION];	// id lokacji, do ktorej gracz jest podlaczony (uzywane tylko jesli target_gamestate_ID = 0)
	char				SID[MAX_SID];				//nr sesji gracza
	INT32				unused;						//nieuzywane, dawniej nr gry
	INT32				game_instance;				//nr instancji gry
	DWORD64				global_group_id;			//globalne id grupy [host, port, group_id] (uzywane w serwisie access)
	INT32				access;						//wypelniane przez serwer accessu, okresla poziom relacji ID->target_gamestate_ID

	struct SInfo
	{
		void Zero(){addr_detected_by_server=0;};
		DWORD32				addr_detected_by_server;
	} info;

	bool	operator==(SPlayerInfo & sp)
	{
		if(RID!=sp.RID) return false;
		if(ID!=sp.ID) return false;
		if (target_gamestate_ID != sp.target_gamestate_ID) return false;
		if (strncmp(target_location_ID, sp.target_location_ID, MAX_LOCATION) != 0) return false;
		if (game_instance != sp.game_instance) return false;
		if (global_group_id != sp.global_group_id) return false;
		if(access!=sp.access) return false;
		return true;
	}
	void	operator=(const SPlayerInfo & sp)
	{
		RID=sp.RID;
		ID=sp.ID;
		target_gamestate_ID = sp.target_gamestate_ID;
		memcpy(target_location_ID, sp.target_location_ID, MAX_LOCATION);
		memcpy(SID,sp.SID,MAX_SID);
		game_instance=sp.game_instance;
		global_group_id = sp.global_group_id;
		access=sp.access;
		info.addr_detected_by_server=sp.info.addr_detected_by_server;
	}
};
typedef GTicketTmpl<SPlayerInfo> GTicketPlayer;
typedef boost::shared_ptr<GTicketPlayer> GTicketPlayerPtr;
/***************************************************************************/
struct SPlayerInfoMini: public GNetStructure
{
	SPlayerInfoMini(){Zero();};
	void Zero(){
		ID=0;
	};
	GNetMsg& W(GNetMsg & msg){msg.WI(ID);return msg;}
	GNetMsg& R(GNetMsg & msg){msg.RI(ID);return msg;}

	INT32				ID;							//id gracza, czyli jego nr w bazie danych

	bool	operator==(const SPlayerInfoMini & sp)
	{
		if(ID!=sp.ID) return false;
		return true;
	}
	bool	operator!=(const SPlayerInfoMini & sp) const
	{
		if(ID==sp.ID) return false;
		return true;
	}
	void	operator=(const SPlayerInfoMini & sp)
	{
		ID=sp.ID;
	}
	void	operator=(const SPlayerInfo & sp)
	{
		ID=sp.ID;
	}
};
typedef boost::shared_ptr<SPlayerInfoMini> SPlayerInfoMiniPtr;
/***************************************************************************/
class GTicketConnection: public GTicket
{
	public:
	GTicketConnection(){Zero();};
	GTicketConnection(INT32 message_p, GNetTarget & target_p):GTicket(){Zero();message=message_p;target=target_p;};
	GNetMsg& W(GNetMsg & msg)
	{
		access.W(msg);
		client_host.W(msg);
		server_host.W(msg);
		return msg;
	}
	GNetMsg& R(GNetMsg & msg)
	{
		access.R(msg);
		client_host.R(msg);
		server_host.R(msg);
		return msg;
	}
	GNetMsg& RE(GNetMsg & msg)
	{
		access.RE(msg);
		client_host.R(msg);
		server_host.R(msg);
		return msg;
	}
	void Zero()
	{
		GTicket::Zero();
		access.Zero();
		client_host.Zero();
		server_host.Zero();
	}

	struct SClientHost: public GNetStructure{
		void Zero(){addr_detected_by_server=0;};
		GNetMsg& W(GNetMsg & msg){msg.WUI(addr_detected_by_server);return msg;}
		GNetMsg& R(GNetMsg & msg){msg.RUI(addr_detected_by_server);return msg;}
		DWORD32				addr_detected_by_server;
	} client_host;

	struct SServerHost: public GNetStructure{
		void Zero(){server_addr_connected_by_client=server_port_connected_by_client=0;};
		GNetMsg& W(GNetMsg & msg){msg.WUI(server_addr_connected_by_client).WUS(server_port_connected_by_client);return msg;}
		GNetMsg& R(GNetMsg & msg){msg.RUI(server_addr_connected_by_client).RUS(server_port_connected_by_client);return msg;}
		DWORD32				server_addr_connected_by_client;
		USHORT16			server_port_connected_by_client;
	} server_host;

	struct SAccess: public GNetStructure
	{
		void Zero()
		{
			RID=ID=0;
			target_gamestate_ID = 0;
			target_location_ID[0] = 0;
			SID[0]=0;
			group_ID=1;
		};
		GNetMsg& W(GNetMsg & msg)
		{
			msg.WI(RID).WI(ID).WI(target_gamestate_ID).WT(target_location_ID).WT(SID).WI(group_ID);
			return msg;
		}
		GNetMsg& R(GNetMsg & msg)
		{
			msg.RI(RID).RI(ID).RI(target_gamestate_ID).RT(SPair(target_location_ID, MAX_LOCATION)()).RT(SPair(SID,MAX_SID)()).RI(group_ID);
			return msg;
		}
		GNetMsg& RE(GNetMsg & msg)
		{
			msg.RI(ID).RI(target_gamestate_ID).RT(SPair(target_location_ID, MAX_LOCATION)()).RT(SPair(SID,MAX_SID)()).RI(group_ID);
			return msg;
		}
		
		INT32				RID;						//random id, czyli takie co sie zawsze zmienia
		INT32				ID;							//id gracza, czyli jego nr w bazie danych
		INT32				target_gamestate_ID;		//id stanu gry do ktorego gracz sie podpina
		char				target_location_ID[MAX_LOCATION];	// id lokacji, do ktorej gracz sie podpina (uzywane tylko jesli target_gamestate_ID = 0)
		char				SID[MAX_SID];				//nr sesji gracza
		INT32				group_ID;

		void ZeroSID(){memset(SID,0,MAX_SID);};
	} access;
};
typedef boost::shared_ptr<GTicketConnection> GTicketConnectionPtr;
/***************************************************************************/
inline void Coerce(SPlayerInfo & a,GTicketConnection & b)
{
	a.RID=b.access.RID;
	a.ID=b.access.ID;
	a.target_gamestate_ID = b.access.target_gamestate_ID;
	strncpy(a.target_location_ID, b.access.target_location_ID, MAX_LOCATION);
	a.target_location_ID[MAX_LOCATION - 1] = 0;
	a.info.addr_detected_by_server=b.client_host.addr_detected_by_server;
}
/***************************************************************************/
inline void Coerce(SPlayerInfoMini & a,SPlayerInfo & b)
{
	a.ID=b.ID;
}
/***************************************************************************/
/***************************************************************************/
struct SPlayerInstance : public GNetStructure
{
	SPlayerInstance(){Zero();};
	GNetMsg& W(GNetMsg & msg)
	{
		msg.WI(user_id).WI(gamestate_ID).WT(location_ID);
		return msg;
	}
	GNetMsg& R(GNetMsg & msg)
	{
		msg.RI(user_id).RI(gamestate_ID).RT(location_ID);
		return msg;
	}
	void					Zero()
	{
		user_id = 0;
		gamestate_ID = 0;
		location_ID = "";
	};

	INT32					user_id;
	INT32					gamestate_ID;
	string					location_ID;

	SPlayerInstance(const SPlayerInstance & r)
	{
		operator=(r);
	}

	void operator=(const SPlayerInstance & sp)
	{
		user_id = sp.user_id;
		gamestate_ID = sp.gamestate_ID;
		location_ID = sp.location_ID;
	}

	bool operator==(const SPlayerInstance & sp)
	{
		if (user_id != sp.user_id ||
			gamestate_ID != sp.gamestate_ID ||
			location_ID != sp.location_ID)
		{
			return false;
		}
		return true;
	}
};
/***************************************************************************/
inline void Coerce(SPlayerInstance & a, SPlayerInfo & b)
{
	a.user_id = b.ID;
	a.gamestate_ID = b.target_gamestate_ID;
	a.location_ID = b.target_location_ID;
}
/***************************************************************************/
class GTicketLocationAddressGet : public GTicket
{
public:
	GTicketLocationAddressGet()
	{
		Zero();
	};
	GTicketLocationAddressGet(INT32 message_p, GNetTarget & target_p):GTicket(){Zero();message=message_p;target=target_p;};

	GNetMsg& W(GNetMsg & msg)
	{
		msg.WT(location_ID).WI((INT32)connection_type);
		return msg;
	}
	GNetMsg& R(GNetMsg & msg)
	{
		INT32 c;
		msg.RT(location_ID).RI(c);
		connection_type = (EServiceTypes)c;
		return msg;
	}

	void Zero()
	{
		GTicket::Zero();
		location_ID = "";
		connection_type = ESTClientBase;
	}

	void operator=(const GTicketLocationAddressGet & sp)
	{
		location_ID = sp.location_ID;
		connection_type = sp.connection_type;
	}

	string				location_ID;
	EServiceTypes		connection_type;
};
typedef boost::shared_ptr<GTicketLocationAddressGet> GTicketLocationAddressGetPtr;
/***************************************************************************/
class GTicketLeaderboardGet : public GTicket
{
public:
	GTicketLeaderboardGet()
	{
		Zero();
	};
	GTicketLeaderboardGet(INT32 message_p, GNetTarget & target_p):GTicket(){Zero();message=message_p;target=target_p;};

	GNetMsg& W(GNetMsg & msg)
	{
		INT32 a, count = friends_id.size();
		msg.WT(lb_key).WBUI(lb_subkey).WI(user_id).WI(count);
		for (a = 0; a < count; a++)
		{
			msg.WI(friends_id[a]);
		}
		return msg;
	}
	GNetMsg& R(GNetMsg & msg)
	{
		INT32 a, count;
		msg.RT(lb_key).RBUI(lb_subkey).RI(user_id).RI(count);
		friends_id.resize(count);
		for (a = 0; a < count; a++)
		{
			msg.RI(friends_id[a]);
		}
		return msg;
	}

	void Zero()
	{
		GTicket::Zero();
		lb_key = "";
		lb_subkey = 0;
		user_id = 0;
		friends_id.clear();
	}

	void operator=(const GTicketLeaderboardGet & t)
	{
		lb_key = t.lb_key;
		lb_subkey = t.lb_subkey;
		user_id = t.user_id;
		friends_id = t.friends_id;
	}

	string				lb_key;
	DWORD64				lb_subkey;
	INT32				user_id;
	vector<INT32>		friends_id;
};
typedef boost::shared_ptr<GTicketLeaderboardGet> GTicketLeaderboardGetPtr;
/***************************************************************************/
class GTicketLeaderboardStandingsGet : public GTicket
{
public:
	GTicketLeaderboardStandingsGet()
	{
		Zero();
	};
	GTicketLeaderboardStandingsGet(INT32 message_p, GNetTarget & target_p):GTicket(){Zero();message=message_p;target=target_p;};

	GNetMsg& W(GNetMsg & msg)
	{
		INT32 a, count = friends_id.size();
		msg.WT(lb_key).WBUI(lb_subkey).WI(user_id).WI(standings_index).WI(max_results).WI(count);
		for (a = 0; a < count; a++)
		{
			msg.WI(friends_id[a]);
		}
		return msg;
	}
	GNetMsg& R(GNetMsg & msg)
	{
		INT32 a, count;
		msg.RT(lb_key).RBUI(lb_subkey).RI(user_id).RI(standings_index).RI(max_results).RI(count);
		friends_id.resize(count);
		for (a = 0; a < count; a++)
		{
			msg.RI(friends_id[a]);
		}
		return msg;
	}

	void Zero()
	{
		GTicket::Zero();
		lb_key = "";
		lb_subkey = 0;
		user_id = 0;
		standings_index = 0;
		max_results = 0;
		friends_id.clear();
	}

	void operator=(const GTicketLeaderboardStandingsGet & t)
	{
		lb_key = t.lb_key;
		lb_subkey = t.lb_subkey;
		user_id = t.user_id;
		standings_index = t.standings_index;
		max_results = t.max_results;
		friends_id = t.friends_id;
	}

	string				lb_key;
	DWORD64				lb_subkey;
	INT32				user_id;
	INT32				standings_index;
	INT32				max_results;
	vector<INT32>		friends_id;
};
typedef boost::shared_ptr<GTicketLeaderboardStandingsGet> GTicketLeaderboardStandingsGetPtr;
/***************************************************************************/
class GTicketLeaderboardBatchInfoGet : public GTicket
{
public:
	GTicketLeaderboardBatchInfoGet()
	{
		Zero();
	};
	GTicketLeaderboardBatchInfoGet(INT32 message_p, GNetTarget & target_p):GTicket(){Zero();message=message_p;target=target_p;};

	GNetMsg& W(GNetMsg & msg)
	{
		INT32 a, count = users_id.size();
		msg.WT(lb_key).WBUI(lb_subkey).WI(user_id).WI(standings_index).WI(count);
		for (a = 0; a < count; a++)
		{
			msg.WI(users_id[a]);
		}
		return msg;
	}
	GNetMsg& R(GNetMsg & msg)
	{
		INT32 a, count;
		msg.RT(lb_key).RBUI(lb_subkey).RI(user_id).RI(standings_index).RI(count);
		users_id.resize(count);
		for (a = 0; a < count; a++)
		{
			msg.RI(users_id[a]);
		}
		return msg;
	}

	void Zero()
	{
		GTicket::Zero();
		lb_key = "";
		lb_subkey = 0;
		user_id = 0;
		standings_index = 0;
		users_id.clear();
	}

	void operator=(const GTicketLeaderboardBatchInfoGet & t)
	{
		lb_key = t.lb_key;
		lb_subkey = t.lb_subkey;
		user_id = t.user_id;
		standings_index = t.standings_index;
		users_id = t.users_id;
	}

	string				lb_key;
	DWORD64				lb_subkey;
	INT32				user_id;
	INT32				standings_index;
	vector<INT32>		users_id;
};
typedef boost::shared_ptr<GTicketLeaderboardBatchInfoGet> GTicketLeaderboardBatchInfoGetPtr;
/***************************************************************************/
class GTicketLeaderboardSet : public GTicket
{
public:
	GTicketLeaderboardSet()
	{
		Zero();
	};
	GTicketLeaderboardSet(INT32 message_p, GNetTarget & target_p):GTicket(){Zero();message=message_p;target=target_p;};

	GNetMsg& W(GNetMsg & msg)
	{
		INT32 a, count = scores.size();
		msg.WT(lb_key).WBUI(lb_subkey).WI(user_id).WI(count);
		for (a = 0; a < count; a++)
		{
			msg.WI(scores[a]);
		}
		return msg;
	}
	GNetMsg& R(GNetMsg & msg)
	{
		INT32 a, count;
		msg.RT(lb_key).RBUI(lb_subkey).RI(user_id).RI(count);
		scores.resize(count);
		for (a = 0; a < count; a++)
		{
			msg.RI(scores[a]);
		}
		return msg;
	}

	void Zero()
	{
		GTicket::Zero();
		lb_key = "";
		lb_subkey = 0;
		user_id = 0;
		scores.clear();
	}

	void operator=(const GTicketLeaderboardSet & t)
	{
		lb_key = t.lb_key;
		lb_subkey = t.lb_subkey;
		user_id = t.user_id;
		scores = t.scores;
	}

	string				lb_key;
	DWORD64				lb_subkey;
	INT32				user_id;
	vector<INT32>		scores;
};
typedef boost::shared_ptr<GTicketLeaderboardSet> GTicketLeaderboardSetPtr;
/***************************************************************************/
struct SLogicMessage : public GNetStructure
{
	SLogicMessage()
	{
		Zero();
	};

	GNetMsg & W(GNetMsg & msg)
	{
		msg.WT(target_location_ID).WT(command).WT(params).WI(ttl_seconds);
		return msg;
	}
	GNetMsg & R(GNetMsg & msg)
	{
		msg.RT(target_location_ID).RT(command).RT(params).RI(ttl_seconds);
		return msg;
	}

	void Zero()
	{
		target_location_ID = "";
		command = "";
		params = "";
		ttl_seconds = 0;
		address_request_sent = false;
		bad_address_received = false;
		source_group_address = "";
		target_group_address = "";
		socketid = 0;
		expiration_time = 0;
	}

	void operator=(const SLogicMessage & t)
	{
		target_location_ID = t.target_location_ID;
		command = t.command;
		params = t.params;
		ttl_seconds = t.ttl_seconds;
		address_request_sent = t.address_request_sent;
		bad_address_received = t.bad_address_received;
		source_group_address = t.source_group_address;
		target_group_address = t.target_group_address;
		socketid = t.socketid;
		expiration_time = t.expiration_time;
	}

	string				target_location_ID;			// id lokacji, do ktorej skierowany jest komunikat.
	string				command;
	string				params;
	INT32				ttl_seconds;

	bool				address_request_sent;		// Wyslalismy juz request o adres gamestatu do serwisu 'nodebalancer'.
	bool				bad_address_received;		// Czy ticket wrocil do nas z informacja 'bad address'.
	string				source_group_address;		// String postaci 'host:port:group_id'
	string				target_group_address;		// String postaci 'host:port:group_id'
	INT32				socketid;					// id socketu, na ktory przyszedl ten komunikat.

	DWORD64				expiration_time;			// Moment do kiedy probujemy dostarczyc wiadomosc.
};
typedef GTicketTmpl<SLogicMessage> GTicketLogicMessage;
typedef boost::shared_ptr<GTicketLogicMessage> GTicketLogicMessagePtr;
/***************************************************************************/
class SRegisterBuddies : public GNetStructure
{
public:
	SRegisterBuddies()
	{
		Zero();
	};

	GNetMsg& W(GNetMsg & msg)
	{
		INT32 count = buddies_ids.size();
		msg.WI(user_id).WI(count);
		for (std::set<INT32>::const_iterator it = buddies_ids.begin(); it != buddies_ids.end(); ++it)
		{
			msg.WI(*it);
		}
		return msg;
	}
	GNetMsg& R(GNetMsg & msg)
	{
		INT32 a, count;
		msg.RI(user_id).RI(count);

		for (a = 0; a < count; a++)
		{
			INT32 buddy_id;
			msg.RI(buddy_id);
			buddies_ids.insert(buddy_id);
		}
		return msg;
	}

	void Zero()
	{
		buddies_ids.clear();
		user_id = 0;
	}

	INT32					user_id;
	std::set<INT32>			buddies_ids;
};
/***************************************************************************/

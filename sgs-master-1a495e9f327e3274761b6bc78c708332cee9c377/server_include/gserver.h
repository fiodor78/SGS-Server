/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#pragma warning (disable: 4511)
class GServer: public GServerBaseConsole
{
	friend class GSocket;
	friend class GSocketAccess;
	friend class GSocketGame;
	friend class GConsole;
public:
	GServer();
	virtual bool				Init();
	void						Destroy();

	virtual bool				InitData(){return true;};
	virtual void				InitServices();
	virtual bool				InitSQL();
	bool						InitHostName();
	bool						ConfigureSockets();
	bool						InitUserPid();
	bool						InitRSA();
	void						InitFireWall();
#ifndef LINUX
	void						ParseKeyboard();
#endif
	void						ReinitRSA();
	void						UpdateHeartBeat();
	void						ConfigureClock();

	bool						TestFirewall(const in_addr addr){return !firewall(addr);};
	void						ParseConsole(GSocketConsole *socket){console.ParseConsole(socket);};
private:
	GFireWall					firewall;
	RSA *						priv_key;
	RSA*						pub_key;

	BYTE						RSA_bits[32];
	BYTE						RSA_crypt[RSA_BYTES];
	GConsole					console;
};
#pragma warning (default: 4511)
/***************************************************************************/


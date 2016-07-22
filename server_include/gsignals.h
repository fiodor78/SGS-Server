/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

class GSignals: public boost::noncopyable
{
public:
	GSignals(){};
	~GSignals(){Destroy();};
	void					Init(){};
	void					Destroy(){};

	boost::signals2::signal<void()>	reinit_connections;
	boost::signals2::signal<void()>	close_fast;
	boost::signals2::signal<void()>	close_slow;
	boost::signals2::signal<void()>	close_gently;
};
/***************************************************************************/

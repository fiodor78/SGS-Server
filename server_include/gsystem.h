/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#define MAX_PRINT_SIZE		4096
/**	
\brief GSystem class is used by Linux to set singnal handlers.
*/
class GSystem
{
	public:
	GSystem();
	void Init();
	virtual ~GSystem();
#ifdef LINUX
		void LogInfo(const char * msg,...);
		void LogError(const char * msg,...);
#endif
};

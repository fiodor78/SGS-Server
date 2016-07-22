// dllmain.cpp : Defines the entry point for the DLL application.
#include "headers_all.h"
#include "glogic_example.h"

#ifndef LINUX
#include "windows.h"
#define EXPORTFUNCTION	__declspec(dllexport) 
#else
#define EXPORTFUNCTION __attribute__ ((visibility("default")))
#endif

extern "C" EXPORTFUNCTION class ILogicEngine * create_logic(class ILogicUpCalls * upcalls);
extern "C" EXPORTFUNCTION void destroy_logic(class ILogicEngine * logic);

/**********************************************************************************************************************************/

class ILogicEngine * create_logic(class ILogicUpCalls * upcalls)
{
	GLogicEngineExample * logic = new GLogicEngineExample(upcalls);
	return static_cast<ILogicEngine *>(logic);
}

void destroy_logic(class ILogicEngine * logic)
{
	GLogicEngineExample * _logic = static_cast<GLogicEngineExample *>(logic);
	if (_logic != NULL)
	{
		delete _logic;
	}
}

/**********************************************************************************************************************************/

#ifndef LINUX

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

#endif

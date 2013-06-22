#include <windows.h>

///////////////////////////////////////////////////////////////////////////

HINSTANCE g_hInst;


BOOL WINAPI DllMain(  HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	switch(fdwReason) {

		case DLL_PROCESS_ATTACH:
			g_hInst = hinstDLL;
			return true;

		case DLL_PROCESS_DETACH:
			break;
	};

	return true;
}


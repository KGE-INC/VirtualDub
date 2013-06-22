#ifndef f_VIDE4ODISPLAY_H
#define f_VIDEODISPLAY_H

#include <windows.h>

#define VIDEODISPLAYCONTROLCLASS (g_szVideoDisplayControlName)

extern const char g_szVideoDisplayControlName[];

ATOM RegisterVideoDisplayControl();

#define		VDCM_SETVIDEO		(WM_USER+0x100)

#endif

#ifndef f_AUDIODISPLAY_H
#define f_AUDIODISPLAY_H

#include <windows.h>

#define AUDIODISPLAYCONTROLCLASS (g_szAudioDisplayControlName)

extern const char g_szAudioDisplayControlName[];

ATOM RegisterAudioDisplayControl();

#define		ADCM_SETAUDIO		(WM_USER+0x100)

#endif

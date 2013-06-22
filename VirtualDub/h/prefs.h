//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifndef f_PREFS_H
#define f_PREFS_H

#include <windows.h>

///////////////////////////////////////////////////////////////////////////

class PreferencesMain {
public:
	enum {
		DEPTH_FASTEST	= 0,
		DEPTH_OUTPUT	= 1,
		DEPTH_DISPLAY	= 2,
		DEPTH_16BIT		= 3,
		DEPTH_24BIT		= 4,
		DEPTH_32BIT		= 5,

		// The order of these must match the flags in cpuaccel.h!!

		OPTF_FORCE			= 0x01,
		OPTF_FPU			= 0x02,
		OPTF_MMX			= 0x04,
		OPTF_INTEGER_SSE	= 0x08,		// Athlon MMX extensions or Intel SSE
		OPTF_SSE			= 0x10,		// Full SSE (PIII)
		OPTF_SSE2			= 0x20,		// (PIV)
		OPTF_3DNOW			= 0x40,
		OPTF_3DNOW_EXT		= 0x80,		// Athlon 3DNow! extensions
	};

	char	iPreviewPriority;
	char	iPreviewDepth;
	char	iDubPriority;
	char	fAttachExtension;
	char	fOptimizations;
};

class PreferencesScene {
public:
	int		iCutThreshold;
	int		iFadeThreshold;
};

class Preferences {
public:
	PreferencesMain	main;
	PreferencesScene scene;

	enum {
		kDisplayDither16	= 0x01,
		kDisplayDisableDX	= 0x02,
		kDisplayUseDXWithTS	= 0x04
	};

	char fDisplay;
	char fAVIRestrict1Gb;
	char fNoCorrectLayer3;
};

///////////////////////////////////////////////////////////////////////////

BOOL APIENTRY PreferencesDlgProc		( HWND hDlg, UINT message, UINT wParam, LONG lParam);

///////////////////////////////////////////////////////////////////////////

#ifndef f_PREFS_CPP
extern Preferences		g_prefs;
#endif

void LoadPreferences();

#endif

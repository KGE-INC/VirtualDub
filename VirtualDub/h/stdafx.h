//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2002 Avery Lee
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

#ifndef f_STDAFX_H

#ifdef _MSC_VER
#pragma warning(disable: 4786)
struct MSVC_C4786_Workaround { MSVC_C4786_Workaround() {} };
static MSVC_C4786_Workaround g_VD_ShutUpYouStupidCompilerAbout255CharacterLimitOnDebugInformation;
#endif

#include <vd2/system/vdtypes.h>
#include <vd2/system/math.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define _WIN32_WINNT 0x0500

#include <windows.h>

#include "VirtualDub.h"

#endif

//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2000 Avery Lee
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

#ifndef f_SYSTEM_TLS_H
#define f_SYSTEM_TLS_H

#include <ctype.h>

class IProgress;
class VDAtomicInt;
class VDSignalPersistent;

struct VirtualDubTLSData {
	int					tmp[16];
	void				*ESPsave;
	IProgress			*pProgress;
	VDAtomicInt			*pbAbort;
	VDSignalPersistent	*pAbortSignal;
	const char			*pszDebugThreadName;

	// Fast text conversion area

	union {
		char	fastBufA[2048];
		wchar_t	fastBufW[2048 / sizeof(wchar_t)];
	};
	void		*pFastBufAlloc;
};

extern __declspec(thread) VirtualDubTLSData g_tlsdata;

void VDInitThreadData(const char *pszThreadName);
void VDDeinitThreadData();

typedef void (*VDThreadInitHook)(bool bInit);

void VDSetThreadInitHook(VDThreadInitHook pHook);

#endif

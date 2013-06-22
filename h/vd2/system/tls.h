//	VirtualDub - Video processing and capture application
//	System library component
//	Copyright (C) 1998-2004 Avery Lee, All Rights Reserved.
//
//	Beginning with 1.6.0, the VirtualDub system library is licensed
//	differently than the remainder of VirtualDub.  This particular file is
//	thus licensed as follows (the "zlib" license):
//
//	This software is provided 'as-is', without any express or implied
//	warranty.  In no event will the authors be held liable for any
//	damages arising from the use of this software.
//
//	Permission is granted to anyone to use this software for any purpose,
//	including commercial applications, and to alter it and redistribute it
//	freely, subject to the following restrictions:
//
//	1.	The origin of this software must not be misrepresented; you must
//		not claim that you wrote the original software. If you use this
//		software in a product, an acknowledgment in the product
//		documentation would be appreciated but is not required.
//	2.	Altered source versions must be plainly marked as such, and must
//		not be misrepresented as being the original software.
//	3.	This notice may not be removed or altered from any source
//		distribution.

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

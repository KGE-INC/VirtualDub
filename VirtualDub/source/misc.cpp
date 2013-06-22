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

#include "stdafx.h"

#include <ctype.h>

#include <wtypes.h>
#include <mmsystem.h>

#include "misc.h"
#include <vd2/system/cpuaccel.h>
#include <vd2/system/log.h>

long __declspec(naked) MulDivTrunc(long a, long b, long c) {
	__asm {
		mov eax,[esp+4]
		imul dword ptr [esp+8]
		idiv dword ptr [esp+12]
		ret
	}
}

unsigned __declspec(naked) __stdcall MulDivUnsigned(unsigned a, unsigned b, unsigned c) {
	__asm {
		mov		eax,[esp+4]
		mov		ecx,[esp+12]
		mul		dword ptr [esp+8]
		shr		ecx,1
		add		eax,ecx
		adc		edx,0
		div		dword ptr [esp+12]
		ret		12
	}
}

int NearestLongValue(long v, const long *array, int array_size) {
	int i;

	for(i=1; i<array_size; i++)
		if (v*2 < array[i-1]+array[i])
			break;

	return i-1;
}

bool isEqualFOURCC(FOURCC fccA, FOURCC fccB) {
	int i;

	for(i=0; i<4; i++) {
		if (tolower((unsigned char)fccA) != tolower((unsigned char)fccB))
			return false;

		fccA>>=8;
		fccB>>=8;
	}

	return true;
}

bool isValidFOURCC(FOURCC fcc) {
	return isprint((unsigned char)(fcc>>24))
		&& isprint((unsigned char)(fcc>>16))
		&& isprint((unsigned char)(fcc>> 8))
		&& isprint((unsigned char)(fcc    ));
}

FOURCC toupperFOURCC(FOURCC fcc) {
	return(toupper((unsigned char)(fcc>>24)) << 24)
		| (toupper((unsigned char)(fcc>>16)) << 16)
		| (toupper((unsigned char)(fcc>> 8)) <<  8)
		| (toupper((unsigned char)(fcc    ))      );
}

#if defined(WIN32) && defined(_M_IX86)

	bool IsMMXState() {
		char	buf[28];
		unsigned short tagword;

		__asm fnstenv buf

		tagword = *(unsigned short *)(buf + 8);

		return (tagword != 0xffff);
	}

	bool IsFPUStateOK() {
		unsigned ctlword = 0;

		__asm fnstcw ctlword

		return ctlword == 0x027f;
	}

	void ClearMMXState() {
		if (MMX_enabled)
			__asm emms
		else {
			__asm {
				ffree st(0)
				ffree st(1)
				ffree st(2)
				ffree st(3)
				ffree st(4)
				ffree st(5)
				ffree st(6)
				ffree st(7)
			}
		}
	}

	void ResetFPUState() {
		static const unsigned ctlword = 0x027f;

		__asm fnclex
		__asm fldcw ctlword
	}

	void VDClearEvilCPUStates() {
		ResetFPUState();
		ClearMMXState();
	}

	void VDPreCheckExternalCodeCall(const char *file, int line) {
		bool bMMXStateBad = IsMMXState();
		bool bFPUStateBad = !IsFPUStateOK();

		if (bMMXStateBad || bFPUStateBad) {
			ClearMMXState();
			ResetFPUState();
		}

		if (bMMXStateBad) {
			VDLog(kVDLogError, VDswprintf(L"Internal error: MMX state was active before entry to external code. "
										L"This indicates an uncaught bug either in an external driver or in VirtualDub itself "
										L"that could cause application instability.  Please report this problem to the author!",
										2,
										&file,
										&line
										));
		}
		if (bFPUStateBad) {
			VDLog(kVDLogError, VDswprintf(L"Internal error: Floating-point state was bad before entry to external code at (%s:%d). "
										L"This indicates an uncaught bug either in an external driver or in VirtualDub itself "
										L"that could cause application instability.  Please report this problem to the author!",
										2,
										&file,
										&line
										));
		}
	}

	void VDPostCheckExternalCodeCall(const wchar_t *mpContext, const char *mpFile, int mLine) {
		bool bMMXStateBad = IsMMXState();
		bool bFPUStateBad = !IsFPUStateOK();
		bool bBadState = bMMXStateBad || bFPUStateBad;

		static bool sbDisableFurtherWarnings = false;

		if (bBadState) {
			ClearMMXState();
			ResetFPUState();
		}

		if (sbDisableFurtherWarnings)
			return;

		if (bMMXStateBad) {
			VDLog(kVDLogWarning, VDswprintf(L"%ls returned to VirtualDub with the CPU's MMX unit still active. "
											L"This indicates a bug in that module which could cause application instability. "
											L"Please check with the module vendor for an updated version which addresses this problem. "
											L"(Trap location: %hs:%d)",
											3,
											&mpContext,
											&mpFile,
											&mLine));
			sbDisableFurtherWarnings = true;
		}
		if (bFPUStateBad) {
			VDLog(kVDLogWarning, VDswprintf(L"%ls returned to VirtualDub with the floating-point unit in an abnormal state. "
											L"This indicates a bug in that module which could cause application instability. "
											L"Please check with the module vendor for an updated version which addresses this problem. "
											L"(Trap location: %hs:%d)",
											3,
											&mpContext,
											&mpFile,
											&mLine));
			sbDisableFurtherWarnings = true;
		}
	}

#else

	bool IsMMXState() {
		return false;
	}

	void ClearMMXState() {
	}

	void VDClearEvilCPUStates() {
	}

	void VDPreCheckExternalCodeCall(const char *file, int line) {
	}

	void VDPostCheckExternalCodeCall(const wchar_t *mpContext, const char *mpFile, int mLine) {
	}

#endif

char *strCify(const char *s) {
	static char buf[2048];
	char c,*t = buf;

	while(c=*s++) {
		if (!isprint((unsigned char)c))
			t += sprintf(t, "\\x%02x", (int)c & 0xff);
		else {
			if (c=='"' || c=='\\')
				*t++ = '\\';
			*t++ = c;
		}
	}
	*t=0;

	return buf;
}

VDStringA VDEncodeScriptString(const VDStringW& sw) {
	const VDStringA sa(VDTextWToU8(sw));
	VDStringA out;

	// this is not very fast, but it's only used during script serialization
	for(VDStringA::const_iterator it(sa.begin()), itEnd(sa.end()); it != itEnd; ++it) {
		char c = *it;
		char buf[16];

		if (!isprint((unsigned char)c)) {
			sprintf(buf, "\\x%02x", (int)c & 0xff);
			out.append(buf);
		} else {
			if (c == '"' || c=='\\')
				out += '\\';
			out += c;
		}
	}

	return out;
}
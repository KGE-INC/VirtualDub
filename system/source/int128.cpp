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

#include <math.h>

#include <vd2/system/int128.h>

#ifndef _M_AMD64
	void __declspec(naked) int128::setSquare(sint64 v) {
		__asm {
			push	edi
			push	esi
			push	ebx
			mov		eax, [esp+20]
			cdq
			mov		esi, eax
			mov		eax, [esp+16]
			xor		eax, edx
			xor		esi, edx
			sub		eax, edx
			sbb		esi, edx
			mov		ebx, eax
			mul		eax
			mov		[ecx], eax
			mov		edi, edx
			mov		eax, ebx
			mul		esi
			mov		ebx, 0
			add		eax, eax
			adc		edx, edx
			add		eax, edi
			adc		edx, 0
			mov		edi, edx
			adc		ebx, 0
			mov		[ecx+4], eax
			mov		eax, esi
			mul		esi
			add		eax, edi
			adc		edx, ebx
			mov		[ecx+8], eax
			mov		[ecx+12], edx
			pop		ebx
			pop		esi
			pop		edi
			ret		8
		}
	}

	const int128 __declspec(naked) int128::operator+(const int128& x) const {
		__asm {
			push	ebx

			mov		ebx,[esp+12]
			mov		edx,[esp+8]

			mov		eax,[ecx+0]
			add		eax,[ebx+0]
			mov		[edx+0],eax
			mov		eax,[ecx+4]
			adc		eax,[ebx+4]
			mov		[edx+4],eax
			mov		eax,[ecx+8]
			adc		eax,[ebx+8]
			mov		[edx+8],eax
			mov		eax,[ecx+12]
			adc		eax,[ebx+12]
			mov		[edx+12],eax

			pop		ebx
			mov		eax,[esp+4]
			ret		8
		}
	}

	const int128 __declspec(naked)& int128::operator+=(const int128& x) {
		__asm {
			mov		edx,[esp+4]

			mov		eax,[edx+0]
			add		[ecx+0],eax
			mov		eax,[edx+4]
			adc		[ecx+4],eax
			mov		eax,[edx+8]
			adc		[ecx+8],eax
			mov		eax,[edx+12]
			adc		[ecx+12],eax

			mov		eax,ecx
			ret		4
		}
	}

	const int128 __declspec(naked) int128::operator-(const int128& x) const {
		__asm {
			push	ebx

			mov		ebx,[esp+12]
			mov		edx,[esp+8]

			mov		eax,[ecx+0]
			sub		eax,[ebx+0]
			mov		[edx+0],eax
			mov		eax,[ecx+4]
			sbb		eax,[ebx+4]
			mov		[edx+4],eax
			mov		eax,[ecx+8]
			sbb		eax,[ebx+8]
			mov		[edx+8],eax
			mov		eax,[ecx+12]
			sbb		eax,[ebx+12]
			mov		[edx+12],eax

			pop		ebx
			mov		eax,[esp+4]
			ret		8
		}
	}

	extern "C" static void __declspec(naked) mult64x64(int128 *pDest, unsigned __int64 y, unsigned __int64 x) {
		__asm {
			push	ecx
			mov		ecx,[esp+8]

			mov		eax,[esp+12]
			mul		dword ptr [esp+20]		;EDX:EAX = BD
			mov		[ecx+0],eax
			mov		[ecx+4],edx

			mov		eax,[esp+16]
			mul		dword ptr [esp+24]		;EDX:EAX = AC
			mov		[ecx+8],eax
			mov		[ecx+12],edx

			mov		eax,[esp+12]
			mul		dword ptr [esp+24]		;EDX:EAX = BC
			add		[ecx+4],eax
			adc		[ecx+8],edx
			adc		[ecx+12], 0

			mov		eax,[esp+16]
			mul		dword ptr [esp+20]		;EDX:EAX = AD
			add		[ecx+4],eax
			adc		[ecx+8],edx
			adc		[ecx+12], 0

			pop		ecx
			ret
		}
	}

	const int128 int128::operator*(const int128& x) const {
		int128 ad, bc, bd;
		int128 X = x.abs();
		int128 Y = abs();

		mult64x64(&ad, X.v[1], Y.v[0]);
		mult64x64(&bc, X.v[0], Y.v[1]);
		mult64x64(&bd, X.v[0], Y.v[0]);

		return (v[1]^x.v[1])<0 ? -(bd + ((ad + bc)<<64)) : bd + ((ad + bc)<<64);
	}

	const int128 __declspec(naked) int128::operator<<(int v) const {
		__asm {
			push	ebp
			push	ebx
			push	esi
			push	edi

			mov		esi,ecx
			mov		edx,[esp+20]

			mov		ecx,[esp+24]
			cmp		ecx,128
			jae		zeroit

			mov		eax,[esi+12]
			mov		ebx,[esi+8]
			mov		edi,[esi+4]
			mov		ebp,[esi]

	dwordloop:
			cmp		ecx,32
			jb		bits

			mov		eax,ebx
			mov		ebx,edi
			mov		edi,ebp
			xor		ebp,ebp
			sub		ecx,32
			jmp		short dwordloop

	bits:
			shld	eax,ebx,cl
			shld	ebx,edi,cl
			mov		[edx+12],eax
			mov		[edx+8],ebx
			shld	edi,ebp,cl

			shl		ebp,cl
			mov		[edx+4],edi
			mov		[edx],ebp

			pop		edi
			pop		esi
			pop		ebx
			pop		ebp
			mov		eax,[esp+4]
			ret		8

	zeroit:
			xor		eax,eax
			mov		[edx+0],eax
			mov		[edx+4],eax
			mov		[edx+8],eax
			mov		[edx+12],eax

			pop		edi
			pop		esi
			pop		ebx
			pop		ebp
			mov		eax,[esp+4]
			ret		8
		}
	}

	const int128 __declspec(naked) int128::operator>>(int v) const {
		__asm {
			push	ebp
			push	ebx
			push	esi
			push	edi

			mov		esi,ecx
			mov		edx,[esp+20]

			mov		eax,[esi+12]
			mov		ecx,[esp+24]
			cmp		ecx,127
			jae		clearit

			mov		ebx,[esi+8]
			mov		edi,[esi+4]
			mov		ebp,[esi]

	dwordloop:
			cmp		ecx,32
			jb		bits

			mov		ebp,edi
			mov		edi,ebx
			mov		ebx,eax
			sar		eax,31
			sub		ecx,32
			jmp		short dwordloop

	bits:
			shrd	ebp,edi,cl
			shrd	edi,ebx,cl
			mov		[edx],ebp
			mov		[edx+4],edi
			shrd	ebx,eax,cl

			sar		eax,cl
			mov		[edx+8],ebx
			mov		[edx+12],eax

			pop		edi
			pop		esi
			pop		ebx
			pop		ebp
			mov		eax,[esp+4]
			ret		8

	clearit:
			sar		eax, 31
			mov		[edx+0],eax
			mov		[edx+4],eax
			mov		[edx+8],eax
			mov		[edx+12],eax

			pop		edi
			pop		esi
			pop		ebx
			pop		ebp
			mov		eax,[esp+4]
			ret		8
		}
	}

	const int128 __declspec(naked) int128::operator-() const {
		__asm {
			push	ebx
			push	esi
			push	edi

			mov		edi,[esp+16]

			mov		eax,[ecx+12]
			mov		ebx,[ecx+8]
			mov		edx,[ecx+4]
			mov		esi,[ecx+0]

			xor		esi,-1
			xor		edx,-1
			xor		ebx,-1
			xor		eax,-1

			add		esi,1
			adc		edx,0
			adc		ebx,0
			adc		eax,0

			mov		[edi+0],esi
			mov		[edi+4],edx
			mov		[edi+8],ebx
			mov		[edi+12],eax
			
			mov		eax,edi
			pop		edi
			pop		esi
			pop		ebx
			ret		4
		}
	}

	const int128 __declspec(naked) int128::abs() const {
		__asm {
			push	ebx
			push	esi
			push	edi

			mov		edi,[esp+16]

			mov		eax,[ecx+12]
			mov		ebx,[ecx+8]
			mov		edx,[ecx+4]
			or		eax,eax
			mov		esi,[ecx+0]
			jns		positive

			xor		esi,-1
			xor		edx,-1
			xor		ebx,-1
			xor		eax,-1

			add		esi,1
			adc		edx,0
			adc		ebx,0
			adc		eax,0

	positive:
			mov		[edi+0],esi
			mov		[edi+4],edx
			mov		[edi+8],ebx
			mov		[edi+12],eax

			mov		eax, edi
			pop		edi
			pop		esi
			pop		ebx
			ret		4
		}
	}
#endif

int128::operator double() const {
	return (double)(unsigned long)v[0]
		+ ldexp((double)(unsigned long)((unsigned __int64)v[0]>>32), 32)
		+ ldexp((double)v[1], 64);
}

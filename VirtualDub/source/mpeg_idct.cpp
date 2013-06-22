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

///////////////////////////////////////////////////////////////////////////
//
//                                 WARNING
//
// This code is heavily based off of the Java MPEG video player written by
// Joerg Anders.  Because his code was released under the GNU GPL v2, this
// means VirtualDub must also be released under GNU GPL v2 when MPEG
// support is included.
//
// (Like that's any different.)
//
// This code is really nasty...
//
///////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "mpeg_idct.h"
#include <vd2/system/cpuaccel.h>

#pragma warning(disable: 4799)		// function has no EMMS instruction

extern "C" unsigned char YUV_clip_table[];

#ifdef __INTEL_COMPILER

#error Cannot compile mpeg_idct.cpp with the Intel C/C++ compiler.

// The 6.0 release of the Intel C/C++ compiler miscompiles some inline
// assembly in this module.  Specifically, instructions of the form:
//
//		ADD r32, offset symbol + disp
//
// cause jump instructions to branch beyond their intended branch targets.
// None of the C/C++ code in this module is performance critical, so
// the Intel compiler should be disabled for mpeg_idct.cpp by placing
// through a /D_USE_NON_INTEL_COMPILER switch on the command line, or
// through the Preprocessor section of the C++ tab in Project Settings.
//
// Also, if you happen to spot any warnings of this form:
//
//     module(x): warning: offset of 174 is too large for a short jmp
//
// interrupt the compilation immediately and delete the 'short' modifier
// from the jump instruction, as the compiler has just generated a short
// jump to a random target; Microsoft Visual C++ generates a proper
// long jump instead.  This should really be an error.

#endif


///////////////////////////////////////////////////////////////////////////

// enable this define for CPU profiling

//#define PROFILE

#define	CYCLES_PER_SECOND		(200000000)

///////////////////////////////////////////////////////////////////////////

//#define FAST_IMUL

#ifdef PROFILE
	static long profile_start;
	static long profile_last;
	static long profile_fastput_cycles;
	static long profile_fastadd_cycles;
	static long profile_slow_cycles;
	static long profile_fastputs;
	static long profile_fastadds;
	static long profile_slows;

	#include <windows.h>

	extern "C" void ODS(const char *s) { OutputDebugString(s); }

	void profile_update() {
		char buf[256];
		long pcnt = (profile_fastput_cycles + profile_fastadd_cycles + profile_slow_cycles)/(CYCLES_PER_SECOND/1000);

		sprintf(buf, "%ld fast puts (%d/idct), %ld fast adds (%d/idct), %ld slows (%d/idct), total %3d.%c%% of CPU\n"
			,profile_fastputs
			,profile_fastputs ? profile_fastput_cycles/profile_fastputs:0
			,profile_fastadds
			,profile_fastadds ? profile_fastadd_cycles/profile_fastadds:0
			,profile_slows
			,profile_slows ? profile_slow_cycles/profile_slows:0
			,pcnt/10,pcnt%10 + '0'
			);

		OutputDebugString(buf);

		profile_last += CYCLES_PER_SECOND;
		profile_fastputs = profile_fastadds = profile_slows = 0;
		profile_fastput_cycles = profile_fastadd_cycles = profile_slow_cycles = 0;
	}
#endif


#ifdef BE_REALLY_SLOW

#define C6			2*sin(pi/8)
#define C4C6		2*sqrt(2)*sin(pi/8)
#define C4			sqrt(2)
#define Q			2*(cos(pi/8)-sin(pi/8))
#define C4Q			2*sqrt(2)*(cos(pi/8)-sin(pi/8))
#define R			2*(cos(pi/8)+sin(pi/8))
#define C4R			2*sqrt(2)*(cos(pi/8)+sin(pi/8))

#else

// For some reason, adding +1 to C6 or -1 to R (but not both) is enough
// to bring this IDCT into IEEE-1180 compliance.  C6+1 is the better
// adjustment.

#define C6			((int)(0.7653668647094 * 2048.0 + 0.5))+1
#define C4C6		((int)(1.082392200292  * 2048.0 + 0.5))
#define C4			((int)(1.414213562373  * 2048.0 + 0.5))
#define Q			((int)(1.08239220029   * 2048.0 + 0.5))
#define C4Q			((int)(1.53073372946   * 2048.0 + 0.5))
#define R			((int)(2.613125929753  * 2048.0 + 0.5))
#define C4R			((int)(3.69551813004   * 2048.0 + 0.5))

//	C6		= 1567
//	C4C6	= 2217
//	C4		= 2896
//	Q		= 2217
//	C4Q		= 3135
//	R		= 5352
//	C4R		= 7568

#endif

#define MATH_PI		(3.141592653589793238462643)

static struct {
	int data[16][64];
} idct_precomputed;

int dct_coeff[64];
static int matr1[64], matr2[64];

extern void MJPEG_IDCT(int *);

void IDCT_init() {
	for (int i=0; i<8; i++) {
		double d0 = 0.125 * sqrt(2) * 256.0;
		double d1 = 0.25 * 256.0;

		if (!i) {
			d0 = 0.125 * 256.0;
			d1 = 0.125 * sqrt(2) * 256.0;
		}

		for (int k = 0; k < 8; k++) {
			for (int l = 0; l < 8; l++) {
				idct_precomputed.data[i][k * 8 + l] = (int)floor(0.5 + d0 * cos(MATH_PI * (2*k+1) / 16.0) * cos(MATH_PI * i * (2*l+1) / 16.0));
				idct_precomputed.data[i+8][k * 8 + l] = (int)floor(0.5 + d1 * cos(MATH_PI * (2*k+1) / 16.0) * cos(MATH_PI * i * (2*l+1) / 16.0));
			}
		}
#if 0
		unsigned char out[64];

		for(k=0; k<64; k++)
			dct_coeff[k] = 0;

		dct_coeff[i] = 4;

		IDCT_norm(dct_coeff);

		dct_coeff[0] += 262144 + 1024;

		IDCT(out, 8, true);

		if (i)
			for(k=0; k<64; k++)
				if (abs(((int)out[k] - 128) - idct_precomputed.data[i][k]/4)>1)
					__asm int 3
#endif
	}
}

void __declspec(naked) IDCT_fast_put(int pos, void *dst, long pitch) {
	__asm {
		push	esi
		push	edi
		push	ebp
		push	ebx

#ifdef PROFILE
		rdtsc
		mov		profile_start, eax
#endif

		mov		esi,[esp+4+16]				;eax = coefficient #
		xor		ecx,ecx
		mov		eax,[dct_coeff + esi*4]		;ecx = coefficient
		mov		[dct_coeff + esi*4],ecx
		mov		edi,[esp+8+16]				;edi = dest

		or		esi,esi
		jnz		AC_coeff

		;we're doing the DC coefficient... easy!

		sar		eax,11
		mov		ecx,[esp+12+16]				;esi = pitch
		jns		DC_above_zero
		xor		eax,eax
DC_above_zero:
		cmp		eax,256
		jb		DC_below_255
		mov		eax,255
DC_below_255:

		mov		ebx,eax
		mov		edx,4
		shl		eax,8
		or		ebx,eax
		mov		eax,ebx
		shl		eax,16
		or		eax,ebx

DC_loop:
		mov		[edi+0],eax
		mov		[edi+4],eax
		mov		[edi+ecx+0],eax
		mov		[edi+ecx+4],eax
		lea		edi,[edi+ecx*2]
		dec		edx
		jne		DC_loop
		jmp		fnexit

		;AC coefficient... we have to scale the table.  Damn.
		;
		;	eax = AC coefficient
		;	ecx = DC coefficient
		;	edx = V counter
		;	esi	= table pointer
		;	ebp = H counter

AC_coeff:
		mov		ebp,esi
		and		esi,7
		shl		esi,8
		test	ebp,038h
		jnz		AC_coeff_1
		add		esi,64*4*8
AC_coeff_1:
		mov		ecx,dct_coeff
		add		esi,offset idct_precomputed
		mov		dword ptr [dct_coeff],0
		shl		ecx,8
		mov		edx,8

		shl		ebp,3
		and		ebp,0e0h
		push	ebp

		mov		ebp,8
AC_loop_vert:
		push	esi
AC_loop_horiz:
		mov		ebx,eax
		imul	ebx,[esi+edx]
		add		ebx,ecx
		sar		ebx,19
		mov		bl,[YUV_clip_table+ebx+256]
		mov		[edi],bl
		add		esi,4
		inc		edi
		add		ebp,20000000h
		jnc		AC_loop_horiz
		pop		esi
		add		edx,[esp]
		and		edx,0e0h

		mov		ebx,[esp+12+16+4]
		dec		ebp
		lea		edi,[edi+ebx-8]
		jne		AC_loop_vert

		pop		eax

fnexit:
#ifdef PROFILE
		rdtsc
		mov		ebx,eax
		sub		eax,profile_start
		inc		profile_fastputs
		add		profile_fastput_cycles,eax
		sub		ebx,profile_last
		cmp		ebx,CYCLES_PER_SECOND
		jb		notsec
		call	profile_update
notsec:
#endif
		pop		ebx
		pop		ebp
		pop		edi
		pop		esi
		ret
	}
}

void __declspec(naked) IDCT_fast_add(int pos, void *dst, long pitch) {
	__asm {
		push	esi
		push	edi
		push	ebp
		push	ebx
#ifdef PROFILE
		rdtsc
		mov		profile_start, eax
#endif

		mov		esi,[esp+4+16]				;eax = coefficient #
		xor		ecx,ecx
		mov		eax,[dct_coeff + esi*4]		;ecx = coefficient
		mov		edi,[esp+8+16]				;edi = dest
		mov		[dct_coeff + esi*4],ecx		;clear coefficient :)

		or		esi,esi
		jnz		AC_coeff

		;we're doing the DC coefficient... easy!

		sar		eax,11
		mov		ebp,[esp+12+16]

		add		eax,offset YUV_clip_table+256
		mov		esi,8

		push	ebp
		mov		ebp,eax
		xor		eax,eax
		xor		ebx,ebx
		xor		ecx,ecx
		xor		edx,edx
DC_loop:
		mov		al,[edi+0]
		mov		bl,[edi+4]
		mov		cl,[edi+1]
		mov		dl,[edi+5]
		mov		al,[ebp+eax]
		mov		bl,[ebp+ebx]
		mov		cl,[ebp+ecx]
		mov		dl,[ebp+edx]
		mov		[edi+0],al
		mov		[edi+4],bl
		mov		[edi+1],cl
		mov		[edi+5],dl

		mov		al,[edi+2]
		mov		bl,[edi+6]
		mov		cl,[edi+3]
		mov		dl,[edi+7]
		mov		al,[ebp+eax]
		mov		bl,[ebp+ebx]
		mov		cl,[ebp+ecx]
		mov		dl,[ebp+edx]
		mov		[edi+2],al
		mov		[edi+6],bl
		mov		[edi+3],cl
		mov		[edi+7],dl

		add		edi,[esp]
		dec		esi
		jne		DC_loop
		pop		ebp
		jmp		fnexit

		;AC coefficient... we have to scale the table.  Damn.
		;
		;	eax = AC coefficient
		;	ecx = DC coefficient
		;	edx = V counter
		;	esi	= table pointer
		;	ebp = H counter

AC_coeff:
		mov		ebp,esi
		and		esi,7
		shl		esi,8
		test	ebp,038h
		jnz		AC_coeff_1
		add		esi,64*4*8
AC_coeff_1:
		mov		ecx,dct_coeff
		add		esi,offset idct_precomputed
		mov		dword ptr [dct_coeff],0
		shl		ecx,8
		mov		edx,8

		shl		ebp,3
		and		ebp,0e0h
		push	ebp

		mov		ebp,8
AC_loop_vert:
AC_loop_horiz:
		mov		ebx,[esp]
		xor		eax,eax
		imul	ebx,[esi]
		add		ebx,ecx
		sar		ebx,19
		mov		al,[edi]
		mov		bl,[YUV_clip_table+ebx+eax+256]
		mov		[edi],bl
		add		esi,4
		inc		edi
		add		ebp,20000000h
		jnc		AC_loop_horiz

		mov		ebx,[esp+12+20]
		dec		ebp
		lea		edi,[edi+ebx-8]
		jne		AC_loop_vert

		pop		eax

fnexit:
#ifdef PROFILE
		rdtsc
		mov		ebx,eax
		sub		eax,profile_start
		inc		profile_fastadds
		add		profile_fastadd_cycles,eax
		sub		ebx,profile_last
		cmp		ebx,CYCLES_PER_SECOND
		jb		notsec
		call	profile_update
notsec:
#endif
		pop		ebx
		pop		ebp
		pop		edi
		pop		esi
		ret
	}
}

void __declspec(naked) IDCT_fast_put_MMX(int val, void *dst, long pitch) {
	__asm {
		push	esi
		push	edi
		push	ebp
		push	ebx

#ifdef PROFILE
		rdtsc
		mov		profile_start, eax
#endif

		xor		ecx,ecx
		mov		edi,[esp+8+16]				;edi = dest
		mov		ecx,[esp+12+16]				;esi = pitch

		movd		mm6,[esp+4+16]			;mm6 = coefficient
		psrad		mm6,3
		packuswb	mm6,mm6
		punpcklbw	mm6,mm6
		punpcklwd	mm6,mm6
		mov			edx,4
		punpckldq	mm6,mm6

DC_loop_MMX:
		movq		[edi],mm6
		movq		[edi+ecx],mm6
		lea			edi,[edi+ecx*2]
		dec			edx
		jne			DC_loop_MMX

#ifdef PROFILE
		rdtsc
		mov		ebx,eax
		sub		eax,profile_start
		inc		profile_fastputs
		add		profile_fastput_cycles,eax
		sub		ebx,profile_last
		cmp		ebx,CYCLES_PER_SECOND
		jb		notsec
		call	profile_update
notsec:
#endif
		pop		ebx
		pop		ebp
		pop		edi
		pop		esi
		ret
	}
}

void __declspec(naked) __cdecl IDCT_fast_add_MMX(int val, void *dst, long pitch) {
	__asm {
		push	esi
		push	edi
		push	ebp
		push	ebx
#ifdef PROFILE
		rdtsc
		mov		profile_start, eax
#endif

		xor		ecx,ecx
		mov		eax,[esp+4+16]				;eax = coefficient
		mov		edi,[esp+8+16]				;edi = dest

		sar		eax,3
		mov		ebp,[esp+12+16]

		movd		mm6,eax
		pxor		mm7,mm7
		punpcklbw	mm6,mm6
		punpcklwd	mm6,mm6
		mov			edx,4
		punpckldq	mm6,mm6
		psubb		mm7,mm6
		or			eax,eax
		js			DC_loop_MMX_sub

DC_loop_MMX_add:
		movq		mm0,[edi]
		movq		mm1,[edi+ebp]
		paddusb		mm0,mm6
		paddusb		mm1,mm6
		movq		[edi],mm0
		movq		[edi+ebp],mm1
		lea			edi,[edi+ebp*2]
		dec			edx
		jne			DC_loop_MMX_add
		jmp			fnexit

DC_loop_MMX_sub:
		movq		mm0,[edi]
		movq		mm1,[edi+ebp]
		psubusb		mm0,mm7
		psubusb		mm1,mm7
		movq		[edi],mm0
		movq		[edi+ebp],mm1
		lea			edi,[edi+ebp*2]
		dec			edx
		jne			DC_loop_MMX_sub

fnexit:
#ifdef PROFILE
		rdtsc
		mov		ebx,eax
		sub		eax,profile_start
		inc		profile_fastadds
		add		profile_fastadd_cycles,eax
		sub		ebx,profile_last
		cmp		ebx,CYCLES_PER_SECOND
		jb		notsec
		call	profile_update
notsec:
#endif
		pop		ebx
		pop		ebp
		pop		edi
		pop		esi
		ret
	}
}

#define coeff dct_coeff

void IDCT_norm(int *m1) {
	double d;
	int i,j;

	for (j = 0; j < 8; j++) {
		for (i = 0; i < 8; i++) {
			d = (double)m1[j*8+i];
			if (i == 0 && j == 0) {
				d /= 8.0;
			}
			else if (i == 0 || j == 0) {
				d /= 4.0 * sqrt(2.0);
			}
			else {
				d /= 4.0;
			}
			m1[j*8+i] = (int)floor(2048.0 * 16.0 * d * cos(i * (3.1415926535 / 16.0)) * cos(j * (3.1415926535 / 16.0)) + 0.5);
		}
	}
}

///////////////////////////////////////////////////////////////////////////

void __declspec(naked) IDCT(void *dst, long modulo, int intra) {

	__asm {
		push	esi
		push	edi
		push	ebp
		push	ebx

#ifdef PROFILE
		rdtsc
		mov		profile_start, eax
#endif

	// compute B1 (horizontal / vertical algoritm):
	// (the vertical part is in tensor product)
	//
	// <0> = [0]
	// <1> = [4]
	// <2> = [2] - [6]
	// <3> = [2] + [6]
	// <4> = [5] - [3]
	// <5> = [1] + [7] - ([5] + [3])
	// <6> = [1] - [7]
	// <7> = [1] + [7] + ([5] + [3])

		mov		ebp,7*4
		mov		edi,0

	idct_B1_loop:
		mov		eax,[coeff+ebp*8+1*4]		;eax = [1]
		mov		ebx,[coeff+ebp*8+7*4]		;ebx = [7]

		mov		[coeff+ebp*8+1*4],edi
		mov		[coeff+ebp*8+7*4],edi

		mov		edx,[coeff+ebp*8+5*4]		;edx = [5]
		mov		esi,[coeff+ebp*8+3*4]		;esi = [3]

		mov		[coeff+ebp*8+5*4],edi
		mov		[coeff+ebp*8+3*4],edi

		lea		ecx,[eax+ebx]				;ecx = [1] + [7]
		sub		eax,ebx						;eax = [1] - [7]

		lea		ebx,[edx+esi]				;ebx = [5] + [3]
		sub		edx,esi						;edx = [5] - [3]

		mov		[matr1+ebp+4*32],edx
		mov		esi,[coeff+ebp*8+2*4]		;esi = [2]

		lea		edx,[ecx+ebx]				;edx = ([1] + [7]) + ([5] + [3])
		mov		[matr1+ebp+6*32],eax

		sub		ecx,ebx						;ecx = ([1] + [7]) - ([5] + [3])
		mov		ebx,[coeff+ebp*8+6*4]		;ebx = [6]

		mov		[coeff+ebp*8+2*4],edi
		mov		[coeff+ebp*8+6*4],edi

		mov		[matr1+ebp+7*32],edx
		mov		edx,[coeff+ebp*8+0*4]		;edx = [0]

		lea		eax,[esi+ebx]				;eax = [2] + [6]
		mov		[matr1+ebp+5*32],ecx

		mov		[matr1+ebp+3*32],eax
		sub		esi,ebx						;esi = [2] - [6]

		mov		ecx,[coeff+ebp*8+4*4]		;ecx = [4]
		mov		[matr1+ebp+0*32],edx

		mov		[matr1+ebp+2*32],esi
		mov		[coeff+ebp*8+0*4],edi

		mov		[coeff+ebp*8+4*4],edi
		mov		[matr1+ebp+1*32],ecx

		sub		ebp,4
		jnc		idct_B1_loop

	// compute the vertical part and the (tensor product) M:

		xor		ebp,ebp
		call	IDCT_vert_0137
		mov		ebp,1*8*4
		call	IDCT_vert_0137
		mov		ebp,3*8*4
		call	IDCT_vert_0137
		mov		ebp,7*8*4
		call	IDCT_vert_0137
		call	IDCT_vert_25
		call	IDCT_vert_46

	//	co35=matr1[5+32]-matr1[3+32];
	//	co17=matr1[1+48]-matr1[7+48];
	//	l2 = co35+co17;
	//	l0 = co35-co17;

	//	co35=matr1[5+48]-matr1[3+48];
	//	co17=matr1[1+32]-matr1[7+32];
	//	l1 = co35+co17;
	//	l3 = co35-co17; 

		mov		ecx,matr1[(5+32)*4]
		mov		esi,matr1[(1+48)*4]
		sub		ecx,matr1[(3+32)*4]		;ecx = co35
		sub		esi,matr1[(7+48)*4]		;eax = co17
		mov		edi,matr1[(5+48)*4]
		mov		ebx,matr1[(1+32)*4]
		sub		edi,matr1[(3+48)*4]		;edi = co35
		sub		ebx,matr1[(7+32)*4]		;ebx = co17

		mov		eax,ecx
		mov		edx,edi
		add		ecx,esi					;ecx = l2 = co35+co17
		add		edi,ebx					;edi = l1 = co35+co17
		sub		eax,esi					;eax = l0 = co35-co17
		sub		edx,ebx					;edx = l3 = co35-co17

	//	g0 = C4*(l0+l1);
	//	g1 = C4*(l0-l1);
	//	g2 = l2<<12;
	//	g3 = l3<<12;

		shl		ecx,12					;ecx = g2 = l2<<12
		mov		ebx,eax
		shl		edx,12					;edx = g3 = l3<<12
		add		eax,edi					;eax = l0+l1	(a)
		sub		ebx,edi					;ebx = l0-l1	(b)
		nop

#ifdef FAST_IMUL
		imul	eax,2896
		imul	ebx,2896
#else
		shl		ebx,4					;b/1
		lea		esi,[eax*8+eax]			;a/1
		lea		edi,[ebx*8+ebx]			;b/2
		lea		esi,[esi*4+esi]			;a/2
		lea		edi,[edi*4+edi]			;b/3
		lea		eax,[esi*4+eax]			;a/3
		shl		eax,4					;a/4	eax = g0 = C4*(l0+l1)
		lea		ebx,[edi*4+ebx]			;b/4	ebx = g1 = C4*(l0-l1)
#endif

		;	eax = g0
		;	ebx = g1
		;	ecx = g2
		;	edx = g3

	//	matr2[38] = g1+g3;
	//	matr2[52] = g1-g3;
	//	matr2[36] = g2+g0;
	//	matr2[54] = g2-g0;

		mov		esi,ebx
		mov		edi,ecx
		add		ebx,edx					;ebx = g1+g3
		add		ecx,eax					;ecx = g2+g0
		sub		esi,edx					;esi = g1-g3
		sub		edi,eax					;edi = g2-g0

		mov		matr2[(6+32)*4],ebx
		mov		matr2[(4+48)*4],esi
		mov		matr2[(4+32)*4],ecx
		mov		matr2[(6+48)*4],edi

	//	tmp = C6*(matr2[32]+matr2[48]);
	//	matr2[32] = -Q*matr2[32]-tmp;
	//	matr2[48] =  R*matr2[48]-tmp;

	//	tmp = C6*(matr2[33] + matr2[49]);
	//	matr2[33] = -Q*matr2[33]-tmp;
	//	matr2[49] =  R*matr2[49]-tmp;

	//	tmp = C4C6 * (matr2[34] + matr2[50]);
	//	matr2[34] = -C4Q*matr2[34]-tmp;
	//	matr2[50] =  C4R*matr2[50]-tmp;

	//	tmp = C6*(matr2[35] + matr2[51]);
	//	matr2[35] = -Q*matr2[35]-tmp;
	//	matr2[51] =  R*matr2[51]-tmp;

	//	tmp = C4C6 * (matr2[37] + matr2[53]);
	//	matr2[37] = -C4Q*matr2[37]-tmp;
	//	matr2[53] =  C4R*matr2[53]-tmp;

	//	tmp = C6*(matr2[39] + matr2[55]);
	//	matr2[39] = -Q*matr2[39]-tmp;
	//	matr2[55] =  R*matr2[55]-tmp;

		;--- 0,1

		mov		eax,matr2[32*4]		;(1)
		mov		ebx,matr2[33*4]		;(2)
		mov		ecx,matr2[48*4]		;(1)
		mov		edx,matr2[49*4]		;(2)

		add		ecx,eax				;u (1)
		add		edx,ebx				;v (2)

		mov		eax,matr2[32*4]		;u (1)
		mov		ebx,matr2[48*4]		;v (1)

#ifdef FAST_IMUL
		imul	ecx,1567+1
		imul	eax,2217
		imul	ebx,5352
#else
		lea		ebp,[ecx+ecx*2]		;u (1c/1) ecx = C6*(matr2[32] + matr2[48])
		nop							;v
		lea		esi,[eax+eax]		;u (1a/1) Q*matr2[32]
		lea		edi,[ebx*4+ebx]		;v (1b/1) R*matr2[48]
		shl		edi,3				;u (1b/2)
		nop							;v
		shl		ebp,09H				;u (1c/2)
		lea		esi,[esi*8+eax]		;v (1a/2)
		shl		ecx,05H				;u (1c/3)
		lea		ebx,[edi*4+ebx]		;v (1b/3)
		lea		esi,[esi*4+eax]		;u (1a/3)
		add		ecx,ebp				;v (1c/4)
		nop							;u
		lea		ebx,[ebx*8+edi]		;v (1b/4)
		lea		esi,[esi*4+eax]		;u (1a/4)
		nop							;v

		lea		ebx,[ebx*4+edi]		;u (1b/5)
		nop							;v
		lea		eax,[esi*8+eax]		;u (1a/5)
#endif
		sub		ebx,ecx				;v (1) R*matr2[48]-tmp
		add		ecx,eax				;u (1) Q*matr2[32]+tmp
		xor		ecx,-1				;u (1)
		mov		matr2[48*4],ebx		;v (1)
		inc		ecx					;u (1)
		mov		eax,matr2[33*4]		;v (2)
		mov		matr2[32*4],ecx		;u (1)
		mov		ebx,matr2[49*4]		;v (2)

#ifdef FAST_IMUL
		imul	edx,1567+1
		imul	eax,2217
		imul	ebx,5352
#else
		lea		ebp,[edx+edx*2]		;u (2c/1) edx = C6*(matr2[33] + matr2[49])
		nop							;v
		lea		esi,[eax+eax]		;u (2a/1) Q*matr2[33]
		lea		edi,[ebx*4+ebx]		;v (2b/1) R*matr2[49]
		shl		edi,3				;u (2b/2)
		nop							;v
		shl		ebp,09H				;u (2c/2)
		lea		esi,[esi*8+eax]		;v (2a/2)
		nop							;u
		lea		ebx,[edi*4+ebx]		;v (2b/3)
		lea		esi,[esi*4+eax]		;u (2a/3)
		nop							;v
		shl		edx,05H				;u (2c/3)
		lea		ebx,[ebx*8+edi]		;v (2b/4)
		lea		esi,[esi*4+eax]		;u (2a/4)
		add		edx,ebp				;v (2c/4)

		lea		ebx,[ebx*4+edi]		;u (2b/5)
		nop							;v
		lea		eax,[esi*8+eax]		;u (2a/5)
#endif
		sub		ebx,edx				;v (2) R*matr2[49]-tmp
		add		edx,eax				;u (2) Q*matr2[33]+tmp
		xor		edx,-1				;u (2)
		mov		matr2[49*4],ebx		;v (2)
		inc		edx					;u (2)
		mov		eax,matr2[35*4]		;v (1) [3,7]
		mov		matr2[33*4],edx		;u (2)
		mov		ebx,matr2[39*4]		;v (2) [3,7]

		;--- 3, 7

		mov		ecx,matr2[51*4]		;(1)
		mov		edx,matr2[55*4]		;(2)

		add		ecx,eax				;(1)
		add		edx,ebx				;(2)

		mov		eax,matr2[35*4]		;(1)
		mov		ebx,matr2[51*4]		;(2)

#ifdef FAST_IMUL
		imul	ecx,1567+1
		imul	eax,2217
		imul	ebx,5352
#else
		lea		ebp,[ecx+ecx*2]		;u (1c/1) ecx = C6*(matr2[32] + matr2[48])
		nop							;v
		lea		esi,[eax+eax]		;u (1a/1) Q*matr2[32]
		lea		edi,[ebx*4+ebx]		;v (1b/1) R*matr2[48]
		shl		edi,3				;u (1b/2)
		nop							;v
		shl		ebp,09H				;u (1c/2)
		lea		esi,[esi*8+eax]		;v (1a/2)
		nop							;u
		lea		ebx,[edi*4+ebx]		;v (1b/3)
		lea		esi,[esi*4+eax]		;u (1a/3)
		nop
		shl		ecx,05H				;u (1c/3)
		lea		ebx,[ebx*8+edi]		;v (1b/4)
		lea		esi,[esi*4+eax]		;u (1a/4)
		add		ecx,ebp				;v (1c/4)
		lea		ebx,[ebx*4+edi]		;u (1b/5)
		nop							;v
		lea		eax,[esi*8+eax]		;u (1a/5)

#endif

		sub		ebx,ecx				;u (1) R*matr2[48]-tmp
		add		ecx,eax				;v (1) Q*matr2[32]+tmp
		xor		ecx,-1				;u (1)
		mov		matr2[51*4],ebx		;v (1)
		inc		ecx					;u (1)
		mov		eax,matr2[39*4]		;v (2)
		mov		matr2[35*4],ecx		;u (1)
		mov		ebx,matr2[55*4]		;v (2)

#ifdef FAST_IMUL
		imul	edx,1567+1
		imul	eax,2217
		imul	ebx,5352
#else
		lea		ebp,[edx+edx*2]		;u (2c/1) edx = C6*(matr2[33] + matr2[49])
		nop							;v
		lea		esi,[eax+eax]		;u (2a/1) Q*matr2[33]
		lea		edi,[ebx*4+ebx]		;v (2b/1) R*matr2[49]
		shl		edi,3				;u (2b/2)
		nop							;v
		shl		ebp,09H				;u (2c/2)
		lea		esi,[esi*8+eax]		;v (2a/2)
		nop							;u
		lea		ebx,[edi*4+ebx]		;v (2b/3)
		lea		esi,[esi*4+eax]		;u (2a/3)
		nop							;v
		shl		edx,05H				;u (2c/3)
		lea		ebx,[ebx*8+edi]		;v (2b/4)
		lea		esi,[esi*4+eax]		;u (2a/4)
		add		edx,ebp				;v (2c/4)

		lea		ebx,[ebx*4+edi]		;u (2b/5)
		nop							;v
		lea		eax,[esi*8+eax]		;u (2a/5)
#endif
		sub		ebx,edx				;v (2) R*matr2[49]-tmp
		add		edx,eax				;u (2) Q*matr2[33]+tmp
		xor		edx,-1				;u (2)
		mov		matr2[55*4],ebx		;v (2)
		inc		edx					;u (2)
		mov		eax,matr2[34*4]		;v (1) [2,5]
		mov		matr2[39*4],edx		;u (2)
		mov		ebx,matr2[37*4]		;v (2) [2,5]

		;--- 2,5

		mov		ecx,matr2[50*4]		;u (1)
		mov		edx,matr2[53*4]		;v (2)

		add		ecx,eax				;u (1)
		add		edx,ebx				;v (2)

		mov		eax,matr2[34*4]		;u (1)
		mov		ebx,matr2[50*4]		;v (1)

#ifdef FAST_IMUL
		imul	ebx,7568
		imul	eax,3135
		imul	ecx,2217
#else
		lea		edi,[ebx*4+ebx]		;u (1b/1) C4R*matr2[50]
		lea		esi,[eax*2+eax]		;v (1a/1) C4Q*matr2[34]
		lea		ebp,[ecx+ecx]		;u (1c/1) ecx = C4C6*(matr2[32] + matr2[48])
		nop
		lea		ebx,[ebx*8+edi]		;u (1b/2)
		lea		eax,[esi*8]			;v (1a/2)
		lea		ebp,[ebp*8+ecx]		;u (1c/2)
		nop
		lea		ebx,[ebx*8+ebx]		;u (1b/3)
		lea		eax,[eax*8+esi]		;v (1a/3)
		lea		ebp,[ebp*4+ecx]		;u (1c/3)
		nop
		lea		ebx,[ebx*4+edi]		;u (1b/4)
		lea		eax,[eax*4+esi]		;v (1a/4)
		lea		ebp,[ebp*4+ecx]		;u (1c/4)
		nop
		shl		ebx,4				;u (1b/5)
		lea		eax,[eax*4+esi]		;v (1a/5)
		lea		ecx,[ebp*8+ecx]		;u (1c/5)
#endif
		sub		ebx,ecx				;u (1) C4R*matr2[50]-tmp
		add		ecx,eax				;v (1) C4Q*matr2[34]+tmp
		xor		ecx,-1				;u (1)
		mov		matr2[50*4],ebx		;v (1)

		mov		eax,matr2[37*4]		;u (2)
		inc		ecx					;v (1)
		mov		ebx,matr2[53*4]		;u (2)
		mov		matr2[34*4],ecx		;v (1)

#ifdef FAST_IMUL
		imul	ebx,7568
		imul	eax,3135
		imul	edx,2217
#else
		lea		ebp,[edx+edx]		;v (2c/1) edx = C4C6*(matr2[33] + matr2[49])
		nop							;u
		lea		esi,[eax*2+eax]		;v (2a/1) C4Q*matr2[37]
		lea		edi,[ebx*4+ebx]		;u (2b/1) C4R*matr2[53]

		lea		ebp,[ebp*8+edx]		;u (2c/2)
		nop
		lea		ebx,[ebx*8+edi]		;u (2b/2)
		lea		eax,[esi*8]			;v (2a/2)
		lea		ebp,[ebp*4+edx]		;u (2c/3)
		nop
		lea		ebx,[ebx*8+ebx]		;u (2b/3)
		lea		eax,[eax*8+esi]		;(2a/3)
		lea		ebp,[ebp*4+edx]		;u (2c/4)
		nop
		lea		ebx,[ebx*4+edi]		;u (2b/4)
		lea		eax,[eax*4+esi]		;(2a/4)
		lea		edx,[ebp*8+edx]		;u (2c/5)
		nop
		shl		ebx,4				;u (2b/5)
		lea		eax,[eax*4+esi]		;(2a/5)
#endif
		add		eax,edx				;u (2) C4Q*matr2[37]+tmp
		sub		ebx,edx				;v (2) C4R*matr2[53]-tmp
		xor		eax,-1				;u (2)
		mov		matr2[53*4],ebx		;v (2)
		inc		eax					;u (2)
		mov		matr2[37*4],eax		;u (2)

		;--- done
		
	// compute A1 x A2 x A3 (horizontal/vertical algoritm):

		mov		ebp,7*4

	idct_A1A2_loop:
		mov		edi,[matr2+ebp*8+0*4]		;edi = [0]
		mov		eax,[matr2+ebp*8+1*4]		;eax = [1]

		mov		ebx,[matr2+ebp*8+3*4]		;ebx = [3]
		mov		ecx,[matr2+ebp*8+2*4]		;ecx = [2]

		lea		esi,[edi+eax]				;esi = [0]+[1]
		sub		edi,eax						;edi = [0]-[1]

		mov		edx,[matr2+ebp*8+7*4]		;edx = [7]
		sub		ecx,ebx						;ecx = [2]-[3]

		lea		eax,[esi+ebx]				;eax = [0]+[1]+[3]
		mov		ebx,edi						;ebx = [0]-[1]

		add		ebx,ecx						;ebx = [0]-[1]+[2]-[3]
		sub		edi,ecx						;edi = [0]-[1]-[2]+[3]

		lea		ecx,[eax+edx]				;ecx = [0]+[1]+[3]+[7]
		sub		eax,edx						;eax = [0]+[1]+[3]-[7]

		sub		edx,[matr2+ebp*8+6*4]		;edx = -[6]+[7]
		sub		esi,[matr2+ebp*8+3*4]		;esi = [0]+[1]+[3]

		mov		[matr1+ebp+0*32],ecx
		mov		ecx,[matr2+ebp*8+5*4]		;ecx = [5]

		mov		[matr1+ebp+7*32],eax
		lea		eax,[ebx+edx]				;eax = [0]-[1]+[2]-[3]-[6]+[7]

		sub		ebx,edx						;ebx = [0]-[1]+[2]-[3]+[6]-[7]
		mov		[matr1+ebp+6*32],eax

		add		edx,ecx						;edx = [5]-[6]+[7]
		mov		ecx,[matr2+ebp*8+4*4]		;eax = [4]

		mov		[matr1+ebp+1*32],ebx
		add		ecx,edx						;ecx = [4]+[5]-[6]+[7]

		lea		ebx,[edi+edx]				;ebx = [0]-[1]-[2]+[3]+[5]-[6]+[7]
		sub		edi,edx						;edi = [0]-[1]-[2]+[3]-[5]+[6]-[7]

		lea		eax,[esi+ecx]				;eax = [0]+[1]-[3]+[4]+[5]-[6]+[7]
		mov		[matr1+ebp+2*32],ebx

		mov		[matr1+ebp+5*32],edi
		sub		esi,ecx						;esi = [0]+[1]-[3]-[4]-[5]+[6]-[7]

		mov		[matr1+ebp+4*32],eax
		mov		[matr1+ebp+3*32],esi

		sub		ebp,4
		jnc		idct_A1A2_loop

		;******************************************

		mov		ebp,-8*32
		mov		esi,[esp+4+16]
		mov		eax,esi

		test	dword ptr [esp+12+16],-1
		jnz		idct_final_loop_intra

	idct_final_loop_inter:
		mov		eax,[matr1+ebp+8*32+0*4]			;eax = [0]
		mov		ebx,[matr1+ebp+8*32+1*4]			;ebx = [1]

		mov		edi,[matr1+ebp+8*32+3*4]			;edi = [3]
		add		eax,ebx			;eax = [0] + [1]

		add		eax,edi			;eax = [0] + [1] + [3]
		mov		edx,[matr1+ebp+8*32+7*4]			;edx = [7]

		mov		ebx,eax			;ebx = [0] + [1] + [3]
		add		eax,edx			;eax = [0] + [1] + [3] + [7]

		sub		ebx,edx			;ebx = [0] + [1] + [3] - [7]

		mov		edx,eax
		xor		ecx,ecx

		sar		edx,22
		mov		cl,[esi+0]

		mov		dl,[YUV_clip_table+edx+ecx+256]
		mov		cl,[esi+7]

		mov		[esi+0],dl
		mov		edx,ebx

		sar		edx,22

		mov		dl,[YUV_clip_table+edx+ecx+256]

		mov		[esi+7],dl
		mov		edi,[matr1+ebp+8*32+1*4]			;edi = [1]

		mov		edx,[matr1+ebp+8*32+3*4]			;edx = [3]
		mov		ecx,[matr1+ebp+8*32+2*4]			;ecx = [2]

		add		edi,edx			;edi = [1] + [3]
		mov		edx,[matr1+ebp+8*32+6*4]			;esi = [6]

		add		edi,edi			;edi = 2[1] + 2[3]
		sub		eax,edx			;eax = [0] + [1] + [3] - [6] + [7]

		sub		edi,ecx			;edi = 2[1] - [2] + 2[3]
		add		ebx,edx			;ebx = [0] + [1] + [3] + [6] - [7]

		sub		eax,edi			;eax = [0] - [1] + [2] - [3] - [6] + [7]
		sub		ebx,edi			;ebx = [0] - [1] + [2] - [3] + [6] - [7]

		mov		ecx,eax
		xor		edx,edx

		sar		ecx,22
		mov		dl,[esi+6]

		mov		cl,[YUV_clip_table+ecx+edx+256]
		mov		dl,[esi+1]

		mov		[esi+6],cl
		mov		ecx,ebx

		sar		ecx,22

		mov		cl,[YUV_clip_table+ecx+edx+256]

		mov		[esi+1],cl
		mov		edx,[matr1+ebp+8*32+2*4]

		sub		edx,[matr1+ebp+8*32+3*4]			;edx = [2] - [3]
		mov		ecx,[matr1+ebp+8*32+5*4]

		add		edx,edx			;edx = 2[2] - 2[3]
		add		eax,ecx			;eax = [0] - [1] + [2] - [3] + [5] - [6] + [7]

		sub		ebx,ecx			;ebx = [0] - [1] + [2] - [3] - [5] + [6] - [7]
		sub		eax,edx			;eax = [0] - [1] - [2] + [3] + [5] - [6] + [7]

		sub		ebx,edx			;ebx = [0] - [1] - [2] + [3] - [5] + [6] - [7]
		mov		edx,eax

		sar		edx,22
		xor		ecx,ecx

		mov		cl,[esi+2]

		mov		dl,[YUV_clip_table+ecx+edx+256]
		mov		cl,[esi+5]

		mov		[esi+2],dl
		mov		edx,ebx

		sar		edx,22

		mov		dl,[YUV_clip_table+ecx+edx+256]

		mov		[esi+5],dl
		mov		ecx,[matr1+ebp+8*32+1*4]

		shl		ecx,2			;esi = 4[1]
		mov		edx,[matr1+ebp+8*32+4*4]

		add		eax,edx			;eax = [0] - [1] - [2] + [3] + [4] + [5] - [6] + [7]
		sub		edi,ecx			;edi = -2[1] - [2] + 2[3]

		sub		ebx,edx			;ebx = [0] - [1] - [2] + [3] - [4] - [5] + [6] - [7]
		sub		eax,edi			;eax = [0] + [1]       - [3] - [4] + [5] - [6] + [7]

		sar		eax,22
		sub		ebx,edi			;ebx = [0] + [1]       - [3] + [4] - [5] + [6] - [7]

		sar		ebx,22
		xor		ecx,ecx

		xor		edx,edx
		mov		edi,[esp+8+16]

		mov		cl,[esi+4]
		mov		dl,[esi+3]

		mov		al,[YUV_clip_table+eax+ecx+256]
		mov		bl,[YUV_clip_table+ebx+edx+256]

		mov		[esi+4],al
		mov		[esi+3],bl

		add		esi,edi

		add		ebp,32
		jnz		idct_final_loop_inter

finish:
#ifdef PROFILE
		rdtsc
		mov		ebx,eax
		sub		eax,profile_start
		inc		profile_slows
		add		profile_slow_cycles,eax
		sub		ebx,profile_last
		cmp		ebx,CYCLES_PER_SECOND
		jb		notsec
		call	profile_update
notsec:
#endif
		pop		ebx
		pop		ebp
		pop		edi
		pop		esi
		ret

		align	16
idct_final_loop_intra:
		mov		cl,[eax]

		mov		eax,[matr1+ebp+8*32+4*0]	;eax = [0]
		mov		ebx,[matr1+ebp+8*32+4*1]	;ebx = [1]

		mov		esi,[matr1+ebp+8*32+4*3]	;esi = [3]
		mov		edx,[matr1+ebp+8*32+4*2]	;edx = [2]

		lea		ecx,[eax+ebx]				;ecx = [0]+[1]
		sub		eax,ebx						;eax = [0]-[1]

		add		ecx,esi						;ecx = [0]+[1]+[3]
		mov		ebx,[matr1+ebp+8*32+4*7]	;ebx = [7]

		sub		esi,edx						;esi = [3]-[2]
		mov		edi,[matr1+ebp+8*32+4*6]	;edi = [6]

		sub		eax,esi						;eax = [0]-[1]+[2]-[3]
		lea		edx,[ecx+ebx]				;edx = [0]+[1]+[3]+[7]

		sub		ecx,ebx						;ecx = [0]+[1]+[3]-[7]
		sub		ebx,edi						;ebx = -[6]+[7]

		sar		edx,22						;edx = <0>
		mov		edi,[esp+4+16]

		sar		ecx,22						;ecx = <7>
		mov		dl,[YUV_clip_table+edx+256]	;dl = FINAL[0]

		mov		dh,[YUV_clip_table+ecx+256]	;dh = FINAL[7]
		lea		ecx,[eax+ebx]				;ecx = [0]-[1]+[2]-[3]-[6]+[7]

		lea		esi,[esi*2+eax]				;esi = [0]-[1]-[2]+[3]
		mov		[edi+0],dl

		mov		[edi+7],dh
		sub		eax,ebx						;eax = [0]-[1]+[2]-[3]+[6]-[7]

		sar		eax,22
		mov		edx,[matr1+ebp+8*32+4*5]	;edi = [5]

		sar		ecx,22
		add		ebx,edx						;ebx = [5]-[6]+[7]

		mov		al,[YUV_clip_table+eax+256]	;al = FINAL[1]
		mov		edx,[matr1+ebp+8*32+4*0]	;edx = [0]

		mov		ah,[YUV_clip_table+ecx+256]	;ah = FINAL[6]
		lea		ecx,[esi+ebx]				;ecx = [0]-[1]-[2]+[3]+[5]-[6]+[7]

		sub		esi,ebx						;esi = [0]-[1]-[2]+[3]-[5]+[6]-[7]
		mov		[edi+1],al

		mov		[edi+6],ah
		mov		eax,[matr1+ebp+8*32+4*1]	;eax = [1]

		sar		ecx,22
		add		edx,eax						;edx = [0]+[1]

		sar		esi,22
		mov		eax,[matr1+ebp+8*32+4*3]	;eax = [3]

		sub		edx,eax						;edx = [0]+[1]-[3]
		mov		eax,[matr1+ebp+8*32+4*4]	;eax = [4]

		mov		cl,[YUV_clip_table+ecx+256]	;cl = FINAL[2]
		add		ebx,eax						;ebx = [4]+[5]-[6]+[7]

		mov		ch,[YUV_clip_table+esi+256]	;ch = FINAL[5]
		mov		eax,[esp+8+16]

		add		eax,edi
		lea		esi,[edx+ebx]				;esi = [0]+[1]-[3]+[4]+[5]-[6]+[7]

		sar		esi,22
		sub		edx,ebx						;edx = [0]+[1]-[3]-[4]-[5]+[6]-[7]

		sar		edx,22
		mov		[edi+2],cl

		mov		[edi+5],ch
		mov		bl,[YUV_clip_table+esi+256]

		mov		cl,[YUV_clip_table+edx+256]
		mov		[edi+4],bl

		mov		[edi+3],cl
		add		ebp,32

		mov		[esp+4+16],eax
		jnz		idct_final_loop_intra

		jmp		finish
	}


IDCT_vert_0137:
	__asm {

//	tmp4 = matr1[3+p]-matr1[5+p];
//	tmp6 = matr1[1+p]-matr1[7+p];
//	tmp = C6 * (tmp6-tmp4);
//	matr2[p+4] =  Q*tmp4-tmp;
//	matr2[p+6] =  R*tmp6-tmp;

		mov		eax,matr1[ebp+3*4]			;eax = matr1[3+p]
		mov		ebx,matr1[ebp+1*4]			;ebx = matr1[1+p]
		sub		eax,matr1[ebp+5*4]			;eax = tmp4 = matr1[3+p] - matr1[5+p]
		sub		ebx,matr1[ebp+7*4]			;ebx = tmp6 = matr1[1+p] - matr1[7+p]

#ifdef FAST_IMUL
		mov		ecx,ebx
		sub		ecx,eax						;ecx = C6*(tmp6-tmp4)
		imul	ecx,1567+1
		imul	eax,2217
		imul	ebx,5352
#else
		lea		esi,[eax+eax]				;(a/1) Q*tmp4
		lea		edi,[ebx*4+ebx]				;(b/1) R*tmp6
		shl		edi,3						;(b/2)
		mov		ecx,ebx

		lea		esi,[esi*8+eax]				;(a/2)
		sub		ecx,eax						;ecx = tmp6-tmp4
		nop
		lea		ebx,[edi*4+ebx]				;(b/3)

		lea		esi,[esi*4+eax]				;(a/3)
		lea		edx,[ecx+ecx*2]				;(c/1) eax = C6*(tmp6-tmp4)
		shl		edx,09H						;(c/2)
		lea		ebx,[ebx*8+edi]				;(b/4)

		nop									;
		lea		esi,[esi*4+eax]				;(a/4)
		shl		ecx,05H						;(c/3)
		lea		ebx,[ebx*4+edi]				;(b/5)

		lea		eax,[esi*8+eax]				;(a/5)
		add		ecx,edx						;(c/4)
#endif

		sub		eax,ecx						;eax = Q*tmp4 - tmp;
		sub		ebx,ecx						;ebx = R*tmp6 - tmp;

		mov		matr2[ebp+4*4],eax			;matr2[p+4] = Q*tmp4 - tmp;
		mov		matr2[ebp+6*4],ebx			;matr2[p+6] = R*tmp6 - tmp;

//	co17 = matr1[p+1] + matr1[p+7];
//	co35 = matr1[p+3] + matr1[p+5];
//	matr2[p+5] =  (co17-co35)*C4;
//	matr2[p+7] =  (co17+co35)<<11;

//	matr2[p+2] =  (matr1[p+2]-matr1[p+6])*C4;
//	matr2[p+3] =  (matr1[p+2]+matr1[p+6]) << 11;

		mov		eax,matr1[ebp+1*4]			;eax = matr1[p+1]
		mov		ebx,matr1[ebp+3*4]			;ebx = matr1[p+3]
		add		eax,matr1[ebp+7*4]			;eax = co17 = matr1[p+1] + matr1[p+7]
		add		ebx,matr1[ebp+5*4]			;ebx = co35 = matr1[p+3] + matr1[p+5]

		mov		ecx,ebx						;ecx = co35
		add		ebx,eax						;ebx = co17 + co35
		shl		ebx,11						;ebx = (co17 + co35)<<11
		sub		eax,ecx						;eax = co17 - co35
		mov		matr2[ebp+7*4],ebx			;matr2[p+7] = (co17 + co35)<<11

		mov		ebx,matr1[ebp+2*4]			;ebx = matr1[p+2]
		mov		edx,matr1[ebp+6*4]			;edx = matr1[p+6]
		mov		ecx,edx						;ecx = matr1[p+6]
		add		edx,ebx						;edx = matr1[p+2] + matr1[p+6]
		shl		edx,11						;edx = (matr1[p+2] + matr1[p+6])<<11
		sub		ebx,ecx						;ebx = matr1[p+2] - matr1[p+6]
		mov		matr2[ebp+3*4],edx			;matr2[p+3] = (matr1[p+2] + matr1[p+6])<<11

		;multiply eax, ebx by C4

#ifdef FAST_IMUL
		imul	eax,2896
		imul	ebx,2896
#else
		shl		ebx,4
		lea		esi,[eax*8+eax]

		lea		esi,[esi*4+esi]
		lea		edi,[ebx*8+ebx]

		lea		eax,[esi*4+eax]
		lea		edi,[edi*4+edi]

		shl		eax,4
		lea		ebx,[edi*4+ebx]
#endif

//	matr2[p+0] =  matr1[p+0] << 11;
//	matr2[p+1] =  matr1[p+4] << 11;

		mov		ecx,matr1[ebp+0*4]
		mov		edx,matr1[ebp+4*4]

		shl		ecx,11
		mov		matr2[ebp+5*4],eax
		shl		edx,11
		mov		matr2[ebp+2*4],ebx

		mov		matr2[ebp+0*4],ecx
		mov		matr2[ebp+1*4],edx

		ret
	};


IDCT_vert_25:
	__asm {
		mov		ebp,2*8*4
		call	IDCT_vert_25_dorow
		mov		ebp,5*8*4

IDCT_vert_25_dorow:
//	tmp4 = matr1[p+3]-matr1[p+5];
//	tmp6 = matr1[p+1]-matr1[p+7];
		mov		eax, matr1[ebp+3*4]
		mov		ebx, matr1[ebp+1*4]
		sub		eax, matr1[ebp+5*4]		;eax = tmp4
		sub		ebx, matr1[ebp+7*4]		;ebx = tmp6


//	tmp = C4C6 * (tmp6-tmp4);			c
//	matr2[p+4] = C4Q*tmp4-tmp;			a
//	matr2[p+6] = C4R*tmp6-tmp;			b

#ifdef FAST_IMUL
		mov		ecx,ebx
		sub		ecx,eax					;ecx = tmp6 - tmp4
		imul	ecx,2217				;ecx = C4C6*(tmp6 - tmp4)
		imul	eax,3135
		imul	ebx,7568
#else
		mov		ecx,ebx
		nop
		lea		esi,[eax*2+eax]				;a/1
		lea		edi,[ebx*4+ebx]				;b/1

		sub		ecx,eax					;ecx = tmp6 - tmp4
		nop
		lea		eax,[esi*8]					;a/2
		lea		ebx,[ebx*8+edi]				;b/2

		nop
		lea		edx,[ecx+ecx]				;c/1
		lea		eax,[eax*8+esi]				;a/3
		lea		ebx,[ebx*8+ebx]				;b/3

		nop
		lea		edx,[edx*8+ecx]				;c/2
		lea		eax,[eax*4+esi]				;a/4
		lea		ebx,[ebx*4+edi]				;b/4

		shl		ebx,4						;b/5
		lea		edx,[edx*4+ecx]				;c/3

		lea		eax,[eax*4+esi]				;a/5
		lea		edx,[edx*4+ecx]				;c/4
		lea		ecx,[edx*8+ecx]				;c/5
#endif

		sub		eax,ecx
		sub		ebx,ecx
		mov		matr2[ebp+4*4],eax
		mov		matr2[ebp+6*4],ebx

//	co17 = co1 + co7;
//	co35 = co3 + co5;
//	matr2[p+5] = (co17-co35)<<12;
//	matr2[p+7] = (co17+co35)*C4;		(a)

//	co2=matr1[p+2];
//	co6=matr1[p+6];
//	matr2[p+2] = (co2-co6)<<12;
//	matr2[p+3] = (co2+co6)*C4;			(b)

		mov		eax,matr1[ebp+1*4]
		mov		ebx,matr1[ebp+3*4]
		mov		ecx,matr1[ebp+2*4]
		mov		edx,matr1[ebp+6*4]
		add		eax,matr1[ebp+7*4]		;eax = co17
		add		ebx,matr1[ebp+5*4]		;ebx = co35

		mov		esi,eax
		add		eax,ebx					;eax = co17+co35

		mov		edi,ecx
		sub		esi,ebx

		shl		esi,12
		add		ecx,edx
		sub		edi,edx

		shl		edi,12
		mov		matr2[ebp+5*4],esi
		mov		matr2[ebp+2*4],edi

#ifdef FAST_IMUL
		imul	ecx,2896
		imul	eax,2896
#else
		shl		ecx,4					;b/1
		lea		esi,[eax*8+eax]			;a/1
		lea		esi,[esi*4+esi]			;a/2
		lea		edi,[ecx*8+ecx]			;b/2
		lea		eax,[esi*4+eax]			;a/3
		lea		edi,[edi*4+edi]			;b/3
		shl		eax,4					;a/4
		lea		ecx,[edi*4+ecx]			;b/4
#endif

		mov		matr2[ebp+7*4],eax
		mov		matr2[ebp+3*4],ecx

//	matr2[p+0] = C4*matr1[p  ];			(a)
//	matr2[p+1] = C4*matr1[p+4];			(b)

		mov		eax,matr1[ebp+0*4]
		mov		ebx,matr1[ebp+4*4]

#ifdef FAST_IMUL
		imul	ebx,2896
		imul	eax,2896
#else
		shl		ebx,4					;b/1
		lea		esi,[eax*8+eax]			;a/1
		lea		esi,[esi*4+esi]			;a/2
		lea		edi,[ebx*8+ebx]			;b/2
		lea		eax,[esi*4+eax]			;a/3
		lea		edi,[edi*4+edi]			;b/3
		shl		eax,4					;a/4
		lea		ebx,[edi*4+ebx]			;b/4
#endif

		mov		matr2[ebp+0*4],eax
		mov		matr2[ebp+1*4],ebx

		ret
	}


IDCT_vert_46:
	__asm {
		mov		ebp,4*8*4
		call	IDCT_vert_46_dorow
		mov		ebp,6*8*4
IDCT_vert_46_dorow:
		mov		eax,matr1[ebp+0*4]
		mov		ebx,matr1[ebp+4*4]
		mov		matr2[ebp+0*4],eax
		mov		matr2[ebp+1*4],ebx

		mov		esi,matr1[ebp+2*4]
		mov		edi,matr1[ebp+6*4]

		mov		eax,matr1[ebp+1*4]
		mov		ebx,matr1[ebp+3*4]
		add		eax,matr1[ebp+7*4]
		add		ebx,matr1[ebp+5*4]

		lea		edx,[esi+edi]
		sub		esi,edi
		lea		ecx,[eax+ebx]
		sub		eax,ebx

		mov		matr2[ebp+2*4],esi
		mov		matr2[ebp+3*4],edx
		mov		matr2[ebp+5*4],eax
		mov		matr2[ebp+7*4],ecx

		ret
	};
}

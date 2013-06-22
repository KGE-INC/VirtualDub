;	VirtualDub - Video processing and capture application
;	Copyright (C) 1998-2000 Avery Lee
;
;	This program is free software; you can redistribute it and/or modify
;	it under the terms of the GNU General Public License as published by
;	the Free Software Foundation; either version 2 of the License, or
;	(at your option) any later version.
;
;	This program is distributed in the hope that it will be useful,
;	but WITHOUT ANY WARRANTY; without even the implied warranty of
;	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;	GNU General Public License for more details.
;
;	You should have received a copy of the GNU General Public License
;	along with this program; if not, write to the Free Software
;	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
;
;--------------
;
;	We include three versions of the resize filter here.  The first
;	version is the integer point-sampling filter.  There's no sense
;	in optimizing a point-sampled filter with the FPU since the
;	bottlenecks are address computation and memory bandwidth.  MMX
;	might help on Pentium-MMXs with qword-length writes, but the
;	point-sampling filter runs so fast already that there's no
;	reason to try to speed it up.  AMD K6's and PPro/PIIs will
;	write-allocate cache lines, so there's absolutely no win there.
;
;	The next 3 versions of the filter are bilinearly filtered.  The
;	integer version is included for systems with a slow FPU or no
;	FPU at all (386).  It is *very* slow at 10 multiplies per pixel.
;	It could be significantly sped up by computing all four
;	corner coefficients and then using those; with one red/blue
;	multiply and one green multiply per sampled pixel, the routine
;	could probably be sped up by 20% or so, but it's not worth the
;	work.  The MMX version works the same way; it could be modified
;	similarly, but MMX multiplies are so much faster that it would
;	slow the routine down.
;
;	The last version is the most interesting.  It's an FPU version.
;	It works by kicking the FPU into 80-bit mode and then processing
;	pixels as packed 64-bit integers: r/b in one 32-bit half, and
;	green in the other.  FPU multiplies are almost as fast as MMX
;	ones, so we gain a lot of speed.  The FPU version is about 3x
;	faster than integer on a Pentium, and still a lot faster on a
;	486.  It's the first ASM float code I've ever written, so it's
;	really messy; kind of weird that my first FPU code would have
;	nothing to do with numerics, eh?
;
;	Approximate ideal cycle counts per pixel (Pentium):
;
;		Point sampled:	3
;		Integer:	130-150
;		FPU:		40-50
;		MMX:		10-20

	.586
	.387
	.mmx
	.model	flat
	.code

	extern _MMX_enabled : byte
	extern _FPU_enabled : byte


;**************************************************************************

	public	_asm_resize_interp_row_run

;asm_resize_interp_row_run(
;	[esp+ 4] void *dst,
;	[esp+ 8] void *src,
;	[esp+12] ulong width,
;	[esp+16] ulong xaccum,
;	[esp+20] ulong x_inc);


_asm_resize_interp_row_run:
	test	_MMX_enabled,1
	jnz	_asm_resize_interp_row_run_MMX

	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	sub	esp,8

	mov	ebp,[esp+12+28+8]
	mov	edi,[esp+4+28+8]
	mov	esi,[esp+8+28+8]

	lea	edi,[edi+ebp*4]
	neg	ebp

;******************************************************
;
;	[esp+ 4] x_frac2
;	[esp+ 0] x_frac

	mov	eax,[esp+16+28+8]
	shr	eax,8
	and	eax,255
	mov	[esp],eax
	xor	eax,255
	inc	eax
	mov	[esp+4],eax

colloop_interp_row:
	mov	eax,[esi]
	mov	ecx,[esi+4]
	mov	ebx,eax
	mov	edx,ecx
	and	eax,00ff00ffh
	and	ebx,0000ff00h
	and	ecx,00ff00ffh
	and	edx,0000ff00h
	imul	eax,[esp+4]		;x_frac2
	imul	ebx,[esp+4]		;x_frac2
	imul	ecx,[esp]		;x_frac
	imul	edx,[esp]		;x_frac
	add	eax,ecx
	add	ebx,edx

	shr	eax,8
	and	ebx,00ff0000h
	shr	ebx,8
	and	eax,00ff00ffh

	or	eax,ebx			;[data write ] u

	mov	ebx,[esp+16+28+8]	;[frac update] u x_accum
	mov	ecx,[esp+20+28+8]	;[frac update] v x_inc

	mov	[edi+ebp*4],eax		;[data write ] u
	add	ebx,ecx			;[frac update] v

	mov	eax,ebx			;[frac update] u
	and	ebx,0000ffffh		;[frac update] v

	shr	eax,14			;[frac update] u
	mov	[esp+16+28+8],ebx	;[frac update] v x_accum

	shr	ebx,8			;[frac update] u
	and	eax,0fffffffch		;[frac update] v

	mov	[esp],ebx		;[frac update] u x_frac
	add	esi,eax			;[frac update] v

	xor	ebx,255			;[frac update] u

	inc	ebx			;[frac update] u
	inc	ebp

	mov	[esp+4],ebx		;[frac update] u x_frac2
	jne	colloop_interp_row

	add	esp,8

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp
	ret

	align 16

x0000FFFF0000FFFF	dq	0000FFFF0000FFFFh
x0000010100000101	dq	0000010100000101h

_asm_resize_interp_row_run_MMX:
	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	esi,[esp+8+28]
	mov	edi,[esp+4+28]
	mov	ebp,[esp+12+28]

	movd	mm4,[esp+16+28]
	pxor	mm7,mm7
	movd	mm6,[esp+20+28]
	punpcklwd mm4,mm4
	punpckldq mm4,mm4
	punpcklwd mm6,mm6
	punpckldq mm6,mm6

	mov	eax,[esp+16+28]
	mov	ebx,eax
	shr	ebx,16
	shl	eax,16
	add	esi,ebx

	mov	ebx,[esp+20+28]
	mov	ecx,ebx
	shl	ebx,16
	shr	ecx,16

	shl	ebp,2
	add	edi,ebp
	neg	ebp
	shr	esi,2


colloop_interp_row_MMX:
	movd	mm0,[esi*4]
	movd	mm2,[esi*4+4]

	punpcklbw mm0,mm7
	punpcklbw mm2,mm7

	movq	mm1,mm0
	punpcklwd mm0,mm2
	punpckhwd mm1,mm2

	movq	mm5,mm4
	paddw	mm4,mm6
	psrlw	mm5,8
	pxor	mm5,x0000FFFF0000FFFF
	paddw	mm5,x0000010100000101

	pmaddwd	mm0,mm5
	pmaddwd	mm1,mm5

	add	eax,ebx
	adc	esi,ecx

	psrad	mm0,8
	psrad	mm1,8

	packssdw mm0,mm1
	packuswb mm0,mm0

	movd	[edi+ebp],mm0

	add	ebp,4
	jnz	colloop_interp_row_MMX

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp
	ret

;**************************************************************************

	public	_asm_resize_interp_col_run

;asm_resize_interp_col_run(
;	[esp+ 4] void *dst,
;	[esp+ 8] void *src1,
;	[esp+12] void *src2,
;	[esp+16] ulong width,
;	[esp+20] ulong yaccum);


_asm_resize_interp_col_run:
	test	_MMX_enabled,1
	jnz	_asm_resize_interp_col_run_MMX

	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	sub	esp,8

	mov	esi,[esp+8+28+8]
	mov	edi,[esp+12+28+8]
	mov	ebp,[esp+16+28+8]
	sub	edi,esi

;******************************************************
;
;	[esp+ 4] x_frac2
;	[esp+ 0] x_frac

	mov	eax,[esp+20+28+8]
	shr	eax,8
	and	eax,255
	mov	[esp],eax
	xor	eax,255
	inc	eax
	mov	[esp+4],eax

colloop_interp_col:
	mov	eax,[esi]
	mov	ecx,[esi+edi]
	mov	ebx,eax
	mov	edx,ecx
	and	eax,00ff00ffh
	and	ebx,0000ff00h
	and	ecx,00ff00ffh
	and	edx,0000ff00h
	imul	eax,[esp+4]		;x_frac2
	imul	ebx,[esp+4]		;x_frac2
	imul	ecx,[esp]		;x_frac
	imul	edx,[esp]		;x_frac
	add	eax,ecx
	add	ebx,edx

	shr	eax,8
	and	ebx,00ff0000h
	shr	ebx,8
	and	eax,00ff00ffh

	or	eax,ebx			;[data write ] u
	mov	edx,[esp+4+28+8]	;[data write ] v

	mov	[edx],eax		;[data write ] u
	add	edx,4			;[data write ] v

	mov	[esp+4+28+8],edx	;[data write ] v
	add	esi,4

	dec	ebp
	jne	colloop_interp_col

	add	esp,8

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp
	ret

_asm_resize_interp_col_run_MMX:
	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	esi,[esp+8+28]
	mov	edx,[esp+12+28]
	mov	edi,[esp+4+28]
	mov	ebp,[esp+16+28]

	movd	mm4,[esp+20+28]
	pxor	mm7,mm7
	punpcklwd mm4,mm4
	punpckldq mm4,mm4
	psrlw	mm4,8
	pxor	mm4,x0000FFFF0000FFFF
	paddw	mm4,x0000010100000101

	shl	ebp,2
	add	edi,ebp
	add	esi,ebp
	add	edx,ebp
	neg	ebp

colloop_interp_col_MMX:
	movd	mm0,[esi+ebp]
	movd	mm2,[edx+ebp]

	punpcklbw mm0,mm7
	punpcklbw mm2,mm7

	movq	mm1,mm0
	punpcklwd mm0,mm2
	punpckhwd mm1,mm2

	pmaddwd	mm0,mm4
	pmaddwd	mm1,mm4

	psrad	mm0,8
	psrad	mm1,8

	packssdw mm0,mm1
	packuswb mm0,mm0

	movd	[edi+ebp],mm0

	add	ebp,4
	jnz	colloop_interp_col_MMX

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp
	ret

;--------------------------------------------------------------------------

	public	_asm_resize_ccint

	align	16

x0000200000002000	dq	0000200000002000h

;asm_resize_ccint(dst, src1, src2, src3, src4, count, xaccum, xinc, tbl);

_asm_resize_ccint:
	push	ebx
	push	esi
	push	edi
	push	ebp

	mov	ebx,[esp + 4 + 16]	;ebx = dest addr
	mov	ecx,[esp + 24 + 16]	;ecx = count
	shl	ecx,2			;ecx = count*4
	add	ecx,ebx			;ecx = dst limit

	mov	ebp,[esp + 32 + 16]	;ebp = increment
	mov	edi,ebp			;edi = increment
	shl	ebp,16			;ebp = fractional increment
	mov	esi,[esp + 28 + 16]	;esi = 16:16 position
	shr	edi,16			;edi = integer increment
	mov	[esp + 32 + 16],ebp	;xinc = fractional increment
	mov	ebp,esi			;ebp = 16:16 position
	shr	esi,16			;esi = integer position
	shl	ebp,16			;ebp = fraction
	mov	[esp + 28 + 16], ebp	;xaccum = fraction

	mov	eax,[esp + 8 + 16]
	mov	[esp + 8 + 16],ecx	;src1 = dest limit
	mov	ebx,[esp + 12 + 16]
	mov	ecx,[esp + 16 + 16]
	mov	edx,[esp + 20 + 16]

	shr	ebp,24			;ebp = fraction (0...255)
	mov	[esp + 20 + 16],edi	;src4 = integer increment
	shl	ebp,4			;ebp = fraction*16
	mov	edi,[esp + 36 + 16]	;edi = coefficient table
	add	edi,ebp
	mov	ebp,[esp + 4 + 16]	;ebp = destination

	movq		mm6,x0000200000002000
	pxor		mm7,mm7

ccint_loop_MMX:
	movd		mm0,[eax+esi*4]
	;<xxx>

	movd		mm1,[ebx+esi*4]
	punpcklbw	mm0,mm7				;mm0 = [a1][r1][g1][b1]

	movd		mm2,[ecx+esi*4]
	punpcklbw	mm1,mm7				;mm1 = [a2][r2][g2][b2]

	movd		mm3,[edx+esi*4]
	punpcklbw	mm2,mm7				;mm2 = [a3][r3][g3][b3]

	punpcklbw	mm3,mm7				;mm3 = [a4][r4][g4][b4]
	movq		mm4,mm0				;mm0 = [a1][r1][g1][b1]

	punpcklwd	mm0,mm1				;mm0 = [g2][g1][b2][b1]
	movq		mm5,mm2				;mm2 = [a3][r3][g3][b3]

	pmaddwd		mm0,[edi]
	punpcklwd	mm2,mm3				;mm2 = [g4][g3][b4][b3]

	pmaddwd		mm2,[edi+8]
	punpckhwd	mm4,mm1				;mm4 = [a2][a1][r2][r1]

	pmaddwd		mm4,[edi]
	punpckhwd	mm5,mm3				;mm5 = [a4][a3][b4][b3]

	pmaddwd		mm5,[edi+8]
	paddd		mm0,mm2				;mm0 = [ g ][ b ]

	paddd		mm0,mm6
	mov		edi,[esp + 28 + 16]		;edi = fractional accumulator

	paddd		mm4,mm6
	psrad		mm0,14

	paddd		mm4,mm5				;mm4 = [ a ][ r ]
	add		edi,[esp + 32 + 16]		;add fractional incrmeent

	adc		esi,[esp + 20 + 16]		;add integer increment and fractional bump to offset
	psrad		mm4,14

	packssdw	mm0,mm4				;mm0 = [ a ][ r ][ g ][  b ]
	mov		[esp + 28 + 16],edi		;save fractional accumulator

	shr		edi,24				;edi = fraction (0...255)
	packuswb	mm0,mm0				;mm0 = [a][r][g][b][a][r][g][b]

	shl		edi,4				;edi = fraction (0...255)*16
	add		ebp,4

	add		edi,[esp + 36 + 16]		;edi = pointer to coefficient entry
	cmp		ebp,[esp + 8 + 16]

	movd		[ebp-4],mm0
	jne		ccint_loop_MMX

	pop	ebp
	pop	edi
	pop	esi
	pop	ebx
	emms
	ret

;asm_resize_ccint_col(dst, src1, src2, src3, src4, count, tbl);

	public	_asm_resize_ccint_col

_asm_resize_ccint_col:
	push	ebx
	push	esi
	push	edi
	push	ebp

	mov	ebp,[esp + 4 + 16]	;ebx = dest addr
	mov	ecx,[esp + 24 + 16]	;ecx = count
	shl	ecx,2			;ecx = count*4
	add	ecx,ebp			;ecx = dst limit
	mov	[esp + 24 + 16], ecx

	mov	eax,[esp + 8 + 16]
	mov	ebx,[esp + 12 + 16]
	mov	ecx,[esp + 16 + 16]
	mov	edx,[esp + 20 + 16]
	mov	edi,[esp + 28 + 16]

	movq		mm6,x0000200000002000
	pxor		mm7,mm7

	xor	esi,esi

	movd		mm2,[ecx]
	jmp		short ccint_col_loop_MMX@entry

ccint_col_loop_MMX:
	movd		mm2,[ecx+esi]
	packuswb	mm0,mm0				;mm0 = [a][r][g][b][a][r][g][b]

	movd		[ebp-4],mm0

ccint_col_loop_MMX@entry:
	movd		mm1,[ebx+esi]
	punpcklbw	mm2,mm7				;mm2 = [a3][r3][g3][b3]

	movd		mm0,[eax+esi]
	punpcklbw	mm1,mm7				;mm1 = [a2][r2][g2][b2]

	movd		mm3,[edx+esi]
	punpcklbw	mm0,mm7				;mm0 = [a1][r1][g1][b1]

	punpcklbw	mm3,mm7				;mm3 = [a4][r4][g4][b4]
	movq		mm4,mm0				;mm0 = [a1][r1][g1][b1]

	punpcklwd	mm0,mm1				;mm0 = [g2][g1][b2][b1]
	movq		mm5,mm2				;mm2 = [a3][r3][g3][b3]

	pmaddwd		mm0,[edi]
	punpcklwd	mm2,mm3				;mm2 = [g4][g3][b4][b3]

	pmaddwd		mm2,[edi+8]
	punpckhwd	mm4,mm1				;mm4 = [a2][a1][r2][r1]

	pmaddwd		mm4,[edi]
	punpckhwd	mm5,mm3				;mm5 = [a4][a3][b4][b3]

	pmaddwd		mm5,[edi+8]
	paddd		mm0,mm2				;mm0 = [ g ][ b ]

	paddd		mm0,mm6
	add		esi,4

	paddd		mm4,mm6
	add		ebp,4

	paddd		mm4,mm5				;mm4 = [ a ][ r ]
	psrad		mm0,14

	psrad		mm4,14
	cmp		ebp,[esp + 24 + 16]

	packssdw	mm0,mm4				;mm0 = [ a ][ r ][ g ][  b ]
	jne		ccint_col_loop_MMX

	packuswb	mm0,mm0				;mm0 = [a][r][g][b][a][r][g][b]
	movd		[ebp-4],mm0

	pop	ebp
	pop	edi
	pop	esi
	pop	ebx
	emms
	ret

	end

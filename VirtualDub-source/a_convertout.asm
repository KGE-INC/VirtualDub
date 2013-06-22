;	VirtualDub - Video processing and capture application
;	Copyright (C) 1998-2001 Avery Lee
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

	.586
	.mmx
	.model	flat

	extern _MMX_enabled : byte

	public	_DIBconvert_32_to_16
	public	_DIBconvert_32_to_16_dithered
	public	_DIBconvert_32_to_16_565
	public	_DIBconvert_32_to_16_565_dithered
	public	_DIBconvert_32_to_24
	public	_DIBconvert_32_to_32

	.code

;***************************************************

; void DIBconvert_32_to_32(
;	void *dest,		[ESP+ 4]
;	ulong dest_pitch,	[ESP+ 8]
;	void *src,		[ESP+12]
;	ulong src_pitch,	[ESP+16]
;	ulong width,		[ESP+20]
;	ulong height);		[ESP+24]
;
;	this isn't THAT stupid a function!!

_DIBconvert_32_to_32:
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	edx,[esp+24+24]

	mov	eax,[esp+20+24]
	mov	esi,[esp+12+24]
	mov	edi,[esp+ 4+24]

DIBconvert3232@y:
	push	esi
	push	edi
	mov	ecx,eax
	rep	movsd
	pop	edi
	pop	esi
	add	esi,[esp+16+24]
	add	edi,[esp+ 8+24]

	dec	edx
	jne	DIBconvert3232@y

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi

	ret

; void DIBconvert_32_to_24(
;	void *dest,		[ESP+ 4]
;	ulong dest_pitch,	[ESP+ 8]
;	void *src,		[ESP+12]
;	ulong src_pitch,	[ESP+16]
;	ulong width,		[ESP+20]
;	ulong height);		[ESP+24]

_DIBconvert_32_to_24:
	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	esi,[esp+12+28]
	mov	edi,[esp+4+28]

	mov	ecx,[esp+20+28]
	lea	eax,[ecx+ecx*2]
	lea	ebx,[ecx*4]
	sub	[esp+8+28],eax
	sub	[esp+16+28],ebx

	mov	edx,[esp+24+28]
DIBconvert3224@y:
	mov	ecx,[esp+20+28]
	push	ecx
	push	edx
	shr	ecx,2
	jz	DIBconvert3224@x2
DIBconvert3224@x:
	mov	eax,[esi]		;EAX = xxr0g0b0
	mov	ebx,[esi+4]		;EBX = xxr1g1b1
	mov	edx,ebx			;EDX = xxr1g1b1
	mov	ebp,[esi+8]		;EBP = xxr2g2b2
	shl	ebx,24			;EBX = b1000000
	and	eax,00ffffffh		;EAX = 00r0g0b0
	shr	edx,8			;EDX = 00xxr1g1
	or	eax,ebx			;EAX = b1r0g0b0
	mov	[edi+0],eax
	mov	ebx,ebp			;EBX = xxr2g2b2
	shl	ebp,16			;EBP = g2b20000
	and	edx,0000ffffh		;EDX = 0000r1g1
	or	ebp,edx			;EBP = g2b2r1g1
	mov	eax,[esi+12]		;EAX = xxr3g3b3
	shr	ebx,16			;EBX = 0000xxr2
	add	edi,12
	shl	eax,8			;EAX = r3g3b300
	and	ebx,000000ffh		;EBX = 000000r2
	or	eax,ebx			;EAX = r3g3b3r2
	mov	[edi+4-12],ebp
	add	esi,16
	mov	[edi+8-12],eax
	dec	ecx
	jne	DIBconvert3224@x
DIBconvert3224@x2:
	pop	edx
	pop	ecx
	and	ecx,3
	jz	DIBconvert3224@x3
DIBconvert3224@x4:
	mov	eax,[esi]
	add	esi,4
	mov	[edi],ax
	shr	eax,16
	mov	[edi+2],al
	add	edi,3
	dec	ecx
	jnz	DIBconvert3224@x4
DIBconvert3224@x3:

	add	esi,[esp+16+28]
	add	edi,[esp+ 8+28]

	dec	edx
	jne	DIBconvert3224@y

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp

	ret

;******************************************************
;
; void DIBconvert_32_to_16(
;	void *dest,		[ESP+ 4]
;	ulong dest_pitch,	[ESP+ 8]
;	void *src,		[ESP+12]
;	ulong src_pitch,	[ESP+16]
;	ulong width,		[ESP+20]
;	ulong height);		[ESP+24]

_DIBconvert_32_to_16:
	test	_MMX_enabled,1
	jnz	_DIBconvert_32_to_16MMX


	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	esi,[esp+12+28]
	mov	edi,[esp+4+28]

	mov	ebp,[esp+20+28]
	lea	eax,[ebp+ebp]
	lea	ebx,[ebp*4]
	sub	[esp+8+28],eax
	sub	[esp+16+28],ebx

	mov	edx,[esp+24+28]
DIBconvert3216@y:
	mov	ebp,[esp+20+28]
	push	ebp
	push	edx
	shr	ebp,1
	jz	DIBconvert3216@x2
DIBconvert3216@x:
	mov	eax,[esi+4]		;u
	add	esi,8			;v

	mov	ebx,eax			;u
	mov	ecx,eax			;v
	shr	ebx,3			;u
	and	ecx,0000f800h		;v
	shr	eax,9			;u
	and	ebx,0000001fh		;v
	shr	ecx,6			;u
	and	eax,00007c00h		;v
	or	ebx,ecx			;u
	add	edi,4			;v
	or	ebx,eax			;u

	mov	ecx,[esi-8]		;v
	mov	edx,ebx			;u
	mov	eax,ecx			;v

	shl	edx,16			;u
	mov	ebx,ecx			;v
	shr	ebx,3			;u
	and	ecx,0000f800h		;v
	shr	eax,9			;u
	and	ebx,0000001fh		;v
	shr	ecx,6			;u
	and	eax,00007c00h		;v
	or	eax,ecx			;u
	or	edx,ebx			;v
	or	edx,eax			;u
	dec	ebp			;v
	mov	[edi-4],edx		;u
	jne	DIBconvert3216@x	;v
DIBconvert3216@x2:
	pop	edx
	pop	ebp
	and	ebp,1
	jz	DIBconvert3216@x3
	mov	eax,[esi]
	add	esi,4

	mov	ebx,eax
	mov	ecx,eax
	shr	ebx,3
	and	ecx,0000f800h
	shr	eax,9
	and	ebx,0000001fh
	shr	ecx,6
	and	eax,00007c00h
	or	ebx,ecx
	or	ebx,eax
	mov	[edi+0],bl
	mov	[edi+1],bh
	add	edi,2
DIBconvert3216@x3:

	add	esi,[esp+16+28]
	add	edi,[esp+ 8+28]

	dec	edx
	jne	DIBconvert3216@y

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp

	ret

;******************************************************
;
; void DIBconvert_32_to_16_565(
;	void *dest,		[ESP+ 4]
;	ulong dest_pitch,	[ESP+ 8]
;	void *src,		[ESP+12]
;	ulong src_pitch,	[ESP+16]
;	ulong width,		[ESP+20]
;	ulong height);		[ESP+24]

_DIBconvert_32_to_16_565:
	test	_MMX_enabled,1
	jnz	_DIBconvert_32_to_16_565MMX


	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	esi,[esp+12+28]
	mov	edi,[esp+4+28]

	mov	ebp,[esp+20+28]
	lea	eax,[ebp+ebp]
	lea	ebx,[ebp*4]
	sub	[esp+8+28],eax
	sub	[esp+16+28],ebx

	mov	edx,[esp+24+28]
DIBconvert3216565@y:
	mov	ebp,[esp+20+28]
	push	ebp
	push	edx
	shr	ebp,1
	jz	DIBconvert3216565@x2
DIBconvert3216565@x:
	mov	eax,[esi+4]		;u
	add	esi,8			;v

	mov	ebx,eax			;u
	mov	ecx,eax			;v
	shr	ebx,3			;u
	and	ecx,0000fc00h		;v
	shr	eax,8			;u
	and	ebx,0000001fh		;v
	shr	ecx,5			;u
	and	eax,0000f800h		;v
	or	ebx,ecx			;u
	add	edi,4			;v
	or	ebx,eax			;u

	mov	ecx,[esi-8]		;v
	mov	edx,ebx			;u
	mov	eax,ecx			;v

	shl	edx,16			;u
	mov	ebx,ecx			;v
	shr	ebx,3			;u
	and	ecx,0000fc00h		;v
	shr	eax,8			;u
	and	ebx,0000001fh		;v
	shr	ecx,5			;u
	and	eax,0000f800h		;v
	or	eax,ecx			;u
	or	edx,ebx			;v
	or	edx,eax			;u
	dec	ebp			;v
	mov	[edi-4],edx		;u
	jne	DIBconvert3216565@x	;v
DIBconvert3216565@x2:
	pop	edx
	pop	ebp
	and	ebp,1
	jz	DIBconvert3216565@x3
	mov	eax,[esi]
	add	esi,4

	mov	ebx,eax
	mov	ecx,eax
	shr	ebx,3
	and	ecx,0000fc00h
	shr	eax,8
	and	ebx,0000001fh
	shr	ecx,5
	and	eax,0000f800h
	or	ebx,ecx
	or	ebx,eax
	mov	[edi+0],bl
	mov	[edi+1],bh
	add	edi,2
DIBconvert3216565@x3:

	add	esi,[esp+16+28]
	add	edi,[esp+ 8+28]

	dec	edx
	jne	DIBconvert3216565@y

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp

	ret

;***********************************************************************************

	.const
	align	8

DIBconvert3216MMX@red_blue_mask		dq	00f800f800f800f8h
DIBconvert3216MMX@green_mask		dq	0000f8000000f800h
DIBconvert3216MMX@red_blue_mult		dq	2000000820000008h
DIBconvert3216MMX@565_green_mask	dq	0000fc000000f800h
DIBconvert3216MMX@565_red_blue_mult	dq	2000000420000004h

	.code

	align	16
_DIBconvert_32_to_16MMX:
	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	esi,[esp+12+28]
	mov	edi,[esp+4+28]

	mov	ebp,[esp+20+28]
	lea	eax,[ebp+ebp]
	lea	ebx,[ebp*4]
	sub	[esp+8+28],eax
	sub	[esp+16+28],ebx

	mov	edx,[esp+24+28]
DIBconvert3216MMX@y:
	mov	ebp,[esp+20+28]
	push	ebp
	push	edx
	shr	ebp,3
	jz	DIBconvert3216MMX@x2
	shl	ebp,4

	movq		mm5,DIBconvert3216MMX@red_blue_mult
	movq		mm6,DIBconvert3216MMX@red_blue_mask
	movq		mm7,DIBconvert3216MMX@green_mask

	movq		mm0,[esi+ebp*2-32]	;allocate 0	(01234)
	movq		mm2,mm0			;allocate 2	(0 234)

	movq		mm1,[esi+ebp*2-24]	;allocate 1	(01234)
	movq		mm3,mm1			;allocate 3	(01234)
	pand		mm0,mm6
	pmaddwd		mm0,mm5
	pand		mm1,mm6
	pmaddwd		mm1,mm5
	pand		mm2,mm7
	pand		mm3,mm7
	jmp		short DIBconvert3216MMX@loopstart

	align 16
DIBconvert3216MMX@x:

;*******************************

	movq		mm0,[esi+ebp*2-32]	;allocate 0	(01234)
	por		mm4,mm2			;free 2		(01 34)

	por		mm3,mm1			;free 3		(01 34)
	psrlq		mm4,6

	psrlq		mm3,6
	movq		mm2,mm0			;allocate 2	(0 234)

	movq		mm1,[esi+ebp*2-24]	;allocate 1	(01234)
	packssdw	mm4,mm3			;free 3		(012 4)

	movq		mm3,mm1			;allocate 3	(01234)
	pand		mm0,mm6

	pmaddwd		mm0,mm5
	pand		mm1,mm6

	pmaddwd		mm1,mm5
	pand		mm2,mm7

	movq		[edi+ebp+8],mm4		;free 4		(0123 )
	pand		mm3,mm7

DIBconvert3216MMX@loopstart:
	movq		mm4,[esi+ebp*2-16]	;allocate 4	(01234)
	por		mm0,mm2			;free 2		(01 34)

	por		mm1,mm3			;free 3		(01  4)
	psrlq		mm0,6

	movq		mm3,[esi+ebp*2-8]	;allocate 3	(01 34)
	psrlq		mm1,6

	movq		mm2,mm4			;allocate 2	(01234)
	packssdw	mm0,mm1			;free 1		(0 234)

	movq		mm1,mm3			;allocate 1	(01234)
	pand		mm4,mm6

	movq		[edi+ebp-16],mm0	;free 0		( 1234)
	pand		mm3,mm6

	pmaddwd		mm4,mm5
	pand		mm2,mm7

	pmaddwd		mm3,mm5
	pand		mm1,mm7

	sub		ebp,16
	jne		DIBconvert3216MMX@x

	;--------------

	por		mm4,mm2			;free 2		(01 34)
	por		mm3,mm1			;free 3		(01 34)
	psrlq		mm4,6
	psrlq		mm3,6
	packssdw	mm4,mm3			;free 3		(012 4)
	movq		[edi+8],mm4		;free 4		(0123 )

;*******************************

DIBconvert3216MMX@x2:
	pop	edx
	pop	ebp

	mov	eax,ebp
	and	eax,0fffffff8h
	add	eax,eax
	add	edi,eax
	add	eax,eax
	add	esi,eax
	push	ebp

	and	ebp,6
	jz	DIBconvert3216MMX@x4

DIBconvert3216MMX@x5:
	movq		mm0,[esi]
	add		esi,8
	movq		mm1,mm0
	pand		mm0,DIBconvert3216MMX@red_blue_mask
	pand		mm1,DIBconvert3216MMX@green_mask
	pmaddwd		mm0,DIBconvert3216MMX@red_blue_mult
	paddw		mm0,mm1
	psrld		mm0,6
	packssdw	mm0,mm0
	movd		[edi],mm0
	add		edi,4
	sub	ebp,2
	jne	DIBconvert3216MMX@x5

DIBconvert3216MMX@x4:
	pop	ebp

	and	ebp,1
	jz	DIBconvert3216MMX@x3
	mov	eax,[esi]
	add	esi,4

	mov	ebx,eax
	mov	ecx,eax
	shr	ebx,3
	and	ecx,0000f800h
	shr	eax,9
	and	ebx,0000001fh
	shr	ecx,6
	and	eax,00007c00h
	or	ebx,ecx
	add	edi,2
	or	ebx,eax
	mov	[edi+0],bl
	mov	[edi+1],bh
DIBconvert3216MMX@x3:

	add	esi,[esp+16+28]
	add	edi,[esp+ 8+28]

	dec	edx
	jne	DIBconvert3216MMX@y

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp
	emms

	ret



_DIBconvert_32_to_16_565MMX:
	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	esi,[esp+12+28]
	mov	edi,[esp+4+28]

	mov	ebp,[esp+20+28]
	lea	eax,[ebp+ebp]
	lea	ebx,[ebp*4]
	sub	[esp+8+28],eax
	sub	[esp+16+28],ebx

	mov	edx,[esp+24+28]
DIBconvert3216565MMX@y:
	mov	ebp,[esp+20+28]
	push	ebp
	push	edx
	shr	ebp,3
	jz	DIBconvert3216565MMX@x2
	shl	ebp,4

	movq		mm5,DIBconvert3216MMX@565_red_blue_mult
	movq		mm6,DIBconvert3216MMX@red_blue_mask
	movq		mm7,DIBconvert3216MMX@565_green_mask

	movq		mm0,[esi+ebp*2-32]	;allocate 0	(0123)
	movq		mm2,mm0			;allocate 2	(0 23)

	movq		mm1,[esi+ebp*2-24]	;allocate 1	(0123)
	movq		mm3,mm1			;allocate 3	(0123)
	pand		mm0,mm6
	pmaddwd		mm0,mm5
	pand		mm1,mm6
	pmaddwd		mm1,mm5
	pand		mm2,mm7
	pand		mm3,mm7
	jmp		short DIBconvert3216565MMX@loopstart

	align 16

DIBconvert3216565MMX@x:

;*******************************

	movq		mm0,[esi+ebp*2-32]	;allocate 0	(01234)
	por		mm4,mm2			;free 2		(01 34)

	por		mm3,mm1			;free 3		(01 34)
	pslld		mm4,16-5

	pslld		mm3,16-5
	movq		mm2,mm0			;allocate 2	(0 234)

	movq		mm1,[esi+ebp*2-24]	;allocate 1	(01234)
	psrad		mm4,16

	psrad		mm3,16
	pand		mm0,mm6

	packssdw	mm4,mm3			;free 3		(012 4)
	movq		mm3,mm1			;allocate 3	(01234)

	pmaddwd		mm0,mm5
	pand		mm1,mm6

	pmaddwd		mm1,mm5
	pand		mm2,mm7

	movq		[edi+ebp+8],mm4		;free 4		(0123 )
	pand		mm3,mm7

DIBconvert3216565MMX@loopstart:
	movq		mm4,[esi+ebp*2-16]	;allocate 4	(01234)
	por		mm0,mm2			;free 2		(01 34)

	por		mm1,mm3			;free 3		(01  4)
	pslld		mm0,16-5

	movq		mm3,[esi+ebp*2-8]	;allocate 3	(01 34)
	pslld		mm1,16-5

	psrad		mm0,16
	movq		mm2,mm4			;allocate 2	(01234)

	psrad		mm1,16
	pand		mm4,mm6

	packssdw	mm0,mm1			;free 1		(0 234)
	movq		mm1,mm3			;allocate 1	(01234)

	movq		[edi+ebp-16],mm0	;free 0		( 1234)
	pand		mm3,mm6

	pmaddwd		mm4,mm5
	sub		ebp,16

	pmaddwd		mm3,mm5
	pand		mm2,mm7

	pand		mm1,mm7
	jne		DIBconvert3216565MMX@x

	;--------------

	por		mm4,mm2			;free 2		(01 34)
	por		mm3,mm1			;free 3		(01 34)
	psllq		mm4,16-5
	psllq		mm3,16-5
	psrad		mm4,16
	psrad		mm3,16
	packssdw	mm4,mm3			;free 3		(012 4)
	movq		[edi+8],mm4		;free 4		(0123 )


;*******************************

DIBconvert3216565MMX@x2:
	pop	edx
	pop	ebp

	mov	eax,ebp
	and	eax,0fffffff8h
	add	eax,eax
	add	edi,eax
	add	eax,eax
	add	esi,eax
	push	ebp

	and	ebp,6
	jz	DIBconvert3216565MMX@x4

DIBconvert3216565MMX@x5:
	movq		mm0,[esi]
	add		esi,8
	movq		mm1,mm0
	pand		mm0,DIBconvert3216MMX@red_blue_mask
	pand		mm1,DIBconvert3216MMX@565_green_mask
	pmaddwd		mm0,DIBconvert3216MMX@565_red_blue_mult
	paddw		mm0,mm1
	pslld		mm0,16-5
	psrad		mm0,16
	packssdw	mm0,mm0
	movd		[edi],mm0
	add		edi,4
	sub	ebp,2
	jne	DIBconvert3216565MMX@x5

DIBconvert3216565MMX@x4:
	pop	ebp

	and	ebp,1
	jz	DIBconvert3216565MMX@x3
	mov	eax,[esi]
	add	esi,4

	mov	ebx,eax
	mov	ecx,eax
	shr	ebx,3
	and	ecx,0000fc00h
	shr	eax,8
	and	ebx,0000001fh
	shr	ecx,5
	and	eax,0000f800h
	or	ebx,ecx
	add	edi,2
	or	ebx,eax
	mov	[edi+0],bl
	mov	[edi+1],bh
DIBconvert3216565MMX@x3:

	add	esi,[esp+16+28]
	add	edi,[esp+ 8+28]

	dec	edx
	jne	DIBconvert3216565MMX@y

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp
	emms

	ret

;******************************************************
;
; void DIBconvert_32_to_16_dithered(
;	void *dest,		[ESP+ 4]
;	ulong dest_pitch,	[ESP+ 8]
;	void *src,		[ESP+12]
;	ulong src_pitch,	[ESP+16]
;	ulong width,		[ESP+20]
;	ulong height);		[ESP+24]

_DIBconvert_32_to_16_dithered:
	test	_MMX_enabled,1
	jnz	_DIBconvert_32_to_16_ditheredMMX
	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	push	0
	push	0
	push	0
	push	0
	push	0

	mov	esi,[esp+12+48]
	mov	edi,[esp+4+48]

	mov	[esp+8],edi

	mov	ebp,[esp+20+48]
	lea	eax,[ebp+ebp]
	lea	ebx,[ebp*4]
	sub	[esp+8+48],eax
	sub	[esp+16+48],ebx


	mov	edx,[esp+24+48]
DIBconvert3216dithered@y:
	mov	ebp,[esp+20+48]
	push	ebp
	push	edx
	shr	ebp,1
	jz	DIBconvert3216dithered@x2

	neg	ebp
	shl	ebp,2
	sub	[esp+16],ebp
	sub	esi,ebp
	sub	esi,ebp

	;0 8 2 A
	;C 4 E 6
	;3 B 1 9
	;F 7 D 5

	;Dithering algorithm:
	;	Multiply pixel by 249 to get an 5:11 fraction
	;	Add dithering value from above matrix as 0:11 fraction
	;
	;	(Use x241 for double dithering)

DIBconvert3216dithered@x:
	mov	ecx,[esi+ebp*2+4]
	mov	edx,0000ff00h

	and	edx,ecx
	and	ecx,00ff00ffh

	mov	eax,ecx
	mov	ebx,edx

	lea	ecx,[ecx*8]
	lea	edx,[edx*8]

	sub	ecx,eax
	sub	edx,ebx

	shl	eax,8
	shl	ebx,8

	sub	eax,ecx
	sub	ebx,edx

	mov	ecx,[esp+8]
	mov	edx,[esp+12]

	xor	ecx,04000400h
	xor	edx,00040000h

	add	eax,ecx
	add	ebx,edx

	shr	ebx,14
	mov	ecx,eax

	shr	eax,17
	and	ecx,0000f800h

	shr	ecx,11
	and	eax,00007c00h

	and	ebx,000003e0h
	or	eax,ecx

	or	eax,ebx			;combine pixel 2
	mov	ecx,[esi+ebp*2]

	shl	eax,16
	mov	edx,ecx

	and	ecx,00ff00ffh
	and	edx,0000ff00h

	mov	edi,ecx
	mov	ebx,edx

	lea	ecx,[ecx*8]
	lea	edx,[edx*8]

	sub	ecx,edi
	sub	edx,ebx

	shl	edi,8
	shl	ebx,8

	sub	edi,ecx
	sub	ebx,edx

	add	edi,[esp+8]
	add	ebx,[esp+12]

	shr	ebx,14
	mov	ecx,edi

	shr	edi,17
	and	ecx,0000f800h

	shr	ecx,11
	and	edi,00007c00h

	and	ebx,000003e0h
	or	eax,edi

	or	eax,ebx
	mov	edi,[esp+16]

	or	eax,ecx
	mov	ecx,[esp+8]

	mov	[edi+ebp],eax
	mov	edx,[esp+12]

	xor	ecx,01000100h
	xor	edx,00010000h
	mov	[esp+8],ecx
	mov	[esp+12],edx

	add	ebp,4
	jne	DIBconvert3216dithered@x
DIBconvert3216dithered@x2:
	pop	edx
	pop	ebp
	and	ebp,1
	jz	DIBconvert3216dithered@x3
	mov	eax,[esi]
	add	esi,4

	mov	ebx,eax
	mov	ecx,eax
	shr	ebx,3
	and	ecx,0000f800h
	shr	eax,9
	and	ebx,0000001fh
	shr	ecx,6
	and	eax,00007c00h
	or	ebx,ecx
	or	ebx,eax
	mov	[edi+0],bl
	mov	[edi+1],bh
	add	edi,2
DIBconvert3216dithered@x3:

	add	esi,[esp+16+48]
	add	edi,[esp+ 8+48]

	mov	[esp+8],edi

	mov	eax,06000600h
	mov	ebx,00060000h

	test	edx,1
	jz	DIBconvert3216dithered@even

	mov	eax,07800780h
	mov	ebx,00078000h

DIBconvert3216dithered@even:

	xor	eax,[esp+12]
	xor	ebx,[esp+16]
	mov	[esp+12],eax
	mov	[esp+16],ebx
	mov	[esp+0],eax
	mov	[esp+4],ebx

	dec	edx
	jne	DIBconvert3216dithered@y

	pop	eax
	pop	eax
	pop	eax
	pop	eax
	pop	eax

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp

	ret

;******************************************************

	.const

	align	8
DIBconvert3216ditheredMMX@mult			dq	00f100f100f100f1h
DIBconvert3216ditheredMMX@horizstep1mask	dq	0400040004000400h
DIBconvert3216ditheredMMX@horizstep2mask	dq	0100010001000100h
DIBconvert3216ditheredMMX@vertstep1mask		dq	0600060006000600h
DIBconvert3216ditheredMMX@vertstep2mask		dq	0780078007800780h
DIBconvert3216ditheredMMX@mult565		dq	00f100f100f900f1h
DIBconvert3216ditheredMMX@565horizstep1mask	dq	0400040002000400h
DIBconvert3216ditheredMMX@565horizstep2mask	dq	0100010000800100h
DIBconvert3216ditheredMMX@565vertstep1mask	dq	0600060003000600h
DIBconvert3216ditheredMMX@565vertstep2mask	dq	0780078003c00780h

	.code

	align	16
_DIBconvert_32_to_16_ditheredMMX:
	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	push	0
	push	0
	push	0
	push	0

	mov	esi,[esp+12+44]
	mov	edi,[esp+4+44]

	mov	ebp,[esp+20+44]
	lea	eax,[ebp+ebp]
	lea	ebx,[ebp*4]
	sub	[esp+8+44],eax
	sub	[esp+16+44],ebx

	movq	mm6,DIBconvert3216ditheredMMX@mult
	pxor	mm7,mm7
	movq	mm5,DIBconvert3216ditheredMMX@horizstep1mask
	pxor	mm4,mm4


	mov	edx,[esp+24+44]
DIBconvert3216ditheredMMX@y:
	mov	ebp,[esp+20+44]
	shr	ebp,2
	jz	DIBconvert3216ditheredMMX@x2

	neg	ebp
	shl	ebp,3
	sub	edi,ebp
	sub	esi,ebp
	sub	esi,ebp

	movq	[esp+0],mm4
	movq	[esp+8],mm5

	;0 8 2 A
	;C 4 E 6
	;3 B 1 9
	;F 7 D 5

	;Dithering algorithm:
	;	Multiply pixel by 249 to get an 5:11 fraction
	;	Add dithering value from above matrix as 0:11 fraction
	;
	;	(Use x241 for double dithering)
	;
	;	mm6: 241,241,241,241
	;	mm7: zero

DIBconvert3216ditheredMMX@x:

	movq	mm0,[esi+ebp*2]

	movq	mm2,[esi+ebp*2+8]
	movq	mm1,mm0

	punpcklbw mm0,mm7
	movq	mm3,mm2

	pmullw	mm0,mm6
	punpckhbw mm1,mm7

	pmullw	mm1,mm6
	punpcklbw mm2,mm7

	pmullw	mm2,mm6
	punpckhbw mm3,mm7

	pmullw	mm3,mm6
	paddw	mm0,mm4

	psrlw	mm0,8
	paddw	mm1,mm5

	psrlw	mm1,8
	paddw	mm2,mm4

	psrlw	mm2,8
	paddw	mm3,mm5

	psrlw	mm3,8
	packuswb mm0,mm1

	packuswb mm2,mm3
	movq	mm1,mm0

	pand	mm0,DIBconvert3216MMX@red_blue_mask
	movq	mm3,mm2

	pmaddwd mm0,DIBconvert3216MMX@red_blue_mult

	pand	mm2,DIBconvert3216MMX@red_blue_mask

	pmaddwd mm2,DIBconvert3216MMX@red_blue_mult

	pand	mm1,DIBconvert3216MMX@green_mask
	add	ebp,8

	pand	mm3,DIBconvert3216MMX@green_mask
	por	mm0,mm1

	por	mm2,mm3
	psrld	mm0,6

	pxor	mm4,DIBconvert3216ditheredMMX@horizstep2mask
	psrld	mm2,6

	pxor	mm5,DIBconvert3216ditheredMMX@horizstep2mask
	packssdw mm0,mm2

	movq	[edi+ebp-8],mm0
	jne	DIBconvert3216ditheredMMX@x
DIBconvert3216ditheredMMX@x2:
	mov	ebp,[esp+20+44]
	and	ebp,3
	jz	DIBconvert3216ditheredMMX@x3
DIBconvert3216ditheredMMX@xleftover:
	mov	eax,[esi]
	add	esi,4

	mov	ebx,eax
	mov	ecx,eax
	shr	ebx,3
	and	ecx,0000f800h
	shr	eax,9
	and	ebx,0000001fh
	shr	ecx,6
	and	eax,00007c00h
	or	ebx,ecx
	or	ebx,eax
	mov	[edi+0],bl
	mov	[edi+1],bh
	add	edi,2

	dec	ebp
	jne	DIBconvert3216ditheredMMX@xleftover
DIBconvert3216ditheredMMX@x3:

	add	esi,[esp+16+44]
	add	edi,[esp+ 8+44]

	movq	mm4,[esp+0]
	movq	mm5,[esp+8]

	movq	mm0,DIBconvert3216ditheredMMX@vertstep1mask

	test	edx,1
	jz	DIBconvert3216ditheredMMX@even

	movq	mm0,DIBconvert3216ditheredMMX@vertstep2mask

DIBconvert3216ditheredMMX@even:

	pxor	mm4,mm0
	pxor	mm5,mm0

	dec	edx
	jne	DIBconvert3216ditheredMMX@y

	pop	eax
	pop	eax
	pop	eax
	pop	eax

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp

	emms
	ret

;******************************************************

;******************************************************
;
; void DIBconvert_32_to_16_565_dithered(
;	void *dest,		[ESP+ 4]
;	ulong dest_pitch,	[ESP+ 8]
;	void *src,		[ESP+12]
;	ulong src_pitch,	[ESP+16]
;	ulong width,		[ESP+20]
;	ulong height);		[ESP+24]

_DIBconvert_32_to_16_565_dithered:
	test	_MMX_enabled,1
	jnz	_DIBconvert_32_to_16_565_ditheredMMX
	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	push	0
	push	0
	push	0
	push	0
	push	0

	mov	esi,[esp+12+48]
	mov	edi,[esp+4+48]

	mov	[esp+8],edi

	mov	ebp,[esp+20+48]
	lea	eax,[ebp+ebp]
	lea	ebx,[ebp*4]
	sub	[esp+8+48],eax
	sub	[esp+16+48],ebx


	mov	edx,[esp+24+48]
DIBconvert3216565dithered@y:
	mov	ebp,[esp+20+48]
	push	ebp
	push	edx
	shr	ebp,1
	jz	DIBconvert3216565dithered@x2

	neg	ebp
	shl	ebp,2
	sub	[esp+16],ebp
	sub	esi,ebp
	sub	esi,ebp

	;0 8 2 A
	;C 4 E 6
	;3 B 1 9
	;F 7 D 5

	;Dithering algorithm:
	;	Multiply pixel by 249 to get an 5:11 fraction
	;	Add dithering value from above matrix as 0:11 fraction
	;
	;	(Use x241 for double dithering)

DIBconvert3216565dithered@x:
	mov	ecx,[esi+ebp*2+4]
	mov	edx,0000ff00h

	and	edx,ecx
	and	ecx,00ff00ffh

	mov	eax,ecx
	mov	ebx,edx

	lea	ecx,[ecx*8]
	lea	edx,[edx*8]

	sub	ecx,eax
	sub	edx,ebx

	shl	eax,8
	shl	ebx,8

	sub	eax,ecx
	sub	ebx,edx

	mov	ecx,[esp+8]
	mov	edx,[esp+12]

	xor	ecx,04000400h
	xor	edx,00040000h

	add	eax,ecx
	add	ebx,edx

	shr	ebx,13
	mov	ecx,eax

	shr	eax,16
	and	ecx,0000f800h

	shr	ecx,11
	and	eax,0000f800h

	and	ebx,000007e0h
	or	eax,ecx

	or	eax,ebx			;combine pixel 2
	mov	ecx,[esi+ebp*2]

	shl	eax,16
	mov	edx,ecx

	and	ecx,00ff00ffh
	and	edx,0000ff00h

	mov	edi,ecx
	mov	ebx,edx

	lea	ecx,[ecx*8]
	lea	edx,[edx*8]

	sub	ecx,edi
	sub	edx,ebx

	shl	edi,8
	shl	ebx,8

	sub	edi,ecx
	sub	ebx,edx

	add	edi,[esp+8]
	add	ebx,[esp+12]

	shr	ebx,13
	mov	ecx,edi

	shr	edi,16
	and	ecx,0000f800h

	shr	ecx,11
	and	edi,0000f800h

	and	ebx,000007e0h
	or	eax,edi

	or	eax,ebx
	mov	edi,[esp+16]

	or	eax,ecx
	mov	ecx,[esp+8]

	mov	[edi+ebp],eax
	mov	edx,[esp+12]

	xor	ecx,01000100h
	xor	edx,00010000h
	mov	[esp+8],ecx
	mov	[esp+12],edx

	add	ebp,4
	jne	DIBconvert3216565dithered@x
DIBconvert3216565dithered@x2:
	pop	edx
	pop	ebp
	and	ebp,1
	jz	DIBconvert3216565dithered@x3
	mov	eax,[esi]
	add	esi,4

	mov	ebx,eax
	mov	ecx,eax
	shr	ebx,3
	and	ecx,0000f800h
	shr	eax,8
	and	ebx,0000001fh
	shr	ecx,5
	and	eax,0000fc00h
	or	ebx,ecx
	or	ebx,eax
	mov	[edi+0],bl
	mov	[edi+1],bh
	add	edi,2
DIBconvert3216565dithered@x3:

	add	esi,[esp+16+48]
	add	edi,[esp+ 8+48]

	mov	[esp+8],edi

	mov	eax,06000600h
	mov	ebx,00060000h

	test	edx,1
	jz	DIBconvert3216565dithered@even

	mov	eax,07800780h
	mov	ebx,00078000h

DIBconvert3216565dithered@even:

	xor	eax,[esp+12]
	xor	ebx,[esp+16]
	mov	[esp+12],eax
	mov	[esp+16],ebx
	mov	[esp+0],eax
	mov	[esp+4],ebx

	dec	edx
	jne	DIBconvert3216565dithered@y

	pop	eax
	pop	eax
	pop	eax
	pop	eax
	pop	eax

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp

	ret

;******************************************************

	align	8

_DIBconvert_32_to_16_565_ditheredMMX:
	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	push	0
	push	0
	push	0
	push	0

	mov	esi,[esp+12+44]
	mov	edi,[esp+4+44]

	mov	ebp,[esp+20+44]
	lea	eax,[ebp+ebp]
	lea	ebx,[ebp*4]
	sub	[esp+8+44],eax
	sub	[esp+16+44],ebx

	movq	mm6,DIBconvert3216ditheredMMX@mult565
	pxor	mm7,mm7
	movq	mm5,DIBconvert3216ditheredMMX@565horizstep1mask
	pxor	mm4,mm4


	mov	edx,[esp+24+44]
DIBconvert3216565ditheredMMX@y:
	mov	ebp,[esp+20+44]
	shr	ebp,2
	jz	DIBconvert3216565ditheredMMX@x2

	neg	ebp
	shl	ebp,3
	sub	edi,ebp
	sub	esi,ebp
	sub	esi,ebp

	movq	[esp+0],mm4
	movq	[esp+8],mm5

	;0 8 2 A
	;C 4 E 6
	;3 B 1 9
	;F 7 D 5

	;Dithering algorithm:
	;	Multiply pixel by 249 to get an 5:11 fraction
	;	Add dithering value from above matrix as 0:11 fraction
	;
	;	(Use x241 for double dithering)
	;
	;	mm6: 241,241,249,241
	;	mm7: zero

DIBconvert3216565ditheredMMX@x:

	movq	mm0,[esi+ebp*2]

	movq	mm2,[esi+ebp*2+8]
	movq	mm1,mm0

	punpcklbw mm0,mm7
	movq	mm3,mm2

	pmullw	mm0,mm6
	punpckhbw mm1,mm7

	pmullw	mm1,mm6
	punpcklbw mm2,mm7

	pmullw	mm2,mm6
	punpckhbw mm3,mm7

	pmullw	mm3,mm6
	paddw	mm0,mm4

	psrlw	mm0,8
	paddw	mm1,mm5

	psrlw	mm1,8
	paddw	mm2,mm4

	psrlw	mm2,8
	paddw	mm3,mm5

	psrlw	mm3,8
	packuswb mm0,mm1

	packuswb mm2,mm3
	movq	mm1,mm0

	pand	mm0,DIBconvert3216MMX@red_blue_mask
	movq	mm3,mm2

	pmaddwd mm0,DIBconvert3216MMX@565_red_blue_mult

	pand	mm2,DIBconvert3216MMX@red_blue_mask

	pmaddwd mm2,DIBconvert3216MMX@565_red_blue_mult

	pand	mm1,DIBconvert3216MMX@565_green_mask
	add	ebp,8

	pand	mm3,DIBconvert3216MMX@565_green_mask
	por	mm0,mm1

	por	mm2,mm3
	pslld	mm0,16-5

	pxor	mm4,DIBconvert3216ditheredMMX@565horizstep2mask
	pslld	mm2,16-5

	psrad	mm0,16
	psrad	mm2,16

	pxor	mm5,DIBconvert3216ditheredMMX@565horizstep2mask
	packssdw mm0,mm2

	movq	[edi+ebp-8],mm0
	jne	DIBconvert3216565ditheredMMX@x
DIBconvert3216565ditheredMMX@x2:
	mov	ebp,[esp+20+44]
	and	ebp,3
	jz	DIBconvert3216565ditheredMMX@x3
DIBconvert3216565ditheredMMX@xleftover:
	mov	eax,[esi]
	add	esi,4

	mov	ebx,eax
	mov	ecx,eax
	shr	ebx,3
	and	ecx,0000fe00h
	shr	eax,8
	and	ebx,0000001fh
	shr	ecx,5
	and	eax,0000f800h
	or	ebx,ecx
	or	ebx,eax
	mov	[edi+0],bl
	mov	[edi+1],bh
	add	edi,2

	dec	ebp
	jne	DIBconvert3216565ditheredMMX@xleftover
DIBconvert3216565ditheredMMX@x3:

	add	esi,[esp+16+44]
	add	edi,[esp+ 8+44]

	movq	mm4,[esp+0]
	movq	mm5,[esp+8]

	movq	mm0,DIBconvert3216ditheredMMX@565vertstep1mask

	test	edx,1
	jz	DIBconvert3216565ditheredMMX@even

	movq	mm0,DIBconvert3216ditheredMMX@565vertstep2mask

DIBconvert3216565ditheredMMX@even:

	pxor	mm4,mm0
	pxor	mm5,mm0

	dec	edx
	jne	DIBconvert3216565ditheredMMX@y

	pop	eax
	pop	eax
	pop	eax
	pop	eax

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp

	emms
	ret

;******************************************************

	end

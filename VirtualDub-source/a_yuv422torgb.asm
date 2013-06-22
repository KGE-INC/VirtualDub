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

	.586
	.mmx
	.model	flat
	.code

	extern	_YUV_clip_table: byte
	extern	_YUV_clip_table16: byte

	public	_YUV_Y_table2
	public	_YUV_U_table2
	public	_YUV_V_table2

	public	_asm_convert_yuy2_bgr16
	public	_asm_convert_yuy2_bgr16_MMX
	public	_asm_convert_yuy2_bgr24
	public	_asm_convert_yuy2_bgr24_MMX
	public	_asm_convert_yuy2_bgr32
	public	_asm_convert_yuy2_bgr32_MMX

;asm_convert_yuy2_bgr16(void *dst, void *src, int w, int h, ptrdiff_t dstmod, ptrdiff_t srcmod)

_asm_convert_yuy2_bgr16:
	push	ebp
	push	edi
	push	esi
	push	ebx

	mov	edi,[esp+4+16]
	mov	esi,[esp+8+16]

yuy2_bgr16_scalar_y:
	mov	ebp,[esp+12+16]
yuy2_bgr16_scalar_x:
	xor	eax,eax
	xor	ebx,ebx
	xor	ecx,ecx
	xor	edx,edx

	mov	al,[esi+1]
	mov	bl,[esi+3]
	mov	cl,[esi+0]
	mov	dl,[esi+2]

	mov	eax,[_YUV_U_table2 + eax*4]
	mov	ebx,[_YUV_V_table2 + ebx*4]
	mov	ecx,[_YUV_Y_table2 + ecx*4]
	mov	edx,[_YUV_Y_table2 + edx*4]

	add	ecx,eax
	add	edx,eax
	add	ecx,ebx
	add	edx,ebx



	mov	eax,edx
	mov	ebx,edx

	shr	ebx,10
	and	edx,000003ffh
	shr	eax,20
	and	ebx,000003ffh
	movzx	edx,byte ptr [_YUV_clip_table16 + edx + 256 - 308]	;cl = blue
	movzx	eax,byte ptr [_YUV_clip_table16 + eax + 256 - 244]	;al = red
	movzx	ebx,byte ptr [_YUV_clip_table16 + ebx + 256 - 204]	;bl = green
	shl	edx,16
	shl	eax,16+10
	shl	ebx,16+5
	add	edx,eax
	add	edx,ebx



	mov	eax,ecx
	mov	ebx,ecx

	shr	ebx,10
	and	ecx,000003ffh
	shr	eax,20
	and	ebx,000003ffh
	movzx	eax,byte ptr [_YUV_clip_table16 + eax + 256 - 244]		;al = red
	movzx	ebx,byte ptr [_YUV_clip_table16 + ebx + 256 - 204]		;bl = green
	movzx	ecx,byte ptr [_YUV_clip_table16 + ecx + 256 - 308]		;dl = blue

	shl	eax,10
	add	ecx,edx
	shl	ebx,5
	add	ecx,eax
	add	ecx,ebx
	mov	[edi],ecx

	add	esi,4
	add	edi,4

	dec	ebp
	jne	yuy2_bgr16_scalar_x

	add	edi,[esp+20+16]
	add	esi,[esp+24+16]

	dec	dword ptr [esp + 16 + 16]
	jne	yuy2_bgr16_scalar_y

	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	ret


;asm_convert_yuy2_bgr24(void *dst, void *src, int w, int h, ptrdiff_t dstmod, ptrdiff_t srcmod)

_asm_convert_yuy2_bgr24:
	push	ebp
	push	edi
	push	esi
	push	ebx

	mov	edi,[esp+4+16]
	mov	esi,[esp+8+16]

yuy2_bgr24_scalar_y:
	mov	ebp,[esp+12+16]
yuy2_bgr24_scalar_x:
	xor	eax,eax
	xor	ebx,ebx
	xor	ecx,ecx
	xor	edx,edx

	mov	al,[esi+1]
	mov	bl,[esi+3]
	mov	cl,[esi+0]
	mov	dl,[esi+2]

	mov	eax,[_YUV_U_table2 + eax*4]
	mov	ebx,[_YUV_V_table2 + ebx*4]
	mov	ecx,[_YUV_Y_table2 + ecx*4]
	mov	edx,[_YUV_Y_table2 + edx*4]

	add	ecx,eax
	add	edx,eax
	add	ecx,ebx
	add	edx,ebx



	mov	eax,ecx
	mov	ebx,ecx

	shr	ebx,10
	and	ecx,000003ffh
	shr	eax,20
	and	ebx,000003ffh
	mov	al,[_YUV_clip_table + eax + 256 - 244]		;al = red
	mov	bl,[_YUV_clip_table + ebx + 256 - 204]		;bl = green
	mov	cl,[_YUV_clip_table + ecx + 256 - 308]		;cl = blue
	mov	[edi + 2],al
	mov	[edi + 1],bl
	mov	[edi + 0],cl



	mov	eax,edx
	mov	ebx,edx

	shr	ebx,10
	and	edx,000003ffh
	shr	eax,20
	and	ebx,000003ffh
	mov	al,[_YUV_clip_table + eax + 256 - 244]		;al = red
	mov	bl,[_YUV_clip_table + ebx + 256 - 204]		;bl = green
	mov	dl,[_YUV_clip_table + edx + 256 - 308]		;dl = blue
	mov	[edi + 5],al
	mov	[edi + 4],bl
	mov	[edi + 3],dl

	add	esi,4
	add	edi,6

	dec	ebp
	jne	yuy2_bgr24_scalar_x

	add	edi,[esp+20+16]
	add	esi,[esp+24+16]

	dec	dword ptr [esp + 16 + 16]
	jne	yuy2_bgr24_scalar_y

	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	ret


;asm_convert_yuy2_bgr32(void *dst, void *src, int w, int h, ptrdiff_t dstmod, ptrdiff_t srcmod)

_asm_convert_yuy2_bgr32:
	push	ebp
	push	edi
	push	esi
	push	ebx

	mov	edi,[esp+4+16]
	mov	esi,[esp+8+16]

yuy2_bgr32_scalar_y:
	mov	ebp,[esp+12+16]
yuy2_bgr32_scalar_x:
	xor	eax,eax
	xor	ebx,ebx
	xor	ecx,ecx
	xor	edx,edx

	mov	al,[esi+1]
	mov	bl,[esi+3]
	mov	cl,[esi+0]
	mov	dl,[esi+2]

	mov	eax,[_YUV_U_table2 + eax*4]
	mov	ebx,[_YUV_V_table2 + ebx*4]
	mov	ecx,[_YUV_Y_table2 + ecx*4]
	mov	edx,[_YUV_Y_table2 + edx*4]

	add	ecx,eax
	add	edx,eax
	add	ecx,ebx
	add	edx,ebx



	mov	eax,ecx
	mov	ebx,ecx

	shr	ebx,10
	and	ecx,000003ffh
	shr	eax,20
	and	ebx,000003ffh
	mov	al,[_YUV_clip_table + eax + 256 - 244]		;al = red
	mov	bl,[_YUV_clip_table + ebx + 256 - 204]		;bl = green
	mov	cl,[_YUV_clip_table + ecx + 256 - 308]		;cl = blue
	mov	[edi + 2],al
	mov	[edi + 1],bl
	mov	[edi + 0],cl



	mov	eax,edx
	mov	ebx,edx

	shr	ebx,10
	and	edx,000003ffh
	shr	eax,20
	and	ebx,000003ffh
	mov	al,[_YUV_clip_table + eax + 256 - 244]		;al = red
	mov	bl,[_YUV_clip_table + ebx + 256 - 204]		;bl = green
	mov	dl,[_YUV_clip_table + edx + 256 - 308]		;dl = blue
	mov	[edi + 6],al
	mov	[edi + 5],bl
	mov	[edi + 4],dl

	add	esi,4
	add	edi,8

	dec	ebp
	jne	yuy2_bgr32_scalar_x

	add	edi,[esp+20+16]
	add	esi,[esp+24+16]

	dec	dword ptr [esp + 16 + 16]
	jne	yuy2_bgr32_scalar_y

	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	ret

;**************************************************************************

	.data

Y_mask	dq		0000000000ff00ffh
Y_round	dq		0020002000200020h
Y_bias	dq		0000000000100010h
UV_bias	dq		0000000000800080h
Y_coeff	dq		004a004a004a004ah
U_coeff	dq		00000000ffe70081h
V_coeff	dq		00000066ffcc0000h
mask24	dq		0000ffffffffffffh
G_mask	dq		0000f8000000f800h
RB_mask	dq		00f800f800f800f8h
RB_coef	dq		2000000820000008h

	.code

;asm_convert_yuy2_bgr16_MMX(void *dst, void *src, int w, int h, ptrdiff_t dstmod, ptrdiff_t srcmod)

_asm_convert_yuy2_bgr16_MMX:
	push		ebp
	push		edi
	push		esi
	push		ebx

	mov		edi,[esp+4+16]
	mov		esi,[esp+8+16]
	mov		ebp,[esp+12+16]
	mov		ecx,[esp+16+16]
	mov		eax,[esp+20+16]
	mov		ebx,[esp+24+16]
	movq		mm6,Y_mask
	movq		mm7,Y_round

yuy2_bgr16_MMX_y:
	mov		edx,ebp
yuy2_bgr16_MMX_x:
	movd		mm0,[esi]		;mm0 = [V][Y2][U][Y1]

	movq		mm1,mm0			;mm1 = [V][Y2][U][Y1]
	pand		mm0,mm6			;mm0 = [ Y2  ][ Y1  ]

	psubw		mm0,Y_bias
	psrlw		mm1,8			;mm1 = [ V ][ U ]

	psubw		mm1,UV_bias
	punpcklwd	mm0,mm0			;mm0 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]

	pmullw		mm0,Y_coeff
	punpcklwd	mm1,mm1			;mm1 = [ V ][ V ][ U ][ U ]

	movq		mm3,mm1			;mm3 = [ V ][ V ][ U ][ U ]
	punpckldq	mm1,mm1			;mm1 = [ U ][ U ][ U ][ U ]

	pmullw		mm1,U_coeff		;mm1 = [ 0 ][ 0 ][ UG ][ UB ]
	punpckhdq	mm3,mm3			;mm3 = [ V ][ V ][ V ][ V ]

	pmullw		mm3,V_coeff		;mm3 = [ 0 ][ VR ][ VG ][ 0 ]
	paddw		mm0,mm7			;add rounding to Y

	movq		mm2,mm0			;mm2 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]
	punpckldq	mm0,mm0			;mm0 = [ Y1 ][ Y1 ][ Y1 ][ Y1 ]

	punpckhdq	mm2,mm2			;mm2 = [ Y2 ][ Y2 ][ Y2 ][ Y2 ]
	paddw		mm0,mm1

	paddw		mm2,mm1
	paddw		mm0,mm3

	paddw		mm2,mm3
	psraw		mm0,6

	movq		mm1,G_mask
	psraw		mm2,6

	movq		mm3,RB_mask
	packuswb	mm0,mm2

	pand		mm1,mm0
	pand		mm3,mm0

	pmaddwd		mm3,RB_coef
	;<-->

	add		edi,4
	add		esi,4

	;<-->
	;<-->

	por		mm3,mm1

	psrlq		mm3,6

	packssdw	mm3,mm3

	movd		[edi-4],mm3

	dec		edx
	jne		yuy2_bgr16_MMX_x

	add		edi,eax
	add		esi,ebx

	dec		ecx
	jne		yuy2_bgr16_MMX_y

	pop		ebx
	pop		esi
	pop		edi
	pop		ebp
	emms
	ret

;asm_convert_yuy2_bgr24_MMX(void *dst, void *src, int w, int h, ptrdiff_t dstmod, ptrdiff_t srcmod)

_asm_convert_yuy2_bgr24_MMX:
	push		ebp
	push		edi
	push		esi
	push		ebx

	mov		edi,[esp+4+16]
	mov		esi,[esp+8+16]
	mov		ebp,[esp+12+16]
	mov		ecx,[esp+16+16]
	mov		eax,[esp+20+16]
	mov		ebx,[esp+24+16]
	movq		mm6,Y_mask
	movq		mm7,Y_round

yuy2_bgr24_MMX_y:
	mov		edx,ebp

	dec		edx
	jz		yuy2_bgr24_MMX_doodd
yuy2_bgr24_MMX_x:
	movd		mm0,[esi]		;mm0 = [V][Y2][U][Y1]

	movq		mm1,mm0			;mm1 = [V][Y2][U][Y1]
	pand		mm0,mm6			;mm0 = [ Y2  ][ Y1  ]

	psubw		mm0,Y_bias
	psrlw		mm1,8			;mm1 = [ V ][ U ]

	psubw		mm1,UV_bias
	punpcklwd	mm0,mm0			;mm0 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]

	pmullw		mm0,Y_coeff
	punpcklwd	mm1,mm1			;mm1 = [ V ][ V ][ U ][ U ]

	movq		mm3,mm1			;mm3 = [ V ][ V ][ U ][ U ]
	punpckldq	mm1,mm1			;mm1 = [ U ][ U ][ U ][ U ]

	pmullw		mm1,U_coeff		;mm1 = [ 0 ][ 0 ][ UG ][ UB ]
	punpckhdq	mm3,mm3			;mm3 = [ V ][ V ][ V ][ V ]

	pmullw		mm3,V_coeff		;mm3 = [ 0 ][ VR ][ VG ][ 0 ]
	paddw		mm0,mm7			;add rounding to Y

	movq		mm2,mm0			;mm2 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]
	punpckldq	mm0,mm0			;mm0 = [ Y1 ][ Y1 ][ Y1 ][ Y1 ]

	punpckhdq	mm2,mm2			;mm2 = [ Y2 ][ Y2 ][ Y2 ][ Y2 ]
	paddw		mm0,mm1

	paddw		mm2,mm1
	paddw		mm0,mm3

	paddw		mm2,mm3
	psraw		mm0,6

	psraw		mm2,6
	pxor		mm5,mm5

	pand		mm0,mask24
	packuswb	mm2,mm5			;mm1 = [ 0][ 0][ 0][ 0][ 0][R1][G1][B1]

	psllq		mm2,24			;mm1 = [ 0][ 0][R1][G1][B1][ 0][ 0][ 0]

	pand		mm2,mask24
	packuswb	mm0,mm5			;mm0 = [ 0][ 0][ 0][ 0][ 0][R0][G0][B0]

	por		mm0,mm2			;mm0 = [ 0][ 0][R1][G1][B1][R0][G0][B0]

	;----------------------------------

	movd		mm4,[esi+4]		;mm4 = [V][Y2][U][Y1]

	movq		mm1,mm4			;mm1 = [V][Y2][U][Y1]
	pand		mm4,mm6			;mm4 = [ Y2  ][ Y1  ]

	psubw		mm4,Y_bias
	psrlw		mm1,8			;mm1 = [ V ][ U ]

	psubw		mm1,UV_bias
	punpcklwd	mm4,mm4			;mm4 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]

	pmullw		mm4,Y_coeff
	punpcklwd	mm1,mm1			;mm1 = [ V ][ V ][ U ][ U ]

	movq		mm3,mm1			;mm3 = [ V ][ V ][ U ][ U ]
	punpckldq	mm1,mm1			;mm1 = [ U ][ U ][ U ][ U ]

	pmullw		mm1,U_coeff		;mm1 = [ 0 ][ 0 ][ UG ][ UB ]
	punpckhdq	mm3,mm3			;mm3 = [ V ][ V ][ V ][ V ]

	pmullw		mm3,V_coeff		;mm3 = [ 0 ][ VR ][ VG ][ 0 ]
	paddw		mm4,mm7			;add rounding to Y

	movq		mm2,mm4			;mm2 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]
	punpckldq	mm4,mm4			;mm4 = [ Y1 ][ Y1 ][ Y1 ][ Y1 ]

	punpckhdq	mm2,mm2			;mm2 = [ Y2 ][ Y2 ][ Y2 ][ Y2 ]
	paddw		mm4,mm1

	paddw		mm2,mm1
	paddw		mm4,mm3

	paddw		mm2,mm3
	psraw		mm4,6

	pand		mm4,mask24
	psraw		mm2,6

	pand		mm2,mask24
	packuswb	mm4,mm5			;mm4 = [ 0][ 0][ 0][ 0][ 0][R2][G2][B2]

	packuswb	mm2,mm5			;mm2 = [ 0][ 0][ 0][ 0][ 0][R3][G3][B3]
	movq		mm1,mm4			;mm1 = [ 0][ 0][ 0][ 0][ 0][R2][G2][B2]

	psllq		mm4,48			;mm4 = [G2][B2][ 0][ 0][ 0][ 0][ 0][ 0]

	por		mm0,mm4			;mm0 = [G2][B2][R1][G1][B1][R0][G0][B0]
	psrlq		mm1,16			;mm1 = [ 0][ 0][ 0][ 0][ 0][ 0][ 0][R2]

	psllq		mm2,8			;mm2 = [ 0][ 0][ 0][ 0][R3][G3][B3][ 0]
	movq		mm3,mm0

	por		mm2,mm1			;mm2 = [ 0][ 0][ 0][ 0][R3][G3][B3][R2]
	psrlq		mm0,32			;mm0 = [ 0][ 0][ 0][ 0][G2][B2][R1][G1]

	movd		[edi],mm3
	movd		[edi+4],mm0
	movd		[edi+8],mm2

	add		edi,12
	add		esi,8

	sub		edx,2
	ja		yuy2_bgr24_MMX_x

	jnz		yuy2_bgr24_MMX_noodd

yuy2_bgr24_MMX_doodd:
	movd		mm0,[esi]		;mm0 = [V][Y2][U][Y1]

	movq		mm1,mm0			;mm1 = [V][Y2][U][Y1]
	pand		mm0,mm6			;mm0 = [ Y2  ][ Y1  ]

	psubw		mm0,Y_bias
	psrlw		mm1,8			;mm1 = [ V ][ U ]

	psubw		mm1,UV_bias
	punpcklwd	mm0,mm0			;mm0 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]

	pmullw		mm0,Y_coeff
	punpcklwd	mm1,mm1			;mm1 = [ V ][ V ][ U ][ U ]

	movq		mm3,mm1			;mm3 = [ V ][ V ][ U ][ U ]
	punpckldq	mm1,mm1			;mm1 = [ U ][ U ][ U ][ U ]

	pmullw		mm1,U_coeff		;mm1 = [ 0 ][ 0 ][ UG ][ UB ]
	punpckhdq	mm3,mm3			;mm3 = [ V ][ V ][ V ][ V ]

	pmullw		mm3,V_coeff		;mm3 = [ 0 ][ VR ][ VG ][ 0 ]
	paddw		mm0,mm7			;add rounding to Y

	movq		mm2,mm0			;mm2 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]
	punpckldq	mm0,mm0			;mm0 = [ Y1 ][ Y1 ][ Y1 ][ Y1 ]

	punpckhdq	mm2,mm2			;mm2 = [ Y2 ][ Y2 ][ Y2 ][ Y2 ]
	paddw		mm0,mm1

	paddw		mm2,mm1
	paddw		mm0,mm3

	paddw		mm2,mm3
	psraw		mm0,6

	psraw		mm2,6
	pxor		mm5,mm5

	packuswb	mm0,mm5
	add		edi,8

	packuswb	mm2,mm5
	add		esi,4

	psllq		mm2,24
	push		eax

	por		mm0,mm2
	push		ebx

	movd		eax,mm0
	psrlq		mm2,32

	movd		ebx,mm2

	mov		[edi],eax
	mov		[edi+4],bx

	pop		ebx
	pop		eax

	add		esi,4
	add		edi,6
	

yuy2_bgr24_MMX_noodd:
	add		edi,eax
	add		esi,ebx

	dec		ecx
	jne		yuy2_bgr24_MMX_y

	pop		ebx
	pop		esi
	pop		edi
	pop		ebp
	emms
	ret

;asm_convert_yuy2_bgr32_MMX(void *dst, void *src, int w, int h, ptrdiff_t dstmod, ptrdiff_t srcmod)

_asm_convert_yuy2_bgr32_MMX:
	push		ebp
	push		edi
	push		esi
	push		ebx

	mov		edi,[esp+4+16]
	mov		esi,[esp+8+16]
	mov		ebp,[esp+12+16]
	mov		ecx,[esp+16+16]
	mov		eax,[esp+20+16]
	mov		ebx,[esp+24+16]
	movq		mm6,Y_mask
	movq		mm7,Y_round

yuy2_bgr32_MMX_y:
	mov		edx,ebp
yuy2_bgr32_MMX_x:
	movd		mm0,[esi]		;mm0 = [V][Y2][U][Y1]

	movq		mm1,mm0			;mm1 = [V][Y2][U][Y1]
	pand		mm0,mm6			;mm0 = [ Y2  ][ Y1  ]

	psubw		mm0,Y_bias
	psrlw		mm1,8			;mm1 = [ V ][ U ]

	psubw		mm1,UV_bias
	punpcklwd	mm0,mm0			;mm0 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]

	pmullw		mm0,Y_coeff
	punpcklwd	mm1,mm1			;mm1 = [ V ][ V ][ U ][ U ]

	movq		mm3,mm1			;mm3 = [ V ][ V ][ U ][ U ]
	punpckldq	mm1,mm1			;mm1 = [ U ][ U ][ U ][ U ]

	pmullw		mm1,U_coeff		;mm1 = [ 0 ][ 0 ][ UG ][ UB ]
	punpckhdq	mm3,mm3			;mm3 = [ V ][ V ][ V ][ V ]

	pmullw		mm3,V_coeff		;mm3 = [ 0 ][ VR ][ VG ][ 0 ]
	paddw		mm0,mm7			;add rounding to Y

	movq		mm2,mm0			;mm2 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]
	punpckldq	mm0,mm0			;mm0 = [ Y1 ][ Y1 ][ Y1 ][ Y1 ]

	punpckhdq	mm2,mm2			;mm2 = [ Y2 ][ Y2 ][ Y2 ][ Y2 ]
	paddw		mm0,mm1

	paddw		mm2,mm1
	paddw		mm0,mm3

	paddw		mm2,mm3
	psraw		mm0,6

	psraw		mm2,6
	add		edi,8

	packuswb	mm0,mm2
	add		esi,4

	movq		[edi-8],mm0

	dec		edx
	jne		yuy2_bgr32_MMX_x

	add		edi,eax
	add		esi,ebx

	dec		ecx
	jne		yuy2_bgr32_MMX_y

	pop		ebx
	pop		esi
	pop		edi
	pop		ebp
	emms
	ret

	.const

_YUV_Y_table2	dd	00100401h, 00300c03h, 00401004h, 00501405h
	dd	00601806h, 00701c07h, 00802008h, 00a0280ah
	dd	00b02c0bh, 00c0300ch, 00d0340dh, 00e0380eh
	dd	00f03c0fh, 01104411h, 01204812h, 01304c13h
	dd	01405014h, 01505415h, 01605816h, 01705c17h
	dd	01906419h, 01a0681ah, 01b06c1bh, 01c0701ch
	dd	01d0741dh, 01e0781eh, 02008020h, 02108421h
	dd	02208822h, 02308c23h, 02409024h, 02509425h
	dd	02709c27h, 0280a028h, 0290a429h, 02a0a82ah
	dd	02b0ac2bh, 02c0b02ch, 02e0b82eh, 02f0bc2fh
	dd	0300c030h, 0310c431h, 0320c832h, 0330cc33h
	dd	0350d435h, 0360d836h, 0370dc37h, 0380e038h
	dd	0390e439h, 03a0e83ah, 03c0f03ch, 03d0f43dh
	dd	03e0f83eh, 03f0fc3fh, 04010040h, 04110441h
	dd	04310c43h, 04411044h, 04511445h, 04611846h
	dd	04711c47h, 04812048h, 04a1284ah, 04b12c4bh
	dd	04c1304ch, 04d1344dh, 04e1384eh, 04f13c4fh
	dd	05114451h, 05214852h, 05314c53h, 05415054h
	dd	05515455h, 05615856h, 05816058h, 05916459h
	dd	05a1685ah, 05b16c5bh, 05c1705ch, 05d1745dh
	dd	05e1785eh, 06018060h, 06118461h, 06218862h
	dd	06318c63h, 06419064h, 06519465h, 06719c67h
	dd	0681a068h, 0691a469h, 06a1a86ah, 06b1ac6bh
	dd	06c1b06ch, 06e1b86eh, 06f1bc6fh, 0701c070h
	dd	0711c471h, 0721c872h, 0731cc73h, 0751d475h
	dd	0761d876h, 0771dc77h, 0781e078h, 0791e479h
	dd	07a1e87ah, 07c1f07ch, 07d1f47dh, 07e1f87eh
	dd	07f1fc7fh, 08020080h, 08120481h, 08320c83h
	dd	08421084h, 08521485h, 08621886h, 08721c87h
	dd	08822088h, 08a2288ah, 08b22c8bh, 08c2308ch
	dd	08d2348dh, 08e2388eh, 08f23c8fh, 09124491h
	dd	09224892h, 09324c93h, 09425094h, 09525495h
	dd	09625896h, 09826098h, 09926499h, 09a2689ah
	dd	09b26c9bh, 09c2709ch, 09d2749dh, 09f27c9fh
	dd	0a0280a0h, 0a1284a1h, 0a2288a2h, 0a328ca3h
	dd	0a4290a4h, 0a6298a6h, 0a729ca7h, 0a82a0a8h
	dd	0a92a4a9h, 0aa2a8aah, 0ab2acabh, 0ac2b0ach
	dd	0ae2b8aeh, 0af2bcafh, 0b02c0b0h, 0b12c4b1h
	dd	0b22c8b2h, 0b32ccb3h, 0b52d4b5h, 0b62d8b6h
	dd	0b72dcb7h, 0b82e0b8h, 0b92e4b9h, 0ba2e8bah
	dd	0bc2f0bch, 0bd2f4bdh, 0be2f8beh, 0bf2fcbfh
	dd	0c0300c0h, 0c1304c1h, 0c330cc3h, 0c4310c4h
	dd	0c5314c5h, 0c6318c6h, 0c731cc7h, 0c8320c8h
	dd	0ca328cah, 0cb32ccbh, 0cc330cch, 0cd334cdh
	dd	0ce338ceh, 0cf33ccfh, 0d1344d1h, 0d2348d2h
	dd	0d334cd3h, 0d4350d4h, 0d5354d5h, 0d6358d6h
	dd	0d8360d8h, 0d9364d9h, 0da368dah, 0db36cdbh
	dd	0dc370dch, 0dd374ddh, 0df37cdfh, 0e0380e0h
	dd	0e1384e1h, 0e2388e2h, 0e338ce3h, 0e4390e4h
	dd	0e6398e6h, 0e739ce7h, 0e83a0e8h, 0e93a4e9h
	dd	0ea3a8eah, 0eb3acebh, 0ed3b4edh, 0ee3b8eeh
	dd	0ef3bcefh, 0f03c0f0h, 0f13c4f1h, 0f23c8f2h
	dd	0f33ccf3h, 0f53d4f5h, 0f63d8f6h, 0f73dcf7h
	dd	0f83e0f8h, 0f93e4f9h, 0fa3e8fah, 0fc3f0fch
	dd	0fd3f4fdh, 0fe3f8feh, 0ff3fcffh, 10040100h
	dd	10140501h, 10340d03h, 10441104h, 10541505h
	dd	10641906h, 10741d07h, 10842108h, 10a4290ah
	dd	10b42d0bh, 10c4310ch, 10d4350dh, 10e4390eh
	dd	10f43d0fh, 11144511h, 11244912h, 11344d13h
	dd	11445114h, 11545515h, 11645916h, 11846118h
	dd	11946519h, 11a4691ah, 11b46d1bh, 11c4711ch
	dd	11d4751dh, 11f47d1fh, 12048120h, 12148521h
	dd	12248922h, 12348d23h, 12449124h, 12649926h
	dd	12749d27h, 1284a128h, 1294a529h, 12a4a92ah
_YUV_U_table2	dd	0001a81eh, 0001a820h, 0001a422h, 0001a424h
	dd	0001a026h, 0001a028h, 0001a02ah, 00019c2ch
	dd	00019c2eh, 00019c30h, 00019832h, 00019834h
	dd	00019436h, 00019438h, 0001943ah, 0001903ch
	dd	0001903eh, 00018c40h, 00018c42h, 00018c44h
	dd	00018846h, 00018848h, 0001844ah, 0001844ch
	dd	0001844eh, 00018050h, 00018052h, 00017c54h
	dd	00017c56h, 00017c58h, 0001785ah, 0001785ch
	dd	0001785eh, 00017460h, 00017462h, 00017064h
	dd	00017066h, 00017068h, 00016c6ah, 00016c6ch
	dd	0001686eh, 00016870h, 00016872h, 00016474h
	dd	00016476h, 00016079h, 0001607bh, 0001607dh
	dd	00015c7fh, 00015c81h, 00015883h, 00015885h
	dd	00015887h, 00015489h, 0001548bh, 0001548dh
	dd	0001508fh, 00015091h, 00014c93h, 00014c95h
	dd	00014c97h, 00014899h, 0001489bh, 0001449dh
	dd	0001449fh, 000144a1h, 000140a3h, 000140a5h
	dd	00013ca7h, 00013ca9h, 00013cabh, 000138adh
	dd	000138afh, 000138b1h, 000134b3h, 000134b5h
	dd	000130b7h, 000130b9h, 000130bbh, 00012cbdh
	dd	00012cbfh, 000128c1h, 000128c3h, 000128c5h
	dd	000124c7h, 000124c9h, 000120cbh, 000120cdh
	dd	000120cfh, 00011cd1h, 00011cd3h, 000118d5h
	dd	000118d7h, 000118d9h, 000114dbh, 000114ddh
	dd	000114dfh, 000110e1h, 000110e3h, 00010ce5h
	dd	00010ce7h, 00010ceah, 000108ech, 000108eeh
	dd	000104f0h, 000104f2h, 000104f4h, 000100f6h
	dd	000100f8h, 0000fcfah, 0000fcfch, 0000fcfeh
	dd	0000f900h, 0000f902h, 0000f504h, 0000f506h
	dd	0000f508h, 0000f10ah, 0000f10ch, 0000f10eh
	dd	0000ed10h, 0000ed12h, 0000e914h, 0000e916h
	dd	0000e918h, 0000e51ah, 0000e51ch, 0000e11eh
	dd	0000e120h, 0000e122h, 0000dd24h, 0000dd26h
	dd	0000d928h, 0000d92ah, 0000d92ch, 0000d52eh
	dd	0000d530h, 0000d132h, 0000d134h, 0000d136h
	dd	0000cd38h, 0000cd3ah, 0000cd3ch, 0000c93eh
	dd	0000c940h, 0000c542h, 0000c544h, 0000c546h
	dd	0000c148h, 0000c14ah, 0000bd4ch, 0000bd4eh
	dd	0000bd50h, 0000b952h, 0000b954h, 0000b556h
	dd	0000b559h, 0000b55bh, 0000b15dh, 0000b15fh
	dd	0000ad61h, 0000ad63h, 0000ad65h, 0000a967h
	dd	0000a969h, 0000a96bh, 0000a56dh, 0000a56fh
	dd	0000a171h, 0000a173h, 0000a175h, 00009d77h
	dd	00009d79h, 0000997bh, 0000997dh, 0000997fh
	dd	00009581h, 00009583h, 00009185h, 00009187h
	dd	00009189h, 00008d8bh, 00008d8dh, 0000898fh
	dd	00008991h, 00008993h, 00008595h, 00008597h
	dd	00008599h, 0000819bh, 0000819dh, 00007d9fh
	dd	00007da1h, 00007da3h, 000079a5h, 000079a7h
	dd	000075a9h, 000075abh, 000075adh, 000071afh
	dd	000071b1h, 00006db3h, 00006db5h, 00006db7h
	dd	000069b9h, 000069bbh, 000069bdh, 000065bfh
	dd	000065c1h, 000061c3h, 000061c5h, 000061c7h
	dd	00005dcah, 00005dcch, 000059ceh, 000059d0h
	dd	000059d2h, 000055d4h, 000055d6h, 000051d8h
	dd	000051dah, 000051dch, 00004ddeh, 00004de0h
	dd	000049e2h, 000049e4h, 000049e6h, 000045e8h
	dd	000045eah, 000045ech, 000041eeh, 000041f0h
	dd	00003df2h, 00003df4h, 00003df6h, 000039f8h
	dd	000039fah, 000035fch, 000035feh, 00003600h
	dd	00003202h, 00003204h, 00002e06h, 00002e08h
	dd	00002e0ah, 00002a0ch, 00002a0eh, 00002610h
	dd	00002612h, 00002614h, 00002216h, 00002218h
	dd	0000221ah, 00001e1ch, 00001e1eh, 00001a20h
_YUV_V_table2	dd	0143a000h, 01539c00h, 01739800h, 01939800h
	dd	01a39400h, 01c39000h, 01d38c00h, 01f38800h
	dd	02038800h, 02238400h, 02438000h, 02537c00h
	dd	02737800h, 02837400h, 02a37400h, 02c37000h
	dd	02d36c00h, 02f36800h, 03036400h, 03236400h
	dd	03436000h, 03535c00h, 03735800h, 03835400h
	dd	03a35400h, 03c35000h, 03d34c00h, 03f34800h
	dd	04034400h, 04234000h, 04434000h, 04533c00h
	dd	04733800h, 04833400h, 04a33000h, 04c33000h
	dd	04d32c00h, 04f32800h, 05032400h, 05232000h
	dd	05432000h, 05531c00h, 05731800h, 05831400h
	dd	05a31000h, 05c30c00h, 05d30c00h, 05f30800h
	dd	06030400h, 06230000h, 0642fc00h, 0652fc00h
	dd	0672f800h, 0682f400h, 06a2f000h, 06b2ec00h
	dd	06d2ec00h, 06f2e800h, 0702e400h, 0722e000h
	dd	0732dc00h, 0752d800h, 0772d800h, 0782d400h
	dd	07a2d000h, 07b2cc00h, 07d2c800h, 07f2c800h
	dd	0802c400h, 0822c000h, 0832bc00h, 0852b800h
	dd	0872b800h, 0882b400h, 08a2b000h, 08b2ac00h
	dd	08d2a800h, 08f2a400h, 0902a400h, 0922a000h
	dd	09329c00h, 09529800h, 09729400h, 09829400h
	dd	09a29000h, 09b28c00h, 09d28800h, 09f28400h
	dd	0a028400h, 0a228000h, 0a327c00h, 0a527800h
	dd	0a727400h, 0a827000h, 0aa27000h, 0ab26c00h
	dd	0ad26800h, 0af26400h, 0b026000h, 0b226000h
	dd	0b325c00h, 0b525800h, 0b725400h, 0b825000h
	dd	0ba25000h, 0bb24c00h, 0bd24800h, 0be24400h
	dd	0c024000h, 0c223c00h, 0c323c00h, 0c523800h
	dd	0c623400h, 0c823000h, 0ca22c00h, 0cb22c00h
	dd	0cd22800h, 0ce22400h, 0d022000h, 0d221c00h
	dd	0d321c00h, 0d521800h, 0d621400h, 0d821000h
	dd	0da20c00h, 0db20800h, 0dd20800h, 0de20400h
	dd	0e020000h, 0e21fc00h, 0e31f800h, 0e51f800h
	dd	0e61f400h, 0e81f000h, 0ea1ec00h, 0eb1e800h
	dd	0ed1e400h, 0ee1e400h, 0f01e000h, 0f21dc00h
	dd	0f31d800h, 0f51d400h, 0f61d400h, 0f81d000h
	dd	0fa1cc00h, 0fb1c800h, 0fd1c400h, 0fe1c400h
	dd	1001c000h, 1021bc00h, 1031b800h, 1051b400h
	dd	1061b000h, 1081b000h, 1091ac00h, 10b1a800h
	dd	10d1a400h, 10e1a000h, 1101a000h, 11119c00h
	dd	11319800h, 11519400h, 11619000h, 11819000h
	dd	11918c00h, 11b18800h, 11d18400h, 11e18000h
	dd	12017c00h, 12117c00h, 12317800h, 12517400h
	dd	12617000h, 12816c00h, 12916c00h, 12b16800h
	dd	12d16400h, 12e16000h, 13015c00h, 13115c00h
	dd	13315800h, 13515400h, 13615000h, 13814c00h
	dd	13914800h, 13b14800h, 13d14400h, 13e14000h
	dd	14013c00h, 14113800h, 14313800h, 14513400h
	dd	14613000h, 14812c00h, 14912800h, 14b12800h
	dd	14d12400h, 14e12000h, 15011c00h, 15111800h
	dd	15311400h, 15511400h, 15611000h, 15810c00h
	dd	15910800h, 15b10400h, 15c10400h, 15e10000h
	dd	1600fc00h, 1610f800h, 1630f400h, 1640f400h
	dd	1660f000h, 1680ec00h, 1690e800h, 16b0e400h
	dd	16c0e000h, 16e0e000h, 1700dc00h, 1710d800h
	dd	1730d400h, 1740d000h, 1760d000h, 1780cc00h
	dd	1790c800h, 17b0c400h, 17c0c000h, 17e0c000h
	dd	1800bc00h, 1810b800h, 1830b400h, 1840b000h
	dd	1860ac00h, 1880ac00h, 1890a800h, 18b0a400h
	dd	18c0a000h, 18e09c00h, 19009c00h, 19109800h
	dd	19309400h, 19409000h, 19608c00h, 19808c00h
	dd	19908800h, 19b08400h, 19c08000h, 19e07c00h
	dd	1a007800h, 1a107800h, 1a307400h, 1a407000h
	dd	1a606c00h, 1a806800h, 1a906800h, 1ab06400h

	end


		.586
		.mmx
		.xmm
		.model	flat
		.const

y_co	dq		02543254325432543h
cr_co_r	dq		03313331333133313h
cb_co_b	dq		0408d408d408d408dh
cr_co_g	dq		0e5fce5fce5fce5fch
cb_co_g	dq		0f377f377f377f377h
r_bias	dq		0ff21ff21ff21ff21h
g_bias	dq		00088008800880088h
b_bias	dq		0feebfeebfeebfeebh
interp	dq		06000400020000000h
rb_mask_555	dq		07c1f7c1f7c1f7c1fh
g_mask_555	dq		003e003e003e003e0h
rb_mask_565	dq		0f81ff81ff81ff81fh
g_mask_565	dq		007e007e007e007e0h

cr_coeff	dq	000003313e5fc0000h
cb_coeff	dq	000000000f377408dh
bias		dq	000007f2180887eebh

msb_inv	dq		08000800080008000h

		.code

;============================================================================

_vdasm_pixblt_YUV411Planar_to_XRGB1555_scan_MMX	proc	near public
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			eax, [esp+4+16]
		mov			ecx, [esp+8+16]
		mov			edx, [esp+12+16]
		mov			ebx, [esp+16+16]
		mov			ebp, [esp+20+16]

		pxor		mm7, mm7

@xloop:
		movd		mm0, dword ptr [ecx]		;mm0 = Y3Y2Y1Y0
		add			ecx, 4
		punpcklbw	mm0, mm7			;mm0 = Y3 | Y2 | Y1 | Y0
		psllw		mm0, 3
		pmulhw		mm0, y_co

		movzx		esi, word ptr [ebx]
		movzx		edi, word ptr [edx]
		add			ebx, 1
		add			edx, 1

		movd		mm1, esi
		movd		mm2, edi

		punpcklbw	mm1, mm7
		punpcklwd	mm1, mm1
		movq		mm3, mm1
		punpckldq	mm1, mm1
		punpckhdq	mm3, mm3

		punpcklbw	mm2, mm7
		punpcklwd	mm2, mm2
		movq		mm4, mm2
		punpckldq	mm2, mm2
		punpckhdq	mm4, mm4

		psubw		mm3, mm1
		psubw		mm4, mm2
		paddw		mm3, mm3
		paddw		mm4, mm4

		pmulhw		mm3, interp
		pmulhw		mm4, interp

		paddw		mm1, mm3
		paddw		mm2, mm4

		psllw		mm1, 3
		psllw		mm2, 3

		movq		mm3, mm1
		movq		mm4, mm2

		pmulhw		mm1, cr_co_r
		pmulhw		mm2, cb_co_b
		pmulhw		mm3, cr_co_g
		pmulhw		mm4, cb_co_g

		paddw		mm1, mm0
		paddw		mm3, mm4
		paddw		mm2, mm0
		paddw		mm3, mm0

		paddw		mm1, r_bias
		paddw		mm3, g_bias
		paddw		mm2, b_bias

		packuswb	mm1, mm1
		packuswb	mm2, mm2
		packuswb	mm3, mm3

		psrlw		mm1, 1
		psrlw		mm2, 3
		punpcklbw	mm2, mm1
		punpcklbw	mm3, mm3
		psllw		mm3, 2
		pand		mm2, rb_mask_555
		pand		mm3, g_mask_555
		por			mm2, mm3

		movq		[eax], mm2
		add			eax, 8

		sub			ebp, 1
		jne			@xloop

		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret
_vdasm_pixblt_YUV411Planar_to_XRGB1555_scan_MMX	endp

;============================================================================

_vdasm_pixblt_YUV411Planar_to_RGB565_scan_MMX	proc	near public
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			eax, [esp+4+16]
		mov			ecx, [esp+8+16]
		mov			edx, [esp+12+16]
		mov			ebx, [esp+16+16]
		mov			ebp, [esp+20+16]

		pxor		mm7, mm7

@xloop:
		movd		mm0, dword ptr [ecx]		;mm0 = Y3Y2Y1Y0
		add			ecx, 4
		punpcklbw	mm0, mm7			;mm0 = Y3 | Y2 | Y1 | Y0
		psllw		mm0, 3
		pmulhw		mm0, y_co

		movzx		esi, word ptr [ebx]
		movzx		edi, word ptr [edx]
		add			ebx, 1
		add			edx, 1

		movd		mm1, esi
		movd		mm2, edi

		punpcklbw	mm1, mm7
		punpcklwd	mm1, mm1
		movq		mm3, mm1
		punpckldq	mm1, mm1
		punpckhdq	mm3, mm3

		punpcklbw	mm2, mm7
		punpcklwd	mm2, mm2
		movq		mm4, mm2
		punpckldq	mm2, mm2
		punpckhdq	mm4, mm4

		psubw		mm3, mm1
		psubw		mm4, mm2
		paddw		mm3, mm3
		paddw		mm4, mm4

		pmulhw		mm3, interp
		pmulhw		mm4, interp

		paddw		mm1, mm3
		paddw		mm2, mm4

		psllw		mm1, 3
		psllw		mm2, 3

		movq		mm3, mm1
		movq		mm4, mm2

		pmulhw		mm1, cr_co_r
		pmulhw		mm2, cb_co_b
		pmulhw		mm3, cr_co_g
		pmulhw		mm4, cb_co_g

		paddw		mm1, mm0
		paddw		mm3, mm4
		paddw		mm2, mm0
		paddw		mm3, mm0

		paddw		mm1, r_bias
		paddw		mm3, g_bias
		paddw		mm2, b_bias

		packuswb	mm1, mm1
		packuswb	mm2, mm2
		packuswb	mm3, mm3

		psrlw		mm2, 3
		punpcklbw	mm2, mm1
		punpcklbw	mm3, mm3
		psllw		mm3, 3
		pand		mm2, rb_mask_565
		pand		mm3, g_mask_565
		por			mm2, mm3

		movq		[eax], mm2
		add			eax, 8

		sub			ebp, 1
		jne			@xloop

		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret
_vdasm_pixblt_YUV411Planar_to_RGB565_scan_MMX	endp

;============================================================================

_vdasm_pixblt_YUV411Planar_to_XRGB8888_scan_MMX	proc	near public
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			eax, [esp+4+16]
		mov			ecx, [esp+8+16]
		mov			edx, [esp+12+16]
		mov			ebx, [esp+16+16]
		mov			ebp, [esp+20+16]

		pxor		mm7, mm7

@xloop:
		movd		mm0, dword ptr [ecx]		;mm0 = Y3Y2Y1Y0
		add			ecx, 4
		punpcklbw	mm0, mm7			;mm0 = Y3 | Y2 | Y1 | Y0
		psllw		mm0, 3
		pmulhw		mm0, y_co

		movzx		esi, word ptr [ebx]
		movzx		edi, word ptr [edx]
		add			ebx, 1
		add			edx, 1

		movd		mm1, esi
		movd		mm2, edi

		punpcklbw	mm1, mm7
		pshufw		mm3, mm1, 01010101b
		pshufw		mm1, mm1, 00000000b

		punpcklbw	mm2, mm7
		pshufw		mm4, mm2, 01010101b
		pshufw		mm2, mm2, 00000000b

		psubw		mm3, mm1
		psubw		mm4, mm2
		paddw		mm3, mm3
		paddw		mm4, mm4

		pmulhw		mm3, interp
		pmulhw		mm4, interp

		paddw		mm1, mm3
		paddw		mm2, mm4

		psllw		mm1, 3
		psllw		mm2, 3

		movq		mm3, cr_co_g
		movq		mm4, cb_co_g

		pmulhw		mm3, mm1
		pmulhw		mm4, mm2
		pmulhw		mm1, cr_co_r
		pmulhw		mm2, cb_co_b

		paddw		mm1, mm0
		paddw		mm3, mm4
		paddw		mm2, mm0
		paddw		mm3, mm0

		paddw		mm1, r_bias
		paddw		mm3, g_bias
		paddw		mm2, b_bias

		packuswb	mm1, mm1
		packuswb	mm2, mm2
		packuswb	mm3, mm3

		punpcklbw	mm2, mm1
		punpcklbw	mm3, mm3
		movq		mm1, mm2
		punpcklbw	mm1, mm3
		punpckhbw	mm2, mm3

		movq		[eax], mm1
		movq		[eax+8], mm2
		add			eax, 16

		sub			ebp, 1
		jne			@xloop

		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret
_vdasm_pixblt_YUV411Planar_to_XRGB8888_scan_MMX	endp

;============================================================================

_vdasm_pixblt_YUV411Planar_to_XRGB1555_scan_ISSE	proc	near public
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			eax, [esp+4+16]
		mov			ecx, [esp+8+16]
		mov			edx, [esp+12+16]
		mov			ebx, [esp+16+16]
		mov			ebp, [esp+20+16]

		pxor		mm7, mm7

@xloop:
		movd		mm0, dword ptr [ecx]		;mm0 = Y3Y2Y1Y0
		add			ecx, 4
		punpcklbw	mm0, mm7			;mm0 = Y3 | Y2 | Y1 | Y0
		psllw		mm0, 3
		pmulhw		mm0, y_co

		movzx		esi, word ptr [ebx]
		movzx		edi, word ptr [edx]
		add			ebx, 1
		add			edx, 1

		movd		mm1, esi
		movd		mm2, edi

		punpcklbw	mm1, mm7
		pshufw		mm3, mm1, 01010101b
		pshufw		mm1, mm1, 00000000b

		punpcklbw	mm2, mm7
		pshufw		mm4, mm2, 01010101b
		pshufw		mm2, mm2, 00000000b

		psubw		mm3, mm1
		psubw		mm4, mm2
		paddw		mm3, mm3
		paddw		mm4, mm4

		pmulhw		mm3, interp
		pmulhw		mm4, interp

		paddw		mm1, mm3
		paddw		mm2, mm4

		psllw		mm1, 3
		psllw		mm2, 3

		movq		mm3, cr_co_g
		movq		mm4, cb_co_g

		pmulhw		mm3, mm1
		pmulhw		mm4, mm2
		pmulhw		mm1, cr_co_r
		pmulhw		mm2, cb_co_b

		paddw		mm1, mm0
		paddw		mm3, mm4
		paddw		mm2, mm0
		paddw		mm3, mm0

		paddw		mm1, r_bias
		paddw		mm3, g_bias
		paddw		mm2, b_bias

		packuswb	mm1, mm1
		packuswb	mm2, mm2
		packuswb	mm3, mm3

		psrlw		mm1, 1
		psrlw		mm2, 3
		punpcklbw	mm2, mm1
		punpcklbw	mm3, mm3
		psllw		mm3, 2
		pand		mm2, rb_mask_555
		pand		mm3, g_mask_555
		por			mm2, mm3

		movq		[eax], mm2
		add			eax, 8

		sub			ebp, 1
		jne			@xloop

		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret
_vdasm_pixblt_YUV411Planar_to_XRGB1555_scan_ISSE	endp

;============================================================================

_vdasm_pixblt_YUV411Planar_to_RGB565_scan_ISSE	proc	near public
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			eax, [esp+4+16]
		mov			ecx, [esp+8+16]
		mov			edx, [esp+12+16]
		mov			ebx, [esp+16+16]
		mov			ebp, [esp+20+16]

		pxor		mm7, mm7

@xloop:
		movd		mm0, dword ptr [ecx]		;mm0 = Y3Y2Y1Y0
		add			ecx, 4
		punpcklbw	mm0, mm7			;mm0 = Y3 | Y2 | Y1 | Y0
		psllw		mm0, 3
		pmulhw		mm0, y_co

		movzx		esi, word ptr [ebx]
		movzx		edi, word ptr [edx]
		add			ebx, 1
		add			edx, 1

		movd		mm1, esi
		movd		mm2, edi

		punpcklbw	mm1, mm7
		pshufw		mm3, mm1, 01010101b
		pshufw		mm1, mm1, 00000000b

		punpcklbw	mm2, mm7
		pshufw		mm4, mm2, 01010101b
		pshufw		mm2, mm2, 00000000b

		psubw		mm3, mm1
		psubw		mm4, mm2
		paddw		mm3, mm3
		paddw		mm4, mm4

		pmulhw		mm3, interp
		pmulhw		mm4, interp

		paddw		mm1, mm3
		paddw		mm2, mm4

		psllw		mm1, 3
		psllw		mm2, 3

		movq		mm3, cr_co_g
		movq		mm4, cb_co_g

		pmulhw		mm3, mm1
		pmulhw		mm4, mm2
		pmulhw		mm1, cr_co_r
		pmulhw		mm2, cb_co_b

		paddw		mm1, mm0
		paddw		mm3, mm4
		paddw		mm2, mm0
		paddw		mm3, mm0

		paddw		mm1, r_bias
		paddw		mm3, g_bias
		paddw		mm2, b_bias

		packuswb	mm1, mm1
		packuswb	mm2, mm2
		packuswb	mm3, mm3

		psrlw		mm2, 3
		punpcklbw	mm2, mm1
		punpcklbw	mm3, mm3
		psllw		mm3, 3
		pand		mm2, rb_mask_565
		pand		mm3, g_mask_565
		por			mm2, mm3

		movq		[eax], mm2
		add			eax, 8

		sub			ebp, 1
		jne			@xloop

		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret
_vdasm_pixblt_YUV411Planar_to_RGB565_scan_ISSE	endp

;============================================================================

_vdasm_pixblt_YUV411Planar_to_XRGB8888_scan_ISSE	proc	near public
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			eax, [esp+4+16]
		mov			ecx, [esp+8+16]
		mov			edx, [esp+12+16]
		mov			ebx, [esp+16+16]
		mov			ebp, [esp+20+16]

		pxor		mm7, mm7

		movzx		esi, byte ptr [ebx]
		movzx		edi, byte ptr [edx]
		add			ebx, 1
		add			edx, 1

		movd		mm1, esi
		movd		mm2, edi

		psllw		mm1, 3
		psllw		mm2, 3

		pshufw		mm5, mm1, 0
		pshufw		mm6, mm2, 0

		pmulhw		mm5, cr_coeff
		pmulhw		mm6, cb_coeff
		paddw		mm6, mm5
		paddw		mm6, bias

@xloop:
		movd		mm0, dword ptr [ecx];mm0 = Y3Y2Y1Y0
		add			ecx, 4
		punpcklbw	mm0, mm7			;mm0 = Y3 | Y2 | Y1 | Y0
		psllw		mm0, 3
		pmulhw		mm0, y_co
		pxor		mm0, msb_inv

		movzx		esi, byte ptr [ebx]
		movzx		edi, byte ptr [edx]
		add			ebx, 1
		add			edx, 1

		movd		mm1, esi
		movd		mm2, edi

		psllw		mm1, 3
		psllw		mm2, 3

		pshufw		mm1, mm1, 0
		pshufw		mm2, mm2, 0

		pmulhw		mm1, cr_coeff
		pmulhw		mm2, cb_coeff
		paddw		mm1, mm2
		paddw		mm1, bias

		movq		mm2, mm1
		pavgw		mm2, mm6			;mm2 = 1/2
		pshufw		mm3, mm0, 00000000b
		paddw		mm3, mm6
		pavgw		mm6, mm2			;mm1 = 1/4
		pshufw		mm4, mm0, 01010101b
		paddw		mm4, mm6
		packuswb	mm3, mm4
		movq		[eax], mm3

		pshufw		mm3, mm0, 10101010b
		paddw		mm3, mm2
		pshufw		mm0, mm0, 11111111b
		pavgw		mm2, mm1			;mm2 = 3/4
		paddw		mm2, mm0
		packuswb	mm3, mm2
		movq		[eax+8], mm3

		movq		mm6, mm1

		add			eax, 16

		sub			ebp, 1
		jne			@xloop

		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret
_vdasm_pixblt_YUV411Planar_to_XRGB8888_scan_ISSE	endp

;==========================================================================

_vdasm_pixblt_YUV444Planar_to_XRGB1555_scan_MMX	proc	near public
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			eax, [esp+4+16]
		mov			ecx, [esp+8+16]
		mov			edx, [esp+12+16]
		mov			ebx, [esp+16+16]
		mov			ebp, [esp+20+16]

		pxor		mm7, mm7
		movq		mm5, rb_mask_555
		movq		mm6, g_mask_555

		sub			ebp, 3
		jbe			@oddcheck
@xloop4:
		movd		mm0, dword ptr [ecx];mm0 = Y3Y2Y1Y0
		movd		mm1, dword ptr [ebx]
		movd		mm2, dword ptr [edx]
		add			ecx, 4
		add			ebx, 4
		add			edx, 4
		punpcklbw	mm0, mm7			;mm0 = Y3 | Y2 | Y1 | Y0
		punpcklbw	mm1, mm7
		punpcklbw	mm2, mm7
		psllw		mm0, 3
		psllw		mm1, 3
		psllw		mm2, 3
		pmulhw		mm0, y_co

		movq		mm3, cr_co_g
		movq		mm4, cb_co_g

		pmulhw		mm3, mm1
		pmulhw		mm4, mm2
		pmulhw		mm1, cr_co_r
		pmulhw		mm2, cb_co_b

		paddw		mm1, mm0
		paddw		mm3, mm4
		paddw		mm2, mm0
		paddw		mm3, mm0

		paddw		mm1, r_bias
		paddw		mm3, g_bias
		paddw		mm2, b_bias

		packuswb	mm1, mm1
		packuswb	mm2, mm2
		packuswb	mm3, mm3

		psrlw		mm1, 1
		psrlw		mm2, 3
		punpcklbw	mm2, mm1
		punpcklbw	mm3, mm3
		psllw		mm3, 2
		pand		mm2, mm5
		pand		mm3, mm6
		por			mm2, mm3

		movq		[eax], mm2
		add			eax, 8

		sub			ebp, 4
		ja			@xloop4
@oddcheck:
		add			ebp, 3
		jz			@noodd
@xloop:
		movzx		edi, byte ptr [ecx]			;mm0 = Y3Y2Y1Y0
		movd		mm0, edi
		movzx		edi, byte ptr [ebx]
		movd		mm1, edi
		movzx		edi, byte ptr [edx]
		movd		mm2, edi
		add			ecx, 1
		add			ebx, 1
		add			edx, 1
		punpcklbw	mm0, mm7			;mm0 = Y3 | Y2 | Y1 | Y0
		psllw		mm0, 3
		psllw		mm1, 3
		psllw		mm2, 3
		pmulhw		mm0, y_co

		movq		mm3, cr_co_g
		movq		mm4, cb_co_g

		pmulhw		mm3, mm1
		pmulhw		mm4, mm2
		pmulhw		mm1, cr_co_r
		pmulhw		mm2, cb_co_b

		paddw		mm1, mm0
		paddw		mm3, mm4
		paddw		mm2, mm0
		paddw		mm3, mm0

		paddw		mm1, r_bias
		paddw		mm3, g_bias
		paddw		mm2, b_bias

		packuswb	mm1, mm1
		packuswb	mm2, mm2
		packuswb	mm3, mm3

		psrlw		mm1, 1
		psrlw		mm2, 3
		punpcklbw	mm2, mm1
		punpcklbw	mm3, mm3
		psllw		mm3, 2
		pand		mm2, mm5
		pand		mm3, mm6
		por			mm2, mm3

		movd		edi, mm2
		mov			[eax], di
		add			eax, 2

		sub			ebp, 1
		jnz			@xloop
@noodd:
		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret
_vdasm_pixblt_YUV444Planar_to_XRGB1555_scan_MMX	endp

;==========================================================================

_vdasm_pixblt_YUV444Planar_to_RGB565_scan_MMX	proc	near public
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			eax, [esp+4+16]
		mov			ecx, [esp+8+16]
		mov			edx, [esp+12+16]
		mov			ebx, [esp+16+16]
		mov			ebp, [esp+20+16]

		pxor		mm7, mm7
		movq		mm5, rb_mask_565
		movq		mm6, g_mask_565

		sub			ebp, 3
		jbe			@oddcheck
@xloop4:
		movd		mm0, dword ptr [ecx];mm0 = Y3Y2Y1Y0
		movd		mm1, dword ptr [ebx]
		movd		mm2, dword ptr [edx]
		add			ecx, 4
		add			ebx, 4
		add			edx, 4
		punpcklbw	mm0, mm7			;mm0 = Y3 | Y2 | Y1 | Y0
		punpcklbw	mm1, mm7
		punpcklbw	mm2, mm7
		psllw		mm0, 3
		psllw		mm1, 3
		psllw		mm2, 3
		pmulhw		mm0, y_co

		movq		mm3, cr_co_g
		movq		mm4, cb_co_g

		pmulhw		mm3, mm1
		pmulhw		mm4, mm2
		pmulhw		mm1, cr_co_r
		pmulhw		mm2, cb_co_b

		paddw		mm1, mm0
		paddw		mm3, mm4
		paddw		mm2, mm0
		paddw		mm3, mm0

		paddw		mm1, r_bias
		paddw		mm3, g_bias
		paddw		mm2, b_bias

		packuswb	mm1, mm1
		packuswb	mm2, mm2
		packuswb	mm3, mm3

		psrlw		mm2, 3
		punpcklbw	mm2, mm1
		punpcklbw	mm3, mm3
		psllw		mm3, 3
		pand		mm2, mm5
		pand		mm3, mm6
		por			mm2, mm3

		movq		[eax], mm2
		add			eax, 8

		sub			ebp, 4
		ja			@xloop4
@oddcheck:
		add			ebp, 3
		jz			@noodd
@xloop:
		movzx		edi, byte ptr [ecx]			;mm0 = Y3Y2Y1Y0
		movd		mm0, edi
		movzx		edi, byte ptr [ebx]
		movd		mm1, edi
		movzx		edi, byte ptr [edx]
		movd		mm2, edi
		add			ecx, 1
		add			ebx, 1
		add			edx, 1
		punpcklbw	mm0, mm7			;mm0 = Y3 | Y2 | Y1 | Y0
		punpcklbw	mm1, mm7
		punpcklbw	mm2, mm7
		psllw		mm0, 3
		psllw		mm1, 3
		psllw		mm2, 3
		pmulhw		mm0, y_co

		movq		mm3, cr_co_g
		movq		mm4, cb_co_g

		pmulhw		mm3, mm1
		pmulhw		mm4, mm2
		pmulhw		mm1, cr_co_r
		pmulhw		mm2, cb_co_b

		paddw		mm1, mm0
		paddw		mm3, mm4
		paddw		mm2, mm0
		paddw		mm3, mm0

		paddw		mm1, r_bias
		paddw		mm3, g_bias
		paddw		mm2, b_bias

		packuswb	mm1, mm1
		packuswb	mm2, mm2
		packuswb	mm3, mm3

		psrlw		mm2, 3
		punpcklbw	mm2, mm1
		punpcklbw	mm3, mm3
		psllw		mm3, 3
		pand		mm2, mm5
		pand		mm3, mm6
		por			mm2, mm3

		movd		edi, mm2
		mov			[eax], di
		add			eax, 2

		sub			ebp, 1
		jnz			@xloop
@noodd:
		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret
_vdasm_pixblt_YUV444Planar_to_RGB565_scan_MMX	endp

;==========================================================================

_vdasm_pixblt_YUV444Planar_to_XRGB8888_scan_MMX	proc	near public
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			eax, [esp+4+16]
		mov			ecx, [esp+8+16]
		mov			edx, [esp+12+16]
		mov			ebx, [esp+16+16]
		mov			ebp, [esp+20+16]

		pxor		mm7, mm7
		movq		mm5, rb_mask_565
		movq		mm6, g_mask_565

		sub			ebp, 3
		jbe			@oddcheck
@xloop4:
		movd		mm0, dword ptr [ecx];mm0 = Y3Y2Y1Y0
		movd		mm1, dword ptr [ebx]
		movd		mm2, dword ptr [edx]
		add			ecx, 4
		add			ebx, 4
		add			edx, 4
		punpcklbw	mm0, mm7			;mm0 = Y3 | Y2 | Y1 | Y0
		punpcklbw	mm1, mm7
		punpcklbw	mm2, mm7
		psllw		mm0, 3
		psllw		mm1, 3
		psllw		mm2, 3
		pmulhw		mm0, y_co

		movq		mm3, cr_co_g
		movq		mm4, cb_co_g

		pmulhw		mm3, mm1
		pmulhw		mm4, mm2
		pmulhw		mm1, cr_co_r
		pmulhw		mm2, cb_co_b

		paddw		mm1, mm0
		paddw		mm3, mm4
		paddw		mm2, mm0
		paddw		mm3, mm0

		paddw		mm1, r_bias
		paddw		mm3, g_bias
		paddw		mm2, b_bias

		packuswb	mm1, mm1
		packuswb	mm2, mm2
		packuswb	mm3, mm3
		punpcklbw	mm2, mm1
		punpcklbw	mm3, mm3
		movq		mm1, mm2
		punpcklbw	mm1, mm3
		punpckhbw	mm2, mm3

		movq		[eax], mm1
		movq		[eax+8], mm2
		add			eax, 16

		sub			ebp, 4
		ja			@xloop4
@oddcheck:
		add			ebp, 3
		jz			@noodd
@xloop:
		movzx		edi, byte ptr [ecx]			;mm0 = Y3Y2Y1Y0
		movd		mm0, edi
		movzx		edi, byte ptr [ebx]
		movd		mm1, edi
		movzx		edi, byte ptr [edx]
		movd		mm2, edi
		add			ecx, 1
		add			ebx, 1
		add			edx, 1
		punpcklbw	mm0, mm7			;mm0 = Y3 | Y2 | Y1 | Y0
		psllw		mm0, 3
		psllw		mm1, 3
		psllw		mm2, 3
		pmulhw		mm0, y_co

		movq		mm3, cr_co_g
		movq		mm4, cb_co_g

		pmulhw		mm3, mm1
		pmulhw		mm4, mm2
		pmulhw		mm1, cr_co_r
		pmulhw		mm2, cb_co_b

		paddw		mm1, mm0
		paddw		mm3, mm4
		paddw		mm2, mm0
		paddw		mm3, mm0

		paddw		mm1, r_bias
		paddw		mm3, g_bias
		paddw		mm2, b_bias

		packuswb	mm1, mm1
		packuswb	mm2, mm2
		packuswb	mm3, mm3

		punpcklbw	mm2, mm1
		punpcklbw	mm3, mm3
		punpcklbw	mm2, mm3

		movd		dword ptr [eax], mm2
		add			eax, 4

		sub			ebp, 1
		jnz			@xloop
@noodd:
		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret
_vdasm_pixblt_YUV444Planar_to_XRGB8888_scan_MMX	endp

		end


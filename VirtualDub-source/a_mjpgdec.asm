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
	
DCTLEN_PROFILE = 0


	IF	DCTLEN_PROFILE

	extern _short_coeffs: dword
	extern _med_coeffs: dword
	extern _long_coeffs: dword

	ENDIF

	.code
__start:


;	EBP	bit bucket
;	ESI	bit source
;	CL	24 - bits in bucket

FILLBITS	macro
	local	noneed, notmarker, loop

	cmp	dl,-8
	jl	noneed
loop:
	movzx	eax,byte ptr [esi]
	mov	ecx,000000101h
	add	ecx,eax
	shr	ecx,8
	add	esi,ecx

	mov	cl,dl
	add	cl,8
	shl	eax,cl
	or	ebp,eax
	sub	dl,8
	jns	loop
noneed:
	endm

NEEDBITS	macro at		;EAX, EDX, EBX available
	local	done,short_ok,ick

	cmp	dl,16-at
	jle	done

	movzx	eax,word ptr [esi]
	bswap	eax
	shr	eax,16
	add	esi,2

	mov	ecx,eax
	lea	ebx,[eax+01010101h]
	and	ecx,80808080h
	not	ebx
	and	ebx,ecx
	jz	short_ok
ick:
	call	sucks2
short_ok:
	mov	cl,dl
	shl	eax,cl
	sub	dl,16
	or	ebp,eax
done:
	endm

NEEDBITS2	macro at		;EAX, EDX available
	local	done,short_ok,ick

;	cmp	dl,16-at
;	jle	done
	or	dl,dl
	js	done

	movzx	eax,word ptr [esi]
	bswap	eax
	shr	eax,16
	add	esi,2

	lea	ecx,[eax+01010101h]
	not	ecx
	and	ecx,eax
	test	ecx,80808080h
	jz	short_ok
ick:
	call	sucks2
short_ok:
	mov	cl,dl
	shl	eax,cl
	sub	dl,16
	or	ebp,eax
done:
	endm

ALIGN16	macro
	local	x
x = ((__start - $) and 15)
	rept	x
	nop
	endm
	endm

; decode_mbs(dword& bitbuf, int& bitcnt, byte *& ptr, int mcu_length, MJPEGBlockDef *pmbd, int **dctptrarray);

l_size		= 32
l_mb		= 0
l_huffac	= 4
l_dctptr	= 8
l_quantptr	= 12
l_quantlimit	= 16
l_acquick	= 20
l_acquick2	= 24
l_bitcnt	= 28
l_save		= 29

p_bitbuf	= l_size + 4 + 16
p_bitcnt	= l_size + 8 + 16
p_ptr		= l_size + 12 + 16
p_mb_count	= l_size + 16 + 16
p_blocks	= l_size + 20 + 16
p_dctptrarray	= l_size + 24 + 16

block		struct
huff_dc		dd	?
huff_ac		dd	?
huff_ac_quick	dd	?
huff_ac_quick2	dd	?
quant		dd	?
dc_ptr		dd	?
ac_last		dd	?
block		ends

	public _asm_mb_decode

_asm_mb_decode:
	push	ebp
	push	edi
	push	esi
	push	ebx

	sub	esp,l_size

	mov	dword ptr [esp + l_mb],0
	mov	ebp, [esp + p_bitbuf]
	mov	ecx, [esp + p_bitcnt]
	mov	esi, [esp + p_ptr]
	mov	ebp,[ebp]
	mov	edx,[ecx]
	mov	esi,[esi]
	sub	dl,8

mb_loop:
	mov	eax,[esp + p_blocks]

	mov	ebx,[eax].block.quant
	add	ebx,8
	mov	[esp + l_quantptr],ebx
	mov	ecx,[eax].block.huff_ac_quick
	mov	[esp + l_acquick],ecx
	mov	ecx,[eax].block.huff_ac_quick2
	mov	[esp + l_acquick2],ecx
	mov	ebx,[eax].block.huff_ac
	mov	[esp + l_huffac],ebx

	push	eax
	mov	ebx,[esp + p_dctptrarray + 4]
	mov	eax,[ebx]
	mov	[esp + l_dctptr + 4],eax
	add	ebx,4
	mov	[esp + p_dctptrarray+ 4],ebx

	mov	eax,[esp+l_quantptr+4]
	add	eax,63*8
	mov	[esp + l_quantlimit+4], eax

	pop	eax

	FILLBITS

	;decode DC coefficient

	mov	[esp + l_bitcnt],dl

	mov	ecx,ebp
	shr	ecx,30
	shl	ebp,2
	xor	eax,eax
	xor	ebx,ebx
	mov	edi,[esp + p_blocks]
	mov	edi,[edi].block.huff_dc

DC_decode_loop:
	movzx	edx,byte ptr [edi + eax + 1]
	sub	ecx,edx
	jc	DC_decode_loop_term
	add	ebp,ebp
	adc	ecx,ecx
	add	ebx,edx
	inc	eax
	jmp	short DC_decode_loop

DC_decode_loop_term:
	add	ecx,edx
	mov	dl,[esp + l_bitcnt]
	add	dl,al
	add	dl,2
	
	add	ebx,ecx
	jz	no_DC_difference

	mov	eax,ebx

	;sign-extend DC difference

	cmp	ebp,80000000h
	sbb	ebx,ebx
	mov	cl,al

	mov	eax,ebx
	shld	eax,ebp,cl
	shl	ebp,cl
	sub	eax,ebx
	add	dl,cl

	;DC difference is now in EAX

no_DC_difference:
	mov	ebx,[esp + p_blocks]
	mov	ebx,[ebx].block.dc_ptr
	mov	edi,[esp + l_quantptr]
	imul	eax,[edi-8]
	add	eax,[ebx]
	mov	[ebx],eax
	mov	ecx,[esp + l_dctptr]
	mov	[ecx+0],al
	mov	[ecx+1],ah

	;***** BEGIN DECODING AC COEFFICIENTS

	mov	ebx,[esp+l_quantptr]
	jmp	short AC_loop


	;AC coefficient loop
	;
	;	EBX	quantization pointer
	;	DL	bitcnt
	;	ESI	bitptr
	;	EBP	bitheap

	ALIGN16
AC_loop:
	or	dl,dl
	jns	AC_reload

AC_reload_done:	
	cmp	ebp,0ff800000h
	jae	AC_long_decode
	cmp	ebp,0b0000000h
	jae	AC_medium_decode

	;table-based decode for short AC coefficients

	IF	DCTLEN_PROFILE
	inc	dword ptr [_short_coeffs]
	ENDIF

	mov	eax,ebp
	mov	edi,[esp + l_acquick]
	shr	eax,25
	mov	cl,[edi + eax*2 + 1]
	movsx	eax,byte ptr [edi + eax*2]

	shl	ebp,cl
	add	dl,cl
	or	eax,eax
	jz	AC_exit

AC_decode_coefficient:

	;multiply coefficient by quant. matrix entry and store

	mov	edi,[esp + l_dctptr]
	mov	ecx,[ebx+4]
	imul	eax,[ebx]
	mov	[edi + ecx],ax
	add	ebx,8

	;end of AC coefficient loop

	cmp	ecx,63*2
	jne	AC_loop
	jmp	AC_exit

	ALIGN16
AC_reload:
	movzx	eax,word ptr [esi]
	bswap	eax
	shr	eax,16
	add	esi,2

	mov	ecx,eax
	lea	edi,[eax+01010101h]
	and	ecx,80808080h
	not	edi
	and	edi,ecx
	jnz	ick
	mov	cl,dl
	sub	dl,16
	shl	eax,cl
	or	ebp,eax
	jmp	AC_reload_done

	ALIGN16
ick:
	call	sucks2
	mov	cl,dl
	sub	dl,16
	shl	eax,cl
	or	ebp,eax
	jmp	AC_reload_done

	ALIGN16
AC_medium_decode:
	IF	DCTLEN_PROFILE
	inc	dword ptr [_med_coeffs]
	ENDIF

	mov	eax,ebp
	mov	edi,[esp + l_acquick2]
	shr	eax,20
	mov	cl,[edi+eax*2 + 1 - 1600h]
	movzx	edi,byte ptr [edi+eax*2 - 1600h]

	shl	ebp,cl
	add	dl,cl

	; parse out actual code

	NEEDBITS2 16

	mov	ecx,edi
	cmp	edi,0f0h
	jz	AC_skip16

AC_do_long:
	mov	eax,ecx
	and	ecx,15			;ebx = size bits

	shr	eax,4			;eax = skip

	lea	ebx,[ebx+eax*8]
	cmp	ebx,[esp + l_quantlimit]
	jae	AC_exit

	cmp	ebp,80000000h
	sbb	eax,eax

	mov	edi,eax
	shld	eax,ebp,cl
	shl	ebp,cl
	sub	eax,edi
	add	dl,cl

	mov	edi,[esp + l_dctptr]
	mov	ecx,[ebx+4]
	imul	eax,[ebx]
	mov	[edi + ecx],ax
	add	ebx,8

	;end of AC coefficient loop

	cmp	ecx,63*2
	jne	AC_loop
	jmp	AC_exit

	ALIGN16
AC_exit:
	mov	ebx,dword ptr [ebx-4]
	shr	ebx,1

	and	ebx,63		;just in case

	;all done with this macroblock!

	mov	eax,[esp + p_blocks]
	mov	[eax].block.ac_last,ebx
	add	eax,sizeof block
	mov	[esp + p_blocks],eax

	mov	eax,[esp + l_mb]
	inc	eax
	cmp	eax,[esp + p_mb_count]
	mov	[esp + l_mb],eax
	jb	mb_loop

	;finish
fastexit:
	mov	eax,[esp + p_bitbuf]
	mov	ebx,[esp + p_bitcnt]
	mov	ecx,[esp + p_ptr]
	add	dl,8

	movsx	edx,dl

	mov	[eax], ebp
	mov	[ebx], edx
	mov	[ecx], esi

	add	esp,l_size
	pop	ebx
	pop	esi
	pop	edi
	pop	ebp

	ret





	;start long AC decode
	;
	;	16: 00-7F
	;	15: 00-3F
	;	14: 00-1F

	ALIGN16
AC_long_decode:
	IF	DCTLEN_PROFILE
	inc	dword ptr [_long_coeffs]
	ENDIF
	mov	eax,ebp
	mov	[esp+l_quantptr],ebx
	shr	eax,32-16
	mov	ebx,[esp + l_huffac]
	movzx	edi,byte ptr [ebx+eax*2-0FF80h*2]
	mov	cl,byte ptr [ebx+eax*2-0FF80h*2+1]
	shl	ebp,cl
	add	dl,cl

	NEEDBITS	16

	mov	ecx,edi
	mov	ebx,[esp + l_quantptr]

	jmp	AC_do_long

	ALIGN16
sucks2:
	movzx	eax,byte ptr [esi-2]
	lea	ecx,[eax+1]
	shr	ecx,8
	add	esi,ecx

	shl	eax,8
	movzx	ecx,byte ptr [esi-1]

	add	eax,ecx
	inc	ecx
	shr	ecx,8
	add	esi,ecx

	ret

	;reset 16 coefficients to zero

	ALIGN16
AC_skip16:
	add	ebx,16*8
	jmp	AC_loop

	end

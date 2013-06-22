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

	extern _MMX_enabled : byte

	public	_asm_average_run

;asm_average_run(
;	[esp+ 4] void *dst,
;	[esp+ 8] void *src,
;	[esp+12] ulong width,
;	[esp+16] ulong height,
;	[esp+20] ulong srcstride,
;	[esp+24] ulong dststride);

_asm_average_run:
	test	_MMX_enabled, 1
	jnz	_asm_average_run_MMX

	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	esi,[esp+ 8+28]
	mov	edi,[esp+20+28]
	mov	edx,[esp+ 4+28]

	mov	ebp,[esp+16+28]

rowloop:
	push	ebp
	mov	ebp,[esp+12+32]
	mov	eax,ebp
	shl	eax,2
	add	esi,eax
colloop:
	push	edx

	mov	eax,[esi-4]
	mov	ebx,[esi+4-4]
	mov	ecx,eax
	mov	edx,ebx

	and	eax,00ff00ffh
	and	ebx,00ff00ffh
	and	ecx,0000ff00h
	and	edx,0000ff00h
	add	eax,ebx
	add	ecx,edx

	mov	ebx,[esi+edi+4-4]
	mov	edx,0000ff00h
	and	edx,ebx
	and	ebx,00ff00ffh
	add	eax,ebx
	add	ecx,edx

	mov	ebx,[esi+edi*2-4]
	mov	edx,0000ff00h
	and	edx,ebx
	and	ebx,00ff00ffh
	add	eax,ebx
	add	ecx,edx

	mov	ebx,[esi+edi*2+4-4]
	mov	edx,0000ff00h
	and	edx,ebx
	and	ebx,00ff00ffh
	add	eax,ebx
	add	ecx,edx

	mov	ebx,[esi-4-4]
	mov	edx,0000ff00h
	and	edx,ebx
	and	ebx,00ff00ffh
	add	eax,ebx
	add	ecx,edx

	mov	ebx,[esi+edi-4-4]
	mov	edx,0000ff00h
	and	edx,ebx
	and	ebx,00ff00ffh
	add	eax,ebx
	add	ecx,edx

	mov	ebx,[esi+edi*2-4-4]
	mov	edx,0000ff00h
	and	edx,ebx
	and	ebx,00ff00ffh
	add	eax,ebx
	add	ecx,edx

	mov	ebx,eax
	mov	edx,ecx
	shl	eax,3
	sub	esi,4
	shl	ecx,3
	sub	eax,ebx
	sub	ecx,edx

	IF 1
	mov	ebx,[esi+edi]

	shl	ebx,5
	mov	edx,001fe000h

	shl	eax,2
	and	edx,ebx

	shl	ecx,2
	and	ebx,1fe01fe0h

	add	eax,ebx
	add	ecx,edx
	ELSE
	mov	ebx,[esi+edi]
	mov	edx,0000ff00h
	and	edx,ebx
	and	ebx,00ff00ffh
	shl	ebx,5
	lea	eax,[eax*4]
	shl	edx,5
	lea	ecx,[ecx*4]
	add	eax,ebx
	add	ecx,edx
	ENDIF

	shr	eax,8
	and	ecx,00ff0000h
	shr	ecx,8
	and	eax,00ff00ffh
	pop	edx
	or	eax,ecx

	mov	[edx+ebp*4-4],eax
	dec	ebp
	jne	colloop

	pop	ebp

	add	esi,edi
	add	edx,[esp+24+28]

	dec	ebp
	jne	rowloop

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp
	ret

	align	8

MMX_multiplier28	dq	001c001c001c001ch

_asm_average_run_MMX:
	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	esi,[esp+ 8+28]
	mov	edi,[esp+20+28]
	mov	edx,[esp+ 4+28]

	mov	ebp,[esp+16+28]

	;mm6: neighbor multiplier (28)
	;mm7: zero

	movq	mm6,MMX_multiplier28
	pxor	mm7,mm7

rowloop_MMX:
	push	ebp
	push	edx
	push	esi
	mov	ebp,[esp+12+40]

	movd	mm0,[esi-4]
	punpcklbw mm0,mm7

	movq	mm1,[esi]
	movq	mm2,mm1
	punpcklbw mm1,mm7
	punpckhbw mm2,mm7
	paddw	mm0,mm1
	paddw	mm0,mm2

	movd	mm1,[esi+edi*2-4]
	punpcklbw mm1,mm7
	paddw	mm0,mm1

	movq	mm1,[esi+edi*2]
	movq	mm2,mm1
	punpcklbw mm1,mm7
	punpckhbw mm2,mm7
	paddw	mm0,mm1
	paddw	mm0,mm2

	movd	mm1,[esi+edi-4]
	punpcklbw mm1,mm7
	movd	mm2,[esi+edi+4]
	punpcklbw mm2,mm7
	paddw	mm1,mm2
	paddw	mm1,mm0
	pmullw	mm1,mm6

	movd	mm2,[esi+edi]
	punpcklbw mm2,mm7
	psllw	mm2,5
	paddw	mm2,mm1
	psrlw	mm2,8
	packuswb mm2,mm2
	movd	[edx],mm2

	add	esi,4
	dec	ebp

	movd	mm1,[esi+4]
	add	edx,4
	movd	mm4,[esi+edi*2+4]
	punpcklbw mm1,mm7
	punpcklbw mm4,mm7
	paddw	mm0,mm1
	movd	mm1,[esi-8]
	paddw	mm0,mm4

	jmp	colloop_MMX_entry

colloop_MMX:
	movd	mm1,[esi+4]
	add	edx,4

	movd	mm4,[esi+edi*2+4]
	punpcklbw mm1,mm7

	paddw	mm3,mm2
	punpcklbw mm4,mm7

	psrlw	mm3,8
	paddw	mm0,mm1

	movd	mm1,[esi-8]
	packuswb mm3,mm3

	movd	[edx-4],mm3
	paddw	mm0,mm4

colloop_MMX_entry:

	movd	mm2,[esi+edi*2-8]
	punpcklbw mm1,mm7

	punpcklbw mm2,mm7
	psubw	mm0,mm1

	movd	mm1,[esi+edi-4]
	psubw	mm0,mm2

	movd	mm2,[esi+edi+4]
	punpcklbw mm1,mm7

	movd	mm3,[esi+edi]
	punpcklbw mm2,mm7

	punpcklbw mm3,mm7
	paddw	mm1,mm0

	psllw	mm3,5
	paddw	mm2,mm1

	pmullw	mm2,mm6
	add	esi,4

	dec	ebp
	jne	colloop_MMX

	paddw	mm3,mm2
	psrlw	mm3,8
	packuswb mm3,mm3
	movd	[edx],mm3

	pop	esi
	pop	edx
	pop	ebp

	add	esi,edi
	add	edx,[esp+24+28]

	dec	ebp
	jne	rowloop_MMX

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp
	emms
	ret

	end

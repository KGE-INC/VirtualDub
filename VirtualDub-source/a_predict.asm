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

;****************************************************
;
;	There is an algorithm used here for MPEG motion prediction which is
;	hard to understand from the source code alone.  It is:
;
;		mov	edi,[ecx]
;		mov	ebx,[edx]
;		mov	eax,edi
;		xor	edi,ebx
;		shr	edi,1
;		adc	eax,ebx
;		rcr	eax,1
;		and	edi,00808080h
;		add	eax,edi
;		mov	[edx],eax
;
;	EDI holds a correction factor which compensates for odd LSB sum bits
;	that got added onto the MSB of the lower byte sum.  The ADD/RCR
;	does the addition; EDI is then added on to correct for 0/1 or 1/0
;	LSB sum bits.  The lowest bit is handled through the ADC:
;
;	A-LSB	B-LSB	XOR	Sum
;	  0	  0	 0	00
;	  0	  1	 1	10
;	  1	  0	 1	10
;	  1	  1	 0	10
;
;	This produces a fully correct MPEG-1 average of four pairs of samples
;	in only 5 clocks.  It is difficult to use, however, because the
;	SHR/ADC/RCR instructions all must issue in the U-pipe (port 0 on
;	the PII), and the carry must be undisturbed between the SHR and the
;	ADC
;
;****************************************************

	.586
	.model	flat

	extern _MMX_enabled: byte


	.const

	align 16

predictors_Y		dd	predict_Y_normal
			dd	predict_Y_halfpelX
			dd	predict_Y_halfpelY
			dd	predict_Y_quadpel
adders_Y		dd	predict_add_Y_normal
			dd	predict_add_Y_halfpelX
			dd	predict_add_Y_halfpelY
			dd	predict_add_Y_quadpel

predictors_C		dd	predict_C_normal
			dd	predict_C_halfpelX
			dd	predict_C_halfpelY
			dd	predict_C_quadpel
adders_C		dd	predict_add_C_normal
			dd	predict_add_C_halfpelX
			dd	predict_add_C_halfpelY
			dd	predict_add_C_quadpel


	.code

;	video_copy_prediction(
;		YUVPixel *dst,
;		YUVPixel *src,
;		long vector_x,
;		long vector_y,
;		long plane_pitch);

src	equ	[esp+4+16]
dst	equ	[esp+8+16]
vx	equ	[esp+12+16]
vy	equ	[esp+16+16]
pitch	equ	[esp+20+16]

	public	_video_copy_prediction_Y
	extern	video_copy_prediction_Y@MMX:near

	align	16
_video_copy_prediction_Y:
	push	ebp
	push	edi
	push	esi
	push	ebx

	mov	esi,pitch
	mov	ecx,src
	mov	eax,vx
	mov	ebx,vy

	mov	edi,eax			;edi = vx
	mov	ebp,ebx			;ebp = vy
	sar	edi,1			;edi = x
	and	eax,1			;eax = x half-pel
	sar	ebp,1			;ebp = y
	add	ecx,edi			;ecx = src + x
	imul	ebp,esi			;ebp = y*pitch
	add	ecx,ebp			;ecx = src + x + y*pitch
	and	ebx,1			;ebx = y half-pel
	shl	eax,2
	mov	edx,dst
	test	_MMX_enabled,1
	jnz	video_copy_prediction_Y@MMX
	call	dword ptr [predictors_Y+eax+ebx*8]

	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	ret

;*********************************************************
;*
;*	Luminance - quadpel
;*
;*********************************************************

	align	16
predict_Y_quadpel:
	push	16
loop_Y1_quadpel:

;	[A][B][C][D][E][F][G][H]
;	[I][J][K][L][M][N][O][P]

quadpel_move	macro	off
	mov	edi,[ecx+off]
	mov	ebp,0fcfcfcfch

	and	ebp,edi
	and	edi,03030303h

	shr	ebp,2
	mov	eax,[ecx+esi+off]

	mov	ebx,0fcfcfcfch
	and	ebx,eax

	and	eax,03030303h
	add	edi,eax

	shr	ebx,2
	mov	eax,[ecx+1+off]

	add	ebp,ebx
	mov	ebx,0fcfcfcfch

	and	ebx,eax
	and	eax,03030303h

	shr	ebx,2
	add	edi,eax

	add	ebp,ebx
	mov	eax,[ecx+esi+1+off]

	add	edi,02020202h
	mov	ebx,0fcfcfcfch

	and	ebx,eax
	and	eax,03030303h

	shr	ebx,2
	add	edi,eax

	shr	edi,2
	add	ebp,ebx

	and	edi,03030303h
	add	ebp,edi

	mov	[edx+off],ebp
	endm

	quadpel_move	0
	quadpel_move	4
	quadpel_move	8
	quadpel_move	12

	mov	eax,[esp]
	lea	ecx,[ecx+esi]
	dec	eax
	lea	edx,[edx+esi]
	mov	[esp],eax
	jne	loop_Y1_quadpel
	pop	eax
	ret


;*********************************************************
;*
;*	Luminance - half-pel Y
;*
;*********************************************************

	align	16
predict_Y_halfpelY:
	push	16
loop_Y1_halfpelV:
	mov	edi,[ecx+0]
	mov	ebx,[ecx+esi+0]
	mov	eax,edi
	xor	edi,ebx
	shr	edi,1			;u
	mov	ebp,[ecx+4]
	adc	eax,ebx			;u
	mov	ebx,[ecx+esi+4]
	rcr	eax,1			;u
	and	edi,00808080h		;v
	add	eax,edi			;u
	xor	ebp,ebx
	shr	ebp,1			;u
	mov	edi,[ecx+4]
	adc	edi,ebx			;u
	mov	[edx+0],eax
	rcr	edi,1			;u
	and	ebp,00808080h
	add	edi,ebp
	mov	ebx,[ecx+esi+8]
	mov	[edx+4],edi
	mov	edi,[ecx+8]
	mov	eax,edi
	xor	edi,ebx
	shr	edi,1			;u
	mov	ebp,[ecx+12]
	adc	eax,ebx			;u
	mov	ebx,[ecx+esi+12]
	rcr	eax,1			;u
	and	edi,00808080h		;v
	add	eax,edi			;u
	xor	ebp,ebx
	shr	ebp,1			;u
	mov	edi,[ecx+12]
	adc	edi,ebx			;u
	mov	[edx+8],eax
	rcr	edi,1			;u
	and	ebp,00808080h
	add	edi,ebp
	mov	eax,[esp]
	add	ecx,esi
	mov	[edx+12],edi
	add	edx,esi
	dec	eax
	mov	[esp],eax
	jne	loop_Y1_halfpelV

	pop	eax
	ret

;*********************************************************
;*
;*	Luminance - half-pel X
;*
;*********************************************************

	align	16
predict_Y_halfpelX:
	mov	edi,16
	sub	edx,esi
loop_Y1_halfpelX:

		mov	eax,[ecx]
		mov	ebx,[ecx+4]

		shl	ebx,24
		mov	ebp,eax

		shr	ebp,8
		;<-->

		or	ebx,ebp
		mov	ebp,eax

		or	ebp,ebx
		and	eax,0fefefefeh

		shr	eax,1
		and	ebp,01010101h

		shr	ebx,1
		add	eax,ebp

		and	ebx,07f7f7f7fh
		add	eax,ebx

		mov	[edx+esi],eax

		;<---------------------->

		mov	eax,[ecx+4]
		mov	ebx,[ecx+8]

		shl	ebx,24
		mov	ebp,eax

		shr	ebp,8

		or	ebx,ebp
		mov	ebp,eax

		and	eax,0fefefefeh
		or	ebp,ebx

		shr	eax,1
		and	ebp,01010101h

		shr	ebx,1
		add	eax,ebp

		and	ebx,07f7f7f7fh
		add	eax,ebx

		mov	[edx+esi+4],eax





		mov	eax,[ecx+8]
		mov	ebx,[ecx+12]

		shl	ebx,24
		mov	ebp,eax

		shr	ebp,8
		;<-->

		or	ebx,ebp
		mov	ebp,eax

		and	eax,0fefefefeh
		or	ebp,ebx

		shr	eax,1
		and	ebp,01010101h

		shr	ebx,1
		add	eax,ebp

		and	ebx,07f7f7f7fh
		add	eax,ebx

		mov	[edx+esi+8],eax

		;<---------------------->

		mov	eax,[ecx+12]
		mov	ebx,[ecx+16]

		shl	ebx,24
		mov	ebp,eax

		shr	ebp,8
		add	edx,esi

		or	ebx,ebp
		mov	ebp,eax

		and	eax,0fefefefeh
		or	ebp,ebx

		shr	eax,1
		and	ebp,01010101h

		shr	ebx,1
		add	eax,ebp

		and	ebx,07f7f7f7fh
		add	eax,ebx

		mov	[edx+12],eax
		add	ecx,esi

		dec	edi
		jne	loop_Y1_halfpelX


	ret


;*********************************************************
;*
;*	Luminance - normal
;*
;*********************************************************

	align	16
predict_Y_normal:
	mov	edi,8
loop_Y:
	mov	eax,[ecx]
	mov	ebx,[ecx+4]
	mov	[edx],eax
	mov	[edx+4],ebx
	mov	eax,[ecx+8]
	mov	ebx,[ecx+12]
	mov	[edx+8],eax
	mov	[edx+12],ebx
	mov	eax,[ecx+esi]
	mov	ebx,[ecx+esi+4]
	mov	[edx+esi],eax
	mov	[edx+esi+4],ebx
	mov	eax,[ecx+esi+8]
	mov	ebx,[ecx+esi+12]
	mov	[edx+esi+8],eax
	mov	[edx+esi+12],ebx
	lea	ecx,[ecx+esi*2]
	lea	edx,[edx+esi*2]
	dec	edi
	jne	loop_Y
	ret




;**************************************************************************
;*
;*
;*
;*  Addition predictors
;*
;*
;*
;**************************************************************************

	public	_video_add_prediction_Y
	extern	video_add_prediction_Y@MMX:near
	align	16
_video_add_prediction_Y:
	push	ebp
	push	edi
	push	esi
	push	ebx

	mov	esi,pitch
	mov	ecx,src
	mov	eax,vx
	mov	ebx,vy

	mov	edi,eax			;edi = vx
	mov	ebp,ebx			;ebp = vy
	sar	edi,1			;edi = x
	and	eax,1			;eax = x half-pel
	sar	ebp,1			;ebp = y
	add	ecx,edi			;ecx = src + x
	imul	ebp,esi			;ebp = y*pitch
	add	ecx,ebp			;ecx = src + x + y*pitch
	and	ebx,1			;ebx = y half-pel
	shl	eax,2
	mov	edx,dst

	test	_MMX_enabled,1
	jnz	video_add_prediction_Y@MMX
	call	dword ptr [adders_Y+eax+ebx*8]

	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	ret

;*********************************************************
;*
;*	Luminance - quadpel
;*
;*********************************************************

	align	16
predict_add_Y_quadpel:
	push	16
add_loop_Y1_quadpel:

;	[A][B][C][D][E][F][G][H]
;	[I][J][K][L][M][N][O][P]

quadpel_add	macro	off
IF 0
	mov	eax,[ecx+off]		;EAX = [D][C][B][A] (#1)
	mov	ebx,[ecx+1+off]		;EBX = [E][D][C][B] (#2)
	mov	edi,eax			;EDI = [D][C][B][A] (#1)
	mov	ebp,ebx			;EBP = [E][D][C][B] (#2)
	shr	edi,8			;EDI = [0][D][C][B] (#1>>8)
	and	eax,00ff00ffh		;EAX = [ C ][ A ] (#1 even)
	shr	ebp,8			;EBP = [0][E][D][C] (#2>>8)
	and	ebx,00ff00ffh		;EBX = [ D ][ B ] (#2 even)
	and	edi,00ff00ffh		;EDI = [ D ][ B ] (#1 odd)
	and	ebp,00ff00ffh		;EBP = [ E ][ C ] (#2 odd)
	add	eax,ebx			;EAX = [C+D][A+B]
	add	edi,ebp			;EDI = [D+E][B+C]

	mov	ebx,[ecx+esi+off]	;EBX = [L][K][J][I]
	add	eax,00040004h		;EAX = [C+D+4][A+B+4]

	mov	ebp,ebx			;EBP = [L][K][J][I]
	and	ebx,00ff00ffh		;EBX = [ K ][ I ]
	shr	ebp,8			;EBP = [0][L][K][J]
	add	eax,ebx			;EAX = [C+D+K+4][A+B+I+4]
	and	ebp,00ff00ffh		;EBP = [ L ][ J ]
	mov	ebx,[ecx+esi+1+off]	;EBX = [M][L][K][J]
	add	edi,ebp			;EDI = [D+E+L][B+C+J]
	mov	ebp,ebx			;EBP = [M][L][K][J]
	
	shr	ebp,8			;EBP = [0][M][L][K]
	and	ebx,00ff00ffh		;EBX = [ L ][ J ]

	add	eax,ebx			;EAX = [C+D+K+L+4][A+B+I+J+4]
	and	ebp,00ff00ffh		;EBP = [ M ][ K ]

	mov	ebx,[edx+off]
	add	edi,ebp			;EDI = [D+E+L+M][B+C+J+K]

	mov	ebp,ebx
	and	ebp,0ff00ff00h

	shr	ebp,6
	and	ebx,00ff00ffh

	shl	ebx,2
	add	edi,ebp

	add	eax,ebx
	add	edi,00040004h
	
	shl	edi,5
	and	eax,07f807f8h

	shr	eax,3
	and	edi,0ff00ff00h

	or	eax,edi
	mov	[edx+off],eax
ELSE
	mov	edi,[ecx+off]
	mov	ebp,0f8f8f8f8h

	and	ebp,edi
	and	edi,07070707h

	shr	ebp,3
	mov	eax,[ecx+esi+off]

	mov	ebx,0f8f8f8f8h
	and	ebx,eax

	and	eax,07070707h
	add	edi,eax

	shr	ebx,3
	mov	eax,[ecx+1+off]

	add	ebp,ebx
	mov	ebx,0f8f8f8f8h

	and	ebx,eax
	and	eax,07070707h

	shr	ebx,3
	add	edi,eax

	add	ebp,ebx
	mov	eax,[ecx+esi+1+off]

	add	edi,04040404h
	mov	ebx,0f8f8f8f8h

	and	ebx,eax
	and	eax,07070707h

	shr	ebx,3
	add	edi,eax

	mov	eax,[edx+off]
	add	ebp,ebx

	mov	ebx,eax
	and	eax,0fefefefeh

	shr	eax,1
	and	ebx,01010101h

	shl	ebx,2
	add	ebp,eax

	add	edi,ebx
	shr	edi,3
	and	edi,07070707h
	add	ebp,edi

	mov	[edx+off],ebp
ENDIF
	endm

	quadpel_add	0
	quadpel_add	4
	quadpel_add	8
	quadpel_add	12

	mov	eax,[esp]
	lea	ecx,[ecx+esi]
	dec	eax
	lea	edx,[edx+esi]
	mov	[esp],eax
	jne	add_loop_Y1_quadpel
	pop	eax
	ret

;*********************************************************
;*
;*	Luminance - half-pel Y
;*
;*********************************************************

	align	16
predict_add_Y_halfpelY:
	push	16
add_loop_Y1_halfpelV:
	mov	eax,[ecx]
	mov	ebx,[edx]
	mov	edi,eax
	mov	ebp,ebx
	and	eax,00ff00ffh
	and	ebx,00ff00ffh
	and	edi,0ff00ff00h
	and	ebp,0ff00ff00h
	shr	edi,8
	shr	ebp,8
	add	ebx,ebx
	add	ebp,ebp
	add	eax,ebx
	add	edi,ebp

	mov	ebx,[ecx+esi]
	mov	ebp,ebx
	and	ebx,00ff00ffh
	and	ebp,0ff00ff00h
	shr	ebp,8
	add	eax,ebx
	add	edi,ebp

	add	eax,00020002h
	add	edi,00020002h
	shl	edi,6
	shr	eax,2
	and	eax,00ff00ffh
	and	edi,0ff00ff00h
	or	eax,edi
	mov	[edx],eax

	mov	eax,[ecx+4]
	mov	ebx,[edx+4]
	mov	edi,eax
	mov	ebp,ebx
	and	eax,00ff00ffh
	and	ebx,00ff00ffh
	and	edi,0ff00ff00h
	and	ebp,0ff00ff00h
	shr	edi,8
	shr	ebp,8
	add	ebx,ebx
	add	ebp,ebp
	add	eax,ebx
	add	edi,ebp

	mov	ebx,[ecx+esi+4]
	mov	ebp,ebx
	and	ebx,00ff00ffh
	and	ebp,0ff00ff00h
	shr	ebp,8
	add	eax,ebx
	add	edi,ebp

	add	eax,00020002h
	add	edi,00020002h
	shl	edi,6
	shr	eax,2
	and	eax,00ff00ffh
	and	edi,0ff00ff00h
	or	eax,edi
	mov	[edx+4],eax

	mov	eax,[ecx+8]
	mov	ebx,[edx+8]
	mov	edi,eax
	mov	ebp,ebx
	and	eax,00ff00ffh
	and	ebx,00ff00ffh
	and	edi,0ff00ff00h
	and	ebp,0ff00ff00h
	shr	edi,8
	shr	ebp,8
	add	ebx,ebx
	add	ebp,ebp
	add	eax,ebx
	add	edi,ebp

	mov	ebx,[ecx+esi+8]
	mov	ebp,ebx
	and	ebx,00ff00ffh
	and	ebp,0ff00ff00h
	shr	ebp,8
	add	eax,ebx
	add	edi,ebp

	add	eax,00020002h
	add	edi,00020002h
	shl	edi,6
	shr	eax,2
	and	eax,00ff00ffh
	and	edi,0ff00ff00h
	or	eax,edi
	mov	[edx+8],eax

	mov	eax,[ecx+12]
	mov	ebx,[edx+12]
	mov	edi,eax
	mov	ebp,ebx
	and	eax,00ff00ffh
	and	ebx,00ff00ffh
	and	edi,0ff00ff00h
	and	ebp,0ff00ff00h
	shr	edi,8
	shr	ebp,8
	add	ebx,ebx
	add	ebp,ebp
	add	eax,ebx
	add	edi,ebp

	mov	ebx,[ecx+esi+12]
	mov	ebp,ebx
	and	ebx,00ff00ffh
	and	ebp,0ff00ff00h
	shr	ebp,8
	add	eax,ebx
	add	edi,ebp

	add	eax,00020002h
	add	edi,00020002h
	shl	edi,6
	shr	eax,2
	and	eax,00ff00ffh
	and	edi,0ff00ff00h
	or	eax,edi
	mov	[edx+12],eax

	add	ecx,esi
	add	edx,esi

	dec	dword ptr [esp]
	jne	add_loop_Y1_halfpelV
	pop	eax
	ret


;*********************************************************
;*
;*	Luminance - half-pel X
;*
;*********************************************************

	align	16
predict_add_Y_halfpelX:
	mov	edi,16
	sub	edx,esi
add_loop_Y1_halfpelX:
	mov	eax,[ecx]
	mov	ebx,[ecx+1]
	mov	ebp,ebx
	add	edx,esi

	shr	ebx,2
	or	ebp,eax
	shr	eax,2
	and	ebx,03f3f3f3fh
	and	eax,03f3f3f3fh
	shr	ebp,1
	add	eax,ebx

	mov	ebx,[edx]

	or	ebp,ebx
	and	ebx,0fefefefeh
	shr	ebx,1
	and	ebp,01010101h
	add	eax,ebx

	add	eax,ebp
	mov	[edx],eax


	mov	eax,[ecx+4]
	mov	ebx,[ecx+5]
	mov	ebp,ebx

	shr	ebx,2
	or	ebp,eax
	shr	eax,2
	and	ebx,03f3f3f3fh
	and	eax,03f3f3f3fh
	shr	ebp,1
	add	eax,ebx

	mov	ebx,[edx+4]
	or	ebp,ebx
	and	ebx,0fefefefeh
	shr	ebx,1
	and	ebp,01010101h
	add	eax,ebx

	add	eax,ebp
	mov	[edx+4],eax





	mov	eax,[ecx+8]
	mov	ebx,[ecx+9]
	mov	ebp,ebx

	shr	ebx,2
	or	ebp,eax
	shr	eax,2
	and	ebx,03f3f3f3fh
	and	eax,03f3f3f3fh
	shr	ebp,1
	add	eax,ebx

	mov	ebx,[edx+8]

	or	ebp,ebx
	and	ebx,0fefefefeh
	shr	ebx,1
	and	ebp,01010101h
	add	eax,ebx

	add	eax,ebp
	mov	[edx+8],eax


	mov	eax,[ecx+12]
	mov	ebx,[ecx+13]
	mov	ebp,ebx
	add	ecx,esi

	shr	ebx,2
	or	ebp,eax
	shr	eax,2
	and	ebx,03f3f3f3fh
	and	eax,03f3f3f3fh
	shr	ebp,1
	add	eax,ebx

	mov	ebx,[edx+12]
	or	ebp,ebx
	and	ebx,0fefefefeh
	shr	ebx,1
	and	ebp,01010101h
	add	eax,ebx

	add	eax,ebp
	mov	[edx+12],eax


	

	dec	edi
	jne	add_loop_Y1_halfpelX
	ret





;*********************************************************
;*
;*	Luminance - normal
;*
;*	See note at top, or this will be unreadable.
;*
;*********************************************************

	align	16
predict_add_Y_normal:
	push	16
add_loop_Y1_addX:
	mov	edi,[ecx+0]		;u
	mov	ebx,[edx+0]		;v
	mov	eax,edi			;u
	xor	edi,ebx			;v
	shr	edi,1			;u
	mov	ebp,[ecx+4]		;v
	adc	eax,ebx			;u
	mov	ebx,[edx+4]		;v
	rcr	eax,1			;u
	and	edi,00808080h		;v
	add	eax,edi			;u
	xor	ebp,ebx			;v
	shr	ebp,1			;u
	mov	edi,[ecx+4]		;v
	adc	edi,ebx			;u
	mov	[edx+0],eax		;v
	rcr	edi,1			;u
	and	ebp,00808080h		;v
	add	edi,ebp			;u
	mov	ebx,[edx+8]		;v
	mov	[edx+4],edi		;u
	mov	edi,[ecx+8]		;v
	mov	eax,edi			;u
	xor	edi,ebx			;v
	shr	edi,1			;u
	mov	ebp,[ecx+12]		;v
	adc	eax,ebx			;u
	mov	ebx,[edx+12]		;v
	rcr	eax,1			;u
	and	edi,00808080h		;v
	add	eax,edi			;u
	xor	ebp,ebx			;v
	shr	ebp,1			;u
	mov	edi,[ecx+12]		;v
	adc	edi,ebx			;u
	mov	[edx+8],eax		;v
	rcr	edi,1			;u
	and	ebp,00808080h		;v
	add	edi,ebp			;u
	mov	eax,[esp+0]		;v
	add	ecx,esi			;u
	mov	[edx+12],edi		;v
	add	edx,esi			;u
	dec	eax			;v
	mov	[esp+0],eax		;u
	jne	add_loop_Y1_addX	;v

	pop	eax

	ret









;**************************************************************************

;	video_copy_prediction_C(
;		YUVPixel *dst,
;		YUVPixel *src,
;		long vector_x,
;		long vector_y,
;		long plane_pitch);

src	equ	[esp+4+16]
dst	equ	[esp+8+16]
vx	equ	[esp+12+16]
vy	equ	[esp+16+16]
pitch	equ	[esp+20+16]

	public	_video_copy_prediction_C
	extern	video_copy_prediction_C@MMX:near

	align	16
_video_copy_prediction_C:
	push	ebp
	push	edi
	push	esi
	push	ebx

	mov	esi,pitch
	mov	ecx,src
	mov	eax,vx
	mov	ebx,vy

	mov	edi,eax			;edi = vx
	mov	ebp,ebx			;ebp = vy
	sar	edi,1			;edi = x
	and	eax,1			;eax = x half-pel
	sar	ebp,1			;ebp = y
	add	ecx,edi			;ecx = src + x
	imul	ebp,esi			;ebp = y*pitch
	add	ecx,ebp			;ecx = src + x + y*pitch
	and	ebx,1			;ebx = y half-pel
	shl	eax,2
	mov	edx,dst
	test	_MMX_enabled,1
	jnz	video_copy_prediction_C@MMX
	call	dword ptr [predictors_C+eax+ebx*8]
	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	ret

;*********************************************************
;*
;*	Luminance - quadpel
;*
;*********************************************************

	align	16
predict_C_quadpel:
	push	8
loop_C1_quadpel:

;	[A][B][C][D][E][F][G][H]
;	[I][J][K][L][M][N][O][P]

quadpel_move	macro	off
	mov	eax,[ecx+off]		;EAX = [D][C][B][A] (#1)
	mov	ebx,[ecx+1+off]		;EBX = [E][D][C][B] (#2)
	mov	edi,eax			;EDI = [D][C][B][A] (#1)
	mov	ebp,ebx			;EBP = [E][D][C][B] (#2)
	shr	edi,8			;EDI = [0][D][C][B] (#1>>8)
	and	eax,00ff00ffh		;EAX = [ C ][ A ] (#1 even)
	shr	ebp,8			;EBP = [0][E][D][C] (#2>>8)
	and	ebx,00ff00ffh		;EBX = [ D ][ B ] (#2 even)
	and	edi,00ff00ffh		;EDI = [ D ][ B ] (#1 odd)
	and	ebp,00ff00ffh		;EBP = [ E ][ C ] (#2 odd)
	add	eax,ebx			;EAX = [C+D][A+B]
	add	edi,ebp			;EDI = [D+E][B+C]

	mov	ebx,[ecx+esi+off]	;EBX = [L][K][J][I]
	add	eax,00020002h		;EAX = [C+D+4][A+B+4]

	mov	ebp,ebx			;EBP = [L][K][J][I]
	and	ebx,00ff00ffh		;EBX = [ K ][ I ]
	shr	ebp,8			;EBP = [0][L][K][J]
	add	eax,ebx			;EAX = [C+D+K+4][A+B+I+4]
	and	ebp,00ff00ffh		;EBP = [ L ][ J ]
	mov	ebx,[ecx+esi+1+off]	;EBX = [M][L][K][J]
	add	edi,ebp			;EDI = [D+E+L][B+C+J]
	mov	ebp,ebx			;EBP = [M][L][K][J]
	
	shr	ebp,8			;EBP = [0][M][L][K]
	add	edi,00020002h

	and	ebx,00ff00ffh		;EBX = [ L ][ J ]
	and	ebp,00ff00ffh		;EBP = [ M ][ K ]

	add	edi,ebp			;EDI = [D+E+L+M][B+C+J+K]
	add	eax,ebx			;EAX = [C+D+K+L+4][A+B+I+J+4]
	
	shl	edi,6
	and	eax,03fc03fch

	shr	eax,2
	and	edi,0ff00ff00h

	or	eax,edi
	mov	[edx+off],eax
	endm

	quadpel_move	0
	quadpel_move	4

	mov	eax,[esp]
	lea	ecx,[ecx+esi]
	dec	eax
	lea	edx,[edx+esi]
	mov	[esp],eax
	jne	loop_C1_quadpel
	pop	eax
	ret




;*********************************************************
;*
;*	Luminance - half-pel Y
;*
;*********************************************************

	align	16
predict_C_halfpelY:
	mov	edi,8
loop_C1_halfpelV:

	mov	eax,[ecx]
	mov	ebx,[ecx+esi]
	mov	ebp,eax
	and	eax,0fefefefeh
	or	ebp,ebx
	and	ebx,0fefefefeh
	shr	eax,1
	and	ebp,01010101h
	shr	ebx,1
	add	eax,ebp
	add	ebx,eax

	mov	eax,[ecx+4]
	mov	[edx],ebx
	
	mov	ebx,[ecx+esi+4]
	add	ecx,esi

	mov	ebp,eax
	and	eax,0fefefefeh
	or	ebp,ebx
	and	ebx,0fefefefeh
	shr	eax,1
	and	ebp,01010101h
	shr	ebx,1
	add	eax,ebp

	add	eax,ebx
	dec	edi

	mov	[edx+4],eax
	lea	edx,[edx+esi]

	jne	loop_C1_halfpelV

	ret


;*********************************************************
;*
;*	Luminance - half-pel X
;*
;*********************************************************

	align	16
predict_C_halfpelX:
	mov	edi,8
	sub	edx,esi
loop_C1_halfpelX:

	mov	eax,[ecx]
	mov	ebx,[ecx+4]

	shl	ebx,24
	mov	ebp,eax

	shr	ebp,8
	;<-->

	or	ebx,ebp
	mov	ebp,eax

	or	ebp,ebx
	and	eax,0fefefefeh

	shr	eax,1
	and	ebp,01010101h

	shr	ebx,1
	add	eax,ebp

	and	ebx,07f7f7f7fh
	add	eax,ebx

	mov	[edx+esi],eax

	;<---------------------->

	mov	eax,[ecx+4]
	mov	ebx,[ecx+8]

	shl	ebx,24
	mov	ebp,eax

	shr	ebp,8
	add	edx,esi

	or	ebx,ebp
	mov	ebp,eax

	or	ebp,ebx
	and	eax,0fefefefeh

	shr	eax,1
	and	ebp,01010101h

	shr	ebx,1
	add	eax,ebp

	and	ebx,07f7f7f7fh
	add	eax,ebx

	mov	[edx+4],eax
	add	ecx,esi

	dec	edi
	jne	loop_C1_halfpelX
	ret


;*********************************************************
;*
;*	Luminance - normal
;*
;*********************************************************

	align	16
predict_C_normal:
	mov	edi,4
loop_C:
	mov	eax,[ecx]
	mov	ebx,[ecx+4]
	mov	[edx],eax
	mov	[edx+4],ebx
	mov	eax,[ecx+esi]
	mov	ebx,[ecx+esi+4]
	mov	[edx+esi],eax
	mov	[edx+esi+4],ebx
	lea	ecx,[ecx+esi*2]
	lea	edx,[edx+esi*2]
	dec	edi
	jne	loop_C
	ret





;**************************************************************************
;*
;*
;*
;*  Addition predictors
;*
;*
;*
;**************************************************************************

	public	_video_add_prediction_C
	extern	video_add_prediction_C@MMX:near

	align	16
_video_add_prediction_C:
	push	ebp
	push	edi
	push	esi
	push	ebx

	mov	esi,pitch
	mov	ecx,src
	mov	eax,vx
	mov	ebx,vy

	mov	edi,eax			;edi = vx
	mov	ebp,ebx			;ebp = vy
	sar	edi,1			;edi = x
	and	eax,1			;eax = x half-pel
	sar	ebp,1			;ebp = y
	add	ecx,edi			;ecx = src + x
	imul	ebp,esi			;ebp = y*pitch
	add	ecx,ebp			;ecx = src + x + y*pitch
	and	ebx,1			;ebx = y half-pel
	shl	eax,2
	mov	edx,dst

	test	_MMX_enabled,1
	jnz	video_add_prediction_C@MMX
	call	dword ptr [adders_C+eax+ebx*8]

	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	ret

;*********************************************************
;*
;*	Luminance - quadpel
;*
;*********************************************************

	align	16
predict_add_C_quadpel:
	push	8
add_loop_C1_quadpel:

;	[A][B][C][D][E][F][G][H]
;	[I][J][K][L][M][N][O][P]

quadpel_add	macro	off
IF 0
	mov	eax,[ecx+off]		;EAX = [D][C][B][A] (#1)
	mov	ebx,[ecx+1+off]		;EBX = [E][D][C][B] (#2)

	mov	edi,eax			;EDI = [D][C][B][A] (#1)
	mov	ebp,ebx			;EBP = [E][D][C][B] (#2)

	shr	edi,8			;EDI = [0][D][C][B] (#1>>8)
	and	eax,00ff00ffh		;EAX = [ C ][ A ] (#1 even)

	shr	ebp,8			;EBP = [0][E][D][C] (#2>>8)
	and	ebx,00ff00ffh		;EBX = [ D ][ B ] (#2 even)

	and	edi,00ff00ffh		;EDI = [ D ][ B ] (#1 odd)
	and	ebp,00ff00ffh		;EBP = [ E ][ C ] (#2 odd)

	add	eax,ebx			;EAX = [C+D][A+B]
	add	edi,ebp			;EDI = [D+E][B+C]

	mov	ebx,[ecx+esi+off]	;EBX = [L][K][J][I]
	add	eax,00040004h		;EAX = [C+D+4][A+B+4]

	mov	ebp,ebx			;EBP = [L][K][J][I]
	and	ebx,00ff00ffh		;EBX = [ K ][ I ]

	shr	ebp,8			;EBP = [0][L][K][J]
	add	eax,ebx			;EAX = [C+D+K+4][A+B+I+4]

	and	ebp,00ff00ffh		;EBP = [ L ][ J ]
	mov	ebx,[ecx+esi+1+off]	;EBX = [M][L][K][J]

	add	edi,ebp			;EDI = [D+E+L][B+C+J]
	mov	ebp,ebx			;EBP = [M][L][K][J]
	
	shr	ebp,8			;EBP = [0][M][L][K]
	and	ebx,00ff00ffh		;EBX = [ L ][ J ]

	add	eax,ebx			;EAX = [C+D+K+L+4][A+B+I+J+4]
	and	ebp,00ff00ffh		;EBP = [ M ][ K ]

	mov	ebx,[edx+off]
	add	edi,ebp			;EDI = [D+E+L+M][B+C+J+K]

	mov	ebp,ebx
	and	ebp,0ff00ff00h

	shr	ebp,6
	and	ebx,00ff00ffh

	shl	ebx,2
	add	edi,ebp

	add	eax,ebx
	add	edi,00040004h
	
	shl	edi,5
	and	eax,07f807f8h

	shr	eax,3
	and	edi,0ff00ff00h

	or	eax,edi
	mov	[edx+off],eax
ELSE
	mov	edi,[ecx+off]
	mov	ebp,0f8f8f8f8h

	and	ebp,edi
	and	edi,07070707h

	shr	ebp,3
	mov	eax,[ecx+esi+off]

	mov	ebx,0f8f8f8f8h
	and	ebx,eax

	and	eax,07070707h
	add	edi,eax

	shr	ebx,3
	mov	eax,[ecx+1+off]

	add	ebp,ebx
	mov	ebx,0f8f8f8f8h

	and	ebx,eax
	and	eax,07070707h

	shr	ebx,3
	add	edi,eax

	add	ebp,ebx
	mov	eax,[ecx+esi+1+off]

	add	edi,04040404h
	mov	ebx,0f8f8f8f8h

	and	ebx,eax
	and	eax,07070707h

	shr	ebx,3
	add	edi,eax

	mov	eax,[edx+off]
	add	ebp,ebx

	mov	ebx,eax
	and	eax,0fefefefeh

	shr	eax,1
	and	ebx,01010101h

	shl	ebx,2
	add	ebp,eax

	add	edi,ebx
	shr	edi,3
	and	edi,07070707h
	add	ebp,edi

	mov	[edx+off],ebp
ENDIF
	endm

	quadpel_add	0
	quadpel_add	4

	mov	eax,[esp]
	lea	ecx,[ecx+esi]
	dec	eax
	lea	edx,[edx+esi]
	mov	[esp],eax
	jne	add_loop_C1_quadpel
	pop	eax
	ret


;*********************************************************
;*
;*	Luminance - half-pel Y
;*
;*********************************************************

	align	16
predict_add_C_halfpelY:
	push	8
add_loop_C1_halfpelV:
	mov	eax,[ecx]
	mov	ebx,[edx]
	mov	edi,eax
	mov	ebp,ebx
	and	eax,00ff00ffh
	and	ebx,00ff00ffh
	and	edi,0ff00ff00h
	and	ebp,0ff00ff00h
	shr	edi,8
	shr	ebp,8
	add	ebx,ebx
	add	ebp,ebp
	add	eax,ebx
	add	edi,ebp

	mov	ebx,[ecx+esi]
	mov	ebp,ebx
	and	ebx,00ff00ffh
	and	ebp,0ff00ff00h
	shr	ebp,8
	add	eax,ebx
	add	edi,ebp

	add	eax,00020002h
	add	edi,00020002h
	shl	edi,6
	shr	eax,2
	and	eax,00ff00ffh
	and	edi,0ff00ff00h
	or	eax,edi
	mov	[edx],eax

	mov	eax,[ecx+4]
	mov	ebx,[edx+4]
	mov	edi,eax
	mov	ebp,ebx
	and	eax,00ff00ffh
	and	ebx,00ff00ffh
	and	edi,0ff00ff00h
	and	ebp,0ff00ff00h
	shr	edi,8
	shr	ebp,8
	add	ebx,ebx
	add	ebp,ebp
	add	eax,ebx
	add	edi,ebp

	mov	ebx,[ecx+esi+4]
	mov	ebp,ebx
	and	ebx,00ff00ffh
	and	ebp,0ff00ff00h
	shr	ebp,8
	add	eax,ebx
	add	edi,ebp

	add	eax,00020002h
	add	edi,00020002h
	shl	edi,6
	shr	eax,2
	and	eax,00ff00ffh
	and	edi,0ff00ff00h
	or	eax,edi
	mov	[edx+4],eax


	add	ecx,esi
	add	edx,esi

	dec	dword ptr [esp]
	jne	add_loop_C1_halfpelV
	pop	eax
	ret


;*********************************************************
;*
;*	Luminance - half-pel X
;*
;*********************************************************

	align	16
predict_add_C_halfpelX:
	mov	edi,8
	sub	edx,esi
add_loop_C1_halfpelX:
	mov	eax,[ecx]
	mov	ebx,[ecx+1]
	mov	ebp,ebx
	add	edx,esi

	shr	ebx,2
	or	ebp,eax
	shr	eax,2
	and	ebx,03f3f3f3fh
	and	eax,03f3f3f3fh
	shr	ebp,1
	add	eax,ebx

	mov	ebx,[edx]

	or	ebp,ebx
	and	ebx,0fefefefeh
	shr	ebx,1
	and	ebp,01010101h
	add	eax,ebx

	add	eax,ebp
	mov	[edx],eax


	mov	eax,[ecx+4]
	mov	ebx,[ecx+5]
	mov	ebp,ebx
	add	ecx,esi

	shr	ebx,2
	or	ebp,eax
	shr	eax,2
	and	ebx,03f3f3f3fh
	and	eax,03f3f3f3fh
	shr	ebp,1
	add	eax,ebx

	mov	ebx,[edx+4]
	or	ebp,ebx
	and	ebx,0fefefefeh
	shr	ebx,1
	and	ebp,01010101h
	add	eax,ebx

	add	eax,ebp
	mov	[edx+4],eax


	

	dec	edi
	jne	add_loop_C1_halfpelX
	ret




;*********************************************************
;*
;*	Luminance - normal
;*
;*********************************************************

	align	16
predict_add_C_normal:
	mov	edi,8
add_loop_C1_addX:

	mov	eax,[ecx]
	mov	ebx,[edx]
	mov	ebp,eax
	and	eax,0fefefefeh
	shr	eax,1
	or	ebp,ebx
	shr	ebx,1
	and	ebp,001010101h
	and	ebx,07f7f7f7fh
	add	ebp,eax
	add	ebx,ebp
	mov	eax,[ecx+4]
	mov	[edx],ebx
	mov	ebx,[edx+4]
	mov	ebp,eax
	and	eax,0fefefefeh
	shr	eax,1
	or	ebp,ebx
	shr	ebx,1
	and	ebp,001010101h
	and	ebx,07f7f7f7fh
	add	ebp,eax
	add	ebx,ebp
	add	ecx,esi
	mov	[edx+4],ebx
	add	edx,esi

	dec	edi
	jne	add_loop_C1_addX
	ret


	end


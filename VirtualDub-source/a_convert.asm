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
	.code

	extern _MMX_enabled : byte

	public	_DIBconvert_8_to_16
	public	_DIBconvert_8_to_24
	public	_DIBconvert_8_to_32
	public	_DIBconvert_16_to_32
	public	_DIBconvert_24_to_32
	public	_DIBconvert_16_to_24
	public	_DIBconvert_24_to_24
	public	_DIBconvert_16_to_16
	public	_DIBconvert_24_to_16



; void DIBconvert_8_to_32(
;	void *dest,		[ESP+ 4]
;	ulong dest_pitch,	[ESP+ 8]
;	void *src,		[ESP+12]
;	ulong src_pitch,	[ESP+16]
;	ulong width,		[ESP+20]
;	ulong height,		[ESP+24]
;	ulong *palette);	[ESP+28]

_DIBconvert_8_to_32:
	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	esi,[esp+12+28]
	mov	edi,[esp+4+28]

	mov	eax,[esp+20+28]
	mov	ebx,eax
	shl	ebx,2
	mov	edx,[esp+28+28]
	mov	ecx,[esp+24+28]

	sub	[esp+16+28],eax
	sub	[esp+8+28],ebx

DIBconvert832@y:
	mov	ebp,[esp+20+28]
	xor	eax,eax
DIBconvert832@x:
	mov	al,[esi]
	inc	esi
	mov	ebx,[eax*4+edx]	
	mov	[edi],ebx
	add	edi,4
	dec	ebp
	jne	DIBconvert832@x

	add	esi,[esp+16+28]
	add	edi,[esp+8+28]

	dec	ecx
	jne	DIBconvert832@y

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp

	ret

; void DIBconvert_8_to_24(
;	void *dest,		[ESP+ 4]
;	ulong dest_pitch,	[ESP+ 8]
;	void *src,		[ESP+12]
;	ulong src_pitch,	[ESP+16]
;	ulong width,		[ESP+20]
;	ulong height,		[ESP+24]
;	ulong *palette);	[ESP+28]

_DIBconvert_8_to_24:
	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	esi,[esp+12+28]
	mov	edi,[esp+4+28]

	mov	eax,[esp+20+28]
	mov	ebx,eax
	lea	ebx,[ebx+eax*2]
	mov	edx,[esp+28+28]
	mov	ecx,[esp+24+28]

	sub	[esp+16+28],eax
	sub	[esp+8+28],ebx

DIBconvert824@y:
	mov	ebp,[esp+20+28]
	xor	eax,eax
DIBconvert824@x:
	mov	al,[esi]
	inc	esi
	mov	ebx,[eax*4+edx]	
	mov	[edi],ebx
	add	edi,3
	dec	ebp
	jne	DIBconvert824@x

	add	esi,[esp+16+28]
	add	edi,[esp+8+28]

	dec	ecx
	jne	DIBconvert824@y

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
; void DIBconvert_8_to_16(
;	void *dest,		[ESP+ 4]
;	ulong dest_pitch,	[ESP+ 8]
;	void *src,		[ESP+12]
;	ulong src_pitch,	[ESP+16]
;	ulong width,		[ESP+20]
;	ulong height,		[ESP+24]
;	ulong *palette);	[ESP+28]
;
; This implementation is painfully bad, but it's not really
; worth optimizing much...

_DIBconvert_8_to_16:
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
	sub	[esp+8+28],eax
	sub	[esp+16+28],ebp

	mov	edx,[esp+24+28]
DIBconvert816@y:
	mov	ebp,[esp+20+28]
	push	ebp
	push	edx
	shr	ebp,1
	jz	DIBconvert816@x2
DIBconvert816@x:
	xor	eax,eax			;u
	mov	ebx,[esp+28+36]		;v
	mov	al,[esi+1]		;u
	add	esi,2			;v
	mov	eax,[ebx+eax*4]		;u [AGI]

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

	xor	ecx,ecx			;v
	mov	eax,[esp+28+36]		;u
	mov	cl,[esi-2]		;v
	mov	edx,ebx			;u
	mov	ecx,[eax+ecx*4]		;v [AGI]
	mov	eax,ecx			;u

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
	jne	DIBconvert816@x		;v
DIBconvert816@x2:
	pop	edx
	pop	ebp
	and	ebp,1
	jz	DIBconvert816@x3
	xor	eax,eax
	mov	ecx,[esp+28+28]
	mov	al,[esi]
	mov	eax,[eax*4+ecx]
	inc	esi

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
DIBconvert816@x3:

	add	esi,[esp+16+28]
	add	edi,[esp+ 8+28]

	dec	edx
	jne	DIBconvert816@y

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp

	ret

; void DIBconvert_16_to_32(
;	void *dest,		[ESP+ 4]
;	ulong dest_pitch,	[ESP+ 8]
;	void *src,		[ESP+12]
;	ulong src_pitch,	[ESP+16]
;	ulong width,		[ESP+20]
;	ulong height);		[ESP+24]

_DIBconvert_16_to_32:
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
	lea	eax,[ebp*4]
	lea	ebx,[ebp*2]
	sub	[esp+8+28],eax
	sub	[esp+16+28],ebx

	mov	edx,[esp+24+28]
DIBconvert1632@y:
	mov	ebp,[esp+20+28]
	push	ebp
	push	edx
	shr	ebp,1
	jz	DIBconvert1632@x2
DIBconvert1632@x:
	mov	eax,[esi]
	add	esi,4
	mov	edx,eax
	add	edi,8
	mov	ebx,eax
	mov	ecx,eax
	shl	ebx,9
	and	eax,0000001fh
	shl	ecx,6
	and	ebx,00f80000h
	shl	eax,3
	and	ecx,0000f800h
	or	ebx,ecx
	mov	ecx,edx
	or	eax,ebx
	mov	ebx,edx
	shr	ebx,7
	and	edx,001f0000h
	shr	ecx,10
	and	ebx,00f80000h
	shr	edx,13
	and	ecx,0000f800h
	or	ebx,ecx
	mov	[edi-8],eax
	or	edx,ebx
	dec	ebp
	mov	[edi-4],edx
	jne	DIBconvert1632@x
DIBconvert1632@x2:
	pop	edx
	pop	ebp
	and	ebp,1
	jz	DIBconvert1632@x3

	mov	eax,[esi]
	add	esi,2

	mov	ebx,eax
	mov	ecx,eax
	shl	ebx,9
	shl	ecx,6
	shl	eax,3
	and	ebx,00f80000h
	and	ecx,0000f800h
	and	eax,000000f8h
	or	ebx,ecx
	or	eax,ebx
	mov	[edi],eax
	add	edi,4

DIBconvert1632@x3:

	add	esi,[esp+16+28]
	add	edi,[esp+ 8+28]

	dec	edx
	jne	DIBconvert1632@y

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp

	ret

;***************************************************
;
; void DIBconvert_24_to_32(
;	void *dest,		[ESP+ 4]
;	ulong dest_pitch,	[ESP+ 8]
;	void *src,		[ESP+12]
;	ulong src_pitch,	[ESP+16]
;	ulong width,		[ESP+20]
;	ulong height);		[ESP+24]

_DIBconvert_24_to_32:
	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx

	mov	esi,[esp+12+24]
	mov	edi,[esp+4+24]

	mov	ecx,[esp+20+24]
	lea	eax,[ecx+ecx*2]
	lea	ebx,[ecx*4]
	sub	[esp+8+24],ebx
	sub	[esp+16+24],eax

	mov	edx,[esp+24+24]
DIBconvert2432@y:
	mov	ebp,[esp+20+24]
	shr	ebp,2
	push	edx
	jz	DIBconvert2432@x2
DIBconvert2432@x1:

	mov	eax,[esi]		;EAX: b1r0g0b0
	mov	ebx,[esi+4]		;EBX: g2b2r1g1

	mov	[edi],eax
	mov	ecx,ebx			;ECX: g2b2r1g1

	shr	eax,24			;EAX: ------b1
	mov	edx,[esi+8]		;EDX: r3g3b3r2

	shr	ecx,16			;ECX: ----g2b2
	add	edi,16

	shl	ebx,8			;EBX: b2r1g1--
	add	esi,12

	or	eax,ebx			;EAX: b2r1g1b1
	mov	ebx,edx			;EBX: r3g3b3r2

	shr	ebx,8			;EBX: --r3g3b3
	mov	[edi+4-16],eax

	shl	edx,16			;EDX: b3r2----
	mov	[edi+12-16],ebx

	or	edx,ecx			;EDX: b3r2g2b2
	dec	ebp

	mov	[edi+8-16],edx
	jne	DIBconvert2432@x1

DIBconvert2432@x2:
	pop	edx
	mov	ebx,[esp+20+24]
	and	ebx,3
	jz	DIBconvert2432@x4
DIBconvert2432@x3:
	mov	ax,[esi]
	mov	cl,[esi+2]
	mov	[edi],ax
	mov	[edi+2],cl
	add	esi,3
	add	edi,4
	dec	ebx
	jne	DIBconvert2432@x3
DIBconvert2432@x4:

	add	esi,[esp+16+24]
	add	edi,[esp+ 8+24]

	dec	edx
	jne	DIBconvert2432@y

	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp

	ret



;****************************************************************************
;
; void DIBconvert_16_to_24(
;	void *dest,		[ESP+ 4]
;	ulong dest_pitch,	[ESP+ 8]
;	void *src,		[ESP+12]
;	ulong src_pitch,	[ESP+16]
;	ulong width,		[ESP+20]
;	ulong height);		[ESP+24]

_DIBconvert_16_to_24:
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
	mov	eax,ebp
	lea	ebx,[ebp*2]
	add	eax,ebx
	sub	[esp+8+28],eax
	sub	[esp+16+28],ebx

	mov	edx,[esp+24+28]
DIBconvert1624@y:
	mov	ebp,[esp+20+28]
	push	ebp
	push	edx
	shr	ebp,1
	jz	DIBconvert1624@x2
DIBconvert1624@x:
	mov	eax,[esi]
	add	esi,4
	mov	edx,eax
	add	edi,6
	mov	ebx,eax
	mov	ecx,eax
	shl	ebx,9
	and	eax,0000001fh
	shl	ecx,6
	and	ebx,00f80000h
	shl	eax,3
	and	ecx,0000f800h
	or	ebx,ecx
	mov	ecx,edx
	or	eax,ebx
	mov	ebx,edx
	shr	ebx,7
	and	edx,001f0000h
	shr	ecx,10
	and	ebx,00f80000h
	shr	edx,13
	and	ecx,0000f800h
	or	ebx,ecx
	mov	[edi-6],eax
	or	edx,ebx
	dec	ebp
	mov	[edi-3],edx
	jne	DIBconvert1624@x
DIBconvert1624@x2:
	pop	edx
	pop	ebp
	and	ebp,1
	jz	DIBconvert1624@x3

	mov	eax,[esi]
	add	esi,2

	mov	ebx,eax
	mov	ecx,eax
	shl	ebx,9
	shl	ecx,6
	shl	eax,3
	and	ebx,00f80000h
	and	ecx,0000f800h
	and	eax,000000f8h
	or	ebx,ecx
	or	eax,ebx
	mov	[edi],eax
	add	edi,3


DIBconvert1624@x3:

	add	esi,[esp+16+28]
	add	edi,[esp+ 8+28]

	dec	edx
	jne	DIBconvert1624@y

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp

	ret

; void DIBconvert_24_to_24(
;	void *dest,		[ESP+ 4]
;	ulong dest_pitch,	[ESP+ 8]
;	void *src,		[ESP+12]
;	ulong src_pitch,	[ESP+16]
;	ulong width,		[ESP+20]
;	ulong height);		[ESP+24]
;
;	this isn't THAT stupid a function!!

_DIBconvert_24_to_24:
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	cld
	mov	edx,[esp+24+24]

	mov	eax,[esp+20+24]
	mov	esi,[esp+12+24]
	mov	ebx,eax
	mov	edi,[esp+ 4+24]
	add	eax,eax
	add	eax,ebx
	mov	ebx,eax
	shr	eax,2
	and	ebx,3

DIBconvert2424@y:
	push	esi
	push	edi
	mov	ecx,eax
	rep	movsd
	mov	ecx,ebx
	rep	movsb
	pop	edi
	pop	esi
	add	esi,[esp+16+24]
	add	edi,[esp+ 8+24]

	dec	edx
	jne	DIBconvert2424@y

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi

	ret

;**********************************************************
;
; void DIBconvert_16_to_16(
;	void *dest,		[ESP+ 4]
;	ulong dest_pitch,	[ESP+ 8]
;	void *src,		[ESP+12]
;	ulong src_pitch,	[ESP+16]
;	ulong width,		[ESP+20]
;	ulong height);		[ESP+24]
;
;	this isn't THAT stupid a function!!

_DIBconvert_16_to_16:
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	cld
	mov	edx,[esp+24+24]

	mov	eax,[esp+20+24]
	mov	esi,[esp+12+24]
	mov	edi,[esp+ 4+24]

DIBconvert1616@y:
	push	esi
	push	edi
	mov	ecx,eax
	shr	ecx,1
	rep	movsd
	mov	ecx,eax
	and	ecx,1
	add	ecx,ecx
	rep	movsb
	pop	edi
	pop	esi
	add	esi,[esp+16+24]
	add	edi,[esp+ 8+24]

	dec	edx
	jne	DIBconvert1616@y

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi

	ret

;******************************************************
;
; void DIBconvert_24_to_16(
;	void *dest,		[ESP+ 4]
;	ulong dest_pitch,	[ESP+ 8]
;	void *src,		[ESP+12]
;	ulong src_pitch,	[ESP+16]
;	ulong width,		[ESP+20]
;	ulong height);		[ESP+24]

_DIBconvert_24_to_16:
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
	lea	ebx,[ebp+eax]
	sub	[esp+8+28],eax
	sub	[esp+16+28],ebx

	mov	edx,[esp+24+28]
DIBconvert2416@y:
	mov	ebp,[esp+20+28]
	push	ebp
	push	edx
	shr	ebp,1
	jz	DIBconvert2416@x2
DIBconvert2416@x:
	mov	eax,[esi+3]		;u
	add	esi,6			;v

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

	mov	ecx,[esi-6]		;v
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
	jne	DIBconvert2416@x	;v
DIBconvert2416@x2:
	pop	edx
	pop	ebp
	and	ebp,1
	jz	DIBconvert2416@x3
	mov	eax,[esi]
	add	esi,3

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
DIBconvert2416@x3:

	add	esi,[esp+16+28]
	add	edi,[esp+ 8+28]

	dec	edx
	jne	DIBconvert2416@y

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp

	ret

	end

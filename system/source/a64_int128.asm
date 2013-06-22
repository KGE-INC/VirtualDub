;	VirtualDub - Video processing and capture application
;	System library component
;	Copyright (C) 1998-2004 Avery Lee, All Rights Reserved.
;
;	Beginning with 1.6.0, the VirtualDub system library is licensed
;	differently than the remainder of VirtualDub.  This particular file is
;	thus licensed as follows (the "zlib" license):
;
;	This software is provided 'as-is', without any express or implied
;	warranty.  In no event will the authors be held liable for any
;	damages arising from the use of this software.
;
;	Permission is granted to anyone to use this software for any purpose,
;	including commercial applications, and to alter it and redistribute it
;	freely, subject to the following restrictions:
;
;	1.	The origin of this software must not be misrepresented; you must
;		not claim that you wrote the original software. If you use this
;		software in a product, an acknowledgment in the product
;		documentation would be appreciated but is not required.
;	2.	Altered source versions must be plainly marked as such, and must
;		not be misrepresented as being the original software.
;	3.	This notice may not be removed or altered from any source
;		distribution.

		.code

vdasm_int128_add	proc public
		mov		rax, [rdx]
		add		rax, [r8]
		mov		[rcx], rax
		mov		rax, [rdx+8]
		add		rax, [r8+8]
		mov		[rcx+8], rax
		ret
vdasm_int128_add	endp

vdasm_int128_sub	proc public
		mov		rax, [rdx]
		sub		rax, [r8]
		mov		[rcx], rax
		mov		rax, [rdx+8]
		sub		rax, [r8+8]
		mov		[rcx+8], rax
		ret
vdasm_int128_sub	endp

vdasm_int128_mul	proc public
		int		3
		ret
vdasm_int128_mul	endp

vdasm_int128_shl	proc public
		mov		rax, [rdx]
		mov		r9, [rdx+8]
		mov		r10, rcx
		mov		ecx, r8d
		shld	r9, rax, cl
		shl		rax, cl
		mov		[r10], rax
		mov		[r10+8], r9
		ret
vdasm_int128_shl	endp

vdasm_int128_sar	proc public
		mov		r9, [rdx+8]
		mov		r10, rcx
		mov		ecx, r8d
		test	cl, 64
		jnz		@highshift
		mov		rax, [rdx]
		shrd	rax, r9, cl
		sar		r9, cl
		mov		[r10], rax
		mov		[r10+8], r9
		ret
@highshift:
		mov		rax, r9
		sar		r9, 31
		sar		rax, cl
		mov		[r10], rax
		mov		[r10+8], r9
		ret
vdasm_int128_sar	endp

		end

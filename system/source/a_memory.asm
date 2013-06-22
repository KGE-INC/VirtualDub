		.686
		.mmx
		.xmm
		.model		flat
		.code

_VDFastMemcpyPartialMMX		proc	near public
		push	edi
		push	esi

		mov		edi, [esp+4+8]
		mov		esi, [esp+8+8]
		mov		ecx, [esp+12+8]
		mov		edx, ecx
		shr		ecx, 2
		and		edx, 3
		rep		movsd
		mov		ecx, edx
		rep		movsb
		pop		esi
		pop		edi
		ret
_VDFastMemcpyPartialMMX		endp

_VDFastMemcpyPartialMMX2	proc	near public
		push	ebp
		push	edi
		push	esi
		push	ebx

		mov		ebx, [esp+4+16]
		mov		edx, [esp+8+16]
		mov		eax, [esp+12+16]
		neg		eax
		add		eax, 63
		jbe		@skipblastloop
@blastloop:
		movq	mm0, [edx]
		movq	mm1, [edx+8]
		movq	mm2, [edx+16]
		movq	mm3, [edx+24]
		movq	mm4, [edx+32]
		movq	mm5, [edx+40]
		movq	mm6, [edx+48]
		movq	mm7, [edx+56]
		movntq	[ebx], mm0
		movntq	[ebx+8], mm1
		movntq	[ebx+16], mm2
		movntq	[ebx+24], mm3
		movntq	[ebx+32], mm4
		movntq	[ebx+40], mm5
		movntq	[ebx+48], mm6
		movntq	[ebx+56], mm7
		add		ebx, 64
		add		edx, 64
		add		eax, 64
		jnc		@blastloop
@skipblastloop:
		sub		eax, 63-7
		jns		@noextras
@quadloop:
		movq	mm0, [edx]
		movntq	[ebx], mm0
		add		edx, 8
		add		ebx, 8
		add		eax, 8
		jnc		@quadloop
@noextras:
		sub		eax, 7
		jz		@nooddballs
		mov		ecx, eax
		neg		ecx
		mov		esi, edx
		mov		edi, ebx
		rep		movsb
@nooddballs:
		pop		ebx
		pop		esi
		pop		edi
		pop		ebp
		ret
_VDFastMemcpyPartialMMX2	endp


		end


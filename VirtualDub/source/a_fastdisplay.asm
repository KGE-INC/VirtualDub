		.686
		.model	flat
		.xmm
		.mmx
		.code

scaleinfo	struct
dst			dd			?	
src			dd			?
xaccum		dd			?
xfracinc	dd			?
xintinc		dd			?
count		dd			?
scaleinfo	ends

ONENTRY	macro
		push	ebp
		push	edi
		push	esi
		push	ebx
		endm

ONEXIT	macro
		pop		ebx
		pop		esi
		pop		edi
		pop		ebp
		endm

ONENTRY_POINTSCALE	macro
		ONENTRY
		mov		eax, [esp+4+16]

		mov		ebx, [eax].scaleinfo.xaccum
		mov		ecx, [eax].scaleinfo.xfracinc
		mov		edx, [eax].scaleinfo.src
		mov		esi, [eax].scaleinfo.xintinc
		mov		edi, [eax].scaleinfo.dst
		mov		ebp, [eax].scaleinfo.count
		endm

		public	_asm_fastdisp_point16
_asm_fastdisp_point16:
		ONENTRY_POINTSCALE
@@:
		mov		ax,[edx+edx]
		add		ebx,ecx
		adc		edx,esi
		mov		[edi+ebp],ax
		add		ebp,2
		jne		@B

		ONEXIT
		ret		4

		public	_asm_fastdisp_point24
_asm_fastdisp_point24:
		ONENTRY_POINTSCALE
@@:
		mov		al,[edx+edx*2+0]
		mov		[edi+ebp],al
		mov		al,[edx+edx*2+1]
		mov		[edi+ebp+1],al
		mov		al,[edx+edx*2+2]
		mov		[edi+ebp+2],al
		add		ebx,ecx
		adc		edx,esi
		add		ebp,3
		jne		@B

		ONEXIT
		ret		4

		public	_asm_fastdisp_point32
_asm_fastdisp_point32:
		ONENTRY_POINTSCALE
@@:
		mov		eax,[edx*4]
		add		ebx,ecx
		adc		edx,esi
		mov		[edi+ebp],ax
		add		ebp,4
		jne		@B

		ONEXIT
		ret		4

		end

		.686
		.mmx
		.model	flat

		extern		_gVDCaptureAudioResamplingKernel : near

		assume		fs:_DATA

		.const

rounder	dd			00002000h

		.code

_vdasm_capture_resample16_MMX	proc	near public

		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			edx, [esp+4+16]		;edx = destination
		mov			ebp, [esp+8+16]		;ebp = stride
		mov			ecx, [esp+12+16]	;ecx = source
		mov			eax, [esp+16+16]	;eax = counter
		shr			ecx, 1
		add			ebp, ebp			;convert sample stride to pointer stride
		mov			esi, [esp+20+16]	;esi = fractional accumulator
		add			edx, [esp+24+16]	;add integer accumulator
		mov			ebx, [esp+32+16]	;ebx = integer increment

		push		0
		push		fs:dword ptr [0]
		mov			fs:dword ptr [0], esp

		mov			esp, [esp+28+24]	;esp = fractional increment

		movd		mm6, rounder

		;eax		loop counter
		;ebx		integer increment
		;ecx		source
		;edx		destination
		;esi		fractional accumulator
		;edi		current filter
		;esp		fractional increment
		;ebp		destination stride

		mov			edi, esi
		shr			edi, 25
		and			edi, 1f0h
		add			edi, offset _gVDCaptureAudioResamplingKernel

xloop:
		movq		mm0, [ecx+ecx]
		pmaddwd		mm0, [edi]
		movq		mm1, [ecx+ecx+8]
		pmaddwd		mm1, [edi+8]
		paddd		mm0, mm1
		movq		mm1, mm0
		psrlq		mm0, 32
		paddd		mm0, mm1
		paddd		mm0, mm6
		psrad		mm0, 14
		packssdw	mm0, mm0
		movd		edi, mm0
		mov			[edx], di

		add			esi, esp
		adc			ecx, ebx
		mov			edi, esi
		shr			edi, 25
		add			edx, ebp
		and			edi, 1f0h
		add			edi, offset _gVDCaptureAudioResamplingKernel

		sub			eax, 1
		jne			xloop

		mov			esp, fs:dword ptr [0]
		add			esp, 8

		emms
		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret

_vdasm_capture_resample16_MMX	endp

		end

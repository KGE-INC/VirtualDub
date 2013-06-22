	.386
	.model	flat
	.code

	public	_asm_scene_lumtile32
	public	_asm_scene_lumtile24
	public	_asm_scene_lumtile16

;asm_scene_lumtile32(src, width, height, modulo, gtotalptr);

_asm_scene_lumtile32:
	push	ebx
	push	ecx
	push	edx
	push	esi
	push	edi
	push	ebp
	mov	esi,[esp+4+24]
	mov	edi,[esp+12+24]
	xor	eax,eax
	xor	ebx,ebx
asm_scene_lumtile32_row:
	mov	ebp,[esp+8+24]
asm_scene_lumtile32_col:
	mov	ecx,[esi + ebp*4 - 4]
	mov	edx,0000ff00h
	and	edx,ecx
	and	ecx,00ff00ffh
	add	eax,ecx
	add	ebx,edx
	dec	ebp
	jne	asm_scene_lumtile32_col
	add	esi,[esp+16+24]
	dec	edi
	jne	asm_scene_lumtile32_row

	add	eax,00200020h
	add	ebx,00002000h
	shr	eax,6
	and	ebx,003fc000h
	shr	ebx,6
	and	eax,00ff00ffh
	add	eax,ebx

	pop	ebp
	pop	edi
	pop	esi
	pop	edx
	pop	ecx
	pop	ebx
	ret

;asm_scene_lumtile24(src, width, height, modulo, btotalptr);

_asm_scene_lumtile24:
	push	ebx
	push	ecx
	push	edx
	push	esi
	push	edi
	push	ebp
	mov	esi,[esp+4+24]
	mov	edi,[esp+12+24]
	xor	eax,eax
	xor	ebx,ebx
asm_scene_lumtile24_row:
	mov	ebp,[esp+8+24]
	push	esi
asm_scene_lumtile24_col:
	mov	ecx,[esi]
	mov	edx,0000ff00h
	and	edx,ecx
	and	ecx,00ff00ffh
	add	eax,ecx
	add	ebx,edx
	add	esi,3
	dec	ebp
	jne	asm_scene_lumtile24_col
	pop	esi
	add	esi,[esp+16+24]
	dec	edi
	jne	asm_scene_lumtile24_row

	add	eax,00200020h
	add	ebx,00002000h
	shr	eax,6
	and	ebx,003fc000h
	shr	ebx,6
	and	eax,00ff00ffh
	add	eax,ebx

	pop	ebp
	pop	edi
	pop	esi
	pop	edx
	pop	ecx
	pop	ebx
	ret


;asm_scene_lumtile16(src, width, height, modulo, btotalptr);

_asm_scene_lumtile16:
	push	ebx
	push	ecx
	push	edx
	push	esi
	push	edi
	push	ebp
	sub	esp,8
	mov	esi,[esp+4+32]
	mov	edi,[esp+12+32]
	xor	eax,eax
	xor	ebx,ebx
asm_scene_lumtile16_row:
	mov	ebp,[esp+8+32]
asm_scene_lumtile16_col:
	mov	ecx,[esi + ebp*2 - 4]
	mov	edx,03e07c1fh
	and	edx,ecx
	and	ecx,7c1f03e0h
	shr	ecx,5
	add	ebx,edx
	add	eax,ecx
	add	esi,4
	sub	ebp,2
	ja	asm_scene_lumtile16_col

	;	3322222222221111111111
	;	10987654321098765432109876543210
	;eax	<--- g ---><--- r ---><---b --->
	;ebx	<--- r ---><---b ---><--- g --->

	mov	ecx,eax
	mov	edx,ebx

	shr	edx,11
	and	ecx,000001ffh

	and	edx,000001ffh
	mov	ebp,eax

	shl	ebp,6
	add	ecx,edx

	mov	edx,ebx
	and	ebp,07ff0000h

	shr	edx,5
	add	ecx,ebp

	add	ecx,edx
	mov	ebp,eax

	shl	ebx,8
	and	eax,0ffe0000h

	shr	eax,9
	and	ebx,0007ff00h

	add	eax,ebx
	add	esi,[esp+16+32]

	mov	ecx,[esp]
	mov	edx,[esp+4]

	add	ecx,ebp		;red/blue
	add	edx,eax		;green

	mov	[esp],ecx
	mov	[esp+4],edx

	dec	edi
	jne	asm_scene_lumtile16_row

	mov	eax,[esp]
	mov	ebx,[esp+4]

	add	eax,00200020h
	add	ebx,00002000h
	shr	eax,6
	and	ebx,003fc000h
	shr	ebx,6
	and	eax,00ff00ffh
	add	eax,ebx

	add	esp,8
	pop	ebp
	pop	edi
	pop	esi
	pop	edx
	pop	ecx
	pop	ebx
	ret

	end

;	VirtualDub - Video processing and capture application
;	Copyright (C) 1998-2004 Avery Lee
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

;This code is based on the fragments from the Intel Application Note AP-922.
;
;Apologies to Intel; Adobe Acrobat screws up formatting royally.
;
;
;
;IEEE-1180 results:
;	pmse      mse       pme      me
;1	0.018000, 0.013742, 0.003100,-0.000083
;2	0.018600, 0.013573, 0.003300, 0.000217
;3	0.014100, 0.011441, 0.003900, 0.000106
;4	0.017900, 0.013700, 0.004500, 0.000056
;5	0.018300, 0.013623, 0.004900,-0.000239
;6	0.014000, 0.011439, 0.003600,-0.000117



;=============================================================================
;
; These examples contain code fragments for first stage iDCT 8x8
; (for rows) and first stage DCT 8x8 (for columns)
;
;=============================================================================

mword	typedef	qword
mptr	equ	mword ptr

BITS_INV_ACC	= 4			; 4 or 5 for IEEE
SHIFT_INV_ROW	= 16 - BITS_INV_ACC
SHIFT_INV_COL	= 1 + BITS_INV_ACC
RND_INV_ROW	= 1024 * (6 - BITS_INV_ACC) ; 1 << (SHIFT_INV_ROW-1)
RND_INV_COL	= 16 * (BITS_INV_ACC - 3) ; 1 << (SHIFT_INV_COL-1)
RND_INV_CORR	= RND_INV_COL - 1 ; correction -1.0 and round

		.const
		Align	16

rounder			sword	4, 4, 4, 4
				sword	0,0,0,0
round_inv_row	dword	RND_INV_ROW, RND_INV_ROW, RND_INV_ROW, RND_INV_ROW
one_corr		sword	1, 1, 1, 1, 1, 1, 1, 1

round_inv_col	sword	RND_INV_COL, RND_INV_COL, RND_INV_COL, RND_INV_COL, RND_INV_COL, RND_INV_COL, RND_INV_COL, RND_INV_COL
round_inv_corr	sword	RND_INV_CORR, RND_INV_CORR, RND_INV_CORR, RND_INV_CORR, RND_INV_CORR, RND_INV_CORR, RND_INV_CORR, RND_INV_CORR
tg_1_16		sword	13036, 13036, 13036, 13036, 13036, 13036, 13036, 13036	; tg * (2<<16)
tg_2_16		sword	27146, 27146, 27146, 27146, 27146, 27146, 27146, 27146	; tg * (2<<16)
tg_3_16		sword	-21746, -21746, -21746, -21746, -21746, -21746, -21746, -21746	; tg * (2<<16) - 1.0
cos_4_16	sword	-19195, -19195, -19195, -19195, -19195, -19195, -19195, -19195	; cos * (2<<16) - 1.0
ocos_4_16	sword	23170, 23170, 23170, 23170, 23170, 23170, 23170, 23170	; cos * (2<<15) + 0.5
ucos_4_16	sword	46341, 46341, 46341, 46341, 46341, 46341, 46341, 46341	; cos * (2<<16)

jump_tab	qword	tail_inter, tail_intra, tail_mjpeg,0


;=============================================================================
;
; The first stage iDCT 8x8 - inverse DCTs of rows
;
;-----------------------------------------------------------------------------
; The 8-point inverse DCT direct algorithm
;-----------------------------------------------------------------------------
;
; static const short w[32] = {
; FIX(cos_4_16), FIX(cos_2_16), FIX(cos_4_16), FIX(cos_6_16),
; FIX(cos_4_16), FIX(cos_6_16), -FIX(cos_4_16), -FIX(cos_2_16),
; FIX(cos_4_16), -FIX(cos_6_16), -FIX(cos_4_16), FIX(cos_2_16),
; FIX(cos_4_16), -FIX(cos_2_16), FIX(cos_4_16), -FIX(cos_6_16),
; FIX(cos_1_16), FIX(cos_3_16), FIX(cos_5_16), FIX(cos_7_16),
; FIX(cos_3_16), -FIX(cos_7_16), -FIX(cos_1_16), -FIX(cos_5_16),
; FIX(cos_5_16), -FIX(cos_1_16), FIX(cos_7_16), FIX(cos_3_16),
; FIX(cos_7_16), -FIX(cos_5_16), FIX(cos_3_16), -FIX(cos_1_16) };
;
; #define DCT_8_INV_ROW(x, y)
;{
; int a0, a1, a2, a3, b0, b1, b2, b3;
;
; a0 =x[0]*w[0]+x[2]*w[1]+x[4]*w[2]+x[6]*w[3];
; a1 =x[0]*w[4]+x[2]*w[5]+x[4]*w[6]+x[6]*w[7];
; a2 = x[0] * w[ 8] + x[2] * w[ 9] + x[4] * w[10] + x[6] * w[11];
; a3 = x[0] * w[12] + x[2] * w[13] + x[4] * w[14] + x[6] * w[15];
; b0 = x[1] * w[16] + x[3] * w[17] + x[5] * w[18] + x[7] * w[19];
; b1 = x[1] * w[20] + x[3] * w[21] + x[5] * w[22] + x[7] * w[23];
; b2 = x[1] * w[24] + x[3] * w[25] + x[5] * w[26] + x[7] * w[27];
; b3 = x[1] * w[28] + x[3] * w[29] + x[5] * w[30] + x[7] * w[31];
;
; y[0] = SHIFT_ROUND ( a0 + b0 );
; y[1] = SHIFT_ROUND ( a1 + b1 );
; y[2] = SHIFT_ROUND ( a2 + b2 );
; y[3] = SHIFT_ROUND ( a3 + b3 );
; y[4] = SHIFT_ROUND ( a3 - b3 );
; y[5] = SHIFT_ROUND ( a2 - b2 );
; y[6] = SHIFT_ROUND ( a1 - b1 );
; y[7] = SHIFT_ROUND ( a0 - b0 );
;}
;
;-----------------------------------------------------------------------------
;
; In this implementation the outputs of the iDCT-1D are multiplied
; for rows 0,4 - by cos_4_16,
; for rows 1,7 - by cos_1_16,
; for rows 2,6 - by cos_2_16,
; for rows 3,5 - by cos_3_16
; and are shifted to the left for better accuracy
;
; For the constants used,
; FIX(float_const) = (short) (float_const * (1<<15) + 0.5)
;
;=============================================================================

		align 16

; Table for rows 0,4 - constants are multiplied by cos_4_16
; Table for rows 1,7 - constants are multiplied by cos_1_16
; Table for rows 2,6 - constants are multiplied by cos_2_16
; Table for rows 3,5 - constants are multiplied by cos_3_16

tabbase			db		0 dup (?)	;can't do "equ $" because ml64 fscks up on lea instruction

tab_i_04_short	sword  16384,  21407,  16384,   8864,  16384,  -8867,  16384, -21407	; w00 w01 w04 w05 w08 w09 w12 w13
				sword  22725,  19266,  19266,  -4525,  12873, -22725,   4520, -12873	; w16 w17 w20 w21 w24 w25 w28 w29
tab_i_17_short	sword  22725,  29692,  22725,  12295,  22725, -12299,  22725, -29692	; w00 w01 w04 w05 w08 w09 w12 w13
				sword  31521,  26722,  26722,  -6271,  17855, -31521,   6270, -17855	; w16 w17 w20 w21 w24 w25 w28 w29
tab_i_26_short	sword  21407,  27969,  21407,  11587,  21407, -11585,  21407, -27969	; w00 w01 w04 w05 w08 w09 w12 w13
				sword  29692,  25172,  25172,  -5902,  16819, -29692,   5906, -16819	; w16 w17 w20 w21 w24 w25 w28 w29
tab_i_35_short	sword  19266,  25172,  19266,  10426,  19266, -10426,  19266, -25172	; w00 w01 w04 w05 w08 w09 w12 w13
				sword  26722,  22654,  22654,  -5312,  15137, -26722,   5315, -15137	; w16 w17 w20 w21 w24 w25 w28 w29


; Table for rows 0,4 - constants are multiplied by cos_4_16
; Table for rows 1,7 - constants are multiplied by cos_1_16
; Table for rows 2,6 - constants are multiplied by cos_2_16
; Table for rows 3,5 - constants are multiplied by cos_3_16

tab_i_04	sword  16384,  21407, -16387, -21407,  16384,  -8867,  16384,  -8867	; w00 w01 w06 w07 w08 w09 w14 w15
			sword  16384,   8867,  16384,   8864, -16384,  21407,  16384, -21407	; w02 w03 w04 w05 w10 w11 w12 w13
			sword  22725,  19266, -22720, -12873,  12873, -22725,  19266, -22725	; w16 w17 w22 w23 w24 w25 w30 w31
			sword  12873,   4520,  19266,  -4525,   4520,  19266,   4520, -12873	; w18 w19 w20 w21 w26 w27 w28 w29
tab_i_17	sword  22725,  29692, -22729, -29692,  22725, -12299,  22725, -12299	; w00 w01 w06 w07 w08 w09 w14 w15
			sword  22725,  12299,  22725,  12295, -22725,  29692,  22725, -29692	; w02 w03 w04 w05 w10 w11 w12 w13
			sword  31521,  26722, -31520, -17855,  17855, -31521,  26722, -31521	; w16 w17 w22 w23 w24 w25 w30 w31
			sword  17855,   6270,  26722,  -6271,   6270,  26722,   6270, -17855	; w18 w19 w20 w21 w26 w27 w28 w29
tab_i_26	sword  21407,  27969, -21405, -27969,  21407, -11585,  21407, -11585	; w00 w01 w06 w07 w08 w09 w14 w15
			sword  21407,  11585,  21407,  11587, -21407,  27969,  21407, -27969	; w02 w03 w04 w05 w10 w11 w12 w13
			sword  29692,  25172, -29696, -16819,  16819, -29692,  25172, -29692	; w16 w17 w22 w23 w24 w25 w30 w31
			sword  16819,   5906,  25172,  -5902,   5906,  25172,   5906, -16819	; w18 w19 w20 w21 w26 w27 w28 w29
tab_i_35	sword  19266,  25172, -19266, -25172,  19266, -10426,  19266, -10426	; w00 w01 w06 w07 w08 w09 w14 w15
			sword  19266,  10426,  19266,  10426, -19266,  25172,  19266, -25172	; w02 w03 w04 w05 w10 w11 w12 w13
			sword  26722,  22654, -26725, -15137,  15137, -26722,  22654, -26722	; w16 w17 w22 w23 w24 w25 w30 w31
			sword  15137,   5315,  22654,  -5312,   5315,  22654,   5315, -15137	; w18 w19 w20 w21 w26 w27 w28 w29

rowstart_tbl2	dq	dorow_7is
		dq	dorow_6is
		dq	dorow_5is
		dq	dorow_4is
		dq	dorow_3is
		dq	dorow_2is
		dq	dorow_1is
		dq	dorow_0is
		dq	do_dc_sse2

pos_tab	db	 1 dup (8*8)		;pos 0:     DC only
		db	 1 dup (7*8)		;pos 1:     1 AC row
		db	 1 dup (6*8)		;pos 2:     2 AC rows
		db	 6 dup (5*8)		;pos 3-8:   3 AC rows
		db	 1 dup (4*8)		;pos 9:     4 AC rows
		db	10 dup (3*8)		;pos 10-19: 5 AC rows
		db	 1 dup (2*8)		;pos 20:	6 AC rows
		db	14 dup (1*8)		;pos 21-34:	7 AC rows
		db	29 dup (0*8)		;pos 35-63: 8 AC rows


;-----------------------------------------------------------------------------

DCT_8_INV_ROW_1_SSE2 MACRO INP:REQ, OUT:REQ, TABLE:REQ
		movdqa		xmm0, mptr [INP]		;xmm0 = x7 x5 x3 x1 x6 x4 x2 x0
		pshufd		xmm1, xmm0, 00010001b	;xmm1 = x2 x0 x6 x4 x2 x0 x6 x4
		pshufd		xmm2, xmm0, 11101110b	;xmm2 = x7 x5 x3 x1 x7 x5 x3 x1
		pshufd		xmm3, xmm0, 10111011b	;xmm3 = x3 x1 x7 x5 x3 x1 x7 x5
		pshufd		xmm0, xmm0, 01000100b	;xmm0 = x6 x4 x2 x0 x6 x4 x2 x0

		pmaddwd		xmm0, mptr [TABLE+00h]
		pmaddwd		xmm1, mptr [TABLE+10h]
		pmaddwd		xmm2, mptr [TABLE+20h]
		pmaddwd		xmm3, mptr [TABLE+30h]

		paddd		xmm0, mptr [rax + (round_inv_row - tabbase)]
		paddd		xmm0, xmm1				;xmm0 = y3 y2 y1 y0
		paddd		xmm2, xmm3				;xmm2 = y4 y5 y6 y7
		movdqa		xmm1, xmm0
		paddd		xmm0, xmm2				;xmm0 = z3 z2 z1 z0
		psubd		xmm1, xmm2				;xmm1 = z4 z5 z6 z7
		psrad		xmm0, SHIFT_INV_ROW
		psrad		xmm1, SHIFT_INV_ROW
		packssdw	xmm0, xmm1
		pshufhw		xmm0, xmm0, 00011011b

		movdqa		mptr [OUT], xmm0
ENDM

DCT_8_INV_ROW_1_SSE2_SHORT MACRO INP:REQ, OUT:REQ, TABLE:REQ
		movdqa		xmm0, mptr [INP]		;xmm0 = -- -- x3 x1 -- -- x2 x0
		pshufd		xmm2, xmm0, 10101010b	;xmm2 = x3 x1 x3 x1 x3 x1 x3 x1
		pshufd		xmm0, xmm0, 00000000b	;xmm0 = x2 x0 x2 x0 x2 x0 x2 x0
		pmaddwd		xmm0, mptr [TABLE+00h]	;xmm0 = y3 y2 y1 y0
		pmaddwd		xmm2, mptr [TABLE+10h]	;xmm2 = y4 y5 y6 y7
		paddd		xmm0, mptr [rax + (round_inv_row - tabbase)]
		movdqa		xmm1, xmm0
		paddd		xmm0, xmm2				;xmm0 = z3 z2 z1 z0
		psubd		xmm1, xmm2				;xmm1 = z4 z5 z6 z7
		psrad		xmm0, SHIFT_INV_ROW
		psrad		xmm1, SHIFT_INV_ROW
		packssdw	xmm0, xmm1
		pshufhw		xmm0, xmm0, 00011011b

		movdqa		mptr [OUT], xmm0
ENDM


;=============================================================================
;
; The second stage iDCT 8x8 - inverse DCTs of columns
;
; The outputs are premultiplied
; for rows 0,4 - on cos_4_16,
; for rows 1,7 - on cos_1_16,
; for rows 2,6 - on cos_2_16,
; for rows 3,5 - on cos_3_16
; and are shifted to the left for rise of accuracy
;
;-----------------------------------------------------------------------------
;
; The 8-point scaled inverse DCT algorithm (26a8m)
;
;-----------------------------------------------------------------------------
;
;	// Reorder and prescale (implicit)
;
;	ev0 = co[0] / 2.0;
;	ev1 = co[2] / 2.0;
;	ev2 = co[4] / 2.0;
;	ev3 = co[6] / 2.0;
;	od0 = co[1] / 2.0;
;	od1 = co[3] / 2.0;
;	od2 = co[5] / 2.0;
;	od3 = co[7] / 2.0;
;
;	// 5) Apply D8T (implicit in row calculation).
;
;	tmp[0] = ev0*LAMBDA(4);
;	tmp[1] = ev2*LAMBDA(4);
;	tmp[2] = ev1*LAMBDA(2);
;	tmp[3] = ev3*LAMBDA(2);
;	tmp[4] = od0*LAMBDA(1);
;	tmp[5] = od3*LAMBDA(1);
;	tmp[6] = od1*LAMBDA(3);
;	tmp[7] = od2*LAMBDA(3);
;
;	// 4) Apply B8T.
;
;	double x0, x1, x2, x3, y0, y1, y2, y3;
;
;	x0 = tmp[0] + tmp[1];
;	x1 = tmp[0] - tmp[1];
;	x2 = tmp[2] + TAN(2)*tmp[3];
;	x3 = tmp[2]*TAN(2) - tmp[3];
;	y0 = tmp[4] + TAN(1)*tmp[5];
;	y1 = tmp[4]*TAN(1) - tmp[5];
;	y2 = tmp[6] + TAN(3)*tmp[7];
;	y3 = tmp[6]*TAN(3) - tmp[7];
;
;	// 3) Apply E8T.
;	//
;	//	1  0  1  0
;	//	0  1  0  1
;	//	0  1  0 -1
;	//	1  0 -1  0
;	//		    1  0  1  0
;	//		    1  0 -1  0
;	//		    0  1  0  1
;	//		    0  1  0 -1
;
;	double e0, e1, e2, e3, o0, o1, o2, o3;
;
;	e0 = x0 + x2;
;	e1 = x1 + x3;
;	e2 = x1 - x3;
;	e3 = x0 - x2;
;	o0 = y0 + y2;
;	o1 = y0 - y2;
;	o2 = y1 + y3;
;	o3 = y1 - y3;
;
;	// 2) Apply F8T.
;
;	double a, b;
;
;	a = (o1 + o2) * LAMBDA(4);
;	b = (o1 - o2) * LAMBDA(4);
;
;	o1 = a;
;	o2 = b;
;
;	// 6) Apply output butterfly (A8T).
;	//
;	// 1 0 0 0  1  0  0  0
;	// 0 1 0 0  0  1  0  0
;	// 0 0 1 0  0  0  1  0
;	// 0 0 0 1  0  0  0  1
;	// 0 0 0 1  0  0  0 -1
;	// 0 0 1 0  0  0 -1  0
;	// 0 1 0 0  0 -1  0  0
;	// 1 0 0 0 -1  0  0  0
;
;	out[0] = e0 + o0;
;	out[1] = e1 + o1;
;	out[2] = e2 + o2;
;	out[3] = e3 + o3;
;	out[4] = e3 - o3;
;	out[5] = e2 - o2;
;	out[6] = e1 - o1;
;	out[7] = e0 - o0;
;
;=============================================================================
DCT_8_INV_COL MACRO INP:REQ, OUT:REQ
LOCAL	x0, x1, x2, x3, x4, x5, x6, x7
LOCAL	y0, y1, y2, y3, y4, y5, y6, y7

x0	equ	[INP + 0*16]
x1	equ	[INP + 1*16]
x2	equ	[INP + 2*16]
x3	equ	[INP + 3*16]
x4	equ	[INP + 4*16]
x5	equ	[INP + 5*16]
x6	equ	[INP + 6*16]
x7	equ	[INP + 7*16]
y0	equ	[OUT + 0*16]
y1	equ	[OUT + 1*16]
y2	equ	[OUT + 2*16]
y3	equ	[OUT + 3*16]
y4	equ	[OUT + 4*16]
y5	equ	[OUT + 5*16]
y6	equ	[OUT + 6*16]
y7	equ	[OUT + 7*16]

	;======= optimized code

	;ODD ELEMENTS

	movdqa	xmm0,mptr [rax + (tg_1_16 - tabbase)]

	movdqa	xmm2,mptr [rax + (tg_3_16 - tabbase)]
	movdqa	xmm3,xmm0

	movdqa	xmm1,x7
	movdqa	xmm6,xmm2

	movdqa	xmm4,x5
	pmulhw	xmm0,xmm1

	movdqa	xmm5,x1
	pmulhw	xmm2,xmm4

	movdqa	xmm7,x3
	pmulhw	xmm3,xmm5

	pmulhw	xmm6,xmm7
	paddw	xmm0,xmm5

	paddw	xmm2,xmm4
	psubw	xmm6,xmm4

	paddw	xmm2,xmm7
	psubw	xmm3,xmm1

	paddw	xmm0,mptr [rax + (one_corr - tabbase)]
	paddw	xmm6,xmm7

	;E8T butterfly - odd elements

	movdqa	xmm1,xmm0
	movdqa	xmm5,xmm3
	paddw	xmm0,xmm2		;xmm0 = o0 = y0 + y2
	psubw	xmm1,xmm2		;xmm1 = o1 = y0 - y2
	paddw	xmm3,xmm6		;xmm3 = o2 = y1 + y3
	psubw	xmm5,xmm6		;xmm5 = o3 = y1 - y3

	;F8T stage - odd elements

	movdqa	xmm2,xmm1
	paddw	xmm1,xmm3			;[F8T] xmm1 = o1 + o2

	movdqa	xmm4,x0			;[B8T1] xmm3 = tmp[0]
	psubw	xmm2,xmm3			;[F8T] xmm2 = o1 - o2

	movdqa	xmm3,x4
	movdqa	xmm6,xmm2			;[F8T]

	pmulhw	xmm2,qword ptr [rax + (cos_4_16 - tabbase)]	;[F8T]
	movdqa	xmm7,xmm1			;[F8T]

	pmulhw	xmm1,qword ptr [rax + (cos_4_16 - tabbase)]	;[F8T]
	paddw	xmm3,xmm4			;[B8T1] xmm3 = x0 = tmp[0] + tmp[1]

	paddw	xmm3,mptr [rax + (round_inv_corr - tabbase)]	;[E8T]
	;<v-pipe>

	psubw	xmm4,x4			;[B8T1] xmm4 = x1 = tmp[0] - tmp[1]
	paddw	xmm2,xmm6			;[F8T]

	por	xmm1,mword ptr one_corr	;[F8T]
	;<v-pipe>

	movdqa	xmm6,x6			;[B8T2] xmm7 = tmp[3]
	paddw	xmm1,xmm7			;[F8T] xmm1 = o1' = (o1 + o2)*LAMBDA(4)

	pmulhw	xmm6,qword ptr [rax + (tg_2_16 - tabbase)]	;xmm7 = tmp[3] * TAN(2)
	;<v-pipe>

	movdqa	xmm7,mptr [rax + (one_corr - tabbase)]
	;<v-pipe>

	paddw	xmm4,mptr [rax + (round_inv_col - tabbase)]	;[out]
	psubw	xmm2,xmm7			;[F8T] xmm2 = o2' = (o1 - o2)*LAMBDA(4)

	paddw	xmm6,x2			;[B8T2] xmm7 = x2 = tmp[2] + tmp[3]*TAN(2)
	paddw	xmm7,xmm3			;[E8T]

	paddw	xmm7,xmm6			;[E8T] xmm6 = e0 = x0+x2
	psubw	xmm3,xmm6			;[E8T] xmm3 = e3 = x0-x2

	;output butterfly - 0 and 3

	movdqa	xmm6,xmm7		;xmm7 = e0
	paddw	xmm7,xmm0		;xmm6 = e0 + o0

	psubw	xmm6,xmm0		;xmm7 = e0 - o0
	psraw	xmm7,SHIFT_INV_COL

	movdqa	xmm0,xmm3		;xmm7 = e3 
	psraw	xmm6,SHIFT_INV_COL

	movdqa	y0,xmm7
	paddw	xmm3,xmm5		;xmm6 = e3 + o3

	movdqa	xmm7,x2		;[B8T] xmm6 = tmp[2]
	psubw	xmm0,xmm5		;[out] xmm7 = e3 - o3

	movdqa	y7,xmm6
	psraw	xmm3,SHIFT_INV_COL

	pmulhw	xmm7,qword ptr [rax + (tg_2_16 - tabbase)]		;[B8T] xmm6 = tmp[2] * TAN(2)
	psraw	xmm0,SHIFT_INV_COL

	movdqa	y3,xmm3
	movdqa	xmm6,xmm4				;[E8T]

	psubw	xmm6,mptr [rax + (one_corr - tabbase)]
	;<v-pipe>


	;B8T stage - x3 element
	;
	;free registers: 03567

	psubw	xmm7,x6				;[B8T] xmm6 = x3 = tmp[2]*TAN(2) - tmp[3]
	movdqa	xmm3,xmm1

	;E8T stage - x1 and x3 elements

	movdqa	y4,xmm0
	paddw	xmm4,xmm7				;[E8T] xmm4 = e1 = x1+x3

	psubw	xmm6,xmm7				;[E8T] xmm7 = e2 = x1-x3
	paddw	xmm3,xmm4				;xmm3 = e1 + o1

	psubw	xmm4,xmm1				;xmm4 = e1 - o1
	psraw	xmm3,SHIFT_INV_COL

	movdqa	xmm5,xmm6				;xmm6 = e2
	psraw	xmm4,SHIFT_INV_COL

	paddw	xmm6,xmm2				;xmm7 = e2 + o2
	psubw	xmm5,xmm2				;xmm6 = e2 - o2

	movdqa	y1,xmm3
	psraw	xmm6,SHIFT_INV_COL

	movdqa	y6,xmm4
	psraw	xmm5,SHIFT_INV_COL

	movdqa	y2,xmm6

	movdqa	y5,xmm5

ENDM

;-----------------------------------------------------------------------------
;
; The half 8-point scaled inverse DCT algorithm (26a8m)
;
;-----------------------------------------------------------------------------
;
;	// Reorder and prescale (implicit)
;
;	ev0 = co[0] / 2.0;
;	ev1 = co[2] / 2.0;
;	od0 = co[1] / 2.0;
;	od1 = co[3] / 2.0;
;
;	// 5) Apply D8T (implicit in row calculation).
;
;	tmp[0] = ev0*LAMBDA(4);
;	tmp[2] = ev1*LAMBDA(2);
;	tmp[4] = od0*LAMBDA(1);
;	tmp[6] = od1*LAMBDA(3);
;
;	// 4) Apply B8T.
;
;	double x0, x1, x2, x3, y0, y1, y2, y3;
;
;	x0 = tmp[0];
;	x1 = tmp[0];
;	x2 = tmp[2];
;	x3 = tmp[2]*TAN(2);
;	y0 = tmp[4];
;	y1 = tmp[4]*TAN(1);
;	y2 = tmp[6];
;	y3 = tmp[6]*TAN(3);
;
;	// 3) Apply E8T.
;	//
;	//	1  0  1  0
;	//	0  1  0  1
;	//	0  1  0 -1
;	//	1  0 -1  0
;	//		    1  0  1  0
;	//		    1  0 -1  0
;	//		    0  1  0  1
;	//		    0  1  0 -1
;
;	double e0, e1, e2, e3, o0, o1, o2, o3;
;
;	e0 = x0 + x2;
;	e1 = x1 + x3;
;	e2 = x1 - x3;
;	e3 = x0 - x2;
;	o0 = y0 + y2;
;	o1 = y0 - y2;
;	o2 = y1 + y3;
;	o3 = y1 - y3;
;
;	// 2) Apply F8T.
;
;	double a, b;
;
;	a = (o1 + o2) * LAMBDA(4);
;	b = (o1 - o2) * LAMBDA(4);
;
;	o1 = a;
;	o2 = b;
;
;	// 6) Apply output butterfly (A8T).
;	//
;	// 1 0 0 0  1  0  0  0
;	// 0 1 0 0  0  1  0  0
;	// 0 0 1 0  0  0  1  0
;	// 0 0 0 1  0  0  0  1
;	// 0 0 0 1  0  0  0 -1
;	// 0 0 1 0  0  0 -1  0
;	// 0 1 0 0  0 -1  0  0
;	// 1 0 0 0 -1  0  0  0
;
;	out[0] = e0 + o0;
;	out[1] = e1 + o1;
;	out[2] = e2 + o2;
;	out[3] = e3 + o3;
;	out[4] = e3 - o3;
;	out[5] = e2 - o2;
;	out[6] = e1 - o1;
;	out[7] = e0 - o0;
;
;=============================================================================

DCT_8_INV_COL_SHORT MACRO INP:REQ, OUT:REQ
LOCAL	x0, x1, x2, x3, x4, x5, x6, x7
LOCAL	y0, y1, y2, y3, y4, y5, y6, y7

x0	equ	[INP + 0*16]
x1	equ	[INP + 1*16]
x2	equ	[INP + 2*16]
x3	equ	[INP + 3*16]
y0	equ	[OUT + 0*16]
y1	equ	[OUT + 1*16]
y2	equ	[OUT + 2*16]
y3	equ	[OUT + 3*16]
y4	equ	[OUT + 4*16]
y5	equ	[OUT + 5*16]
y6	equ	[OUT + 6*16]
y7	equ	[OUT + 7*16]

	;======= optimized code

	;ODD ELEMENTS

	movdqa	xmm3,qword ptr [rax + (tg_1_16 - tabbase)]

	movdqa	xmm0,x1
	movdqa	xmm6,qword ptr [rax + (tg_3_16 - tabbase)]

	movdqa	xmm2,x3
	pmulhw	xmm3,xmm0

	pmulhw	xmm6,xmm2

	paddw	xmm0,qword ptr [rax + (one_corr - tabbase)]
	paddw	xmm6,xmm2

	;E8T butterfly - odd elements

	movdqa	xmm1,xmm0
	movdqa	xmm5,xmm3
	paddw	xmm0,xmm2		;xmm0 = o0 = y0 + y2
	psubw	xmm1,xmm2		;xmm1 = o1 = y0 - y2
	paddw	xmm3,xmm6		;xmm3 = o2 = y1 + y3
	psubw	xmm5,xmm6		;xmm5 = o3 = y1 - y3

	;F8T stage - odd elements

	movdqa	xmm2,xmm1
	paddw	xmm1,xmm3					;[F8T] xmm1 = o1 + o2

	movdqa	xmm4,x0					;[B8T1] xmm4 = x0 = x1 = tmp[0]
	psubw	xmm2,xmm3					;[F8T] xmm2 = o1 - o2

	movdqa	xmm6,xmm2					;[F8T]
	movdqa	xmm7,xmm1					;[F8T]

	pmulhw	xmm2,qword ptr [rax + (cos_4_16 - tabbase)]	;[F8T]
	movdqa	xmm3,xmm4					;[B8T1] xmm3 = x0 = x1 = tmp[0]

	pmulhw	xmm1,qword ptr [rax + (cos_4_16 - tabbase)]	;[F8T]

	paddw	xmm3,mptr [rax + (round_inv_corr - tabbase)]	;[E8T]
	;<v-pipe>

	paddw	xmm4,qword ptr [rax + (round_inv_col - tabbase)]	;[out]
	paddw	xmm2,xmm6					;[F8T]

	por		xmm1,qword ptr [rax + (one_corr - tabbase)]	;[F8T]
	;<v-pipe>

	psubw	xmm2,qword ptr [rax + (one_corr - tabbase)]		;[F8T] xmm2 = o2' = (o1 - o2)*LAMBDA(4)
	paddw	xmm1,xmm7					;[F8T] xmm1 = o1' = (o1 + o2)*LAMBDA(4)

	movdqa	xmm6,x2					;[B8T2] xmm7 = x2 = tmp[2]
	movdqa	xmm7,xmm4					;[E8T]

	paddw	xmm7,xmm6					;[E8T] xmm6 = e0 = x0+x2
	psubw	xmm3,xmm6					;[E8T] xmm3 = e3 = x0-x2

	;output butterfly - 0 and 3

	movdqa	xmm6,xmm7		;xmm7 = e0
	paddw	xmm7,xmm0		;xmm6 = e0 + o0

	psubw	xmm6,xmm0		;xmm7 = e0 - o0
	psraw	xmm7,SHIFT_INV_COL

	movdqa	xmm0,xmm3		;xmm7 = e3 
	psraw	xmm6,SHIFT_INV_COL

	movdqa	y0,xmm7
	paddw	xmm3,xmm5		;xmm6 = e3 + o3

	movdqa	xmm7,x2		;[B8T] xmm6 = tmp[2]
	psubw	xmm0,xmm5		;[out] xmm7 = e3 - o3

	movdqa	y7,xmm6
	psraw	xmm3,SHIFT_INV_COL

	pmulhw	xmm7,qword ptr [rax + (tg_2_16 - tabbase)]		;[B8T] xmm6 = x3 = tmp[2] * TAN(2)
	psraw	xmm0,SHIFT_INV_COL

	movdqa	y3,xmm3
	movdqa	xmm6,xmm4				;[E8T]

	psubw	xmm6,mptr [rax + (one_corr - tabbase)]
	;<v-pipe>


	;B8T stage - x3 element
	;
	;free registers: 03567

	movdqa	xmm3,xmm1

	;E8T stage - x1 and x3 elements

	movdqa	y4,xmm0
	paddw	xmm4,xmm7				;[E8T] xmm4 = e1 = x1+x3

	psubw	xmm6,xmm7				;[E8T] xmm7 = e2 = x1-x3
	paddw	xmm3,xmm4				;xmm3 = e1 + o1

	psubw	xmm4,xmm1				;xmm4 = e1 - o1
	psraw	xmm3,SHIFT_INV_COL

	movdqa	xmm5,xmm6				;xmm6 = e2
	psraw	xmm4,SHIFT_INV_COL

	paddw	xmm6,xmm2				;xmm7 = e2 + o2
	psubw	xmm5,xmm2				;xmm6 = e2 - o2

	movdqa	y1,xmm3
	psraw	xmm6,SHIFT_INV_COL

	movdqa	y6,xmm4
	psraw	xmm5,SHIFT_INV_COL

	movdqa	y2,xmm6

	movdqa	y5,xmm5
ENDM

;==========================================================================

	.code

	public IDCT_sse2


IDCT_sse2:
	movlhps	xmm14, xmm6
	movlhps	xmm15, xmm7
	movsxd	r9, r9d
	movsxd	r10, dword ptr [rsp+40]
	lea		rax, tabbase
	movzx	r11,byte ptr [rax+r10+(pos_tab - tabbase)]

	jmp		qword ptr [rax+r11+(rowstart_tbl2 - tabbase)]

	align	16
dorow_3is:
	DCT_8_INV_ROW_1_SSE2_SHORT	rcx+3*16, rcx+3*16, rax + (tab_i_35_short - tabbase)
dorow_2is:
	DCT_8_INV_ROW_1_SSE2_SHORT	rcx+2*16, rcx+2*16, rax + (tab_i_26_short - tabbase)
dorow_1is:
	DCT_8_INV_ROW_1_SSE2_SHORT	rcx+1*16, rcx+1*16, rax + (tab_i_17_short - tabbase)
dorow_0is:
	DCT_8_INV_ROW_1_SSE2_SHORT	rcx+0*16, rcx+0*16, rax + (tab_i_04_short - tabbase)

	DCT_8_INV_COL_SHORT		rcx, rcx

	movhlps	xmm6, xmm14
	movhlps	xmm7, xmm15
	jmp	qword ptr [rax + (jump_tab - tabbase) + r9*8]

	align	16
dorow_7is:
		DCT_8_INV_ROW_1_SSE2	rcx+7*16, rcx+7*16, rax + (tab_i_17 - tabbase)
dorow_6is:
		DCT_8_INV_ROW_1_SSE2	rcx+6*16, rcx+6*16, rax + (tab_i_26 - tabbase)
dorow_5is:
		DCT_8_INV_ROW_1_SSE2	rcx+5*16, rcx+5*16, rax + (tab_i_35 - tabbase)
dorow_4is:
		DCT_8_INV_ROW_1_SSE2	rcx+4*16, rcx+4*16, rax + (tab_i_04 - tabbase)
		DCT_8_INV_ROW_1_SSE2	rcx+3*16, rcx+3*16, rax + (tab_i_35 - tabbase)
		DCT_8_INV_ROW_1_SSE2	rcx+2*16, rcx+2*16, rax + (tab_i_26 - tabbase)
		DCT_8_INV_ROW_1_SSE2	rcx+1*16, rcx+1*16, rax + (tab_i_17 - tabbase)
		DCT_8_INV_ROW_1_SSE2	rcx+0*16, rcx+0*16, rax + (tab_i_04 - tabbase)

	DCT_8_INV_COL		rcx, rcx

	movhlps	xmm6, xmm14
	movhlps	xmm7, xmm15
	jmp	qword ptr [rax + (jump_tab - tabbase) + r9*8]




	align 16

tail_intra:
	xor			rax,rax
	sub			rax,8*16				;ml64 truncates all constants on mov rax,imm64 to 32-bit unsigned... f*cking useless assembler
intra_loop:
	movdqa		xmm0,[rcx+rax+8*16]
	packuswb	xmm0,xmm0
	movq		qword ptr [rdx],xmm0
	add			rdx, r8
	add			rax, 16
	jne			intra_loop
tail_mjpeg:
	ret

	align		16
tail_inter:
	xor			rax,rax
	sub			rax,8*16
	pxor		xmm4, xmm4
inter_loop:
	movdqa		xmm0, [rcx+rax+8*16]
	movq		xmm2, qword ptr [rdx]
	punpcklbw	xmm2, xmm4
	paddw		xmm0, xmm2
	packuswb	xmm0, xmm0
	movq		qword ptr [rdx], xmm0
	add			rdx, r8
	add			rax, 16
	jne			inter_loop
	ret

;--------------------------------------------------------------------------

do_dc_sse2:
	pshuflw		xmm0, [rcx], 0
	pxor		xmm1, xmm1
	paddw		xmm0, rounder
	psraw		xmm0, 3
	pshufd		xmm0, xmm0, 0
	cmp			r9d, 1
	jb			do_ac_sse2
	ja			do_dc_mjpeg
	packuswb	xmm0,xmm0

	movq		qword ptr [rdx],xmm0		;row 0
	lea			rax,[r8+r8*2]
	movq		qword ptr [rdx+r8],xmm0		;row 1
	add			rax,rdx
	movq		qword ptr [rdx+r8*2],xmm0		;row 2
	lea			rdx,[rdx+r8*2]
	movq		qword ptr [rax],xmm0			;row 3
	movq		qword ptr [rdx+r8*2],xmm0		;row 4
	movq		qword ptr [rax+r8*2],xmm0		;row 5
	movq		qword ptr [rdx+r8*4],xmm0		;row 6
	movq		qword ptr [rax+r8*4],xmm0		;row 7
	ret

	align		16
do_ac_sse2:
	psubw		xmm1,xmm0
	packuswb	xmm0,xmm0			;mm0 = adder
	packuswb	xmm1,xmm1			;mm1 = subtractor

	mov		rax,8
do_ac_sse2@loop:
	movq		xmm2, qword ptr [rdx]
	paddusb		xmm2,xmm0
	psubusb		xmm2,xmm1
	movq		qword ptr [rdx],xmm2
	add			rdx,r8
	sub			rax,1
	jne			do_ac_sse2@loop
	ret

do_dc_mjpeg:
	pshufd		xmm0, xmm0, 0
	movdqa		qword ptr [rdx],xmm0			;row 0
	lea			rax,[rcx+r8*2]
	movdqa		qword ptr [rdx+r8],xmm0		;row 1
	add			rax,rdx
	movdqa		qword ptr [rdx+r8*2],xmm0		;row 2
	lea			rdx,[rdx+r8*2]
	movdqa		qword ptr [rax],xmm0			;row 3
	movdqa		qword ptr [rdx+r8*2],xmm0		;row 4
	movdqa		qword ptr [rax+r8*2],xmm0		;row 5
	movdqa		qword ptr [rdx+r8*4],xmm0		;row 6
	movdqa		qword ptr [rax+r8*4],xmm0		;row 7
	ret

	end

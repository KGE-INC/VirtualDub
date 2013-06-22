const unsigned char mpeg_dctcoeff_decodeL1_1[8][4]={
	 7, 1,7,+0,  7, 1,7,-1,  6, 1,7,+0,  6, 1,7,-1,  1, 2,7,+0,  1, 2,7,-1,  5, 1,7,+0,  5, 1,7,-1,
};

const unsigned char mpeg_dctcoeff_decodeL1_2[32][4]={
	13, 1,9,+0, 13, 1,9,-1,  0, 6,9,+0,  0, 6,9,-1, 12, 1,9,+0, 12, 1,9,-1, 11, 1,9,+0, 11, 1,9,-1,
	 3, 2,9,+0,  3, 2,9,-1,  1, 3,9,+0,  1, 3,9,-1,  0, 5,9,+0,  0, 5,9,-1, 10, 1,9,+0, 10, 1,9,-1,
	 0, 3,6,+0,  0, 3,6,+0,  0, 3,6,+0,  0, 3,6,+0,  0, 3,6,+0,  0, 3,6,+0,  0, 3,6,+0,  0, 3,6,+0,
	 0, 3,6,-1,  0, 3,6,-1,  0, 3,6,-1,  0, 3,6,-1,  0, 3,6,-1,  0, 3,6,-1,  0, 3,6,-1,  0, 3,6,-1,
};

const unsigned char mpeg_dctcoeff_decodeL1_3[4][4]={ 4, 1,6,+0,  4, 1,6,-1,  3, 1,6,+0,  3, 1,6,-1,};
const unsigned char mpeg_dctcoeff_decodeL1_4[2][4]={ 0, 2,5,+0,  0, 2,5,-1,};
const unsigned char mpeg_dctcoeff_decodeL1_5[2][4]={ 2, 1,5,+0,  2, 1,5,-1,};
const unsigned char mpeg_dctcoeff_decodeL1_6[1][4]={ 1, 1,4,+0,};
const unsigned char mpeg_dctcoeff_decodeL1_7[1][4]={ 1, 1,4,-1,};
const unsigned char mpeg_dctcoeff_decodeL1_8[1][4]={ 0, 0,2,+0,};
const unsigned char mpeg_dctcoeff_decodeL1_C[1][4]={ 0, 1,3,+0,};
const unsigned char mpeg_dctcoeff_decodeL1_E[1][4]={ 0, 1,3,-1,};

const struct { const unsigned char (*tbl)[4]; int bits; } mpeg_dctcoeff_decodeL1[16]={
	{ NULL, 0 },
	{ mpeg_dctcoeff_decodeL1_1, 6-3 },
	{ mpeg_dctcoeff_decodeL1_2, 6-5 },
	{ mpeg_dctcoeff_decodeL1_3, 6-2 },
	{ mpeg_dctcoeff_decodeL1_4, 6-1 },
	{ mpeg_dctcoeff_decodeL1_5, 6-1 },
	{ mpeg_dctcoeff_decodeL1_6, 6-0 },
	{ mpeg_dctcoeff_decodeL1_7, 6-0 },
	{ mpeg_dctcoeff_decodeL1_8, 6-0 },
	{ mpeg_dctcoeff_decodeL1_8, 6-0 },
	{ mpeg_dctcoeff_decodeL1_8, 6-0 },
	{ mpeg_dctcoeff_decodeL1_8, 6-0 },
	{ mpeg_dctcoeff_decodeL1_C, 6-0 },
	{ mpeg_dctcoeff_decodeL1_C, 6-0 },
	{ mpeg_dctcoeff_decodeL1_E, 6-0 },
	{ mpeg_dctcoeff_decodeL1_E, 6-0 },
};

const unsigned char mpeg_dctcoeff_decodeL0[10][4]={
{ 1, 1,4,+0,},
{ 1, 1,4,-1,},
{ 0, 0,2,+0,},
{ 0, 0,2,+0,},
{ 0, 0,2,+0,},
{ 0, 0,2,+0,},
{ 0, 1,3,+0,},
{ 0, 1,3,+0,},
{ 0, 1,3,-1,},
{ 0, 1,3,-1,},
};

const unsigned char mpeg_dctcoeff_decodeL2_00[128][4]={
	 0, 0,13,+0,  0, 0,13,+0,  0, 0,13,+0,  0, 0,13,+0,  0, 0,13,+0,  0, 0,13,+0,  0, 0,13,+0,  0, 0,13,+0,
	 0, 0,13,+0,  0, 0,13,+0,  0, 0,13,+0,  0, 0,13,+0,  0, 0,13,+0,  0, 0,13,+0,  0, 0,13,+0,  0, 0,13,+0,
	 0, 0,13,+0,  0, 0,13,+0,  0, 0,13,+0,  0, 0,13,+0,  0, 0,13,+0,  0, 0,13,+0,  0, 0,13,+0,  0, 0,13,+0,
	 0, 0,13,+0,  0, 0,13,+0,  0, 0,13,+0,  0, 0,13,+0,  0, 0,13,+0,  0, 0,13,+0,  0, 0,13,+0,  0, 0,13,+0,
	 1,18,17,+0,  1,18,17,-1,  1,17,17,+0,  1,17,17,-1,  1,16,17,+0,  1,16,17,-1,  1,15,17,+0,  1,15,17,-1,
	 6, 3,17,+0,  6, 3,17,-1, 16, 2,17,+0, 16, 2,17,-1, 15, 2,17,+0, 15, 2,17,-1, 14, 2,17,+0, 14, 2,17,-1,
	13, 2,17,+0, 13, 2,17,-1, 12, 2,17,+0, 12, 2,17,-1, 11, 2,17,+0, 11, 2,17,-1, 31, 1,17,+0, 31, 1,17,-1,
	30, 1,17,+0, 30, 1,17,-1, 29, 1,17,+0, 29, 1,17,-1, 28, 1,17,+0, 28, 1,17,-1, 27, 1,17,+0, 27, 1,17,-1,
	 0,40,16,+0,  0,40,16,+0,  0,40,16,-1,  0,40,16,-1,  0,39,16,+0,  0,39,16,+0,  0,39,16,-1,  0,39,16,-1,
	 0,38,16,+0,  0,38,16,+0,  0,38,16,-1,  0,38,16,-1,  0,37,16,+0,  0,37,16,+0,  0,37,16,-1,  0,37,16,-1,
	 0,36,16,+0,  0,36,16,+0,  0,36,16,-1,  0,36,16,-1,  0,35,16,+0,  0,35,16,+0,  0,35,16,-1,  0,35,16,-1,
	 0,34,16,+0,  0,34,16,+0,  0,34,16,-1,  0,34,16,-1,  0,33,16,+0,  0,33,16,+0,  0,33,16,-1,  0,33,16,-1,
	 0,32,16,+0,  0,32,16,+0,  0,32,16,-1,  0,32,16,-1,  1,14,16,+0,  1,14,16,+0,  1,14,16,-1,  1,14,16,-1,
	 1,13,16,+0,  1,13,16,+0,  1,13,16,-1,  1,13,16,-1,  1,12,16,+0,  1,12,16,+0,  1,12,16,-1,  1,12,16,-1,
	 1,11,16,+0,  1,11,16,+0,  1,11,16,-1,  1,11,16,-1,  1,10,16,+0,  1,10,16,+0,  1,10,16,-1,  1,10,16,-1,
	 1, 9,16,+0,  1, 9,16,+0,  1, 9,16,-1,  1, 9,16,-1,  1, 8,16,+0,  1, 8,16,+0,  1, 8,16,-1,  1, 8,16,-1,
};

const unsigned char mpeg_dctcoeff_decodeL2_01[32][4]={
	 0,31,15,+0,  0,31,15,-1,  0,30,15,+0,  0,30,15,-1,  0,29,15,+0,  0,29,15,-1,  0,28,15,+0,  0,28,15,-1,
	 0,27,15,+0,  0,27,15,-1,  0,26,15,+0,  0,26,15,-1,  0,25,15,+0,  0,25,15,-1,  0,24,15,+0,  0,24,15,-1,
	 0,23,15,+0,  0,23,15,-1,  0,22,15,+0,  0,22,15,-1,  0,21,15,+0,  0,21,15,-1,  0,20,15,+0,  0,20,15,-1,
	 0,19,15,+0,  0,19,15,-1,  0,18,15,+0,  0,18,15,-1,  0,17,15,+0,  0,17,15,-1,  0,16,15,+0,  0,16,15,-1,
};

const unsigned char mpeg_dctcoeff_decodeL2_02[16][4]={
	10, 2,14,+0, 10, 2,14,-1,  9, 2,14,+0,  9, 2,14,-1,  5, 3,14,+0,  5, 3,14,-1,  3, 4,14,+0,  3, 4,14,-1,
	 2, 5,14,+0,  2, 5,14,-1,  1, 7,14,+0,  1, 7,14,-1,  1, 6,14,+0,  1, 6,14,-1,  0,15,14,+0,  0,15,14,-1,
};

const unsigned char mpeg_dctcoeff_decodeL2_03[16][4]={
	 0,14,14,+0,  0,14,14,-1,  0,13,14,+0,  0,13,14,-1,  0,12,14,+0,  0,12,14,-1, 26, 1,14,+0, 26, 1,14,-1,
	25, 1,14,+0, 25, 1,14,-1, 24, 1,14,+0, 24, 1,14,-1, 23, 1,14,+0, 23, 1,14,-1, 22, 1,14,+0, 22, 1,14,-1,
};

const unsigned char mpeg_dctcoeff_decodeL2_04[8][4]={
	 0,11,13,+0,  0,11,13,-1,  8, 2,13,+0,  8, 2,13,-1,  4, 3,13,+0,  4, 3,13,-1,  0,10,13,+0,  0,10,13,-1,
};

const unsigned char mpeg_dctcoeff_decodeL2_05[8][4]={
	 2, 4,13,+0,  2, 4,13,-1,  7, 2,13,+0,  7, 2,13,-1, 21, 1,13,+0, 21, 1,13,-1, 20, 1,13,+0, 20, 1,13,-1,
};

const unsigned char mpeg_dctcoeff_decodeL2_06[8][4]={
	 0, 9,13,+0,  0, 9,13,-1, 19, 1,13,+0, 19, 1,13,-1, 18, 1,13,+0, 18, 1,13,-1,  1, 5,13,+0,  1, 5,13,-1,
};

const unsigned char mpeg_dctcoeff_decodeL2_07[8][4]={
	 3, 3,13,+0,  3, 3,13,-1,  0, 8,13,+0,  0, 8,13,-1,  6, 2,13,+0,  6, 2,13,-1, 17, 1,13,+0, 17, 1,13,-1,
};

const unsigned char mpeg_dctcoeff_decodeL2_08[2][4]={16, 1,11,+0, 16, 1,11,-1,};
const unsigned char mpeg_dctcoeff_decodeL2_09[2][4]={ 5, 2,11,+0,  5, 2,11,-1,};
const unsigned char mpeg_dctcoeff_decodeL2_0A[2][4]={ 0, 7,11,+0,  0, 7,11,-1,};
const unsigned char mpeg_dctcoeff_decodeL2_0B[2][4]={ 2, 3,11,+0,  2, 3,11,-1,};
const unsigned char mpeg_dctcoeff_decodeL2_0C[2][4]={ 1, 4,11,+0,  1, 4,11,-1,};
const unsigned char mpeg_dctcoeff_decodeL2_0D[2][4]={15, 1,11,+0, 15, 1,11,-1,};
const unsigned char mpeg_dctcoeff_decodeL2_0E[2][4]={14, 1,11,+0, 14, 1,11,-1,};
const unsigned char mpeg_dctcoeff_decodeL2_0F[2][4]={ 4, 2,11,+0,  4, 2,11,-1,};
const unsigned char mpeg_dctcoeff_decodeL2_10[1][4]={ 0, 0,6,+0,};
const unsigned char mpeg_dctcoeff_decodeL2_20[1][4]={ 2, 2,8,+0,};
const unsigned char mpeg_dctcoeff_decodeL2_24[1][4]={ 2, 2,8,-1,};
const unsigned char mpeg_dctcoeff_decodeL2_28[1][4]={ 9, 1,8,+0,};
const unsigned char mpeg_dctcoeff_decodeL2_2C[1][4]={ 9, 1,8,-1,};
const unsigned char mpeg_dctcoeff_decodeL2_30[1][4]={ 0, 4,8,+0,};
const unsigned char mpeg_dctcoeff_decodeL2_34[1][4]={ 0, 4,8,-1,};
const unsigned char mpeg_dctcoeff_decodeL2_38[1][4]={ 8, 1,8,+0,};
const unsigned char mpeg_dctcoeff_decodeL2_3C[1][4]={ 8, 1,8,-1,};

const struct { const unsigned char (*tbl)[4]; int bits; } mpeg_dctcoeff_decodeL2[64]={
	{ mpeg_dctcoeff_decodeL2_00, 22-7 },
	{ mpeg_dctcoeff_decodeL2_01, 22-5 },
	{ mpeg_dctcoeff_decodeL2_02, 22-4 },
	{ mpeg_dctcoeff_decodeL2_03, 22-4 },
	{ mpeg_dctcoeff_decodeL2_04, 22-3 },
	{ mpeg_dctcoeff_decodeL2_05, 22-3 },
	{ mpeg_dctcoeff_decodeL2_06, 22-3 },
	{ mpeg_dctcoeff_decodeL2_07, 22-3 },
	{ mpeg_dctcoeff_decodeL2_08, 22-1 },
	{ mpeg_dctcoeff_decodeL2_09, 22-1 },
	{ mpeg_dctcoeff_decodeL2_0A, 22-1 },
	{ mpeg_dctcoeff_decodeL2_0B, 22-1 },
	{ mpeg_dctcoeff_decodeL2_0C, 22-1 },
	{ mpeg_dctcoeff_decodeL2_0D, 22-1 },
	{ mpeg_dctcoeff_decodeL2_0E, 22-1 },
	{ mpeg_dctcoeff_decodeL2_0F, 22-1 },
	{ mpeg_dctcoeff_decodeL2_10, 22-0 },
	{ mpeg_dctcoeff_decodeL2_10, 22-0 },
	{ mpeg_dctcoeff_decodeL2_10, 22-0 },
	{ mpeg_dctcoeff_decodeL2_10, 22-0 },
	{ mpeg_dctcoeff_decodeL2_10, 22-0 },
	{ mpeg_dctcoeff_decodeL2_10, 22-0 },
	{ mpeg_dctcoeff_decodeL2_10, 22-0 },
	{ mpeg_dctcoeff_decodeL2_10, 22-0 },
	{ mpeg_dctcoeff_decodeL2_10, 22-0 },
	{ mpeg_dctcoeff_decodeL2_10, 22-0 },
	{ mpeg_dctcoeff_decodeL2_10, 22-0 },
	{ mpeg_dctcoeff_decodeL2_10, 22-0 },
	{ mpeg_dctcoeff_decodeL2_10, 22-0 },
	{ mpeg_dctcoeff_decodeL2_10, 22-0 },
	{ mpeg_dctcoeff_decodeL2_10, 22-0 },
	{ mpeg_dctcoeff_decodeL2_10, 22-0 },
	{ mpeg_dctcoeff_decodeL2_20, 22-0 },
	{ mpeg_dctcoeff_decodeL2_20, 22-0 },
	{ mpeg_dctcoeff_decodeL2_20, 22-0 },
	{ mpeg_dctcoeff_decodeL2_20, 22-0 },
	{ mpeg_dctcoeff_decodeL2_24, 22-0 },
	{ mpeg_dctcoeff_decodeL2_24, 22-0 },
	{ mpeg_dctcoeff_decodeL2_24, 22-0 },
	{ mpeg_dctcoeff_decodeL2_24, 22-0 },
	{ mpeg_dctcoeff_decodeL2_28, 22-0 },
	{ mpeg_dctcoeff_decodeL2_28, 22-0 },
	{ mpeg_dctcoeff_decodeL2_28, 22-0 },
	{ mpeg_dctcoeff_decodeL2_28, 22-0 },
	{ mpeg_dctcoeff_decodeL2_2C, 22-0 },
	{ mpeg_dctcoeff_decodeL2_2C, 22-0 },
	{ mpeg_dctcoeff_decodeL2_2C, 22-0 },
	{ mpeg_dctcoeff_decodeL2_2C, 22-0 },
	{ mpeg_dctcoeff_decodeL2_30, 22-0 },
	{ mpeg_dctcoeff_decodeL2_30, 22-0 },
	{ mpeg_dctcoeff_decodeL2_30, 22-0 },
	{ mpeg_dctcoeff_decodeL2_30, 22-0 },
	{ mpeg_dctcoeff_decodeL2_34, 22-0 },
	{ mpeg_dctcoeff_decodeL2_34, 22-0 },
	{ mpeg_dctcoeff_decodeL2_34, 22-0 },
	{ mpeg_dctcoeff_decodeL2_34, 22-0 },
	{ mpeg_dctcoeff_decodeL2_38, 22-0 },
	{ mpeg_dctcoeff_decodeL2_38, 22-0 },
	{ mpeg_dctcoeff_decodeL2_38, 22-0 },
	{ mpeg_dctcoeff_decodeL2_38, 22-0 },
	{ mpeg_dctcoeff_decodeL2_3C, 22-0 },
	{ mpeg_dctcoeff_decodeL2_3C, 22-0 },
	{ mpeg_dctcoeff_decodeL2_3C, 22-0 },
	{ mpeg_dctcoeff_decodeL2_3C, 22-0 },
};

#include <string.h>
#include <math.h>

#include <vd2/system/vdtypes.h>
#include <vd2/Meia/MPEGIDCT.h>
#include "tables.h"

extern "C" void IDCT_mmx(signed short *dct_coeff, void *dst, long pitch, int intra_flag, int pos);
extern "C" void IDCT_isse(signed short *dct_coeff, void *dst, long pitch, int intra_flag, int pos);

using namespace nsVDMPEGTables;

//#define VERIFY_ROW_SHORTCUT

namespace nsVDMPEGIDCTMMX {

	static void mmx_idct_intra(unsigned char *dst, int pitch, short *tmp, int last_pos) {
		IDCT_mmx(tmp, dst, pitch, 1, last_pos);
	}

	static void mmx_idct_nonintra(unsigned char *dst, int pitch, short *tmp, int last_pos) {
		IDCT_mmx(tmp, dst, pitch, 0, last_pos);
	}

	static void mmx_idct_test(short *tmp, int last_pos) {
		IDCT_mmx(tmp, NULL, 0, 2, last_pos);
		__asm emms
	}

	static void isse_idct_intra(unsigned char *dst, int pitch, short *tmp, int last_pos) {
#ifdef VERIFY_ROW_SHORTCUT
		short test1[64], test2[64];
		memcpy(test1, tmp, 128);
		memcpy(test2, tmp, 128);
		IDCT_isse(test1, 0, 0, 2, last_pos?last_pos:1);
		IDCT_isse(test2, 0, 0, 2, 63);
		VDASSERT(!memcmp(test1, test2, 128));
#endif
		
		IDCT_isse(tmp, dst, pitch, 1, last_pos);
	}

	static void isse_idct_nonintra(unsigned char *dst, int pitch, short *tmp, int last_pos) {
#ifdef VERIFY_ROW_SHORTCUT
		short test1[64], test2[64];
		memcpy(test1, tmp, 128);
		memcpy(test2, tmp, 128);
		IDCT_isse(test1, 0, 0, 2, last_pos?last_pos:1);
		IDCT_isse(test2, 0, 0, 2, 63);
		VDASSERT(!memcmp(test1, test2, 128));
#endif

		IDCT_isse(tmp, dst, pitch, 0, last_pos);
	}

	static void isse_idct_test(short *tmp, int last_pos) {
		IDCT_isse(tmp, NULL, 0, 2, last_pos);
		__asm emms
		__asm sfence
	}

	///////////////////////////////////////////////////////////////////////

	static const int zigzag_std[64]={
		 0,  1,  8, 16,  9,  2,  3, 10,
		17, 24, 32, 25, 18, 11,  4,  5,
		12, 19, 26, 33, 40, 48, 41, 34,
		27, 20, 13,  6,  7, 14, 21, 28,
		35, 42, 49, 56, 57, 50, 43, 36,
		29, 22, 15, 23, 30, 37, 44, 51,
		58, 59, 52, 45, 38, 31, 39, 46,
		53, 60, 61, 54, 47, 55, 62, 63,
	};

	// Rows need to be stored in order: 0, 4, 1, 5, 2, 6, 3, 7

	#define POS(x) ((zigzag_std[x]&56) + (zigzag_std[x]&3)*2 + ((zigzag_std[x]&4)>>2))
	#define ROW(x) POS(x*8+0),POS(x*8+1),POS(x*8+2),POS(x*8+3),POS(x*8+4),POS(x*8+5),POS(x*8+6),POS(x*8+7)

	static const int zigzag_reordered[64]={
		ROW(0),ROW(1),ROW(2),ROW(3),ROW(4),ROW(5),ROW(6),ROW(7)
	};

	#undef ROW
	#undef POS


};

const struct VDMPEGIDCTSet g_VDMPEGIDCT_mmx = {
	(tVDMPEGIDCT)nsVDMPEGIDCTMMX::mmx_idct_intra,
	(tVDMPEGIDCT)nsVDMPEGIDCTMMX::mmx_idct_nonintra,
	(tVDMPEGIDCTTest)nsVDMPEGIDCTMMX::mmx_idct_test,
	NULL,
	nsVDMPEGIDCTMMX::zigzag_reordered,
};

const struct VDMPEGIDCTSet g_VDMPEGIDCT_isse = {
	(tVDMPEGIDCT)nsVDMPEGIDCTMMX::isse_idct_intra,
	(tVDMPEGIDCT)nsVDMPEGIDCTMMX::isse_idct_nonintra,
	(tVDMPEGIDCTTest)nsVDMPEGIDCTMMX::isse_idct_test,
	NULL,
	nsVDMPEGIDCTMMX::zigzag_reordered,
};

#include <vd2/system/math.h>
#include <vd2/system/int128.h>
#include <vd2/system/fraction.h>
#include "test.h"

DEFINE_TEST(Math) {
	VDASSERT(VDRoundToInt(-2.00f) ==-2);
	VDASSERT(VDRoundToInt(-1.51f) ==-2);
	VDASSERT(VDRoundToInt(-1.49f) ==-1);
	VDASSERT(VDRoundToInt(-1.00f) ==-1);
	VDASSERT(VDRoundToInt(-0.51f) ==-1);
	VDASSERT(VDRoundToInt(-0.49f) == 0);
	VDASSERT(VDRoundToInt( 0.00f) == 0);
	VDASSERT(VDRoundToInt( 0.49f) == 0);
	VDASSERT(VDRoundToInt( 0.51f) == 1);
	VDASSERT(VDRoundToInt( 1.00f) == 1);
	VDASSERT(VDRoundToInt( 1.49f) == 1);
	VDASSERT(VDRoundToInt( 1.51f) == 2);
	VDASSERT(VDRoundToInt( 2.00f) == 2);

	VDASSERT(VDFloorToInt(-2.0f) == -2);
	VDASSERT(VDFloorToInt(-1.5f) == -2);
	VDASSERT(VDFloorToInt(-1.0f) == -1);
	VDASSERT(VDFloorToInt(-0.5f) == -1);
	VDASSERT(VDFloorToInt( 0.0f) ==  0);
	VDASSERT(VDFloorToInt( 0.5f) ==  0);
	VDASSERT(VDFloorToInt( 1.0f) ==  1);
	VDASSERT(VDFloorToInt( 1.5f) ==  1);
	VDASSERT(VDFloorToInt( 2.0f) ==  2);

	VDASSERT(VDCeilToInt(-2.0f) == -2);
	VDASSERT(VDCeilToInt(-1.5f) == -1);
	VDASSERT(VDCeilToInt(-1.0f) == -1);
	VDASSERT(VDCeilToInt(-0.5f) ==  0);
	VDASSERT(VDCeilToInt( 0.0f) ==  0);
	VDASSERT(VDCeilToInt( 0.5f) ==  1);
	VDASSERT(VDCeilToInt( 1.0f) ==  1);
	VDASSERT(VDCeilToInt( 1.5f) ==  2);
	VDASSERT(VDCeilToInt( 2.0f) ==  2);

	VDASSERT(VDUMul64x64To128(1, 1) == vduint128(1));
	VDASSERT(VDUMul64x64To128(3, 7) == vduint128(21));
	VDASSERT(VDUMul64x64To128(0xFFFFFFFF, 0xFFFFFFFF) == vduint128(0xFFFFFFFE00000001));
	VDASSERT(VDUMul64x64To128(0x123456789ABCDEF0, 0xBAADF00DDEADBEEF) == vduint128(0x0D4665441D7CFEBC, 0xD182EA976BFA4210));
	VDASSERT(VDUMul64x64To128(0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF) == vduint128(0xFFFFFFFFFFFFFFFE, 0x0000000000000001));

	uint64 quotient;
	uint64 remainder;
	VDASSERT(((quotient = VDUDiv128x64To64(vduint128(21), 7, remainder)), quotient == 3 && remainder == 0));
	VDASSERT(((quotient = VDUDiv128x64To64(vduint128(27), 7, remainder)), quotient == 3 && remainder == 6));
	VDASSERT(((quotient = VDUDiv128x64To64(vduint128(0xFFFFFFFFFFFFFFFE, 0x0000000000000001), 0xFFFFFFFFFFFFFFFF, remainder)), quotient == 0xFFFFFFFFFFFFFFFF && remainder == 0));
	VDASSERT(((quotient = VDUDiv128x64To64(vduint128(0xFFFFFFFFFFFFFFFF, 0x0000000000000000), 0xFFFFFFFFFFFFFFFF, remainder)), quotient == 0xFFFFFFFFFFFFFFFF && remainder == 0xFFFFFFFFFFFFFFFF));
	VDASSERT(((quotient = VDUDiv128x64To64(vduint128(0x123456789ABCDEF0, 0xBAADF00DDEADBEEF), 0xFEDCBA9876543210, remainder)), quotient == 0x1249249249249238 && remainder == 0xA72CB7FA5D75AB6F));

	VDASSERT(VDFraction(3,2) * VDFraction(2,3) == VDFraction(1,1));
	VDASSERT(VDFraction(3,2) * VDFraction(5,8) == VDFraction(15,16));
	VDASSERT(VDFraction(15,16) / VDFraction(5,8) == VDFraction(3,2));
	VDASSERT(VDFraction(1024,768) == VDFraction(4,3));
	VDASSERT(VDFraction(0xFFFFFFFC/3,0xFFFFFFFF) * VDFraction(0xFFFFFFFF,0xFFFFFFFC) == VDFraction(1,3));

	VDASSERT(VDMulDiv64(-10000000000000000, -10000000000000000, -10000000000000000) == -10000000000000000);
	VDASSERT(VDMulDiv64(-1000000000000, -100000, 17) == 5882352941176471);

	return 0;
}


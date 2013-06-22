#ifndef f_VD2_MATH_H
#define f_VD2_MATH_H

#include <math.h>
#include <vd2/system/vdtypes.h>

// Rounding functions
//
// Round a double to an int or a long.  Behavior is not specified at
// int(y)+0.5, if x is NaN or Inf, or if x is out of range.

int VDRoundToInt(double x);
long VDRoundToLong(double x);

inline sint32 VDRoundToIntFast(float x) {
	union {
		float f;
		sint32 i;
	} u = {x + 12582912.0f};		// 2^22+2^23

	return (sint32)u.i - 0x4B400000;
}

inline sint32 VDRoundToIntFastFullRange(float x) {
	union {
		double f;
		sint32 i[2];
	} u = {x + 6755399441055744.0f};		// 2^51+2^52

	return (sint32)u.i[0];
}

#ifdef _M_AMD64
	inline sint32 VDFloorToIntFast(float x) {
		return (sint32)floor(x);
	}
#else
	#pragma warning(push)
	#pragma warning(disable: 4035)		// warning C4035: 'VDFloorToIntFast' : no return value
	inline sint32 VDFloorToIntFast(float x) {
		sint32 temp;

		__asm {
			fld x
			fist temp
			fild temp
			mov eax, temp
			fsub
			fstp temp
			cmp	temp, 80000001h
			adc eax, -1
		}
	}
	#pragma warning(pop)
#endif

#endif

//	VirtualDub - Video processing and capture application
//	System library component
//	Copyright (C) 1998-2004 Avery Lee, All Rights Reserved.
//
//	Beginning with 1.6.0, the VirtualDub system library is licensed
//	differently than the remainder of VirtualDub.  This particular file is
//	thus licensed as follows (the "zlib" license):
//
//	This software is provided 'as-is', without any express or implied
//	warranty.  In no event will the authors be held liable for any
//	damages arising from the use of this software.
//
//	Permission is granted to anyone to use this software for any purpose,
//	including commercial applications, and to alter it and redistribute it
//	freely, subject to the following restrictions:
//
//	1.	The origin of this software must not be misrepresented; you must
//		not claim that you wrote the original software. If you use this
//		software in a product, an acknowledgment in the product
//		documentation would be appreciated but is not required.
//	2.	Altered source versions must be plainly marked as such, and must
//		not be misrepresented as being the original software.
//	3.	This notice may not be removed or altered from any source
//		distribution.

#ifndef f_VD2_INT128_H
#define f_VD2_INT128_H

#include <vd2/system/vdtypes.h>

#ifdef _M_AMD64
	extern "C" __int64 _mul128(__int64 x, __int64 y, __int64 *hiresult);
	extern "C" unsigned __int64 __shiftleft128(unsigned __int64 low, unsigned __int64 high, unsigned char shift);
	extern "C" unsigned __int64 __shiftright128(unsigned __int64 low, unsigned __int64 high, unsigned char shift);

	#pragma intrinsic(_mul128)
	#pragma intrinsic(_shiftleft128)
	#pragma intrinsic(_shiftright128)

	extern "C" {
		void vdasm_int128_add(sint64 *dst, const sint64 x[2], const sint64 y[2]);
		void vdasm_int128_sub(sint64 *dst, const sint64 x[2], const sint64 y[2]);
		void vdasm_int128_mul(sint64 *dst, const sint64 x[2], const sint64 y[2]);
	}
#endif

class int128 {
protected:
	sint64 v[2];

public:
	int128() {}

	int128(sint64 x) {
		v[0] = x;
		v[1] = x>>63;
	}

	int128(uint64 x) {
		v[0] = (sint64)x;
		v[1] = 0;
	}

	int128(int x) {
		v[0] = x;
		v[1] = (sint64)x >> 63;
	}

	int128(unsigned int x) {
		v[1] = 0;
		v[0] = x;
	}

	int128(unsigned long x) {
		v[1] = 0;
		v[0] = x;
	}

	sint64 getHi() const { return v[1]; }
	uint64 getLo() const { return v[0]; }

	operator double() const;
	operator sint64() const {
		return (sint64)v[0];
	}
	operator uint64() const {
		return (uint64)v[0];
	}

#ifdef _M_AMD64
	void setSquare(sint64 v) {
		const int128 v128(v);
		operator=(v128*v128);
	}

	const int128 operator+(const int128& x) const {
		int128 t;
		vdasm_int128_add(t.v, v, x.v);
		return t;
	}

	const int128& operator+=(const int128& x) {
		vdasm_int128_add(v, v, x.v);
		return *this;
	}

	const int128 operator-(const int128& x) const {
		int128 t;
		vdasm_int128_sub(t.v, v, x.v);
		return t;
	}

	const int128 operator*(const int128& x) const {
		int128 t;
		vdasm_int128_mul(t.v, v, x.v);
		return t;
	}

	const int128 operator<<(int count) const {
		int128 t;

		if (count >= 64) {
			t.v[0] = 0;
			t.v[1] = v[0] << (count-64);
		} else {
			t.v[0] = v[0] << count;
			t.v[1] = __shiftleft128(v[0], v[1], count);
		}

		return t;
	}

	const int128 operator>>(int count) const {
		int128 t;

		if (count >= 64) {
			t.v[0] = v[1] >> (count-64);
			t.v[1] = v[1] >> 63;
		} else {
			t.v[0] = __shiftright128(v[0], v[1], count);
			t.v[1] = v[1] >> count;
		}

		return t;
	}

	const int128 operator-() const {
		int128 t(0);
		vdasm_int128_sub(t.v, t.v, v);
		return t;
	}

	const int128 abs() const {
		int128 t(0);
		return t.v[1] < 0 ? -t : t;
	}
#else
	void setSquare(sint64 v);

	const int128 operator+(const int128& x) const;
	const int128& operator+=(const int128& x);
	const int128 operator-(const int128& x) const;
	const int128 operator*(const int128& x) const;
	const int128 operator<<(int v) const;
	const int128 operator>>(int v) const;
	const int128 operator-() const;
	const int128 abs() const;
#endif
};

#endif

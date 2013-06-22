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
	extern "C" {
		void vdasm_int128_add(sint64 *dst, const sint64 x[2], const sint64 y[2]);
		void vdasm_int128_sub(sint64 *dst, const sint64 x[2], const sint64 y[2]);
		void vdasm_int128_mul(sint64 *dst, const sint64 x[2], const sint64 y[2]);
		void vdasm_int128_shl(sint64 *dst, const sint64 x[2], int shift);
		void vdasm_int128_sar(sint64 *dst, const sint64 x[2], int shift);
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

	operator double() const;
	operator sint64() const {
		return (sint64)v[0];
	}
	operator uint64() const {
		return (uint64)v[0];
	}

#ifdef _M_AMD64
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
		vdasm_int128_shl(t.v, v, count);
		return *this;
	}

	const int128 operator>>(int count) const {
		int128 t;
		vdasm_int128_sar(t.v, v, count);
		return *this;
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

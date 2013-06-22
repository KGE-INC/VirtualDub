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

#include <math.h>

#include <vd2/system/fraction.h>
#include <vd2/system/vdtypes.h>

VDFraction::VDFraction(double d) {
	int xp;
	double mant = frexp(d, &xp);

	if (xp >= 31) {
		hi = 0xFFFFFFFF;
		lo = 1;
	} else if (xp < -32) {
		hi = 0;
		lo = 1;
	} else if (xp >= 0) {
		*this = reduce((uint64)(0.5 + ldexp(mant, 62)), 1i64<<(62-xp));
	} else {
		*this = reduce((uint64)(0.5 + ldexp(mant, xp+62)), 1i64<<62);
	}
}

VDFraction VDFraction::reduce(uint64 hi, uint64 lo) {

	// Check for undefined.

	if (!lo)
		return VDFraction(0,0);

	// Check for zero.

	if (!hi) {
		return VDFraction(0,1);
	}

	// Check for infinity.

	if (!(lo>>32) && hi > ((uint64)lo<<32)-lo)
		return VDFraction(0xFFFFFFFFUL, 1);

	// Check for one.

	if (lo == hi)
		return VDFraction(1,1);

	// Remove factors of two.

	while(!(((unsigned)hi | (unsigned)lo) & 1)) {
		hi >>= 1;
		lo >>= 1;
	}

	// Remove factors of 3.

	while(!(lo%3) && !(hi%3)) {
		lo /= 3;
		hi /= 3;
	}

	// Use Euclid's algorithm to find the GCD of the two numbers.
	//
	// If D is the GCD of A and B, A>=B, then let A=xD and B=yD.
	// It follows that x>=y and that (A-B) = (x-y)D is also
	// divisible by D.  From this, we can repeat the subtraction,
	// giving C = (x % y)D = A % B being divisible by D.

	uint64 A, B, C;

	A = hi;
	B = lo;

	if (lo > hi) {
		A = lo;
		B = hi;
	}

	do {
		C = A % B;
		A = B;
		B = C;
	} while(B > 0);

	// Since A>0 and B>0, then A>1 after this operation.

	lo /= A;
	hi /= A;

	// Return the fraction if it's within range.

	if (lo == (unsigned)lo && hi == (unsigned)hi)
		return VDFraction(hi, lo);

	// Reduce the fraction in range, crudely.

//	return VDFraction(((uint64)hi * 0xFFFFFFFFUL + lo/2) / lo, 0xFFFFFFFFUL);

	while(lo != (unsigned)lo || hi != (unsigned)hi) {
		lo >>= 1;
		hi >>= 1;
	}

	if (!lo)
		return VDFraction(1,0xFFFFFFFFUL);

	return VDFraction(hi, lo);
}

// a (cond) b
// a-b (cond) 0
// aH*bL - aL*bh (cond) 0
// aH*bL (cond) aL*bH

bool VDFraction::operator==(VDFraction b) const {
	return (uint64)hi * b.lo == (uint64)lo * b.hi;
}

bool VDFraction::operator!=(VDFraction b) const {
	return (uint64)hi * b.lo != (uint64)lo * b.hi;
}

bool VDFraction::operator< (VDFraction b) const {
	return (uint64)hi * b.lo < (uint64)lo * b.hi;
}

bool VDFraction::operator<=(VDFraction b) const {
	return (uint64)hi * b.lo <= (uint64)lo * b.hi;
}

bool VDFraction::operator> (VDFraction b) const {
	return (uint64)hi * b.lo > (uint64)lo * b.hi;
}

bool VDFraction::operator>=(VDFraction b) const {
	return (uint64)hi * b.lo >= (uint64)lo * b.hi;
}

VDFraction VDFraction::operator*(VDFraction b) const {
	return reduce((uint64)hi * b.hi, (uint64)lo * b.lo);
}

VDFraction VDFraction::operator/(VDFraction b) const {
	return reduce((uint64)hi * b.lo, (uint64)lo * b.hi);
}

VDFraction VDFraction::operator*(unsigned long b) const {
	return reduce((uint64)hi * b, lo);
}

VDFraction VDFraction::operator/(unsigned long b) const {
	return reduce(hi, (uint64)lo * b);
}

///////////////////////////////////////////////////////////////////////////

#ifdef _M_IX86
	sint64 __declspec(naked) __stdcall VDFractionScale64(uint64 a, uint32 b, uint32 c, uint32& remainder) {
		__asm {
			push	edi
			push	ebx
			mov		edi, [esp+12+8]			;edi = b
			mov		eax, [esp+4+8]			;eax = a[lo]
			mul		edi						;edx:eax = a[lo]*b
			mov		ecx, eax				;ecx = (a*b)[lo]
			mov		eax, [esp+8+8]			;eax = a[hi]
			mov		ebx, edx				;ebx = (a*b)[mid]
			mul		edi						;edx:eax = a[hi]*b
			add		eax, ebx
			mov		ebx, [esp+16+8]			;ebx = c
			adc		edx, 0
			div		ebx						;eax = (a*b)/c [hi], edx = (a[hi]*b)%c
			mov		edi, eax				;edi = (a[hi]*b)/c
			mov		eax, ecx				;eax = (a*b)[lo]
			mov		ecx, [esp+20+8]
			div		ebx						;eax = (a*b)/c [lo], edx = (a*b)%c
			mov		[ecx], edx
			mov		edx, edi
			pop		ebx
			pop		edi
			ret		20
		}
	}
#else
	extern "C" sint64 VDFractionScale64(uint64 a, uint64 b, uint64 c, uint32& remainder);
#endif

sint64 VDFraction::scale64t(sint64 v) const {
	uint32 r;
	return v<0 ? -VDFractionScale64(-v, hi, lo, r) : VDFractionScale64(v, hi, lo, r);
}

sint64 VDFraction::scale64u(sint64 v) const {
	uint32 r;
	if (v<0) {
		v = -VDFractionScale64(-v, hi, lo, r);
		return v;
	} else {
		v = +VDFractionScale64(+v, hi, lo, r);
		return v + (r > 0);
	}
}

sint64 VDFraction::scale64r(sint64 v) const {
	uint32 r;
	if (v<0) {
		v = -VDFractionScale64(-v, hi, lo, r);
		return v - (r >= (lo>>1));
	} else {
		v = +VDFractionScale64(+v, hi, lo, r);
		return v + (r >= (lo>>1));
	}
}

sint64 VDFraction::scale64it(sint64 v) const {
	uint32 r;
	return v<0 ? -VDFractionScale64(-v, lo, hi, r) : +VDFractionScale64(+v, lo, hi, r);
}

sint64 VDFraction::scale64ir(sint64 v) const {
	uint32 r;
	if (v<0) {
		v = -VDFractionScale64(-v, lo, hi, r);
		return v - (r >= (hi>>1));
	} else {
		v = +VDFractionScale64(+v, lo, hi, r);
		return v + (r >= (hi>>1));
	}
}

sint64 VDFraction::scale64iu(sint64 v) const {
	uint32 r;
	if (v<0) {
		v = -VDFractionScale64(-v, lo, hi, r);
		return v;
	} else {
		v = +VDFractionScale64(+v, lo, hi, r);
		return v + (r > 0);
	}
}

///////////////////////////////////////////////////////////////////////////

VDFraction::operator long() const {
	return (long)((hi + lo/2) / lo);
}

VDFraction::operator unsigned long() const {
	return (hi + lo/2) / lo;
}

VDFraction::operator double() const {
	return (double)hi / (double)lo;
}

unsigned long VDFraction::roundup32ul() const {
	return (hi + (lo-1)) / lo;
}


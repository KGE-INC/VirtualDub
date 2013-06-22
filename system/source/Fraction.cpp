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
		*this = reduce((sint64)(0.5 + ldexp(mant, 62)), 1i64<<(62-xp));
	} else {
		*this = reduce((sint64)(0.5 + ldexp(mant, xp+62)), 1i64<<62);
	}
}

VDFraction VDFraction::reduce(sint64 hi, sint64 lo) {

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

	sint64 A, B, C;

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

//	return VDFraction(((sint64)hi * 0xFFFFFFFFUL + lo/2) / lo, 0xFFFFFFFFUL);

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
	return (sint64)hi * b.lo == (sint64)lo * b.hi;
}

bool VDFraction::operator!=(VDFraction b) const {
	return (sint64)hi * b.lo != (sint64)lo * b.hi;
}

bool VDFraction::operator< (VDFraction b) const {
	return (sint64)hi * b.lo < (sint64)lo * b.hi;
}

bool VDFraction::operator<=(VDFraction b) const {
	return (sint64)hi * b.lo <= (sint64)lo * b.hi;
}

bool VDFraction::operator> (VDFraction b) const {
	return (sint64)hi * b.lo > (sint64)lo * b.hi;
}

bool VDFraction::operator>=(VDFraction b) const {
	return (sint64)hi * b.lo >= (sint64)lo * b.hi;
}


VDFraction VDFraction::operator+(VDFraction b) const {

	//  aH   bH   aH*bL + aL*bH
	//  -- + -- = -------------
	//  aL   bL       aL*bL

	return reduce(hi * (sint64)b.lo + (sint64)lo * b.hi, (sint64)lo * b.lo);
}


VDFraction VDFraction::operator-(VDFraction b) const {
	return reduce(hi * (sint64)b.lo - (sint64)lo * b.hi, (sint64)lo * b.lo);
}

VDFraction VDFraction::operator*(VDFraction b) const {
	return reduce((sint64)hi * b.hi, (sint64)lo * b.lo);
}

VDFraction VDFraction::operator/(VDFraction b) const {
	return reduce((sint64)hi * b.lo, (sint64)lo * b.hi);
}

VDFraction VDFraction::operator*(unsigned long b) const {
	return reduce((sint64)hi * b, lo);
}

VDFraction VDFraction::operator/(unsigned long b) const {
	return reduce(hi, (sint64)lo * b);
}

///////////////////////////////////////////////////////////////////////////

sint64 VDFraction::scale64t(sint64 v) const {
	return (v*hi)/lo;
}

sint64 VDFraction::scale64u(sint64 v) const {
	return (v*hi + lo - 1)/lo;
}

sint64 VDFraction::scale64r(sint64 v) const {
	return (v*hi + lo/2) / lo;
}

sint64 VDFraction::scale64it(sint64 v) const {
	return (v*lo)/hi;
}

sint64 VDFraction::scale64ir(sint64 v) const {
	return (v*lo + hi/2) / hi;
}

sint64 VDFraction::scale64iu(sint64 v) const {
	return (v*lo + hi - 1) / hi;
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

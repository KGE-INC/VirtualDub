#ifndef f_VD2_SYSTEM_FRACTION_H
#define f_VD2_SYSTEM_FRACTION_H

#include <vd2/system/vdtypes.h>

class VDFraction {
friend VDFraction operator*(unsigned long b, const VDFraction f);
friend VDFraction operator*(int b, const VDFraction f);
private:
	unsigned long	hi, lo;

	static VDFraction reduce(sint64 hi, sint64 lo);

public:
	VDFraction() {}
	explicit VDFraction(int i) : hi(i), lo(1) {}
	explicit VDFraction(unsigned long i) : hi(i), lo(1) { }
	explicit VDFraction(unsigned long i, unsigned long j) : hi(i), lo(j) {}
	explicit VDFraction(double d);

	bool	operator<(VDFraction b) const;
	bool	operator<=(VDFraction b) const;
	bool	operator>(VDFraction b) const;
	bool	operator>=(VDFraction b) const;
	bool	operator==(VDFraction b) const;
	bool	operator!=(VDFraction b) const;

	VDFraction operator+(VDFraction b) const;
	VDFraction operator-(VDFraction b) const;
	VDFraction operator*(VDFraction b) const;
	VDFraction operator/(VDFraction b) const;

	VDFraction operator*(unsigned long b) const;
	VDFraction operator/(unsigned long b) const;

	VDFraction operator*(int b) const { return operator*((unsigned long)b); }
	VDFraction operator/(int b) const { return operator/((unsigned long)b); }

	sint64 scale64t(sint64) const;
	sint64 scale64r(sint64) const;
	sint64 scale64u(sint64) const;
	sint64 scale64it(sint64) const;
	sint64 scale64ir(sint64) const;
	sint64 scale64iu(sint64) const;

	operator long() const;
	operator unsigned long() const;
	operator double() const;

	double asDouble() const { return (double)*this; }

	unsigned long roundup32ul() const;

	unsigned long getHi() const { return hi; }
	unsigned long getLo() const { return lo; }

	VDFraction reduce() const { return reduce(hi, lo); }

	static inline VDFraction reduce64(sint64 hi, sint64 lo) { return reduce(hi, lo); }
};

inline VDFraction operator*(unsigned long b, const VDFraction f) { return f*b; }
inline VDFraction operator*(int b, const VDFraction f) { return f*b; }

typedef VDFraction Fraction;

#endif

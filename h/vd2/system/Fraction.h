#ifndef f_VD2_SYSTEM_FRACTION_H
#define f_VD2_SYSTEM_FRACTION_H

class VDFraction {
friend VDFraction operator*(unsigned long b, const VDFraction f);
friend VDFraction operator*(int b, const VDFraction f);
private:
	unsigned long	hi, lo;

	static VDFraction reduce(__int64 hi, __int64 lo);

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

	__int64 scale64t(__int64) const;
	__int64 scale64r(__int64) const;
	__int64 scale64u(__int64) const;
	__int64 scale64it(__int64) const;
	__int64 scale64ir(__int64) const;

	operator long() const;
	operator unsigned long() const;
	operator double() const;

	unsigned long roundup32ul() const;

	unsigned long getHi() const { return hi; }
	unsigned long getLo() const { return lo; }

	VDFraction reduce() const { return reduce(hi, lo); }

	static inline VDFraction reduce64(__int64 hi, __int64 lo) { return reduce(hi, lo); }
};

inline VDFraction operator*(unsigned long b, const VDFraction f) { return f*b; }
inline VDFraction operator*(int b, const VDFraction f) { return f*b; }

typedef VDFraction Fraction;

#endif

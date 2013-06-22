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

#ifndef f_VD2_SYSTEM_VECTORS_H
#define f_VD2_SYSTEM_VECTORS_H

#include <vd2/system/vdtypes.h>
#include <math.h>

#ifndef VDFORCEINLINE
	#define VDFORCEINLINE __forceinline
#endif

///////////////////////////////////////////////////////////////////////////

bool VDSolveLinearEquation(double *src, int n, ptrdiff_t stride_elements, double *b, double tolerance = 1e-5);

///////////////////////////////////////////////////////////////////////////

template<class T>
class VDVector2 {
public:
	typedef VDVector2 self_type;
	typedef T value_type;

	VDFORCEINLINE VDVector2() {}
	VDFORCEINLINE VDVector2(T x2, T y2) {x=x2; y=y2;}
	VDFORCEINLINE VDVector2(const T src[2]) {x=src[0]; y=src[1];}

	void set(T x2, T y2) { x=x2; y=y2; }

	T&			operator[](int k)					{ return v[k]; }
	const T&	operator[](int k) const				{ return v[k]; }

	T			lensq() const						{ return x*x + y*y; }
	T			len() const							{ return (T)sqrt(lensq()); }
	self_type	normalized() const					{ return *this / len(); }

	self_type	operator-() const					{ return self_type(-x, -y); }

	self_type	operator+(const self_type& r) const	{ return self_type(x+r.x, y+r.y); }
	self_type	operator-(const self_type& r) const	{ return self_type(x-r.x, y-r.y); }
	T			operator*(const self_type& r) const	{ return x*r.x + y*r.y; }

	self_type&	operator+=(const self_type& r)		{ x+=r.x; y+=r.y; return *this; }
	self_type&	operator-=(const self_type& r)		{ x-=r.x; y-=r.y; return *this; }

	self_type	operator*(const T s) const			{ return self_type(x*s, x*s); }
	self_type&	operator*=(const T s)				{ x*=s; y*=s; return *this; }

	self_type	operator/(const T s) const			{ const T inv(T(1)/s); return self_type(x*inv, y*inv); }
	self_type&	operator/=(const T s)				{ const T inv(T(1)/s); x*=inv; y*=inv; return *this; }

	self_type	scaled(const T x2, const T y2) const	{ return self_type(x*x2, y*y2); }

	union {
		struct {
			T x;
			T y;
		};
		T v[2];
	};
};

template<class T>
VDFORCEINLINE VDVector2<T> operator*(const T s, const VDVector2<T>& v) { return v*s; }

///////////////////////////////////////////////////////////////////////////

template<class T>
class VDVector3 {
public:
	typedef VDVector3 self_type;
	typedef T value_type;

	VDFORCEINLINE VDVector3() {}
	VDFORCEINLINE VDVector3(T x2, T y2, T z2) { x=x2; y=y2; z=z2; }
	VDFORCEINLINE VDVector3(const T src[3]) { x=src[0]; y=src[1]; z=src[2]; }

	T&			operator[](int k)					{ return v[k]; }
	const T&	operator[](int k) const				{ return v[k]; }

	T			lensq() const						{ return x*x + y*y + z*z; }
	T			len() const							{ return (T)sqrt(lensq()); }
	self_type	normalized() const					{ return *this / len(); }

	VDVector2<T> project() const					{ const T inv(T(1)/z); return VDVector2<T>(x*inv, y*inv); }
	VDVector2<T> as2d() const						{ return VDVector2<T>(x, y); }

	self_type	operator-() const					{ return self_type(-x, -y, -z); }

	self_type	operator+(const self_type& r) const	{ return self_type(x+r.x, y+r.y, z+r.z); }
	self_type	operator-(const self_type& r) const	{ return self_type(x-r.x, y-r.y, z-r.z); }
	T			operator*(const self_type& r) const	{ return x*r.x + y*r.y + z*r.z; }
	self_type	operator^(const self_type& r) const	{ return self_type(y*r.z - z*r.y, z*r.x - x*r.z, x*r.y - y*r.x); }

	self_type&	operator+=(const self_type& r)		{ x+=r.x; y+=r.y; z+=r.z; return *this; }
	self_type&	operator-=(const self_type& r)		{ x-=r.x; y-=r.y; z-=r.z; return *this; }
	self_type&	operator^=(const self_type& r)		{ return operator=(*this ^ r); }

	self_type	operator*(const T s) const			{ return self_type(x*s, y*s, z*s); }
	self_type&	operator*=(const T s)				{ x*=s; y*=s; z*=s; return *this; }

	self_type	operator/(const T s) const			{ const T inv(T(1)/s); return self_type(x*inv, y*inv, z*inv); }
	self_type&	operator/=(const T s)				{ const T inv(T(1)/s); x*=inv; y*=inv; z*=inv; return *this; }

	self_type	scaled(const T x2, const T y2, const T z2) const	{ return self_type(x2*x, y2*y, z2*z); }

	union {
		struct {
			T x;
			T y;
			T z;
		};
		T v[3];
	};
};

template<class T>
VDFORCEINLINE VDVector3<T> operator*(const T s, const VDVector3<T>& v) { return v*s; }

///////////////////////////////////////////////////////////////////////////

template<class T>
class VDVector4 {
public:
	typedef VDVector4 self_type;
	typedef T value_type;

	VDFORCEINLINE VDVector4() {}
	VDFORCEINLINE VDVector4(T x2, T y2, T z2, T w2) { x=x2; y=y2; z=z2; w=w2; }
	VDFORCEINLINE VDVector4(const T src[4]) { x=src[0]; y=src[1]; z=src[2]; w=src[3]; }

	T&			operator[](int i) { return v[i]; }
	const T&	operator[](int i) const { return v[i]; }

	T			lensq() const						{ return x*x + y*y + z*z + w*w; }
	T			len() const							{ return (T)sqrt(lensq()); }
	self_type	normalized() const					{ return *this / len(); }

	VDVector3<T> project() const					{ const T inv(T(1)/w); return VDVector3<T>(x*inv, y*inv, z*inv); }

	self_type	operator-() const					{ return self_type(-x, -y, -z, -w); }

	self_type	operator+(const self_type& r) const	{ return self_type(x+r.x, y+r.y, z+r.z, w+r.w); }
	self_type	operator-(const self_type& r) const	{ return self_type(x-r.x, y-r.y, z-r.z, w-r.w); }
	T			operator*(const self_type& r) const	{ return x*r.x + y*r.y + z*r.z + w*r.w; }

	self_type&	operator+=(const self_type& r)		{ x+=r.x; y+=r.y; z+=r.z; w+=r.w; return *this; }
	self_type&	operator-=(const self_type& r)		{ x-=r.x; y-=r.y; z-=r.z; w-=r.w; return *this; }

	self_type	operator*(const T factor)			{ return self_type(x*factor, y*factor, z*factor, w*factor); }
	self_type	operator/(const T factor)			{ const T inv(T(1) / factor); return self_type(x*inv, y*inv, z*inv, w*inv); }

	self_type&	operator*=(const T factor)			{ x *= factor; y *= factor; z *= factor; w *= factor; return *this; }
	self_type&	operator/=(const T factor)			{ const T inv(T(1) / factor); x *= inv; y *= inv; z *= inv; w *= inv; return *this; }

	union {
		struct {
			T x;
			T y;
			T z;
			T w;
		};
		T v[4];
	};
};

template<class T>
VDFORCEINLINE VDVector4<T> operator*(const T s, const VDVector4<T>& v) { return v*s; }

///////////////////////////////////////////////////////////////////////////

template<class T>
class VDMatrix2 {
public:
	enum zero_type { zero };
	enum identity_type { identity };

	typedef T				value_type;
	typedef VDVector2<T>	vector_type;
	typedef VDMatrix2<T>	self_type;

	VDMatrix2() {}
	VDMatrix2(zero_type) { m[0] = m[1] = vector_type(0, 0, 0); }
	VDMatrix2(identity_type) {
		m[0] = vector_type(1, 0);
		m[1] = vector_type(0, 1);
	}

	vector_type& operator[](int k) { return m[k]; }
	const vector_type& operator[](int k) const { return m[k]; }

	self_type operator*(const self_type& v) const {
		self_type result;

#define DO(i,j) result.m[i].v[j] = m[i].v[0]*v.m[0].v[j] + m[i].v[1]*v.m[1].v[j] + m[i].v[2]*v.m[2].v[j]
		DO(0,0);
		DO(0,1);
		DO(1,0);
		DO(1,1);
#undef DO

		return result;
	}

	vector_type operator*(const vector_type& r) const {
		return vector_type(
				m[0].v[0]*r.v[0] + m[0].v[1]*r.v[1],
				m[1].v[0]*r.v[0] + m[1].v[1]*r.v[1]);
	}

	self_type transpose() const {
		self_type res;

		res.m[0].v[0] = m[0].v[0];
		res.m[0].v[1] = m[1].v[0];
		res.m[1].v[0] = m[0].v[1];
		res.m[1].v[1] = m[1].v[1];

		return res;
	}

	self_type adjunct() const {
		self_type res = {
			vector_type( m[1].v[1], -m[0].v[1]),
			vector_type(-m[1].v[0], -m[0].v[0]),
		};

		return res;
	}

	T det() const {
		return m[0].v[0]*m[1].v[1] - m[1].v[0]*m[0].v[1];
	}

	self_type operator~() const {
		return adjunct() / det();
	}

	self_type& operator*=(const T factor) {
		m[0] *= factor;
		m[1] *= factor;

		return *this;
	}

	self_type& operator/=(const T factor) {
		return operator*=(T(1)/factor);
	}

	self_type operator*(const T factor) const {
		return self_type(*this) *= factor;
	}

	self_type operator/(const T factor) const {
		return self_type(*this) /= factor;
	}

	vector_type m[2];
};

template<class T>
class VDMatrix3 {
public:
	enum zero_type { zero };
	enum identity_type { identity };
	enum rotation_x_type { rotation_x };
	enum rotation_y_type { rotation_y };
	enum rotation_z_type { rotation_z };

	typedef T				value_type;
	typedef VDVector3<T>	vector_type;
	typedef VDMatrix3<T>	self_type;

	VDMatrix3() {}
	VDMatrix3(zero_type) { m[0] = m[1] = m[2] = vector_type(0, 0, 0); }
	VDMatrix3(identity_type) {
		m[0] = vector_type(1, 0, 0);
		m[1] = vector_type(0, 1, 0);
		m[2] = vector_type(0, 0, 1);
	}
	VDMatrix3(rotation_x_type, T angle) {
		const T s(sin(angle));
		const T c(cos(angle));

		m[0] = vector_type( 1, 0, 0);
		m[1] = vector_type( 0, c,-s);
		m[2] = vector_type( 0, s, c);
	}

	VDMatrix3(rotation_y_type, T angle) {
		const T s(sin(angle));
		const T c(cos(angle));

		m[0] = vector_type( c, 0, s);
		m[1] = vector_type( 0, 1, 0);
		m[2] = vector_type(-s, 0, c);
	}
	VDMatrix3(rotation_z_type, T angle) {
		const T s(sin(angle));
		const T c(cos(angle));

		m[0] = vector_type( c,-s, 0);
		m[1] = vector_type( s, c, 0);
		m[2] = vector_type( 0, 0, 1);
	}

	vector_type& operator[](int k) { return m[k]; }
	const vector_type& operator[](int k) const { return m[k]; }

	self_type operator*(const self_type& v) const {
		self_type result;

#define DO(i,j) result.m[i].v[j] = m[i].v[0]*v.m[0].v[j] + m[i].v[1]*v.m[1].v[j] + m[i].v[2]*v.m[2].v[j]
		DO(0,0);
		DO(0,1);
		DO(0,2);
		DO(1,0);
		DO(1,1);
		DO(1,2);
		DO(2,0);
		DO(2,1);
		DO(2,2);
#undef DO

		return result;
	}

	vector_type operator*(const vector_type& r) const {
		return vector_type(
				m[0].v[0]*r.v[0] + m[0].v[1]*r.v[1] + m[0].v[2]*r.v[2],
				m[1].v[0]*r.v[0] + m[1].v[1]*r.v[1] + m[1].v[2]*r.v[2],
				m[2].v[0]*r.v[0] + m[2].v[1]*r.v[1] + m[2].v[2]*r.v[2]);
	}

	self_type transpose() const {
		self_type res;

		res.m[0].v[0] = m[0].v[0];
		res.m[0].v[1] = m[1].v[0];
		res.m[0].v[2] = m[2].v[0];
		res.m[1].v[0] = m[0].v[1];
		res.m[1].v[1] = m[1].v[1];
		res.m[1].v[2] = m[2].v[1];
		res.m[2].v[0] = m[0].v[2];
		res.m[2].v[1] = m[1].v[2];
		res.m[2].v[2] = m[2].v[2];

		return res;
	}

	self_type adjunct() const {
		self_type res;

		res.m[0] = m[1] ^ m[2];
		res.m[1] = m[2] ^ m[0];
		res.m[2] = m[0] ^ m[1];

		return res.transpose();
	}

	T det() const {
		return	+ m[0].v[0] * m[1].v[1] * m[2].v[2]
				+ m[1].v[0] * m[2].v[1] * m[0].v[2]
				+ m[2].v[0] * m[0].v[1] * m[1].v[2]
				- m[0].v[0] * m[2].v[1] * m[1].v[2]
				- m[1].v[0] * m[0].v[1] * m[2].v[2]
				- m[2].v[0] * m[1].v[1] * m[0].v[2];
	}

	self_type operator~() const {
		return adjunct() / det();
	}

	self_type& operator*=(const T factor) {
		m[0] *= factor;
		m[1] *= factor;
		m[2] *= factor;

		return *this;
	}

	self_type& operator/=(const T factor) {
		return operator*=(T(1)/factor);
	}

	self_type operator*(const T factor) const {
		return self_type(*this) *= factor;
	}

	self_type operator/(const T factor) const {
		return self_type(*this) /= factor;
	}

	vector_type m[3];
};

template<class T>
class VDMatrix4 {
public:
	enum zero_type { zero };
	enum identity_type { identity };
	enum rotation_x_type { rotation_x };
	enum rotation_y_type { rotation_y };
	enum rotation_z_type { rotation_z };

	typedef T				value_type;
	typedef VDVector4<T>	vector_type;

	VDMatrix4() {}
	VDMatrix4(const VDMatrix3<T>& v) {
		m[0] = v.m[0];
		m[1] = v.m[1];
		m[2] = v.m[2];
		m[3] = vector_type(0, 0, 0, 1);
	}

	VDMatrix4(zero_type) { m[0] = m[1] = m[2] = m[3] = vector_type(0, 0, 0, 0); }
	VDMatrix4(identity_type) {
		m[0] = vector_type(1, 0, 0, 0);
		m[1] = vector_type(0, 1, 0, 0);
		m[2] = vector_type(0, 0, 1, 0);
		m[3] = vector_type(0, 0, 0, 1);
	}
	VDMatrix4(rotation_x_type, T angle) {
		const T s(sin(angle));
		const T c(cos(angle));

		m[0] = vector_type( 1, 0, 0, 0);
		m[1] = vector_type( 0, c,-s, 0);
		m[2] = vector_type( 0, s, c, 0);
		m[3] = vector_type( 0, 0, 0, 1);
	}
	VDMatrix4(rotation_y_type, T angle) {
		const T s(sin(angle));
		const T c(cos(angle));

		m[0] = vector_type( c, 0, s, 0);
		m[1] = vector_type( 0, 1, 0, 0);
		m[2] = vector_type(-s, 0, c, 0);
		m[3] = vector_type( 0, 0, 0, 1);
	}
	VDMatrix4(rotation_z_type, T angle) {
		const T s(sin(angle));
		const T c(cos(angle));

		m[0] = vector_type( c,-s, 0, 0);
		m[1] = vector_type( s, c, 0, 0);
		m[2] = vector_type( 0, 0, 1, 0);
		m[3] = vector_type( 0, 0, 0, 1);
	}

	const T *data() const { return &m[0][0]; }

	vector_type& operator[](int n) { return m[n]; }
	const vector_type& operator[](int n) const { return m[n]; }

	VDMatrix4 operator*(const VDMatrix4& v) const {
		VDMatrix4 result;

#define DO(i,j) result.m[i].v[j] = m[i].v[0]*v.m[0].v[j] + m[i].v[1]*v.m[1].v[j] + m[i].v[2]*v.m[2].v[j] + m[i].v[3]*v.m[3].v[j]
		DO(0,0);
		DO(0,1);
		DO(0,2);
		DO(0,3);
		DO(1,0);
		DO(1,1);
		DO(1,2);
		DO(1,3);
		DO(2,0);
		DO(2,1);
		DO(2,2);
		DO(2,3);
		DO(3,0);
		DO(3,1);
		DO(3,2);
		DO(3,3);
#undef DO

		return result;
	}

	VDMatrix4& operator*=(const VDMatrix4& v) {
		return operator=(operator*(v));
	}

	vector_type operator*(const VDVector3<T>& r) const {
		return vector_type(
				m[0].v[0]*r.v[0] + m[0].v[1]*r.v[1] + m[0].v[2]*r.v[2] + m[0].v[3],
				m[1].v[0]*r.v[0] + m[1].v[1]*r.v[1] + m[1].v[2]*r.v[2] + m[1].v[3],
				m[2].v[0]*r.v[0] + m[2].v[1]*r.v[1] + m[2].v[2]*r.v[2] + m[2].v[3],
				m[3].v[0]*r.v[0] + m[3].v[1]*r.v[1] + m[3].v[2]*r.v[2] + m[3].v[3]);
	}

	vector_type operator*(const vector_type& r) const {
		return vector_type(
				m[0].v[0]*r.v[0] + m[0].v[1]*r.v[1] + m[0].v[2]*r.v[2] + m[0].v[3]*r.v[3],
				m[1].v[0]*r.v[0] + m[1].v[1]*r.v[1] + m[1].v[2]*r.v[2] + m[1].v[3]*r.v[3],
				m[2].v[0]*r.v[0] + m[2].v[1]*r.v[1] + m[2].v[2]*r.v[2] + m[2].v[3]*r.v[3],
				m[3].v[0]*r.v[0] + m[3].v[1]*r.v[1] + m[3].v[2]*r.v[2] + m[3].v[3]*r.v[3]);
	}

	vector_type m[4];
};


typedef VDVector2<sint32>	vdvector2i;
typedef VDVector3<sint32>	vdvector3i;
typedef VDVector4<sint32>	vdvector4i;

typedef VDVector2<float>	vdvector2f;
typedef VDVector3<float>	vdvector3f;
typedef VDVector4<float>	vdvector4f;

typedef VDVector2<double>	vdvector2d;
typedef VDVector3<double>	vdvector3d;
typedef VDVector4<double>	vdvector4d;

typedef VDMatrix2<float>	vdmatrix2f;
typedef VDMatrix3<float>	vdmatrix3f;
typedef VDMatrix4<float>	vdmatrix4f;

typedef VDMatrix2<double>	vdmatrix2d;
typedef VDMatrix3<double>	vdmatrix3d;
typedef VDMatrix4<double>	vdmatrix4d;


template<class T>
struct VDSize {
	typedef T value_type;

	int w, h;

	VDSize() {}
	VDSize(int _w, int _h) : w(_w), h(_h) {}

	bool operator==(const VDSize& s) const { return w==s.w && h==s.h; }
	bool operator!=(const VDSize& s) const { return w!=s.w || h!=s.h; }

	VDSize& operator+=(const VDSize& s) {
		w += s.w;
		h += s.h;
		return *this;
	}

	T area() const { return w*h; }

	void include(const VDSize& s) {
		if (w < s.w)
			w = s.w;
		if (h < s.h)
			h = s.h;
	}
};

template<class T>
class VDRect {
public:
	typedef T value_type;

	VDRect() {}
	VDRect(T left_, T top_, T right_, T bottom_) : left(left_), top(top_), right(right_), bottom(bottom_) {}

	void clear() { left = top = right = bottom = 0; }

	void set(T l, T t, T r, T b) {
		left = l;
		top = t;
		right = r;
		bottom = b;
	}

	bool operator==(const VDRect& r) const { return left==r.left && top==r.top && right==r.right && bottom==r.bottom; }
	bool operator!=(const VDRect& r) const { return left!=r.left || top!=r.top || right!=r.right || bottom!=r.bottom; }

	T width() const { return right-left; }
	T height() const { return bottom-top; }
	T area() const { return (right-left)*(bottom-top); }
	VDSize<T> size() const { return VDSize<T>(right-left, bottom-top); }

	T left, top, right, bottom;
};

typedef VDSize<sint32>	vdsize32;
typedef	VDRect<sint32>	vdrect32;

#endif

//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifndef f_VD2_SYSTEM_VECTORS_H
#define f_VD2_SYSTEM_VECTORS_H

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
	VDFORCEINLINE VDVector2(T x, T y) {v[0]=x; v[1]=y;}
	VDFORCEINLINE VDVector2(const T src[2]) {v[0]=src[0]; v[1]=src[1];}

	T&			operator[](int k)					{ return v[k]; }
	const T&	operator[](int k) const				{ return v[k]; }

	T			lensq() const						{ return v[0]*v[0] + v[1]*v[1]; }
	T			len() const							{ return (T)sqrt(lensq()); }
	self_type	normalized() const					{ return *this / len(); }

	self_type	operator-() const					{ return self_type(-v[0], -v[1]); }

	self_type	operator+(const self_type& r) const	{ return self_type(v[0]+r.v[0], v[1]+r.v[1]); }
	self_type	operator-(const self_type& r) const	{ return self_type(v[0]-r.v[0], v[1]-r.v[1]); }
	T			operator*(const self_type& r) const	{ return v[0]*r.v[0] + v[1]*r.v[1]; }

	self_type&	operator+=(const self_type& r)		{ v[0]+=r.v[0]; v[1]+=r.v[1]; return *this; }
	self_type&	operator-=(const self_type& r)		{ v[0]-=r.v[0]; v[1]-=r.v[1]; return *this; }

	self_type	operator*(const T s) const			{ return self_type(v[0]*s, v[1]*s); }
	self_type&	operator*=(const T s)				{ v[0]*=s; v[1]*=s; return *this; }

	self_type	operator/(const T s) const			{ const T inv(T(1)/s); return self_type(v[0]*inv, v[1]*inv); }
	self_type&	operator/=(const T s)				{ const T inv(T(1)/s); v[0]*=inv; v[1]*=inv; return *this; }

	self_type	scaled(const T x, const T y) const	{ return self_type(v[0]*x, v[1]*y); }

	T v[2];
};

///////////////////////////////////////////////////////////////////////////

template<class T>
class VDVector3 {
public:
	typedef VDVector3 self_type;
	typedef T value_type;

	VDFORCEINLINE VDVector3() {}
	VDFORCEINLINE VDVector3(T x, T y, T z) { v[0]=x; v[1]=y; v[2]=z; }
	VDFORCEINLINE VDVector3(const T src[3]) { v[0]=src[0]; v[1]=src[1]; v[2]=src[2]; }

	T&			operator[](int k)					{ return v[k]; }
	const T&	operator[](int k) const				{ return v[k]; }

	T			lensq() const						{ return v[0]*v[0] + v[1]*v[1] + v[2]*v[2]; }
	T			len() const							{ return (T)sqrt(lensq()); }
	self_type	normalized() const					{ return *this / len(); }

	VDVector2<T> project() const					{ const T inv(T(1)/v[2]); return VDVector2<T>(v[0]*inv, v[1]*inv); }
	VDVector2<T> as2d() const						{ return VDVector2<T>(v[0], v[1]); }

	self_type	operator-() const					{ return self_type(-v[0], -v[1], -v[2]); }

	self_type	operator+(const self_type& r) const	{ return self_type(v[0]+r.v[0], v[1]+r.v[1], v[2]+r.v[2]); }
	self_type	operator-(const self_type& r) const	{ return self_type(v[0]-r.v[0], v[1]-r.v[1], v[2]-r.v[2]); }
	T			operator*(const self_type& r) const	{ return v[0]*r.v[0] + v[1]*r.v[1] + v[2]*r.v[2]; }
	self_type	operator^(const self_type& r) const	{ return self_type(v[1]*r.v[2] - v[2]*r.v[1], v[2]*r.v[0] - v[0]*r.v[2], v[0]*r.v[1] - v[1]*r.v[0]); }

	self_type&	operator+=(const self_type& r)		{ v[0]+=r.v[0]; v[1]+=r.v[1]; v[2]+=r.v[2]; return *this; }
	self_type&	operator-=(const self_type& r)		{ v[0]-=r.v[0]; v[1]-=r.v[1]; v[2]-=r.v[2]; return *this; }
	self_type&	operator^=(const self_type& r)		{ return operator=(*this ^ r); }

	self_type	operator*(const T s) const			{ return self_type(v[0]*s, v[1]*s, v[2]*s); }
	self_type&	operator*=(const T s)				{ v[0]*=s; v[1]*=s; v[2]*=s; return *this; }

	self_type	operator/(const T s) const			{ const T inv(T(1)/s); return self_type(v[0]*inv, v[1]*inv, v[2]*inv); }
	self_type&	operator/=(const T s)				{ const T inv(T(1)/s); v[0]*=inv; v[1]*=inv; v[2]*=inv; return *this; }

	self_type	scaled(const T x, const T y, const T z) const	{ return self_type(v[0]*x, v[1]*y, v[2]*z); }

	T v[3];
};

template<class T>
VDFORCEINLINE VDVector3<T> operator*(const T s, const VDVector3<T>& v) { return v*s; }

template<class T>
VDFORCEINLINE VDVector3<T> operator/(const T s, const VDVector3<T>& v) { return v/s; }

///////////////////////////////////////////////////////////////////////////

template<class T>
class VDVector4 {
public:
	typedef VDVector4 self_type;
	typedef T value_type;

	VDFORCEINLINE VDVector4() {}
	VDFORCEINLINE VDVector4(T x, T y, T z, T w) { v[0]=x; v[1]=y; v[2]=z; v[3]=w; }
	VDFORCEINLINE VDVector4(const T src[4]) { v[0]=src[0]; v[1]=src[1]; v[2]=src[2]; v[3]=src[3]; }

	T&			operator[](int i) { return v[i]; }
	const T&	operator[](int i) const { return v[i]; }

	T			lensq() const						{ return v[0]*v[0] + v[1]*v[1] + v[2]*v[2] + v[3]*v[3]; }
	T			len() const							{ return (T)sqrt(lensq()); }
	self_type	normalized() const					{ return *this / len(); }

	VDVector3<T> project() const					{ const T inv(T(1)/v[3]); return VDVector3<T>(v[0]*inv, v[1]*inv, v[2]*inv); }

	self_type	operator-() const					{ return self_type(-x, -y, -z, -w); }

	self_type	operator+(const self_type& r) const	{ return self_type(v[0]+r.v[0], v[1]+r.v[1], v[2]+r.v[2], v[3]+r.v[3]); }
	self_type	operator-(const self_type& r) const	{ return self_type(v[0]-r.v[0], v[1]-r.v[1], v[2]-r.v[2], v[3]-r.v[3]); }
	T			operator*(const self_type& r) const	{ return v[0]*r.v[0] + v[1]*r.v[1] + v[2]*r.v[2] + v[3]*r.v[3]; }

	self_type&	operator+=(const self_type& r)		{ v[0]+=r.v[0]; v[1]+=r.v[1]; v[2]+=r.v[2]; v[3]+=r.v[3]; return *this; }
	self_type&	operator-=(const self_type& r)		{ v[0]-=r.v[0]; v[1]-=r.v[1]; v[2]-=r.v[2]; v[3]-=r.v[3]; return *this; }

	self_type	operator*(const T factor)			{ return self_type(v[0]*factor, v[1]*factor, v[2]*factor, v[3]*factor); }
	self_type	operator/(const T factor)			{ const T inv(T(1) / factor); return self_type(v[0]*inv, v[1]*inv, v[2]*inv, v[3]*inv); }

	self_type&	operator*=(const T factor)			{ v[0] *= factor; v[1] *= factor; v[2] *= factor; v[3] *= factor; return *this; }
	self_type&	operator/=(const T factor)			{ const T inv(T(1) / factor); v[0] *= inv; v[1] *= inv; v[2] *= inv; v[3] *= inv; return *this; }

	T v[4];
};

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
	VDMatrix4(const VDMatrix3& v) {
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

#endif

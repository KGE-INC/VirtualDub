//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee
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

#ifndef f_INT128_H
#define f_INT128_H

class __declspec(novtable) int128 {
private:
	__int64 v[2];

public:
	int128() {}

	int128(signed __int64 x) {
		v[0] = x;
		v[1] = x>>63;
	}

	int128(unsigned __int64 x) {
		v[0] = (__int64)x;
		v[1] = 0;
	}

	int128(int x) {
		v[0] = x;
		v[1] = (__int64)x >> 63;
	}

	int128(unsigned int x) {
		v[1] = 0;
		v[0] = x;
	}

	int128(unsigned long x) {
		v[1] = 0;
		v[0] = x;
	}

	operator double() const throw();
	operator __int64() const throw() {
		return (__int64)v[0];
	}
	operator unsigned __int64() const throw() {
		return (unsigned __int64)v[0];
	}

	const int128 operator+(const int128& x) const throw();
	const int128& operator+=(const int128& x) throw();
	const int128 operator-(const int128& x) const throw();
	const int128 int128::operator*(const int128& x) const throw();
	const int128 int128::operator<<(int v) const throw();
	const int128 int128::operator>>(int v) const throw();
	const int128 int128::operator-() const throw();
	const int128 int128::abs() const throw();
};

#endif

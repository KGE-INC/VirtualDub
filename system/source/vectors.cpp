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

#include <vector>
#include <vd2/system/vectors.h>

bool VDSolveLinearEquation(double *src, int n, ptrdiff_t stride_elements, double *b, double tolerance) {
	std::vector<double *> array(n);
	double **m = &array[0];
	int i, j, k;

	for(i=0; i<n; ++i) {
		m[i] = src;
		src += stride_elements;
	}

	// factor U
	for(i=0; i<n; ++i) {
		int best = i;

		for(j=i+1; j<n; ++j) {
			if (fabs(m[best][i]) < fabs(m[j][i]))
				best = j;
		}

		std::swap(m[i], m[best]);
		std::swap(b[i], b[best]);

		if (fabs(m[i][i]) < tolerance)
			return false;

		double f = 1.0 / m[i][i];

		m[i][i] = 1.0;

		for(j=i+1; j<n; ++j)
			m[i][j] *= f;

		b[i] *= f;

		for(j=i+1; j<n; ++j) {
			b[j] -= b[i] * m[j][i];
			for(k=n-1; k>=i; --k)
				m[j][k] -= m[i][k] * m[j][i];
		}
	}

	// factor L
	for(i=n-1; i>=0; --i)
		for(j=i-1; j>=0; --j)
			b[j] -= b[i] * m[j][i];

	return true;
}

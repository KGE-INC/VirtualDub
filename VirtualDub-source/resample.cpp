//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2000 Avery Lee
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

#include <crtdbg.h>

#include <math.h>

//	void MakeCubic4Table(
//		int *table,			pointer to 256x4 int array
//		double A,			'A' value - determines characteristics
//		mmx_table);			generate interleaved table
//
//	Generates a table suitable for cubic 4-point interpolation.
//
//	Each 4-int entry is a set of four coefficients for a point
//	(n/256) past y[1].  They are in /16384 units.
//
//	A = -1.0 is the original VirtualDub bicubic filter, but it tends
//	to oversharpen video, especially on rotates.  Use A = -0.75
//	for a filter that resembles Photoshop's.


void MakeCubic4Table(int *table, double A, bool mmx_table) throw() {
	int i;

	for(i=0; i<256; i++) {
		double d = (double)i / 256.0;
		int y1, y2, y3, y4, ydiff;

		// Coefficients for all four pixels *must* add up to 1.0 for
		// consistent unity gain.
		//
		// Two good values for A are -1.0 (original VirtualDub bicubic filter)
		// and -0.75 (closely matches Photoshop).

		y1 = (int)floor(0.5 + (        +     A*d -       2.0*A*d*d +       A*d*d*d) * 16384.0);
		y2 = (int)floor(0.5 + (+ 1.0             -     (A+3.0)*d*d + (A+2.0)*d*d*d) * 16384.0);
		y3 = (int)floor(0.5 + (        -     A*d + (2.0*A+3.0)*d*d - (A+2.0)*d*d*d) * 16384.0);
		y4 = (int)floor(0.5 + (                  +           A*d*d -       A*d*d*d) * 16384.0);

		// Normalize y's so they add up to 16384.

		ydiff = (16384 - y1 - y2 - y3 - y4)/4;
		_ASSERT(ydiff > -16 && ydiff < 16);

		y1 += ydiff;
		y2 += ydiff;
		y3 += ydiff;
		y4 += ydiff;

		if (mmx_table) {
			table[i*4 + 0] = table[i*4 + 1] = (y2<<16) | (y1 & 0xffff);
			table[i*4 + 2] = table[i*4 + 3] = (y4<<16) | (y3 & 0xffff);
		} else {
			table[i*4 + 0] = y1;
			table[i*4 + 1] = y2;
			table[i*4 + 2] = y3;
			table[i*4 + 3] = y4;
		}
	}
}

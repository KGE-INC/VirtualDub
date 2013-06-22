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

#include "stdafx.h"

#include "VBitmap.h"
#include <vd2/system/error.h>

#include "SceneDetector.h"

SceneDetector::SceneDetector(PixDim width, PixDim height) {
	cur_lummap = last_lummap = NULL;
	last_valid = FALSE;
	first_diff = TRUE;

	cut_threshold = 50 * tile_w * tile_h;
	fade_threshold = 4 * tile_w * tile_h;

	try {
		tile_w = (width + 7)/8;
		tile_h = (height + 7)/8;

		if (	!(cur_lummap  = new Pixel[tile_h * tile_w])
			||	!(last_lummap = new Pixel[tile_h * tile_w]))

			throw MyMemoryError();
	} catch(...) {
		_destruct();
		throw;
	}
}

void SceneDetector::_destruct() {
	delete[] cur_lummap;
	delete[] last_lummap;
}

SceneDetector::~SceneDetector() {
	_destruct();
}

//////////////////////////////////////////////////////////////////////////

void SceneDetector::SetThresholds(int cut_threshold, int fade_threshold) {
	this->cut_threshold		= (cut_threshold * tile_w * tile_h) >> 4;
	this->fade_threshold	= (fade_threshold * tile_w * tile_h)/16.0f;
}

BOOL SceneDetector::Submit(VBitmap *vbm) {
	long last_frame_diffs = 0;
	long lum_total = 0;
	double lum_sq_total = 0.0;
	long len = tile_w * tile_h;

	if (vbm->w > tile_w*8 || vbm->h > tile_h*8 || (!cut_threshold && !fade_threshold))
		return FALSE;

	FlipBuffers();
	BitmapToLummap(cur_lummap, vbm);

	if (!last_valid) {
		last_valid = TRUE;
		return FALSE;
	}

/////////////

	const Pixel *t1 = cur_lummap, *t2 = last_lummap;

	do {
		Pixel c1 = *t1++;
		Pixel c2 = *t2++;

		last_frame_diffs +=(   54*abs((int)(c2>>16)-(int)(c1>>16))
							+ 183*abs((int)((c2>>8)&255) -(int)((c1>>8)&255))
							+  19*abs((int)(c2&255)-(int)(c1&255))) >> 8;

		long lum = ((c1>>16)*54 + ((c1>>8)&255)*183 + (c1&255)*19 + 128)>>8;

		lum_total += lum;
		lum_sq_total += (double)lum * (double)lum;
	} while(--len);

	const double tile_count = tile_w * tile_h;

//	_RPT3(0,"Last frame diffs=%ld, lum(linear)=%ld, lum(rms)=%f\n",last_frame_diffs,lum_total,sqrt(lum_sq_total));

	if (fade_threshold) {
		// Var(X)	= E(X^2) - E(X)^2 
		//			= sum(X^2)/N - sum(X)^2 / N^2
		// SD(X)	= sqrt(N * sum(X^2) - sum(X)^2)) / N

		bool is_fade = sqrt(lum_sq_total * tile_count - (double)lum_total * lum_total) < fade_threshold;

		if (first_diff) {
			last_fade_state = is_fade;
			first_diff = false;
		} else {
			// If we've encountered a new fade, return 'scene changed'

			if (!last_fade_state && is_fade) {
				last_fade_state = true;
				return true;
			}

			// Hit the end of an initial fade?

			if (last_fade_state && !is_fade)
				last_fade_state = false;
		}
	}

	// Cut/dissolve detection

	return cut_threshold ? last_frame_diffs > cut_threshold : FALSE;
}

void SceneDetector::Reset() {
	last_valid = FALSE;
}

//////////////////////////////////////////////////////////////////////////

namespace {
	Pixel scene_lumtile32(void *src0, long w, long h, ptrdiff_t pitch) {
		w <<= 2;

		const char *src = (const char *)src0 + w;
		uint32 rb_total = 0;
		uint32 g_total = 0;

		w = -w;
		do {
			long x = w;

			do {
				const uint32 px = *(uint32 *)(src + x);

				rb_total += px & 0xff00ff;
				g_total += px & 0x00ff00;
			} while(x += 4);

			src += pitch;
		} while(--h);

		return (((rb_total + 0x00200020) & 0x3fc03fc0) + ((g_total + 0x00002000) & 0x003fc000)) >> 6;
	}

	Pixel scene_lumtile24(void *src0, long w, long h, ptrdiff_t pitch) {
		const uint8 *src = (const uint8 *)src0;
		pitch -= 3*w;
		uint32 r_total = 0;
		uint32 g_total = 0;
		uint32 b_total = 0;
		do {
			long x = w;

			do {
				b_total += src[0];
				g_total += src[1];
				r_total += src[2];
				src += 3;
			} while(--x);

			src += pitch;
		} while(--h);

		r_total = (r_total + 0x20) >> 6;
		g_total = (g_total + 0x20) >> 6;
		b_total = (b_total + 0x20) >> 6;
		return (r_total << 16) + (g_total << 8) + b_total;
	}

	Pixel scene_lumtile16(void *src0, long w, long h, ptrdiff_t pitch) {
		w += w;

		const char *src = (const char *)src0 + w;
		uint32 r_total = 0;
		uint32 g_total = 0;
		uint32 b_total = 0;

		w = -w;
		do {
			long x = w;

			do {
				const uint32 px = *(uint16 *)(src + x);

				r_total += px & 0x7c00;
				g_total += px & 0x03e0;
				b_total += px & 0x001f;
			} while(x += 2);

			src += pitch;
		} while(--h);

		r_total = (r_total + 0x1000) << 3;
		g_total = (g_total + 0x0080);
		b_total = (b_total + 0x0004) >> 3;
		return (r_total & 0xff0000) + (g_total & 0x00ff00) + (b_total & 0x0000ff);
	}
}

#ifdef _M_IX86
	extern "C" Pixel __cdecl asm_scene_lumtile32(void *src, long w, long h, long pitch);
	extern "C" Pixel __cdecl asm_scene_lumtile24(void *src, long w, long h, long pitch);
	extern "C" Pixel __cdecl asm_scene_lumtile16(void *src, long w, long h, long pitch);
#else
	#define asm_scene_lumtile32 scene_lumtile32
	#define asm_scene_lumtile24 scene_lumtile24
	#define asm_scene_lumtile16 scene_lumtile16
#endif

void SceneDetector::BitmapToLummap(Pixel *lummap, VBitmap *vbm) {
	char *src, *src_row = (char *)vbm->data;
	int mh = 8;
	long w,h;

	h = (vbm->h+7)/8;

	switch(vbm->depth) {
	case 32:
		do {
			if (h<=1 && (vbm->h&7)) mh = vbm->h&7;

			src = src_row; src_row += vbm->pitch*8;
			w = vbm->w/8;
			do {
				*lummap++ = asm_scene_lumtile32(src, 8, mh, vbm->pitch);
				src += 32;
			} while(--w);

			if (vbm->w & 7) {
				*lummap++ = asm_scene_lumtile32(src, vbm->w&7, mh, vbm->pitch);
			}
		} while(--h);
		break;
	case 24:
		do {
			if (h<=1 && (vbm->h&7)) mh = vbm->h&7;

			src = src_row; src_row += vbm->pitch*8;
			w = vbm->w/8;
			do {
				*lummap++ = asm_scene_lumtile24(src, 8, mh, vbm->pitch);
				src += 24;
			} while(--w);

			if (vbm->w & 7) {
				*lummap++ = asm_scene_lumtile24(src, vbm->w&7, mh, vbm->pitch);
			}
		} while(--h);
		break;
	case 16:
		do {
			if (h<=1 && (vbm->h&7)) mh = vbm->h&7;

			src = src_row; src_row += vbm->pitch*8;
			w = vbm->w/8;
			do {
				*lummap++ = asm_scene_lumtile16(src, 8, mh, vbm->pitch);
				src += 16;
			} while(--w);

			if (vbm->w & 6) {
				*lummap++ = asm_scene_lumtile16(src, vbm->w&6, mh, vbm->pitch);
			}
		} while(--h);
		break;
	}
}

void SceneDetector::FlipBuffers() {
	Pixel *t;

	t = cur_lummap;
	cur_lummap = last_lummap;
	last_lummap = t;
}

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

#ifndef f_MPEG_TABLES_H
#define f_MPEG_TABLES_H

#define DCT_FULL_SEARCH (0)
#define DCT_ESCAPE (128)
#define DCT_EOB (129)

extern const char			mpeg_macro_block_inc_decode[96*2];
extern const char			mpeg_macro_block_inc_decode2[58*2];
extern const unsigned char mpeg_block_pattern_decode1[128*2];
extern const unsigned char mpeg_block_pattern_decode0[24*2];
extern const unsigned char	mpeg_p_type_mb_type_decode[32*2];
extern const unsigned char	mpeg_b_type_mb_type_decode[32*2];
extern const unsigned char	mpeg_dct_size_luminance_decode[64*2];
extern const unsigned char	mpeg_dct_size_chrominance_decode[64*2];
extern const char			mpeg_motion_code_decode[96*2];
extern const char			mpeg_motion_code_decode2[58*2];
extern const unsigned char	mpeg_dct_coeff_decode0[88*4];
extern const unsigned char	mpeg_dct_coeff_decode1[8*2];
extern const unsigned char	mpeg_dct_coeff_decode2[512*4];

#endif

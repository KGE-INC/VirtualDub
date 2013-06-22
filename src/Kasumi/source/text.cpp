//	VirtualDub - Video processing and capture application
//	Graphics support library
//	Copyright (C) 1998-2007 Avery Lee
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

#include <vd2/system/math.h>
#include <vd2/Kasumi/region.h>
#include <vd2/Kasumi/text.h>

#include "defaultfont.inl"

void VDPixmapConvertTextToPath(VDPixmapPathRasterizer& rast, const VDOutlineFontInfo *pFont, float size, float x, float y, const char *pText, const float transform[2][2]) {
	if (!pFont)
		pFont = &g_VDDefaultFont_FontInfo;

	vdfastvector<vdint2> points;

	float scale = size / ((float)pFont->mEmSquare * 255.0f * 65536.0f);
	float xscale = (float)(pFont->mMaxX - pFont->mMinX) * scale;
	float yscale = -(float)(pFont->mMaxY - pFont->mMinY) * scale;

	float xinitstep = pFont->mMinX * 255.0f * scale;
	float yinitstep = -pFont->mMaxY * scale - pFont->mDescent * size / (float)pFont->mEmSquare;

	static const float kIdentity[2][2]={1,0,0,1};

	if (!transform)
		transform = kIdentity;

	float xoffset = x + xinitstep * transform[0][0] + yinitstep * transform[0][1];
	float yoffset = y + xinitstep * transform[1][0] + yinitstep * transform[1][1];

	while(const char c = *pText++) {
		int index = (unsigned char)c - pFont->mStartGlyph;

		if (index >= pFont->mEndGlyph - pFont->mStartGlyph)
			continue;

		const VDOutlineFontGlyphInfo& glyph = pFont->mpGlyphArray[index];
		const VDOutlineFontGlyphInfo& glyphNext = pFont->mpGlyphArray[index + 1];
		const uint16 *pPoints = pFont->mpPointArray + glyph.mPointArrayStart;
		const uint8 *pCommands = pFont->mpCommandArray + glyph.mCommandArrayStart;
		int nPoints = glyphNext.mPointArrayStart - glyph.mPointArrayStart;
		int nCommands = glyphNext.mCommandArrayStart - glyph.mCommandArrayStart;

		points.clear();
		points.resize(nPoints);

		for(int i=0; i<nPoints; ++i) {
			uint16 pt = *pPoints++;
			float fx1 = (pt & 255) * xscale;
			float fy1 = (pt >> 8) * yscale;
			points[i].set(VDRoundToInt(fx1*transform[0][0] + fy1*transform[0][1] + xoffset), VDRoundToInt(fx1*transform[1][0] + fy1*transform[1][1] + yoffset));
		}

		const vdint2 *srcpt = points.data();
		const vdint2 *startpt = points.data();

		while(nCommands--) {
			uint8 cmd = *pCommands++;
			int countm1 = (cmd & 0x7f) >> 2;

			for(int i=0; i<=countm1; ++i) {
				switch(cmd & 3) {
				case 2:
					rast.Line(srcpt[0], srcpt[1]);
					++srcpt;
					break;

				case 3:
					rast.QuadraticBezier(srcpt);
					srcpt += 2;
					break;
				}
			}

			if (cmd & 0x80) {
				rast.Line(*srcpt, *startpt);
				startpt = ++srcpt;
			}
		}

		float step = (glyph.mAWidth + glyph.mBWidth + glyph.mCWidth) * (size / (float)pFont->mEmSquare);
		xoffset += step * transform[0][0];
		yoffset += step * transform[1][0];
	}
}

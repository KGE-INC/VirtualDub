//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2004 Avery Lee
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

#include <stdafx.h>
#include <vd2/Dita/w32control.h>

class VDUILabelW32 : public VDUIControlW32 {
public:
	bool Create(IVDUIParameters *);
	void PreLayoutBaseW32(const VDUILayoutSpecs&);
};

extern IVDUIWindow *VDCreateUILabel() { return new VDUILabelW32; }

bool VDUILabelW32::Create(IVDUIParameters *pParameters) {
	return CreateW32(pParameters, "STATIC", SS_CENTERIMAGE|SS_LEFT);
}

void VDUILabelW32::PreLayoutBaseW32(const VDUILayoutSpecs& parentConstraints) {
	SIZE siz = SizeText(0, 0, 0);

	mLayoutSpecs.minsize.w	= siz.cx;
	mLayoutSpecs.minsize.h	= siz.cy;
}

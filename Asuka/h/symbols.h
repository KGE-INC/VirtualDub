//	Asuka - VirtualDub Build/Post-Mortem Utility
//	Copyright (C) 2005 Avery Lee
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

#ifndef f_VD2_ASUKA_SYMBOLS_H
#define f_VD2_ASUKA_SYMBOLS_H

#include <vd2/system/vdtypes.h>
#include <vector>

struct VDSymbol {
	long rva;
	int group;
	long start;
	char *name;
};

struct VDSection {
	long	mAbsStart;
	long	mStart;
	long	mLength;
	int		mGroup;

	VDSection(long s=0, long l=0, int g=0) : mStart(s), mLength(l), mGroup(g) {}
};

class VDINTERFACE IVDSymbolSource {
public:
	virtual ~IVDSymbolSource() {}

	virtual void Init(const wchar_t *filename) = 0;
	virtual const VDSymbol *LookupSymbol(uint32 addr) = 0;
	virtual const VDSection *LookupSection(uint32 addr) = 0;
	virtual void GetAllSymbols(std::vector<VDSymbol>&) = 0;

	virtual uint32 GetCodeGroupMask() = 0;

	virtual int GetSectionCount() = 0;
	virtual const VDSection *GetSection(int sec) = 0;

	virtual bool LookupLine(uint32 addr, const char *& filename, int& lineno) = 0;
};

IVDSymbolSource *VDCreateSymbolSourceLinkMap();

#endif

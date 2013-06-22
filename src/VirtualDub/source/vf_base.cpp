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

#include "stdafx.h"
#include "vf_base.h"

sint32 VDVideoFilterBase::Main(sint32 cmd, sint64 n, void *p, sint32 size) {
	switch(cmd) {
	case kVFVCmd_Null:		return 0;
	case kVFVCmd_Destroy:	delete this;
							return 0;
	case kVFVCmd_Ext:		return Ext((const char *)p);
	case kVFVCmd_Run:		return Run();
	case kVFVCmd_Prefetch:	Prefetch(n); return 0;
	case kVFVCmd_Prepare:	return Prepare();
	case kVFVCmd_Start:		Start();	return 0;
	case kVFVCmd_Stop:		Stop();		return 0;
	case kVFVCmd_Suspend:	return Suspend(p, size);
	case kVFVCmd_Resume:	Resume(p, size); return 0;
	case kVFVCmd_GetParam:	return (sint32)GetParam((unsigned)n, p, size);
	case kVFVCmd_SetParam:	SetParam((unsigned)n, p, size); return 0;
	case kVFVCmd_Config:	return Config(*(HWND *)p);
	case kVFVCmd_Blurb:		return (uint32)GetBlurb((wchar_t *)p, (size_t)size);
	case kVFVCmd_PrefetchSrcPrv:	return PrefetchSrcPrv(n);
	case kVFVCmd_GetDirectRange:
		return GetDirectRange(	((VFVCmdInfo_GetDirectRange *)p)->pos,
								((VFVCmdInfo_GetDirectRange *)p)->len);
	}

	return -1;
}

sint32 VDVideoFilterBase::Run() {
	return 0;
}

void VDVideoFilterBase::Prefetch(sint64 frame) {
}

sint32 VDVideoFilterBase::PrefetchSrcPrv(sint64 frame) {
	return -1;
}

int VDVideoFilterBase::GetDirectRange(sint64& pos, sint32& len) {
	return -1;
}

sint32 VDVideoFilterBase::Prepare() {
	return 0;
}

void VDVideoFilterBase::Start() {
}

void VDVideoFilterBase::Stop() {
}

unsigned VDVideoFilterBase::Suspend(void *dst, unsigned size) {
	return 0;
}

void VDVideoFilterBase::Resume(const void *src, unsigned size) {
}

const nsVDVideoFilterBase::ConfigEntryExt *VDVideoFilterBase::GetParamEntry(const unsigned idx) {
	const VDPluginConfigEntry *pEnt = mpContext->mpDefinition->mpConfigInfo;

	if (pEnt)
		for(; pEnt->next; pEnt=pEnt->next) {
			if (pEnt->idx == idx)
				return (nsVDVideoFilterBase::ConfigEntryExt *)(pEnt);
		}

	return NULL;
}

unsigned VDVideoFilterBase::GetParam(unsigned idx, void *dst, unsigned size) {
	const nsVDVideoFilterBase::ConfigEntryExt *pEnt = GetParamEntry(idx);

	if (pEnt) {
		char *ptr = (char *)GetConfigPtr() + pEnt->objoffset;

		using namespace nsVDVideoFilterBase;
		unsigned l;

		switch(pEnt->info.type) {
		case VDPluginConfigEntry::kTypeU32:
			if (size >= sizeof(uint32))
				*(Type_U32 *)dst = *(Type_U32 *)ptr;
			return sizeof(uint32);
		case VDPluginConfigEntry::kTypeS32:
			if (size >= sizeof(sint32))
				*(Type_S32 *)dst = *(Type_S32 *)ptr;
			return sizeof(sint32);
		case VDPluginConfigEntry::kTypeU64:
			if (size >= sizeof(uint64))
				*(Type_U64 *)dst = *(Type_U64 *)ptr;
			return sizeof(uint64);
		case VDPluginConfigEntry::kTypeS64:
			if (size >= sizeof(sint64))
				*(Type_S64 *)dst = *(Type_S64 *)ptr;
			return sizeof(sint64);
		case VDPluginConfigEntry::kTypeDouble:
			if (size >= sizeof(double))
				*(Type_Double *)dst = *(Type_Double *)ptr;
			return sizeof(double);
		case VDPluginConfigEntry::kTypeAStr:
			{
				Type_AStr& str = *(Type_AStr *)ptr;
				l = str.size() + 1;
				if (size >= l)
					memcpy(dst, str.c_str(), l);
				return l;
			}

		case VDPluginConfigEntry::kTypeWStr:
			{
				Type_WStr& str = *(Type_WStr *)ptr;
				l = (str.size() + 1) * sizeof(wchar_t);
				if (size >= l)
					memcpy(dst, str.c_str(), l);
				return l;
			}

		case VDPluginConfigEntry::kTypeBlock:
			{
				Type_Block& blk = *(Type_Block *)ptr;
				if (size >= blk.size())
					memcpy(dst, &blk[0], size);
				return blk.size();
			}

		}
	}

	return 0;
}

void VDVideoFilterBase::SetParam(unsigned idx, const void *src, unsigned size) {
	const nsVDVideoFilterBase::ConfigEntryExt *pEnt = GetParamEntry(idx);

	if (pEnt) {
		char *ptr = (char *)GetConfigPtr() + pEnt->objoffset;

		using namespace nsVDVideoFilterBase;

		switch(pEnt->info.type) {
		case VDPluginConfigEntry::kTypeU32:		*(Type_U32 *)ptr = *(Type_U32 *)src; break;
		case VDPluginConfigEntry::kTypeS32:		*(Type_S32 *)ptr = *(Type_S32 *)src; break;
		case VDPluginConfigEntry::kTypeU64:		*(Type_U64 *)ptr = *(Type_U64 *)src; break;
		case VDPluginConfigEntry::kTypeS64:		*(Type_S64 *)ptr = *(Type_S64 *)src; break;
		case VDPluginConfigEntry::kTypeDouble:	*(Type_Double *)ptr = *(Type_Double *)src; break;
		case VDPluginConfigEntry::kTypeAStr: 	*(Type_AStr *)ptr = (const char *)src; break;
		case VDPluginConfigEntry::kTypeWStr: 	*(Type_WStr *)ptr = (const wchar_t *)src; break;
		case VDPluginConfigEntry::kTypeBlock:
			{
				Type_Block& blk = *(Type_Block *)ptr;
				blk.resize(size);
				memcpy(&blk[0], src, size);
			}
			break;
		}
	}
}

bool VDVideoFilterBase::Config(HWND hwnd) {
	return false;
}

size_t VDVideoFilterBase::GetBlurb(wchar_t *buf, size_t len) {
	buf[0] = 0;
	return 0;
}

///////////////////////////////////////////////////////////////////////////

sint32 VDAPIENTRY VDVideoFilterBase::MainProc(const VDVideoFilterContext *pContext, sint32 cmd, sint64 n, void *p, sint32 size) {
	return ((VDVideoFilterBase *)pContext->mpFilterData)->Main(cmd, n, p, size);
}

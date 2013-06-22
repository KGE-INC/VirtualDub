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

#ifndef f_VD2_VF_BASE_H
#define f_VD2_VF_BASE_H

#include <vector>
#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdvideofilt.h>
#include <vd2/system/VDString.h>

namespace nsVDVideoFilterBase {
	typedef uint32				Type_U32;
	typedef uint64				Type_U64;
	typedef sint32				Type_S32;
	typedef sint64				Type_S64;
	typedef double				Type_Double;
	typedef VDStringA			Type_AStr;
	typedef	VDStringW			Type_WStr;
	typedef	std::vector<char>	Type_Block;

	struct ConfigEntryExt {
		VDFilterConfigEntry info;
		ptrdiff_t objoffset;
	};
}

#define VDVFBASE_BEGIN_CONFIG(filtername_)					\
	namespace nsVDVFConfigInfo_##filtername_ {				\
		template<int n> struct ConfigInfo { static const nsVDVideoFilterBase::ConfigEntryExt members; };	\
		template<int n> const nsVDVideoFilterBase::ConfigEntryExt ConfigInfo<n>::members = {0}

#define VDVFBASE_STRUCT_ENTRY(filtername_, idx_, type_, name_, sdesc_, ldesc_)						\

#define VDVFBASE_CONFIG_ENTRY(filtername_, idx_, type_, name_, sdesc_, ldesc_)						\
		template<> struct ConfigInfo<idx_>;										\
		template<> struct ConfigInfo<idx_> : public ConfigInfo<idx_-1> {		\
			nsVDVideoFilterBase::Type_##type_ name_;							\
			static const nsVDVideoFilterBase::ConfigEntryExt members;			\
		};																		\
																				\
		const nsVDVideoFilterBase::ConfigEntryExt ConfigInfo<idx_>::members={ &ConfigInfo<idx_-1>::members.info, idx_, VDFilterConfigEntry::kType##type_, L#name_, sdesc_, ldesc_, offsetof(ConfigInfo<idx_>, name_) }
		
#define VDVFBASE_END_CONFIG(filtername_, idx_)		\
		}											\
		typedef nsVDAFConfigInfo_##filtername_::ConfigInfo<idx_> VDVideoFilterData_##filtername_


class VDVideoFilterBase {
public:
	VDVideoFilterBase(const VDVideoFilterContext *pContext) : mpContext(pContext) {}
	virtual ~VDVideoFilterBase() {}

	virtual sint32	Ext(const char *pInterfaceName) { return NULL; }
	virtual sint32	Run();
	virtual void	Prefetch(sint64 frame);
	virtual sint32	Prepare();
	virtual void	Start();
	virtual void	Stop();
	virtual unsigned Suspend(void *dst, unsigned size);
	virtual void	Resume(const void *src, unsigned size);
	virtual unsigned GetParam(unsigned idx, void *dst, unsigned size);
	virtual void	SetParam(unsigned idx, const void *src, unsigned size);
	virtual bool	Config(HWND hwnd);

	//////////////////////

	virtual void *GetConfigPtr() { return 0; }

	//////////////////////

	static sint32 VDAPIENTRY MainProc(const VDVideoFilterContext *pContext, sint32 cmd, sint64 n, void *p, sint32 size);

protected:
	sint32 Main(sint32 cmd, sint64 n, void *p, sint32 size);

	const nsVDVideoFilterBase::ConfigEntryExt *GetParamEntry(const unsigned idx);

	const VDVideoFilterContext		*const mpContext;
};

#endif

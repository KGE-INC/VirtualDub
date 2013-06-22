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

#include "stdafx.h"

#include <windows.h>
#include <mmsystem.h>

#include "indeo_if.h"

#include "ScriptInterpreter.h"
#include "ScriptValue.h"
#include "ScriptError.h"
#include <vd2/system/error.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/file.h>
#include <vd2/system/log.h>
#include <vd2/system/VDString.h>
#include <vd2/Dita/services.h>
#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdaudiofilt.h>
#include <vd2/plugin/vdvideofilt.h>
#include "VideoSource.h"
#include "AudioFilterSystem.h"
#include "InputFile.h"
#include "project.h"

#include "command.h"
#include "dub.h"
#include "gui.h"
#include "prefs.h"
#include "resource.h"
#include "script.h"
#include "plugins.h"

extern DubOptions g_dubOpts;
extern HWND g_hWnd;

extern const char g_szError[];
extern bool g_fJobMode;
extern int g_returnCode;

extern VDProject *g_project;

extern HINSTANCE g_hInst;

extern vdrefptr<AudioSource> inputAudio;

extern const char *VDGetStartupArgument(int index);

extern void FreeCompressor(COMPVARS *pCompVars);
static VDScriptValue RootHandler(IVDScriptInterpreter *isi, char *szName, void *lpData);

///////////////////////////////////////////////

void RunScript(const wchar_t *name, void *hwnd) {
	static const wchar_t fileFilters[]=
				L"All scripts (*.vcf,*.syl,*.jobs)\0"		L"*.vcf;*.syl;*.jobs\0"
				L"VirtualDub configuration file (*.vcf)\0"	L"*.vcf\0"
				L"Sylia script for VirtualDub (*.syl)\0"	L"*.syl\0"
				L"VirtualDub job queue (*.jobs)\0"			L"*.jobs\0"
				L"All Files (*.*)\0"						L"*.*\0"
				;

	VDStringW filenameW;

	if (!name) {
		filenameW = VDGetLoadFileName(VDFSPECKEY_SCRIPT, (VDGUIHandle)hwnd, L"Load configuration script", fileFilters, L"script", 0, 0);

		if (filenameW.empty())
			return;

		name = filenameW.c_str();
	}

	const char *line = NULL;
	int lineno = 1;

	VDTextInputFile f(name);

	vdautoptr<IVDScriptInterpreter> isi(VDCreateScriptInterpreter());

	g_project->BeginTimelineUpdate();

	try {
		isi->SetRootHandler(RootHandler, NULL);

		while(line = f.GetNextLine())
			isi->ExecuteLine(line);
	} catch(const VDScriptError& cse) {
		int pos = isi->GetErrorLocation();
		int prelen = std::min<int>(pos, 50);
		const char *s = line ? line : "";

		throw MyError("Error during script execution at line %d, column %d: %s\n\n"
						"    %.*s<!>%.50s"
					, lineno
					, pos+1
					, isi->TranslateScriptError(cse)
					, prelen
					, s + pos - prelen
					, s + pos);
	}

	g_project->EndTimelineUpdate();
}

void RunScriptMemory(char *mem) {
	vdautoptr<IVDScriptInterpreter> isi(VDCreateScriptInterpreter());

	try {
		vdfastvector<char> linebuffer;
		char *s=mem, *t;

		isi->SetRootHandler(RootHandler, NULL);

		while(*s) {
			t = s;
			while(*t && *t!='\n') ++t;

			linebuffer.resize(t+1-s);
			memcpy(linebuffer.data(), s, t-s);
			linebuffer[t-s] = 0;

			isi->ExecuteLine(linebuffer.data());

			s = t;
			if (*s=='\n') ++s;
		}

	} catch(const VDScriptError& cse) {
		throw MyError("%s", isi->TranslateScriptError(cse));
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	General script-handler helper routines.
//
///////////////////////////////////////////////////////////////////////////

static bool strfuzzycompare(const char *s, const char *t) {
	char c,d;

	// Collapse spaces to one, convert symbols to _, and letters to uppercase...

	do {
		c = *s++;

		if (isalpha((unsigned char)c))
			c=(char)toupper((unsigned char)c);

		else if (isspace((unsigned char)c)) {
			c = ' ';
			while(*s && isspace((unsigned char)*s)) ++s;
		} else if (c)
			c = '_';

		d = *t++;

		if (isalpha((unsigned char)d))
			d=(char)toupper((unsigned char)d);
		else if (isspace((unsigned char)d)) {
			d = ' ';
			while(*t && isspace((unsigned char)*t)) ++t;
		} else if (d)
			d = '_';

		if (c!=d) break;
	} while(c && d);

	return c==d;
}

static char base64[]=
	"ABCDEFGHIJKLMNOP"
	"QRSTUVWXYZabcdef"
	"ghijklmnopqrstuv"
	"wxyz0123456789+/"
	"=";

void memunbase64(char *t, const char *s, long cnt) {

	char c1, c2, c3, c4, *ind, *limit = t+cnt;
	long v;

	for(;;) {
		while((c1=*s++) && !(ind = strchr(base64,c1)));
		if (!c1) break;
		c1 = (char)(ind-base64);

		while((c2=*s++) && !(ind = strchr(base64,c2)));
		if (!c2) break;
		c2 = (char)(ind-base64);

		while((c3=*s++) && !(ind = strchr(base64,c3)));
		if (!c3) break;
		c3 = (char)(ind-base64);

		while((c4=*s++) && !(ind = strchr(base64,c4)));
		if (!c4) break;
		c4 = (char)(ind-base64);

		// [c1,c4] valid -> 24 bits (3 bytes)
		// [c1,c3] valid -> 18 bits (2 bytes)
		// [c1,c2] valid -> 12 bits (1 byte)
		// [c1] valid    ->  6 bits (0 bytes)

		v = ((c1 & 63)<<18) | ((c2 & 63)<<12) | ((c3 & 63)<<6) | (c4 & 63);

		if (c1!=64 && c2!=64) {
			*t++ = (char)(v >> 16);

			if (t >= limit) return;

			if (c3!=64) {
				*t++ = (char)(v >> 8);
				if (t >= limit) return;
				if (c4!=64) {
					*t++ = (char)v;
					if (t >= limit) return;
					continue;
				}
			}
		}
		break;
	}
}

void membase64(char *t, const char *s, long l) {

	unsigned char c1, c2, c3;

	while(l>0) {
		c1 = (unsigned char)s[0];
		if (l>1) {
			c2 = (unsigned char)s[1];
			if (l>2)
				c3 = (unsigned char)s[2];
		}

		t[0] = base64[(c1 >> 2) & 0x3f];
		if (l<2) {
			t[1] = base64[(c1<<4)&0x3f];
			t[2] = t[3] = '=';
		} else {
			t[1] = base64[((c1<<4)|(c2>>4))&0x3f];
			if (l<3) {
				t[2] = base64[(c2<<2)&0x3f];
				t[3] = '=';
			} else {
				t[2] = base64[((c2<<2) | (c3>>6))&0x3f];
				t[3] = base64[c3 & 0x3f];
			}
		}

		l-=3;
		t+=4;
		s+=3;
	}
	*t=0;
}

///////////////////////////////////////////////////////////////////////////
//
//	Object: VDParameterCurve
//
///////////////////////////////////////////////////////////////////////////

static void func_VDParameterCurve_AddPoint(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDParameterCurve *obj = (VDParameterCurve *)argv[-1].asObjectPtr();
	VDParameterCurve::PointList& pts = obj->Points();
	VDParameterCurvePoint pt;

	pt.mX = argv[0].asDouble();
	pt.mY = argv[1].asDouble();
	pt.mbLinear = argv[2].asInt() != 0;

	VDParameterCurve::PointList::iterator it(obj->LowerBound(argv[0].asDouble()));

	pts.insert(it, pt);
}

static const VDScriptFunctionDef obj_VDParameterCurve_functbl[]={
	{ func_VDParameterCurve_AddPoint, "AddPoint", "0ddi" },
	{ NULL }
};

static const VDScriptObject obj_VDParameterCurve={
	"VDParameterCurve", NULL, obj_VDParameterCurve_functbl, NULL	
};

///////////////////////////////////////////////////////////////////////////
//
//	Object: VirtualDub.video.filters
//
///////////////////////////////////////////////////////////////////////////

static void func_VDVFiltInst_Remove(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	FilterInstance *fa = (FilterInstance *)argv[-1].asObjectPtr();

	fa->Remove();
	fa->Destroy();
}

static void func_VDVFiltInst_SetClipping(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	FilterInstance *fa = (FilterInstance *)(FilterActivation *)argv[-1].asObjectPtr();

	fa->x1	= argv[0].asInt();
	fa->y1	= argv[1].asInt();
	fa->x2	= argv[2].asInt();
	fa->y2	= argv[3].asInt();
}

static void func_VDVFiltInst_GetClipping(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	FilterInstance *fa = (FilterInstance *)(FilterActivation *)argv[-1].asObjectPtr();

	switch(argv[0].asInt()) {
	case 0:	argv[0] = (int)fa->x1; break;
	case 1: argv[0] = (int)fa->y1; break;
	case 2: argv[0] = (int)fa->x2; break;
	case 3: argv[0] = (int)fa->y2; break;
	}

	argv[0] = VDScriptValue(0);
}

static void func_VDVFiltInst_AddOpacityCurve(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	FilterInstance *fa = (FilterInstance *)(FilterActivation *)argv[-1].asObjectPtr();

	VDParameterCurve *curve = new VDParameterCurve;
	curve->SetYRange(0, 1);
	fa->SetAlphaParameterCurve(curve);

	argv[0] = VDScriptValue(curve, &obj_VDParameterCurve);
}

static VDScriptValue obj_VDVFiltInst_lookup(IVDScriptInterpreter *isi, const VDScriptObject *thisPtr, void *lpVoid, char *szName) {
	FilterInstance *pfi = static_cast<FilterInstance *>((FilterActivation *)lpVoid);

	if (pfi->filter->script_obj)
		return isi->LookupObjectMember(&pfi->mScriptObj, lpVoid, szName);

	return VDScriptValue();
}

static const VDScriptFunctionDef obj_VDVFiltInst_functbl[]={
	{ func_VDVFiltInst_Remove			, "Remove", "0" },
	{ func_VDVFiltInst_SetClipping		, "SetClipping", "0iiii" },
	{ func_VDVFiltInst_GetClipping		, "GetClipping", "ii" },
	{ func_VDVFiltInst_AddOpacityCurve	, "AddOpacityCurve", "v" },
	{ NULL }
};

extern const VDScriptObject obj_VDVFiltInst={
	"VDVideoFilterInstance", obj_VDVFiltInst_lookup, obj_VDVFiltInst_functbl, NULL	
};

///////////////////

static void func_VDVFilters_instance(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	FilterInstance *fa = (FilterInstance *)g_listFA.tail.next, *fa2;
	int index = argv[0].asInt();

	while((fa2 = (FilterInstance *)fa->next) && index--)
		fa = fa2;

	if (index!=-1 || !fa->next) VDSCRIPT_EXT_ERROR(VAR_NOT_FOUND);

	argv[0] = VDScriptValue(static_cast<FilterActivation *>(fa), &obj_VDVFiltInst);
}

static const VDScriptFunctionDef obj_VDVFilters_instance_functbl[]={
	{ func_VDVFilters_instance		, "[]", "vi" },
	{ NULL }
};

static const VDScriptObject obj_VDVFilters_instance={
	"VDVideoFilterList", NULL, obj_VDVFilters_instance_functbl, NULL	
};

static void func_VDVFilters_Clear(IVDScriptInterpreter *, VDScriptValue *, int) {
	FilterInstance *fa;

	filters.DeinitFilters();
	filters.DeallocateBuffers();

	while(fa = (FilterInstance *)g_listFA.RemoveHead()) {
		fa->Destroy();
	}
}

static void func_VDVFilters_Add(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	std::list<FilterBlurb>	filterList;

	FilterEnumerateFilters(filterList);

	for(std::list<FilterBlurb>::const_iterator it(filterList.begin()), itEnd(filterList.end()); it!=itEnd; ++it) {
		const FilterBlurb& fb = *it;

		if (strfuzzycompare(fb.name.c_str(), *argv[0].asString())) {
			FilterInstance *fa = new FilterInstance(fb.key);

			if (!fa) VDSCRIPT_EXT_ERROR(OUT_OF_MEMORY);

			fa->x1 = fa->y1 = fa->x2 = fa->y2 = 0;

			filters.DeinitFilters();
			filters.DeallocateBuffers();

			g_listFA.AddHead(fa);

			int count = 0;

			FilterInstance *fa2;
			fa = (FilterInstance *)g_listFA.tail.next;

			while(fa2 = (FilterInstance *)fa->next) {
				fa = fa2;
				++count;
			}

			argv[0] = VDScriptValue(count-1);
			return;
		}
	}

	throw MyError("Cannot add filter '%s': no such filter loaded", *argv[0].asString());
}

static VDScriptValue obj_VDVFilters_lookup(IVDScriptInterpreter *isi, const VDScriptObject *thisPtr, void *lpVoid, char *szName) {
	if (!strcmp(szName, "instance"))
		return VDScriptValue(lpVoid, &obj_VDVFilters_instance);

	VDSCRIPT_EXT_ERROR(MEMBER_NOT_FOUND);
}

static const VDScriptFunctionDef obj_VDVFilters_functbl[]={
	{ func_VDVFilters_Clear			, "Clear", "0" },
	{ func_VDVFilters_Add			, "Add", "is", },
	{ NULL }
};

static const VDScriptObject obj_VDVFilters={
	"VDVideoFilters", obj_VDVFilters_lookup, obj_VDVFilters_functbl, NULL	
};

///////////////////////////////////////////////////////////////////////////
//
//	Object: VirtualDub.video
//
///////////////////////////////////////////////////////////////////////////

static void func_VDVideo_GetDepth(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	int format;

	if (arglist[0].asInt())
		format = g_dubOpts.video.mOutputFormat;
	else
		format = g_dubOpts.video.mInputFormat;

	switch(format) {
		case nsVDPixmap::kPixFormat_XRGB1555:
		case nsVDPixmap::kPixFormat_RGB565:
			arglist[0] = VDScriptValue(16);
			break;
		default:
			arglist[0] = VDScriptValue(24);
			break;
	}
}

static void func_VDVideo_SetDepth(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	int new_depth1, new_depth2;

	switch(arglist[0].asInt()) {
	case 16:	new_depth1 = nsVDPixmap::kPixFormat_XRGB1555; break;
	case 24:	new_depth1 = nsVDPixmap::kPixFormat_RGB888; break;
	case 32:	new_depth1 = nsVDPixmap::kPixFormat_XRGB8888; break;
	default:
		return;
	}

	switch(arglist[1].asInt()) {
	case 16:	new_depth2 = nsVDPixmap::kPixFormat_XRGB1555; break;
	case 24:	new_depth2 = nsVDPixmap::kPixFormat_RGB888; break;
	case 32:	new_depth2 = nsVDPixmap::kPixFormat_XRGB8888; break;
	default:
		return;
	}

	g_dubOpts.video.mInputFormat = new_depth1;
	g_dubOpts.video.mOutputFormat = new_depth2;
}

static void func_VDVideo_SetInputFormat(IVDScriptInterpreter *, VDScriptValue *argv, int argc) {
	g_dubOpts.video.mInputFormat = argv[0].asInt();
	if (g_dubOpts.video.mInputFormat >= nsVDPixmap::kPixFormat_Max_Standard)
		g_dubOpts.video.mInputFormat = nsVDPixmap::kPixFormat_RGB888;
}

static void func_VDVideo_SetOutputFormat(IVDScriptInterpreter *, VDScriptValue *argv, int argc) {
	g_dubOpts.video.mOutputFormat = argv[0].asInt();
	if (g_dubOpts.video.mOutputFormat >= nsVDPixmap::kPixFormat_Max_Standard)
		g_dubOpts.video.mOutputFormat = nsVDPixmap::kPixFormat_RGB888;
}

static void func_VDVideo_GetMode(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	arglist[0] = VDScriptValue(g_dubOpts.video.mode);
}

static void func_VDVideo_SetMode(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	int new_mode = arglist[0].asInt();

	if (new_mode>=0 && new_mode<4)
		g_dubOpts.video.mode = (char)new_mode;
}

static void func_VDVideo_SetSmartRendering(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	g_dubOpts.video.mbUseSmartRendering = !!arglist[0].asInt();
}

static void func_VDVideo_GetSmartRendering(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	arglist[0] = (int)g_dubOpts.video.mbUseSmartRendering;
}

static void func_VDVideo_SetPreserveEmptyFrames(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	g_dubOpts.video.mbPreserveEmptyFrames = !!arglist[0].asInt();
}

static void func_VDVideo_GetPreserveEmptyFrames(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	arglist[0] = (int)g_dubOpts.video.mbPreserveEmptyFrames;
}

static void func_VDVideo_GetFrameRate(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	switch(arglist[0].asInt()) {
	case 0: arglist[0] = VDScriptValue(g_dubOpts.video.frameRateDecimation); return;
	case 1: arglist[0] = VDScriptValue(g_dubOpts.video.frameRateNewMicroSecs); return;
	case 2: arglist[0] = VDScriptValue(g_dubOpts.video.fInvTelecine); return;
	}
	arglist[0] = VDScriptValue(0);
}

static void func_VDVideo_SetFrameRate(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	g_dubOpts.video.frameRateNewMicroSecs = arglist[0].asInt();
	g_dubOpts.video.frameRateDecimation = arglist[1].asInt();
	g_dubOpts.video.frameRateTargetLo = 0;
	g_dubOpts.video.frameRateTargetHi = 0;

	if (arg_count > 2) {
		if (arglist[2].asInt()) {
			g_dubOpts.video.fInvTelecine = true;
			g_dubOpts.video.fIVTCMode = false;
			g_dubOpts.video.nIVTCOffset = -1;
			g_dubOpts.video.fIVTCPolarity = false;
		} else {
			g_dubOpts.video.fInvTelecine = false;
		}
	}
}

static void func_VDVideo_SetTargetFrameRate(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	g_dubOpts.video.frameRateDecimation = 1;
	g_dubOpts.video.frameRateTargetLo = arglist[1].asInt();
	g_dubOpts.video.frameRateTargetHi = arglist[0].asInt();
}

static void func_VDVideo_GetRange(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	arglist[0] = VDScriptValue(arglist[0].asInt() ? g_dubOpts.video.lEndOffsetMS : g_dubOpts.video.lStartOffsetMS);
}

static void func_VDVideo_SetRangeEmpty(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	g_project->ClearSelection();
}

static void func_VDVideo_SetRange(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	SetSelectionStart(arglist[0].asInt());
	SetSelectionEnd(arglist[1].asInt());
}

static void func_VDVideo_GetCompression(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	if (g_Vcompression.dwFlags & ICMF_COMPVARS_VALID) {
		switch(arglist[0].asInt()) {
		case 0:	arglist[0] = VDScriptValue((int)g_Vcompression.fccHandler); return;
		case 1: arglist[0] = VDScriptValue(g_Vcompression.lKey); return;
		case 2: arglist[0] = VDScriptValue(g_Vcompression.lQ); return;
		case 3: arglist[0] = VDScriptValue(g_Vcompression.lDataRate); return;
		}
	}

	arglist[0] = VDScriptValue(0);
}

static void func_VDVideo_SetCompression(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
//	ICCompressorFree(&g_Vcompression);
	FreeCompressor(&g_Vcompression);

	memset(&g_Vcompression, 0, sizeof(COMPVARS));

	if (!arg_count) return;

	g_Vcompression.cbSize	= sizeof(COMPVARS);
	g_Vcompression.dwFlags |= ICMF_COMPVARS_VALID;

	g_Vcompression.fccType		= ICTYPE_VIDEO;

	if (arglist[0].isString()) {
		g_Vcompression.fccHandler	= 0x20202020;
		memcpy(&g_Vcompression.fccHandler, *arglist[0].asString(), std::min<unsigned>(4,strlen(*arglist[0].asString())));
	} else
		g_Vcompression.fccHandler	= arglist[0].asInt();

	g_Vcompression.lKey			= arglist[1].asInt();
	g_Vcompression.lQ			= arglist[2].asInt();
	g_Vcompression.lDataRate	= arglist[3].asInt();
	g_Vcompression.hic			= ICOpen(g_Vcompression.fccType, g_Vcompression.fccHandler, ICMODE_COMPRESS);
}

static void func_VDVideo_SetCompData(IVDScriptInterpreter *isi, VDScriptValue *arglist, int arg_count) {
	void *mem;
	long l = ((strlen(*arglist[1].asString())+3)/4)*3;

	if (!(g_Vcompression.dwFlags & ICMF_COMPVARS_VALID))
		return;

	if (arglist[0].asInt() > l) return;

	l = arglist[0].asInt();

	if (!(mem = allocmem(l)))
		VDSCRIPT_EXT_ERROR(OUT_OF_MEMORY);

	_CrtCheckMemory();
	memunbase64((char *)mem, *arglist[1].asString(), l);
	_CrtCheckMemory();

	ICSetState(g_Vcompression.hic, mem, l);

	freemem(mem);
}

static void func_VDVideo_EnableIndeoQC(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	R4_ENC_SEQ_DATA r4enc;

	if (!(g_Vcompression.dwFlags & ICMF_COMPVARS_VALID))
		return;

	if ((g_Vcompression.fccHandler & 0xFFFF) != 'VI')
		return;

	r4enc.dwSize			= sizeof(R4_ENC_SEQ_DATA);
	r4enc.dwFourCC			= g_Vcompression.fccHandler;
	r4enc.dwVersion			= SPECIFIC_INTERFACE_VERSION;
	r4enc.mtType			= MT_ENCODE_SEQ_VALUE;
	r4enc.oeEnvironment		= OE_32;
	r4enc.dwFlags			= ENCSEQ_VALID | ENCSEQ_QUICK_COMPRESS;
	r4enc.fQuickCompress	= !!arglist[0].asInt();

	ICSetState(g_Vcompression.hic, &r4enc, sizeof r4enc);
}

static void func_VDVideo_SetIVTC(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	g_dubOpts.video.fInvTelecine = !!arglist[0].asInt();
	g_dubOpts.video.fIVTCMode = !!arglist[1].asInt();
	g_dubOpts.video.nIVTCOffset = arglist[2].asInt();
	if (g_dubOpts.video.nIVTCOffset >= 0)
		g_dubOpts.video.nIVTCOffset %= 5;
	g_dubOpts.video.fIVTCPolarity = !!arglist[3].asInt();
}

static const VDScriptFunctionDef obj_VDVideo_functbl[]={
	{ func_VDVideo_GetDepth			, "GetDepth",		"ii" },
	{ func_VDVideo_SetDepth			, "SetDepth",		"0ii" },
	{ func_VDVideo_GetMode			, "GetMode",		"i" },
	{ func_VDVideo_SetMode			, "SetMode",		"0i" },
	{ func_VDVideo_GetFrameRate		, "GetFrameRate",	"ii" },
	{ func_VDVideo_SetFrameRate		, "SetFrameRate",	"0ii" },
	{ func_VDVideo_SetFrameRate		, NULL,				"0iii" },
	{ func_VDVideo_SetTargetFrameRate, "SetTargetFrameRate", "0ii" },
	{ func_VDVideo_GetRange			, "GetRange",		"ii" },
	{ func_VDVideo_SetRangeEmpty	, "SetRange",		"0" },
	{ func_VDVideo_SetRange			, NULL,				"0ii" },
	{ func_VDVideo_GetCompression	, "GetCompression",	"ii" },
	{ func_VDVideo_SetCompression	, "SetCompression",	"0siii" },
	{ func_VDVideo_SetCompression	, NULL,				"0iiii" },
	{ func_VDVideo_SetCompression	, NULL,				"0" },
	{ func_VDVideo_SetCompData		, "SetCompData",	"0is" },
	{ func_VDVideo_EnableIndeoQC	, "EnableIndeoQC",	"0i" },
	{ func_VDVideo_SetIVTC			, "SetIVTC",		"0iiii" },
	{ func_VDVideo_SetInputFormat	, "SetInputFormat",	"0i" },
	{ func_VDVideo_SetOutputFormat	, "SetOutputFormat", "0i" },
	{ func_VDVideo_GetSmartRendering, "GetSmartRendering", "i" },
	{ func_VDVideo_SetSmartRendering, "SetSmartRendering", "0i" },
	{ func_VDVideo_GetPreserveEmptyFrames, "GetPreserveEmptyFrames", "i" },
	{ func_VDVideo_SetPreserveEmptyFrames, "SetPreserveEmptyFrames", "0i" },
	{ NULL }
};

static const VDScriptObjectDef obj_VDVideo_objtbl[]={
	{ "filters", &obj_VDVFilters },
	{ NULL }
};

static VDScriptValue obj_VirtualDub_video_lookup(IVDScriptInterpreter *isi, const VDScriptObject *obj, void *lpVoid, char *szName) {
	if (!strcmp(szName, "width"))
		return VDScriptValue(inputVideoAVI ? inputVideoAVI->getImageFormat()->biWidth : 0);
	else if (!strcmp(szName, "height"))
		return VDScriptValue(inputVideoAVI ? abs(inputVideoAVI->getImageFormat()->biHeight) : 0);
	else if (!strcmp(szName, "length"))
		return VDScriptValue(inputVideoAVI ? inputVideoAVI->asStream()->getLength() : 0);
	else if (!strcmp(szName, "framerate"))
		return VDScriptValue(inputVideoAVI ? inputVideoAVI->getRate().asDouble() : 0.0);

	VDSCRIPT_EXT_ERROR(MEMBER_NOT_FOUND);
}

static const VDScriptObject obj_VDVideo={
	"VDVideo", obj_VirtualDub_video_lookup, obj_VDVideo_functbl, obj_VDVideo_objtbl
};

///////////////////////////////////////////////////////////////////////////
//
//	Object: VirtualDub.audio.filters.instance
//
///////////////////////////////////////////////////////////////////////////

namespace {
	const VDPluginConfigEntry *GetFilterParamEntry(const VDPluginConfigEntry *pEnt, const unsigned idx) {
		if (pEnt)
			for(; pEnt->next; pEnt=pEnt->next) {
				if (pEnt->idx == idx)
					return pEnt;
			}

		return NULL;
	}

	void SetFilterParam(void *lpVoid, unsigned idx, const VDPluginConfigVariant& v) {
		VDAudioFilterGraph::FilterEntry *pFilt = (VDAudioFilterGraph::FilterEntry *)lpVoid;

		VDPluginDescription *pDesc = VDGetPluginDescription(pFilt->mFilterName.c_str(), kVDPluginType_Audio);

		if (!pDesc)
			throw MyError("VDAFiltInst: Unknown audio filter: \"%s\"", VDTextWToA(pFilt->mFilterName).c_str());

		VDPluginPtr lock(pDesc);

		const VDPluginConfigEntry *pEnt = GetFilterParamEntry(((VDVideoFilterDefinition *)lock->mpInfo->mpTypeSpecificInfo)->mpConfigInfo, idx);

		if (!pEnt)
			throw MyError("VDAFiltInst: Audio filter \"%s\" does not have a parameter with id %d", VDTextWToA(pFilt->mFilterName).c_str(), idx);

		VDPluginConfigVariant& var = pFilt->mConfig[idx];

		switch(pEnt->type) {
		case VDPluginConfigEntry::kTypeU32:
			switch(v.GetType()) {
			case VDPluginConfigVariant::kTypeU32:	var.SetU32(v.GetU32()); return;
			case VDPluginConfigVariant::kTypeS32:	var.SetU32(v.GetS32()); return;
			case VDPluginConfigVariant::kTypeU64:	var.SetU32((uint32)v.GetU64()); return;
			case VDPluginConfigVariant::kTypeS64:	var.SetU32((uint32)v.GetS64()); return;
			}
			break;
		case VDPluginConfigEntry::kTypeS32:
			switch(v.GetType()) {
			case VDPluginConfigVariant::kTypeU32:	var.SetS32(v.GetU32()); return;
			case VDPluginConfigVariant::kTypeS32:	var.SetS32(v.GetS32()); return;
			case VDPluginConfigVariant::kTypeU64:	var.SetS32((sint32)v.GetU64()); return;
			case VDPluginConfigVariant::kTypeS64:	var.SetS32((sint32)v.GetS64()); return;
			}
			break;
		case VDPluginConfigEntry::kTypeU64:
			switch(v.GetType()) {
			case VDPluginConfigVariant::kTypeU32:	var.SetU64(v.GetU32()); return;
			case VDPluginConfigVariant::kTypeS32:	var.SetU64(v.GetS32()); return;
			case VDPluginConfigVariant::kTypeU64:	var.SetU64(v.GetU64()); return;
			case VDPluginConfigVariant::kTypeS64:	var.SetU64(v.GetS64()); return;
			}
			break;
		case VDPluginConfigEntry::kTypeS64:
			switch(v.GetType()) {
			case VDPluginConfigVariant::kTypeU32:	var.SetS64(v.GetU32()); return;
			case VDPluginConfigVariant::kTypeS32:	var.SetS64(v.GetS32()); return;
			case VDPluginConfigVariant::kTypeU64:	var.SetS64(v.GetU64()); return;
			case VDPluginConfigVariant::kTypeS64:	var.SetS64(v.GetS64()); return;
			}
			break;
		case VDPluginConfigEntry::kTypeDouble:
			if (v.GetType() == VDPluginConfigEntry::kTypeDouble) {
				var = v;
				return;
			}
			break;
		case VDPluginConfigEntry::kTypeAStr:
			if (v.GetType() == VDPluginConfigEntry::kTypeWStr) {
				var.SetAStr(VDTextWToA(v.GetWStr()).c_str());
				return;
			}
			break;
		case VDPluginConfigEntry::kTypeWStr:
			if (v.GetType() == VDPluginConfigEntry::kTypeWStr) {
				var = v;
				return;
			}
			break;
		case VDPluginConfigEntry::kTypeBlock:
			if (v.GetType() == VDPluginConfigEntry::kTypeBlock) {
				var = v;
				return;
			}
		}

		pFilt->mConfig.erase(idx);

		throw MyError("VDAFiltInst: Type mismatch on audio filter \"%s\" param %d (\"%s\")", VDTextWToA(pFilt->mFilterName).c_str(), idx, VDTextWToA(pEnt->name).c_str());
	}
};

static void func_VDAFiltInst_SetInt(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDPluginConfigVariant v;
	
	v.SetS32(argv[1].asInt());

	SetFilterParam(argv[-1].asObjectPtr(), argv[0].asInt(), v);
}

static void func_VDAFiltInst_SetLong(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDPluginConfigVariant v;
	
	if (argc == 2)
		v.SetU64(argv[1].asLong());
	else
		v.SetU64((uint32)argv[2].asInt() + ((uint64)(uint32)argv[1].asInt() << 32));

	SetFilterParam(argv[-1].asObjectPtr(), argv[0].asInt(), v);
}

static void func_VDAFiltInst_SetDouble(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDPluginConfigVariant v;

	if (argc == 2)
		v.SetDouble(argv[1].asDouble());
	else {
		union {
			struct {
				uint32 lo;
				uint32 hi;
			} bar;
			double d;
		} foo;

		foo.bar.lo = argv[2].asInt();
		foo.bar.hi = argv[1].asInt();
	
		v.SetDouble(foo.d);
	}

	SetFilterParam(argv[-1].asObjectPtr(), argv[0].asInt(), v);
}

static void func_VDAFiltInst_SetString(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDPluginConfigVariant v;
	
	v.SetWStr(VDTextU8ToW(*argv[1].asString(), -1).c_str());

	SetFilterParam(argv[-1].asObjectPtr(), argv[0].asInt(), v);
}

static void func_VDAFiltInst_SetRaw(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDPluginConfigVariant v;

	int len = argv[1].asInt();
	vdfastvector<char> mem(len);
	long l = ((strlen(*argv[2].asString())+3)/4)*3;

	if (len > l)
		return;

	memunbase64(mem.data(), *argv[2].asString(), len);
	v.SetBlock(&mem.front(), len);

	SetFilterParam(argv[-1].asObjectPtr(), argv[0].asInt(), v);
}

static const VDScriptFunctionDef obj_VDAFiltInst_functbl[]={
	{ func_VDAFiltInst_SetInt,			"SetInt",		"0ii" },
	{ func_VDAFiltInst_SetLong,			"SetLong",		"0iii" },
	{ func_VDAFiltInst_SetLong,			NULL,			"0il" },
	{ func_VDAFiltInst_SetDouble,		"SetDouble",	"0iii" },
	{ func_VDAFiltInst_SetDouble,		NULL,			"0id" },
	{ func_VDAFiltInst_SetString,		"SetString",	"0is" },
	{ func_VDAFiltInst_SetRaw,			"SetRaw",		"0iis" },
	{ NULL }
};

static const VDScriptObject obj_VDAFiltInst={
	"VDAudio", NULL, obj_VDAFiltInst_functbl, NULL	
};

///////////////////////////////////////////////////////////////////////////
//
//	Object: VirtualDub.audio.filters
//
///////////////////////////////////////////////////////////////////////////

static void func_VDAFilters_instance(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	int index = argv[0].asInt();

	if (index < 0 || index >= g_audioFilterGraph.mFilters.size()) {
		VDSCRIPT_EXT_ERROR(VAR_NOT_FOUND);
	}

	VDAudioFilterGraph::FilterList::iterator it(g_audioFilterGraph.mFilters.begin());

	std::advance(it, index);

	argv[0] = VDScriptValue(static_cast<VDAudioFilterGraph::FilterEntry *>(&*it), &obj_VDAFiltInst);
}

static const VDScriptFunctionDef obj_VDAFilters_instance_functbl[]={
	{ func_VDAFilters_instance		, "[]", "vi" },
	{ NULL }
};

static const VDScriptObject obj_VDAFilters_instance={
	"VDAudioFilterList", NULL, obj_VDAFilters_instance_functbl, NULL	
};

static void func_VDAFilters_Clear(IVDScriptInterpreter *, VDScriptValue *, int) {
	g_audioFilterGraph.mFilters.clear();
	g_audioFilterGraph.mConnections.clear();
}

static void func_VDAFilters_Add(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDAudioFilterGraph::FilterEntry filt;

	filt.mFilterName = VDTextU8ToW(*argv[0].asString(), -1);

	VDPluginDescription *pDesc = VDGetPluginDescription(filt.mFilterName.c_str(), kVDPluginType_Audio);

	if (!pDesc)
		throw MyError("VDAFilters.Add(): Unknown audio filter: \"%s\"", VDTextWToA(filt.mFilterName).c_str());

	const VDAudioFilterDefinition *pDef = reinterpret_cast<const VDAudioFilterDefinition *>(pDesc->mpInfo->mpTypeSpecificInfo);

	filt.mInputPins = pDef->mInputPins;
	filt.mOutputPins = pDef->mOutputPins;

	g_audioFilterGraph.mFilters.push_back(filt);

	VDAudioFilterGraph::FilterConnection conn = {-1,-1};

	for(unsigned i=0; i<filt.mInputPins; ++i)
		g_audioFilterGraph.mConnections.push_back(conn);

	argv[0] = (int)(g_audioFilterGraph.mFilters.size()-1);
}

static void func_VDAFilters_Connect(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	int srcfilt	= argv[0].asInt();
	int srcpin	= argv[1].asInt();
	int dstfilt	= argv[2].asInt();
	int dstpin	= argv[3].asInt();
	int nfilts	= g_audioFilterGraph.mFilters.size();

	if (srcfilt<0 || srcfilt>=nfilts)
		throw MyError("VDAFilters.Connect(): Invalid source filter number %d (should be 0-%d)", srcfilt, nfilts-1);
	if (dstfilt<=srcfilt || dstfilt>=nfilts)
		throw MyError("VDAFilters.Connect(): Invalid target filter number %d (should be %d-%d)", dstfilt, srcfilt+1, nfilts-1);

	// #&*$(
	VDAudioFilterGraph::FilterList::const_iterator itsrc = g_audioFilterGraph.mFilters.begin();
	VDAudioFilterGraph::FilterList::const_iterator itdst = g_audioFilterGraph.mFilters.begin();
	int dstconnidx = 0;

	while(dstfilt-->0) {
		dstconnidx += (*itdst).mInputPins;
		++itdst;
	}

	std::advance(itsrc, srcfilt);

	VDASSERT(dstconnidx < g_audioFilterGraph.mConnections.size());

	const VDAudioFilterGraph::FilterEntry& fesrc = *itsrc;
	const VDAudioFilterGraph::FilterEntry& fedst = *itdst;

	if (srcpin<0 || srcpin>=fesrc.mOutputPins)
		throw MyError("VDAFilters.Connect(): Invalid source pin %d (should be 0-%d)", srcpin, fesrc.mOutputPins-1);
	if (dstpin<0 || dstpin>=fedst.mInputPins)
		throw MyError("VDAFilters.Connect(): Invalid target pin %d (should be 0-%d)", dstpin, fedst.mInputPins-1);

	VDAudioFilterGraph::FilterConnection& conn = g_audioFilterGraph.mConnections[dstconnidx + dstpin];

	conn.filt = srcfilt;
	conn.pin = srcpin;
}

static VDScriptValue obj_VDAFilters_lookup(IVDScriptInterpreter *isi, const VDScriptObject *thisPtr, void *lpVoid, char *szName) {
	if (!strcmp(szName, "instance"))
		return VDScriptValue(lpVoid, &obj_VDAFilters_instance);

	VDSCRIPT_EXT_ERROR(MEMBER_NOT_FOUND);
}

static const VDScriptFunctionDef obj_VDAFilters_functbl[]={
	{ func_VDAFilters_Clear			, "Clear", "0" },
	{ func_VDAFilters_Add			, "Add", "is", },
	{ func_VDAFilters_Connect		, "Connect", "0iiii", },
	{ NULL }
};

static const VDScriptObject obj_VDAFilters={
	"VDAudio", obj_VDAFilters_lookup, obj_VDAFilters_functbl, NULL	
};

///////////////////////////////////////////////////////////////////////////
//
//	Object: VirtualDub.audio
//
///////////////////////////////////////////////////////////////////////////

static void func_VDAudio_GetMode(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	arglist[0] = VDScriptValue(g_dubOpts.audio.mode);
}

static void func_VDAudio_SetMode(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	int new_mode = arglist[0].asInt();

	if (new_mode>=0 && new_mode<2)
		g_dubOpts.audio.mode = (char)new_mode;
}

static void func_VDAudio_GetInterleave(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	switch(arglist[0].asInt()) {
	case 0:	arglist[0] = VDScriptValue(g_dubOpts.audio.enabled); return;
	case 1:	arglist[0] = VDScriptValue(g_dubOpts.audio.preload); return;
	case 2:	arglist[0] = VDScriptValue(g_dubOpts.audio.interval); return;
	case 3:	arglist[0] = VDScriptValue(g_dubOpts.audio.is_ms); return;
	case 4: arglist[0] = VDScriptValue(g_dubOpts.audio.offset); return;
	}

	arglist[0] = VDScriptValue(0);
}

static void func_VDAudio_SetInterleave(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	g_dubOpts.audio.enabled		= !!arglist[0].asInt();
	g_dubOpts.audio.preload		= arglist[1].asInt();
	g_dubOpts.audio.interval	= arglist[2].asInt();
	g_dubOpts.audio.is_ms		= !!arglist[3].asInt();
	g_dubOpts.audio.offset		= arglist[4].asInt();
}

static void func_VDAudio_GetClipMode(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	switch(arglist[0].asInt()) {
	case 0:		arglist[0] = VDScriptValue(g_dubOpts.audio.fStartAudio); return;
	case 1:		arglist[0] = VDScriptValue(g_dubOpts.audio.fEndAudio); return;
	}

	arglist[0] = VDScriptValue(0);
}

static void func_VDAudio_SetClipMode(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	g_dubOpts.audio.fStartAudio	= !!arglist[0].asInt();
	g_dubOpts.audio.fEndAudio	= !!arglist[1].asInt();
}

static void func_VDAudio_GetConversion(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	switch(arglist[0].asInt()) {
	case 0:		arglist[0] = VDScriptValue(g_dubOpts.audio.new_rate); return;
	case 1:		arglist[0] = VDScriptValue(g_dubOpts.audio.newPrecision); return;
	case 2:		arglist[0] = VDScriptValue(g_dubOpts.audio.newChannels); return;
	}

	arglist[0] = VDScriptValue(0);
}

static void func_VDAudio_SetConversion(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	g_dubOpts.audio.new_rate		= arglist[0].asInt();
	g_dubOpts.audio.newPrecision	= (char)arglist[1].asInt();
	g_dubOpts.audio.newChannels		= (char)arglist[2].asInt();

	if (arg_count >= 5) {
		if (arglist[3].asInt())
			throw MyError("The \"integral_rate\" feature of the audio.SetConversion() function is no longer supported.");
		g_dubOpts.audio.fHighQuality	= !!arglist[4].asInt();
	}
}

static void func_VDAudio_SetSource(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	if (arglist[0].isInt()) {
		if (arglist[0].asInt())
			g_project->SetAudioSourceNormal();
		else
			g_project->SetAudioSourceNone();
	} else {
		g_project->OpenWAV(VDTextU8ToW(VDStringA(*arglist[0].asString())).c_str());
	}
}

static void func_VDAudio_SetCompressionPCM(IVDScriptInterpreter *isi, VDScriptValue *arglist, int arg_count) {
	PCMWAVEFORMAT *pwf = (PCMWAVEFORMAT *)allocmem(sizeof(PCMWAVEFORMAT));

	if (!pwf) VDSCRIPT_EXT_ERROR(OUT_OF_MEMORY);

	pwf->wf.wFormatTag			= WAVE_FORMAT_PCM;
	pwf->wf.nSamplesPerSec		= arglist[0].asInt();
	pwf->wf.nChannels			= (WORD)arglist[1].asInt();
	pwf->   wBitsPerSample		= (WORD)arglist[2].asInt();
	pwf->wf.nBlockAlign			= (WORD)((pwf->wBitsPerSample/8) * pwf->wf.nChannels);
	pwf->wf.nAvgBytesPerSec		= pwf->wf.nSamplesPerSec * pwf->wf.nBlockAlign;
	g_ACompressionFormatSize	= sizeof(PCMWAVEFORMAT);
	freemem(g_ACompressionFormat);
	g_ACompressionFormat = (WAVEFORMATEX *)pwf;
}

// VirtualDub.audio.SetCompression();
// VirtualDub.audio.SetCompression(tag, sampling_rate, channels, bits, bytes/sec, blockalign);
// VirtualDub.audio.SetCompression(tag, sampling_rate, channels, bits, bytes/sec, blockalign, exdatalen, exdata);

static void func_VDAudio_SetCompression(IVDScriptInterpreter *isi, VDScriptValue *arglist, int arg_count) {
	WAVEFORMATEX *wfex;
	long ex_data=0;

	if (!arg_count) {
		freemem(g_ACompressionFormat);
		g_ACompressionFormat = NULL;
		return;
	}

	if (arg_count > 6)
		ex_data = arglist[6].asInt();

	if (!(wfex = (WAVEFORMATEX *)allocmem(sizeof(WAVEFORMATEX) + ex_data)))
		VDSCRIPT_EXT_ERROR(OUT_OF_MEMORY);

	wfex->wFormatTag		= (WORD)arglist[0].asInt();
	wfex->nSamplesPerSec	= arglist[1].asInt();
	wfex->nChannels			= (WORD)arglist[2].asInt();
	wfex->wBitsPerSample	= (WORD)arglist[3].asInt();
	wfex->nAvgBytesPerSec	= arglist[4].asInt();
	wfex->nBlockAlign		= (WORD)arglist[5].asInt();
	wfex->cbSize			= (WORD)ex_data;

	if (arg_count > 6) {
		long l = ((strlen(*arglist[7].asString())+3)/4)*3;

		if (ex_data > l) {
			freemem(wfex);
			return;
		}

		memunbase64((char *)(wfex+1), *arglist[7].asString(), ex_data);
	}

	_CrtCheckMemory();

	if (g_ACompressionFormat)
		freemem(g_ACompressionFormat);

	g_ACompressionFormat = wfex;

	if (wfex->wFormatTag == WAVE_FORMAT_PCM)
		g_ACompressionFormatSize = sizeof(PCMWAVEFORMAT);
	else
		g_ACompressionFormatSize = sizeof(WAVEFORMATEX)+ex_data;

	_CrtCheckMemory();
}

static void func_VDAudio_SetCompressionWithHint(IVDScriptInterpreter *isi, VDScriptValue *arglist, int arg_count) {
	VDASSERT(arg_count > 0);

	func_VDAudio_SetCompression(isi, arglist, arg_count - 1);

	if (g_ACompressionFormat)
		g_ACompressionFormatHint.assign(*arglist[arg_count - 1].asString());
}

static void func_VDAudio_SetVolume(IVDScriptInterpreter *isi, VDScriptValue *arglist, int arg_count) {
	if (arg_count)
		g_dubOpts.audio.mVolume = arglist[0].asInt() * (1.0f / 256.0f);
	else
		g_dubOpts.audio.mVolume = -1.0f;
}

static void func_VDAudio_GetVolume(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	if (g_dubOpts.audio.mVolume < 0)
		arglist[0] = VDScriptValue(0);
	else
		arglist[0] = VDScriptValue(VDRoundToInt(g_dubOpts.audio.mVolume * 256.0f));
}

static void func_VDAudio_EnableFilterGraph(IVDScriptInterpreter *isi, VDScriptValue *arglist, int arg_count) {
	g_dubOpts.audio.bUseAudioFilterGraph = (arglist[0].asInt() != 0);
}

static VDScriptFunctionDef obj_VDAudio_functbl[]={
	{ func_VDAudio_GetMode			, "GetMode"				, "i"		},
	{ func_VDAudio_SetMode			, "SetMode"				, "0i"		},
	{ func_VDAudio_GetInterleave		, "GetInterleave"		, "ii"		},
	{ func_VDAudio_SetInterleave		, "SetInterleave"		, "0iiiii"	},
	{ func_VDAudio_GetClipMode		, "GetClipMode"			, "ii"		},
	{ func_VDAudio_SetClipMode		, "SetClipMode"			, "0ii"		},
	{ func_VDAudio_GetConversion		, "GetConversion"		, "ii"		},
	{ func_VDAudio_SetConversion		, "SetConversion"		, "0iii"	},
	{ func_VDAudio_SetConversion		, NULL					, "0iiiii"	},
	{ func_VDAudio_SetSource			, "SetSource"			, "0i"		},
	{ func_VDAudio_SetSource			, NULL					, "0s"		},
	{ func_VDAudio_SetCompressionPCM	, "SetCompressionPCM"	, "0iii"	},
	{ func_VDAudio_SetCompression	, "SetCompression"		, "0"		},
	{ func_VDAudio_SetCompression	, NULL					, "0iiiiii" },
	{ func_VDAudio_SetCompression	, NULL					, "0iiiiiiis" },
	{ func_VDAudio_SetCompressionWithHint	, "SetCompressionWithHint"					, "0iiiiiis" },
	{ func_VDAudio_SetCompressionWithHint	, NULL					, "0iiiiiiiss" },
	{ func_VDAudio_SetVolume			, "SetVolume"			, "0" },
	{ func_VDAudio_SetVolume			, NULL					, "0i" },
	{ func_VDAudio_GetVolume			, "GetVolume"			, "i" },
	{ func_VDAudio_EnableFilterGraph	, "EnableFilterGraph"	, "0i" },
	{ NULL }
};

static const VDScriptObjectDef obj_VDAudio_objtbl[]={
	{ "filters", &obj_VDAFilters },
	{ NULL }
};

static VDScriptValue obj_VirtualDub_audio_lookup(IVDScriptInterpreter *isi, const VDScriptObject *obj, void *lpVoid, char *szName) {
	if (!strcmp(szName, "samplerate"))
		return VDScriptValue(inputAudio ? (int)inputAudio->getWaveFormat()->nSamplesPerSec : 0);
	else if (!strcmp(szName, "blockrate")) {
		if (!inputAudio)
			return VDScriptValue(0);

		const WAVEFORMATEX& wfex = *inputAudio->getWaveFormat();
		return VDScriptValue((double)wfex.nAvgBytesPerSec / (double)wfex.nBlockAlign);
	} else if (!strcmp(szName, "length"))
		return VDScriptValue(inputAudio ? (int)inputAudio->getLength() : 0);

	VDSCRIPT_EXT_ERROR(MEMBER_NOT_FOUND);
}

static const VDScriptObject obj_VDAudio={
	"VDAudio", obj_VirtualDub_audio_lookup, obj_VDAudio_functbl, obj_VDAudio_objtbl
};

///////////////////////////////////////////////////////////////////////////
//
//	Object: VirtualDub.subset
//
///////////////////////////////////////////////////////////////////////////

static void func_VDSubset_Delete(IVDScriptInterpreter *isi, VDScriptValue *arglist, int arg_count) {
	g_project->ResetTimeline();
}

static void func_VDSubset_Clear(IVDScriptInterpreter *isi, VDScriptValue *arglist, int arg_count) {
	g_project->ResetTimeline();
	FrameSubset& s = g_project->GetTimeline().GetSubset();
	s.clear();
}

static void func_VDSubset_AddRange(IVDScriptInterpreter *isi, VDScriptValue *arglist, int arg_count) {
	FrameSubset& s = g_project->GetTimeline().GetSubset();
	s.addRange(arglist[0].asLong(), arglist[1].asLong(), false, 0);
}

static void func_VDSubset_AddMaskedRange(IVDScriptInterpreter *isi, VDScriptValue *arglist, int arg_count) {
	FrameSubset& s = g_project->GetTimeline().GetSubset();
	s.addRange(arglist[0].asLong(), arglist[1].asLong(), true, 0);
}

static const VDScriptFunctionDef obj_VDSubset_functbl[]={
	{ func_VDSubset_Delete			, "Delete"				, "0"		},
	{ func_VDSubset_Clear			, "Clear"				, "0"		},
	{ func_VDSubset_AddRange		, "AddFrame"			, "0ll"		},	// DEPRECATED
	{ func_VDSubset_AddRange		, "AddRange"			, "0ll"		},
	{ func_VDSubset_AddMaskedRange	, "AddMaskedRange"		, "0ll"		},

	{ NULL }
};

static VDScriptValue obj_VDSubset_lookup(IVDScriptInterpreter *isi, const VDScriptObject *obj, void *lpVoid, char *szName) {
	if (!strcmp(szName, "length"))
		return VDScriptValue(g_project->GetTimeline().GetLength());

	VDSCRIPT_EXT_ERROR(MEMBER_NOT_FOUND);
}

static const VDScriptObject obj_VDSubset={
	"VDSubset", obj_VDSubset_lookup, obj_VDSubset_functbl
};


///////////////////////////////////////////////////////////////////////////
//
//	Object: VirtualDub.params
//
///////////////////////////////////////////////////////////////////////////

static void func_VDParams(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	const int index = argv[0].asInt();
	const char *s = VDGetStartupArgument(index);

	if (!s)
		VDSCRIPT_EXT_ERROR(ARRAY_INDEX_OUT_OF_BOUNDS);

	const long l = strlen(s);
	char **h = isi->AllocTempString(l);

	strcpy(*h, s);

	argv[0] = VDScriptValue(h);
}

static const VDScriptFunctionDef obj_VDParams_functbl[]={
	{ func_VDParams		, "[]", "vi" },
	{ NULL }
};

static const VDScriptObject obj_VDParams={
	"VDParams", NULL, obj_VDParams_functbl, NULL	
};

///////////////////////////////////////////////////////////////////////////
//
//	Object: VirtualDub.project
//
///////////////////////////////////////////////////////////////////////////

static void func_VDProject_ClearTextInfo(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDProject::tTextInfo& textInfo = g_project->GetTextInfo();

	textInfo.clear();
}

static void func_VDProject_AddTextInfo(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDProject::tTextInfo& textInfo = g_project->GetTextInfo();
	union {
		char buf[4];
		uint32 id;
	} conv;

	strncpy(conv.buf, *argv[0].asString(), 4);

	textInfo.push_back(VDProject::tTextInfo::value_type(conv.id, VDStringA(*argv[1].asString())));
}

static const VDScriptFunctionDef obj_VDProject_functbl[]={
	{ func_VDProject_ClearTextInfo,	"ClearTextInfo", "0" },
	{ func_VDProject_AddTextInfo,	"AddTextInfo", "0ss" },
	{ NULL }
};

static const VDScriptObject obj_VDProject={
	"VDProject", NULL, obj_VDProject_functbl, NULL	
};

///////////////////////////////////////////////////////////////////////////
//
//	Object: VirtualDub
//
///////////////////////////////////////////////////////////////////////////

extern void PreviewAVI(HWND, DubOptions *, int iPriority=0, bool fProp=false);

static void func_VirtualDub_SetStatus(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	guiSetStatus("%s", 255, *arglist[0].asString());
}

static void func_VirtualDub_OpenOld(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	VDStringW filename(VDTextAToW(*arglist[0].asString()));
	IVDInputDriver *pDriver = VDGetInputDriverForLegacyIndex(arglist[1].asInt());

	if (arg_count > 3) {
		long l = ((strlen(*arglist[3].asString())+3)/4)*3;
		char buf[64];

		memunbase64(buf, *arglist[3].asString(), l);

		g_project->Open(filename.c_str(), pDriver, !!arglist[2].asInt(), true, false, buf, l);
	} else
		g_project->Open(filename.c_str(), pDriver, !!arglist[2].asInt(), true, false);
}

static void func_VirtualDub_Open(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	VDStringW filename(VDTextU8ToW(VDStringA(*arglist[0].asString())));
	IVDInputDriver *pDriver = NULL;
	bool extopen = false;
	
	if (arg_count > 1) {
		pDriver = VDGetInputDriverByName(VDTextAToW(*arglist[1].asString()).c_str());

		if (arg_count > 2)
			extopen = !!arglist[2].asInt();
	}

	if (arg_count > 3) {
		long l = ((strlen(*arglist[3].asString())+3)/4)*3;
		char buf[64];

		memunbase64(buf, *arglist[3].asString(), l);

		g_project->Open(filename.c_str(), pDriver, extopen, true, false, buf, l);
	} else
		g_project->Open(filename.c_str(), pDriver, extopen, true, false);
}

static void func_VirtualDub_Append(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	VDStringW filename(VDTextU8ToW(VDStringA(*arglist[0].asString())));

	AppendAVI(filename.c_str());
}

static void func_VirtualDub_Close(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	g_project->Close();
}

static void func_VirtualDub_Preview(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	DubOptions opts(g_dubOpts);
	opts.fShowStatus			= false;
	PreviewAVI(g_hWnd, &opts, g_prefs.main.iPreviewPriority,true);
}

static void func_VirtualDub_RunNullVideoPass(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	g_project->RunNullVideoPass();
}

static void func_VirtualDub_SaveAVI(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	VDStringW filename(VDTextU8ToW(VDStringA(*arglist[0].asString())));
	DubOptions opts(g_dubOpts);
	opts.fShowStatus			= false;

	if (g_fJobMode) {
		opts.fMoveSlider			= true;
		opts.video.fShowInputFrame	= false;
		opts.video.fShowOutputFrame	= false;
		opts.video.fShowDecompressedFrame	= false;
	}
	SaveAVI(filename.c_str(), true, &opts, false);
}

static void func_VirtualDub_SaveCompatibleAVI(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	VDStringW filename(VDTextU8ToW(VDStringA(*arglist[0].asString())));
	DubOptions opts(g_dubOpts);
	opts.fShowStatus			= false;

	if (g_fJobMode) {
		opts.fMoveSlider			= true;
		opts.video.fShowInputFrame	= false;
		opts.video.fShowOutputFrame	= false;
		opts.video.fShowDecompressedFrame	= false;
	}

	SaveAVI(filename.c_str(), true, &opts, true);
}

static void func_VirtualDub_SaveSegmentedAVI(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	const VDStringW filename(VDTextU8ToW(VDStringA(*arglist[0].asString())));
	DubOptions opts(g_dubOpts);
	opts.fShowStatus			= false;

	if (g_fJobMode) {
		opts.fMoveSlider			= true;
		opts.video.fShowInputFrame	= false;
		opts.video.fShowOutputFrame	= false;
		opts.video.fShowDecompressedFrame	= false;
	}

	SaveSegmentedAVI(filename.c_str(), true, &opts, arglist[1].asInt(), arglist[2].asInt());
}

static void func_VirtualDub_SaveImageSequence(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	const VDStringW prefix(VDTextU8ToW(VDStringA(*arglist[0].asString())));
	const VDStringW suffix(VDTextU8ToW(VDStringA(*arglist[1].asString())));

	int q = 95;

	if (arg_count >= 5)
		q = arglist[4].asInt();

	DubOptions opts(g_dubOpts);
	opts.fShowStatus			= false;

	if (g_fJobMode) {
		opts.fMoveSlider			= true;
		opts.video.fShowInputFrame	= false;
		opts.video.fShowOutputFrame	= false;
		opts.video.fShowDecompressedFrame	= false;
	}

	SaveImageSequence(prefix.c_str(), suffix.c_str(), arglist[2].asInt(), true, &opts, arglist[3].asInt(), q);
}

static void func_VirtualDub_SaveWAV(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	const VDStringW filename(VDTextU8ToW(VDStringA(*arglist[0].asString())));

	SaveWAV(filename.c_str());
}

static void func_VirtualDub_SaveRawAudio(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	const VDStringW filename(VDTextU8ToW(VDStringA(*arglist[0].asString())));

	g_project->SaveRawAudio(filename.c_str());
}

static void func_VirtualDub_Log(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	const VDStringW text(VDTextU8ToW(VDStringA(*arglist[0].asString())));

	VDLog(kVDLogInfo, text);
}

static void func_VirtualDub_Exit(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	g_returnCode = arglist[0].asInt();
	PostQuitMessage(0);
}

extern "C" unsigned long version_num;

static VDScriptValue obj_VirtualDub_lookup(IVDScriptInterpreter *isi, const VDScriptObject *obj, void *lpVoid, char *szName) {
	if (!strcmp(szName, "version")) {
		char buf1[256], buf[256], **handle;

		LoadString(g_hInst, IDS_TITLE_INITIAL, buf, sizeof buf);

		_snprintf(buf1, sizeof buf1, buf, version_num,
#ifdef _DEBUG
			"debug"
#else
			"release"
#endif
			);

		handle = isi->AllocTempString(strlen(buf1));

		strcpy(*handle, buf1);

		return VDScriptValue(handle);
	} else if (!strcmp(szName, "video"))
		return VDScriptValue(NULL, &obj_VDVideo);
	else if (!strcmp(szName, "audio"))
		return VDScriptValue(NULL, &obj_VDAudio);
	else if (!strcmp(szName, "subset"))
		return VDScriptValue(NULL, &obj_VDSubset);
	else if (!strcmp(szName, "project"))
		return VDScriptValue(NULL, &obj_VDProject);
	else if (!strcmp(szName, "params"))
		return VDScriptValue(NULL, &obj_VDParams);

	VDSCRIPT_EXT_ERROR(MEMBER_NOT_FOUND);
}

static const VDScriptFunctionDef obj_VirtualDub_functbl[]={
	{ func_VirtualDub_SetStatus,			"SetStatus",			"0s" },
	{ func_VirtualDub_OpenOld,			"Open",					"0sii" },
	{ func_VirtualDub_OpenOld,			NULL,					"0siis" },
	{ func_VirtualDub_Open,				NULL,					"0s" },
	{ func_VirtualDub_Open,				NULL,					"0ssi" },
	{ func_VirtualDub_Open,				NULL,					"0ssis" },
	{ func_VirtualDub_Append,			"Append",				"0s" },
	{ func_VirtualDub_Close,				"Close",				"0" },
	{ func_VirtualDub_Preview,			"Preview",				"0" },
	{ func_VirtualDub_RunNullVideoPass,	"RunNullVideoPass",		"0" },
	{ func_VirtualDub_SaveAVI,			"SaveAVI",				"0s" },
	{ func_VirtualDub_SaveCompatibleAVI, "SaveCompatibleAVI",	"0s" },
	{ func_VirtualDub_SaveSegmentedAVI,	"SaveSegmentedAVI",		"0sii" },
	{ func_VirtualDub_SaveImageSequence,	"SaveImageSequence",	"0ssii" },
	{ func_VirtualDub_SaveImageSequence,	NULL,					"0ssiii" },
	{ func_VirtualDub_SaveWAV,			"SaveWAV",				"0s" },
	{ func_VirtualDub_SaveRawAudio,		"SaveRawAudio",			"0s" },
	{ func_VirtualDub_Log,				"Log",					"0s" },
	{ func_VirtualDub_Exit,				"Exit",					"0i" },
	{ NULL }
};

static const VDScriptObject obj_VirtualDub={
	"VDApplication", &obj_VirtualDub_lookup, obj_VirtualDub_functbl
};

static VDScriptValue RootHandler(IVDScriptInterpreter *isi, char *szName, void *lpData) {
	if (!strcmp(szName, "VirtualDub"))
		return VDScriptValue(NULL, &obj_VirtualDub);

	VDSCRIPT_EXT_ERROR(VAR_NOT_FOUND);
}

 
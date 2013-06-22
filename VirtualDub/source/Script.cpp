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

#include <windows.h>
#include <mmsystem.h>

#include "indeo_if.h"

#include "ScriptInterpreter.h"
#include "ScriptValue.h"
#include "ScriptError.h"
#include <vd2/system/error.h>
#include <vd2/system/vdalloc.h>
#include <vd2/Dita/services.h>
#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdaudiofilt.h>
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

extern VDProject *g_project;

extern HINSTANCE g_hInst;

extern IScriptInterpreter *CreateScriptInterpreter();

extern void FreeCompressor(COMPVARS *pCompVars);
static CScriptValue RootHandler(IScriptInterpreter *isi, char *szName, void *lpData);

bool InitScriptSystem() {
	return true;
}

void DeinitScriptSystem() {
}

///////////////////////////////////////////////

void RunScript(const wchar_t *name, void *hwnd) {
	static wchar_t fileFilters[]=
				L"All scripts (*.vcf,*.syl,*.jobs)\0"		L"*.vcf;*.syl;*.jobs\0"
				L"VirtualDub configuration file (*.vcf)\0"	L"*.vcf\0"
				L"Sylia script for VirtualDub (*.syl)\0"	L"*.syl\0"
				L"VirtualDub job queue (*.jobs)\0"			L"*.jobs\0"
				L"All Files (*.*)\0"						L"*.*\0"
				;

	IScriptInterpreter *isi = NULL;
	FILE *f = NULL;
	VDStringW filenameW;

	if (!name) {
		filenameW = VDGetLoadFileName(VDFSPECKEY_SCRIPT, (VDGUIHandle)hwnd, L"Load configuration script", fileFilters, L"script", 0, 0);

		if (filenameW.empty())
			return;

		name = filenameW.c_str();
	}

	VDStringA filenameA(VDTextWToA(name));

	if (!InitScriptSystem())
		return;

	try {
		std::vector<char> linebuffer;

		isi = CreateScriptInterpreter();
		if (!isi) throw MyError("Not enough memory to create script interpreter");

		isi->SetRootHandler(RootHandler, NULL);

		if (!(f=fopen(filenameA.c_str(), "r")))
			throw MyError("Can't open script file");

		int c;

		do {
			c = getc(f);

			if (c == '\n' || c == EOF) {
				linebuffer.push_back(0);
				isi->ExecuteLine(&linebuffer[0]);
				linebuffer.clear();
			} else
				linebuffer.push_back(c);
		} while(c != EOF);
	} catch(CScriptError cse) {
		MessageBox(NULL, isi->TranslateScriptError(cse), "Sylia script error", MB_OK | MB_TASKMODAL | MB_TOPMOST);
	} catch(MyUserAbortError e) {
		/* do nothing */
	} catch(const MyError& e) {
		e.post(NULL, g_szError);
	}

	isi->Destroy();
	if (f) fclose(f);

	DeinitScriptSystem();
	RemakePositionSlider();
}

void RunScriptMemory(char *mem) {
	IScriptInterpreter *isi = NULL;

	if (!InitScriptSystem())
		return;

	try {
		std::vector<char> linebuffer;
		char *s=mem, *t;

		isi = CreateScriptInterpreter();
		if (!isi) throw MyError("Not enough memory to create script interpreter");

		isi->SetRootHandler(RootHandler, NULL);

		while(*s) {
			t = s;
			while(*t && *t!='\n') ++t;

			linebuffer.resize(t+1-s);
			memcpy(&linebuffer[0], s, t-s);
			linebuffer[t-s] = 0;

			isi->ExecuteLine(&linebuffer[0]);

			s = t;
			if (*s=='\n') ++s;
		}

	} catch(CScriptError cse) {
		MessageBox(NULL, isi->TranslateScriptError(cse), "Sylia script error", MB_OK | MB_TASKMODAL);
	} catch(MyUserAbortError e) {
		isi->Destroy();
		DeinitScriptSystem();
		throw MyError("Aborted by user");
	} catch(const MyError&) {
		isi->Destroy();
		DeinitScriptSystem();

		throw;
//		e.post(NULL, szError);
	}

	isi->Destroy();

	DeinitScriptSystem();
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

		if (isalpha(c)) c=toupper(c);
		else if (isspace(c)) {
			c = ' ';
			while(*s && isspace(*s)) ++s;
		} else if (c)
			c = '_';

		d = *t++;

		if (isalpha(d)) d=toupper(d);
		else if (isspace(d)) {
			d = ' ';
			while(*t && isspace(*t)) ++t;
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
//	Object: VirtualDub.video.filters
//
///////////////////////////////////////////////////////////////////////////

static void func_VDVFiltInst_Remove(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
	FilterInstance *fa = (FilterInstance *)lpVoid;

	fa->Remove();
	fa->Destroy();
}

static void func_VDVFiltInst_SetClipping(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
	FilterInstance *fa = (FilterInstance *)(FilterActivation *)lpVoid;

	fa->x1	= argv[0].asInt();
	fa->y1	= argv[1].asInt();
	fa->x2	= argv[2].asInt();
	fa->y2	= argv[3].asInt();
}

static int func_VDVFiltInst_GetClipping(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
	FilterInstance *fa = (FilterInstance *)(FilterActivation *)lpVoid;

	switch(argv[0].asInt()) {
	case 0:	return fa->x1;
	case 1: return fa->y1;
	case 2: return fa->x2;
	case 3: return fa->y2;
	}

	return 0;
}

static CScriptValue obj_VDVFiltInst_lookup(IScriptInterpreter *isi, CScriptObject *thisPtr, void *lpVoid, char *szName) {
	FilterInstance *fa = (FilterInstance *)(FilterActivation *)lpVoid;

	if (fa->filter->script_obj)
		return isi->LookupObjectMember(fa->filter->script_obj, lpVoid, szName);

	return CScriptValue();
}

static ScriptFunctionDef obj_VDVFiltInst_functbl[]={
	{ (ScriptFunctionPtr)func_VDVFiltInst_Remove			, "Remove", "0" },
	{ (ScriptFunctionPtr)func_VDVFiltInst_SetClipping		, "SetClipping", "0iiii" },
	{ (ScriptFunctionPtr)func_VDVFiltInst_GetClipping		, "GetClipping", "ii" },
	{ NULL }
};

static CScriptObject obj_VDVFiltInst={
	obj_VDVFiltInst_lookup, obj_VDVFiltInst_functbl, NULL	
};

///////////////////

static CScriptValue obj_VDVFilters_instance(IScriptInterpreter *isi, void *, int index) {
	FilterInstance *fa = (FilterInstance *)g_listFA.tail.next, *fa2;
	CScriptValue t = CScriptValue(&obj_VDVFiltInst);

	while((fa2 = (FilterInstance *)fa->next) && index--)
		fa = fa2;

	if (index!=-1 || !fa->next) EXT_SCRIPT_ERROR(VAR_NOT_FOUND);

	t.lpVoid = (FilterActivation *)fa;
	return t;
}

static void func_VDVFilters_Clear(IScriptInterpreter *, void *, CScriptValue *, int) {
	FilterInstance *fa;

	while(fa = (FilterInstance *)g_listFA.RemoveHead()) {
		fa->Destroy();
	}
}

static int func_VDVFilters_Add(IScriptInterpreter *isi, CScriptObject *, CScriptValue *argv, int argc) {
	std::list<FilterBlurb>	filterList;

	FilterEnumerateFilters(filterList);

	for(std::list<FilterBlurb>::const_iterator it(filterList.begin()), itEnd(filterList.end()); it!=itEnd; ++it) {
		const FilterBlurb& fb = *it;

		if (strfuzzycompare(fb.name.c_str(), *argv[0].asString())) {
			FilterInstance *fa = new FilterInstance(fb.key);

			if (!fa) EXT_SCRIPT_ERROR(OUT_OF_MEMORY);

			fa->x1 = fa->y1 = fa->x2 = fa->y2 = 0;

			g_listFA.AddHead(fa);

			{
				int count = 0;

				FilterInstance *fa = (FilterInstance *)g_listFA.tail.next, *fa2;

				while(fa2 = (FilterInstance *)fa->next) {
					fa = fa2;
					++count;
				}

				return count-1;
			}
		}
	}

	throw MyError("Cannot add filter '%s': no such filter loaded", *argv[0].asString());
}

static CScriptValue obj_VDVFilters_lookup(IScriptInterpreter *isi, CScriptObject *thisPtr, void *lpVoid, char *szName) {
	if (!strcmp(szName, "instance"))
		return CScriptValue(thisPtr, &obj_VDVFilters_instance);

	EXT_SCRIPT_ERROR(MEMBER_NOT_FOUND);
}

static ScriptFunctionDef obj_VDVFilters_functbl[]={
	{ (ScriptFunctionPtr)func_VDVFilters_Clear			, "Clear", "0" },
	{ (ScriptFunctionPtr)func_VDVFilters_Add			, "Add", "is", },
	{ NULL }
};

static CScriptObject obj_VDVFilters={
	obj_VDVFilters_lookup, obj_VDVFilters_functbl, NULL	
};

///////////////////////////////////////////////////////////////////////////
//
//	Object: VirtualDub.video
//
///////////////////////////////////////////////////////////////////////////

static int func_VDVideo_GetDepth(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	return arglist[0].asInt() ? g_dubOpts.video.outputDepth : g_dubOpts.video.inputDepth;
}

static void func_VDVideo_SetDepth(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	int new_depth1, new_depth2;

	switch(arglist[0].asInt()) {
	case 16:	new_depth1 = 0; break;
	case 24:	new_depth1 = 1; break;
	case 32:	new_depth1 = 2; break;
	default:
		return;
	}

	switch(arglist[1].asInt()) {
	case 16:	new_depth2 = 0; break;
	case 24:	new_depth2 = 1; break;
	case 32:	new_depth2 = 2; break;
	default:
		return;
	}

	g_dubOpts.video.inputDepth = new_depth1;
	g_dubOpts.video.outputDepth = new_depth2;
}

static int func_VDVideo_GetMode(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	return g_dubOpts.video.mode;
}

static void func_VDVideo_SetMode(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	int new_mode = arglist[0].asInt();

	if (new_mode>=0 && new_mode<4)
		g_dubOpts.video.mode = new_mode;
}

static int func_VDVideo_GetFrameRate(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	switch(arglist[0].asInt()) {
	case 0: return g_dubOpts.video.frameRateDecimation;
	case 1: return g_dubOpts.video.frameRateNewMicroSecs;
	case 2: return g_dubOpts.video.fInvTelecine;
	}
	return 0;
}

static void func_VDVideo_SetFrameRate(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
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

static void func_VDVideo_SetTargetFrameRate(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	g_dubOpts.video.frameRateDecimation = 1;
	g_dubOpts.video.frameRateTargetLo = arglist[1].asInt();
	g_dubOpts.video.frameRateTargetHi = arglist[0].asInt();
}

static int func_VDVideo_GetRange(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	return arglist[0].asInt() ? g_dubOpts.video.lEndOffsetMS : g_dubOpts.video.lStartOffsetMS;
}

static void func_VDVideo_SetRange(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	SetSelectionStart(arglist[0].asInt());
	SetSelectionEnd(arglist[1].asInt());
}

static int func_VDVideo_GetCompression(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	if (!g_Vcompression.dwFlags & ICMF_COMPVARS_VALID)
		return 0;

	switch(arglist[0].asInt()) {
	case 0:	return (int)g_Vcompression.fccHandler;
	case 1: return g_Vcompression.lKey;
	case 2: return g_Vcompression.lQ;
	case 3: return g_Vcompression.lDataRate;
	}

	return 0;
}

static void func_VDVideo_SetCompression(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
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

static void func_VDVideo_SetCompData(IScriptInterpreter *isi, CScriptObject *, CScriptValue *arglist, int arg_count) {
	void *mem;
	long l = ((strlen(*arglist[1].asString())+3)/4)*3;

	if (!(g_Vcompression.dwFlags & ICMF_COMPVARS_VALID))
		return;

	if (arglist[0].asInt() > l) return;

	l = arglist[0].asInt();

	if (!(mem = allocmem(l)))
		EXT_SCRIPT_ERROR(OUT_OF_MEMORY);

	_CrtCheckMemory();
	memunbase64((char *)mem, *arglist[1].asString(), l);
	_CrtCheckMemory();

	ICSetState(g_Vcompression.hic, mem, l);

	freemem(mem);
}

static void func_VDVideo_EnableIndeoQC(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
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

static void func_VDVideo_SetIVTC(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	g_dubOpts.video.fInvTelecine = !!arglist[0].asInt();
	g_dubOpts.video.fIVTCMode = !!arglist[1].asInt();
	g_dubOpts.video.nIVTCOffset = arglist[2].asInt();
	if (g_dubOpts.video.nIVTCOffset >= 0)
		g_dubOpts.video.nIVTCOffset %= 5;
	g_dubOpts.video.fIVTCPolarity = !!arglist[3].asInt();
}

static ScriptFunctionDef obj_VDVideo_functbl[]={
	{ (ScriptFunctionPtr)func_VDVideo_GetDepth			, "GetDepth", "ii" },
	{ (ScriptFunctionPtr)func_VDVideo_SetDepth			, "SetDepth", "0ii" },
	{ (ScriptFunctionPtr)func_VDVideo_GetMode			, "GetMode", "i" },
	{ (ScriptFunctionPtr)func_VDVideo_SetMode			, "SetMode", "0i" },
	{ (ScriptFunctionPtr)func_VDVideo_GetFrameRate		, "GetFrameRate", "ii" },
	{ (ScriptFunctionPtr)func_VDVideo_SetFrameRate		, "SetFrameRate", "0ii" },
	{ (ScriptFunctionPtr)func_VDVideo_SetFrameRate		, NULL, "0iii" },
	{ (ScriptFunctionPtr)func_VDVideo_SetTargetFrameRate, "SetTargetFrameRate", "0ii" },
	{ (ScriptFunctionPtr)func_VDVideo_GetRange			, "GetRange", "ii" },
	{ (ScriptFunctionPtr)func_VDVideo_SetRange			, "SetRange", "0ii" },
	{ (ScriptFunctionPtr)func_VDVideo_GetCompression	, "GetCompression"	, "ii" },
	{ (ScriptFunctionPtr)func_VDVideo_SetCompression	, "SetCompression"	, "0siii" },
	{ (ScriptFunctionPtr)func_VDVideo_SetCompression	, NULL				, "0iiii" },
	{ (ScriptFunctionPtr)func_VDVideo_SetCompression	, NULL				, "0" },
	{ (ScriptFunctionPtr)func_VDVideo_SetCompData		, "SetCompData", "0is" },
	{ (ScriptFunctionPtr)func_VDVideo_EnableIndeoQC		, "EnableIndeoQC", "0i" },
	{ (ScriptFunctionPtr)func_VDVideo_SetIVTC			, "SetIVTC", "0iiii" },
	{ NULL }
};

static ScriptObjectDef obj_VDVideo_objtbl[]={
	{ "filters", &obj_VDVFilters },
	{ NULL }
};

static CScriptValue obj_VirtualDub_video_lookup(IScriptInterpreter *isi, CScriptObject *obj, void *lpVoid, char *szName) {
	if (!strcmp(szName, "width"))
		return CScriptValue(inputVideoAVI ? inputVideoAVI->getImageFormat()->biWidth : 0);
	else if (!strcmp(szName, "height"))
		return CScriptValue(inputVideoAVI ? inputVideoAVI->getImageFormat()->biHeight : 0);

	EXT_SCRIPT_ERROR(MEMBER_NOT_FOUND);
}

static CScriptObject obj_VDVideo={
	obj_VirtualDub_video_lookup, obj_VDVideo_functbl, obj_VDVideo_objtbl
};

///////////////////////////////////////////////////////////////////////////
//
//	Object: VirtualDub.audio.filters.instance
//
///////////////////////////////////////////////////////////////////////////

namespace {
	const VDFilterConfigEntry *GetFilterParamEntry(const VDFilterConfigEntry *pEnt, const unsigned idx) {
		if (pEnt)
			for(; pEnt->next; pEnt=pEnt->next) {
				if (pEnt->idx == idx)
					return pEnt;
			}

		return NULL;
	}

	void SetFilterParam(void *lpVoid, unsigned idx, const VDFilterConfigVariant& v) {
		VDAudioFilterGraph::FilterEntry *pFilt = (VDAudioFilterGraph::FilterEntry *)lpVoid;

		VDPluginDescription *pDesc = VDGetPluginDescription(pFilt->mFilterName.c_str(), kVDPluginType_Audio);

		if (!pDesc)
			throw MyError("VDAFiltInst: Unknown audio filter: \"%s\"", VDTextWToA(pFilt->mFilterName).c_str());

		VDAutolockPlugin<VDAudioFilterDefinition> lock(pDesc);

		const VDFilterConfigEntry *pEnt = GetFilterParamEntry(lock->mpConfigInfo, idx);

		if (!pEnt)
			throw MyError("VDAFiltInst: Audio filter \"%s\" does not have a parameter with id %d", VDTextWToA(pFilt->mFilterName).c_str(), idx);

		VDFilterConfigVariant& var = pFilt->mConfig[idx];

		switch(pEnt->type) {
		case VDFilterConfigEntry::kTypeU32:
			switch(v.GetType()) {
			case VDFilterConfigVariant::kTypeU32:	var.SetU32(v.GetU32()); return;
			case VDFilterConfigVariant::kTypeS32:	var.SetU32(v.GetS32()); return;
			case VDFilterConfigVariant::kTypeU64:	var.SetU32(v.GetU64()); return;
			case VDFilterConfigVariant::kTypeS64:	var.SetU32(v.GetS64()); return;
			}
			break;
		case VDFilterConfigEntry::kTypeS32:
			switch(v.GetType()) {
			case VDFilterConfigVariant::kTypeU32:	var.SetS32(v.GetU32()); return;
			case VDFilterConfigVariant::kTypeS32:	var.SetS32(v.GetS32()); return;
			case VDFilterConfigVariant::kTypeU64:	var.SetS32(v.GetU64()); return;
			case VDFilterConfigVariant::kTypeS64:	var.SetS32(v.GetS64()); return;
			}
			break;
		case VDFilterConfigEntry::kTypeU64:
			switch(v.GetType()) {
			case VDFilterConfigVariant::kTypeU32:	var.SetU64(v.GetU32()); return;
			case VDFilterConfigVariant::kTypeS32:	var.SetU64(v.GetS32()); return;
			case VDFilterConfigVariant::kTypeU64:	var.SetU64(v.GetU64()); return;
			case VDFilterConfigVariant::kTypeS64:	var.SetU64(v.GetS64()); return;
			}
			break;
		case VDFilterConfigEntry::kTypeS64:
			switch(v.GetType()) {
			case VDFilterConfigVariant::kTypeU32:	var.SetS64(v.GetU32()); return;
			case VDFilterConfigVariant::kTypeS32:	var.SetS64(v.GetS32()); return;
			case VDFilterConfigVariant::kTypeU64:	var.SetS64(v.GetU64()); return;
			case VDFilterConfigVariant::kTypeS64:	var.SetS64(v.GetS64()); return;
			}
			break;
		case VDFilterConfigEntry::kTypeDouble:
			if (v.GetType() == VDFilterConfigEntry::kTypeDouble) {
				var = v;
				return;
			}
			break;
		case VDFilterConfigEntry::kTypeAStr:
			if (v.GetType() == VDFilterConfigEntry::kTypeWStr) {
				var.SetAStr(VDTextWToA(v.GetWStr()).c_str());
				return;
			}
			break;
		case VDFilterConfigEntry::kTypeWStr:
			if (v.GetType() == VDFilterConfigEntry::kTypeWStr) {
				var = v;
				return;
			}
			break;
		case VDFilterConfigEntry::kTypeBlock:
			if (v.GetType() == VDFilterConfigEntry::kTypeBlock) {
				var = v;
				return;
			}
		}

		pFilt->mConfig.erase(idx);

		throw MyError("VDAFiltInst: Type mismatch on audio filter \"%s\" param %d (\"%s\")", VDTextWToA(pFilt->mFilterName).c_str(), idx, VDTextWToA(pEnt->name).c_str());
	}
};

static void func_VDAFiltInst_SetInt(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
	VDFilterConfigVariant v;
	
	v.SetS32(argv[1].asInt());

	SetFilterParam(lpVoid, argv[0].asInt(), v);
}

static void func_VDAFiltInst_SetLong(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
	VDFilterConfigVariant v;
	
	v.SetU64((uint32)argv[2].asInt() + ((uint64)(uint32)argv[1].asInt() << 32));

	SetFilterParam(lpVoid, argv[0].asInt(), v);
}

static void func_VDAFiltInst_SetDouble(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
	VDFilterConfigVariant v;

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

	SetFilterParam(lpVoid, argv[0].asInt(), v);
}

static void func_VDAFiltInst_SetString(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
	VDFilterConfigVariant v;
	
	v.SetWStr(VDTextU8ToW(*argv[1].asString(), -1).c_str());

	SetFilterParam(lpVoid, argv[0].asInt(), v);
}

static void func_VDAFiltInst_SetRaw(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
	VDFilterConfigVariant v;

	int len = argv[1].asInt();
	std::vector<char> mem(len);
	long l = ((strlen(*argv[2].asString())+3)/4)*3;

	if (len > l)
		return;

	memunbase64(&mem[0], *argv[2].asString(), len);
	v.SetBlock(&mem.front(), len);

	SetFilterParam(lpVoid, argv[0].asInt(), v);
}

static ScriptFunctionDef obj_VDAFiltInst_functbl[]={
	{ (ScriptFunctionPtr)func_VDAFiltInst_SetInt,		"SetInt",		"0ii" },
	{ (ScriptFunctionPtr)func_VDAFiltInst_SetLong,		"SetLong",		"0iii" },
	{ (ScriptFunctionPtr)func_VDAFiltInst_SetDouble,	"SetDouble",	"0iii" },
	{ (ScriptFunctionPtr)func_VDAFiltInst_SetString,	"SetString",	"0is" },
	{ (ScriptFunctionPtr)func_VDAFiltInst_SetRaw,		"SetRaw",		"0iis" },
	{ NULL }
};

static CScriptObject obj_VDAFiltInst={
	NULL, obj_VDAFiltInst_functbl, NULL	
};

///////////////////////////////////////////////////////////////////////////
//
//	Object: VirtualDub.audio.filters
//
///////////////////////////////////////////////////////////////////////////

static CScriptValue obj_VDAFilters_instance(IScriptInterpreter *isi, void *, int index) {
	if (index < 0 || index >= g_audioFilterGraph.mFilters.size()) {
		EXT_SCRIPT_ERROR(VAR_NOT_FOUND);
	}

	VDAudioFilterGraph::FilterList::iterator it(g_audioFilterGraph.mFilters.begin());

	std::advance(it, index);

	CScriptValue val(&obj_VDAFiltInst);

	val.lpVoid = static_cast<VDAudioFilterGraph::FilterEntry *>(&*it);
	return val;
}

static void func_VDAFilters_Clear(IScriptInterpreter *, void *, CScriptValue *, int) {
	g_audioFilterGraph.mFilters.clear();
	g_audioFilterGraph.mConnections.clear();
}

static int func_VDAFilters_Add(IScriptInterpreter *isi, CScriptObject *, CScriptValue *argv, int argc) {
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

	return g_audioFilterGraph.mFilters.size()-1;
}

static void func_VDAFilters_Connect(IScriptInterpreter *isi, CScriptObject *, CScriptValue *argv, int argc) {
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

static CScriptValue obj_VDAFilters_lookup(IScriptInterpreter *isi, CScriptObject *thisPtr, void *lpVoid, char *szName) {
	if (!strcmp(szName, "instance"))
		return CScriptValue(thisPtr, &obj_VDAFilters_instance);

	EXT_SCRIPT_ERROR(MEMBER_NOT_FOUND);
}

static ScriptFunctionDef obj_VDAFilters_functbl[]={
	{ (ScriptFunctionPtr)func_VDAFilters_Clear			, "Clear", "0" },
	{ (ScriptFunctionPtr)func_VDAFilters_Add			, "Add", "is", },
	{ (ScriptFunctionPtr)func_VDAFilters_Connect		, "Connect", "0iiii", },
	{ NULL }
};

static CScriptObject obj_VDAFilters={
	obj_VDAFilters_lookup, obj_VDAFilters_functbl, NULL	
};

///////////////////////////////////////////////////////////////////////////
//
//	Object: VirtualDub.audio
//
///////////////////////////////////////////////////////////////////////////

static int func_VDAudio_GetMode(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	return g_dubOpts.audio.mode;
}

static void func_VDAudio_SetMode(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	int new_mode = arglist[0].asInt();

	if (new_mode>=0 && new_mode<2)
		g_dubOpts.audio.mode = new_mode;
}

static int func_VDAudio_GetInterleave(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	switch(arglist[0].asInt()) {
	case 0:	return g_dubOpts.audio.enabled;
	case 1:	return g_dubOpts.audio.preload;
	case 2:	return g_dubOpts.audio.interval;
	case 3:	return g_dubOpts.audio.is_ms;
	case 4: return g_dubOpts.audio.offset;
	}

	return 0;
}

static void func_VDAudio_SetInterleave(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	g_dubOpts.audio.enabled		= !!arglist[0].asInt();
	g_dubOpts.audio.preload		= arglist[1].asInt();
	g_dubOpts.audio.interval	= arglist[2].asInt();
	g_dubOpts.audio.is_ms		= !!arglist[3].asInt();
	g_dubOpts.audio.offset		= arglist[4].asInt();
}

static int func_VDAudio_GetClipMode(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	switch(arglist[0].asInt()) {
	case 0:		return g_dubOpts.audio.fStartAudio;
	case 1:		return g_dubOpts.audio.fEndAudio;
	}

	return 0;
}

static void func_VDAudio_SetClipMode(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	g_dubOpts.audio.fStartAudio	= !!arglist[0].asInt();
	g_dubOpts.audio.fEndAudio	= !!arglist[1].asInt();
}

static int func_VDAudio_GetConversion(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	switch(arglist[0].asInt()) {
	case 0:		return g_dubOpts.audio.new_rate;
	case 1:		return g_dubOpts.audio.newPrecision;
	case 2:		return g_dubOpts.audio.newChannels;
	}

	return 0;
}

static void func_VDAudio_SetConversion(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	g_dubOpts.audio.new_rate		= arglist[0].asInt();
	g_dubOpts.audio.newPrecision	= arglist[1].asInt();
	g_dubOpts.audio.newChannels		= arglist[2].asInt();

	if (arg_count >= 5) {
		g_dubOpts.audio.integral_rate	= !!arglist[3].asInt();
		g_dubOpts.audio.fHighQuality	= !!arglist[4].asInt();
	}
}

static void func_VDAudio_SetSource(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	if (arglist[0].isInt())
		audioInputMode = !!arglist[0].asInt();
	else {
		OpenWAV(VDTextU8ToW(VDStringA(*arglist[0].asString())).c_str());
		audioInputMode = AUDIOIN_WAVE;
	}
}

static void func_VDAudio_SetCompressionPCM(IScriptInterpreter *isi, void *, CScriptValue *arglist, int arg_count) {
	PCMWAVEFORMAT *pwf = (PCMWAVEFORMAT *)allocmem(sizeof(PCMWAVEFORMAT));

	if (!pwf) EXT_SCRIPT_ERROR(OUT_OF_MEMORY);

	pwf->wf.wFormatTag			= WAVE_FORMAT_PCM;
	pwf->wf.nSamplesPerSec		= arglist[0].asInt();
	pwf->wf.nChannels			= arglist[1].asInt();
	pwf->   wBitsPerSample		= arglist[2].asInt();
	pwf->wf.nBlockAlign		= (pwf->wBitsPerSample/8) * pwf->wf.nChannels;
	pwf->wf.nAvgBytesPerSec	= pwf->wf.nSamplesPerSec * pwf->wf.nBlockAlign;
	g_ACompressionFormatSize = sizeof(PCMWAVEFORMAT);
	freemem(g_ACompressionFormat);
	g_ACompressionFormat = (WAVEFORMATEX *)pwf;
}

// VirtualDub.audio.SetCompression();
// VirtualDub.audio.SetCompression(tag, sampling_rate, channels, bits, bytes/sec, blockalign);
// VirtualDub.audio.SetCompression(tag, sampling_rate, channels, bits, bytes/sec, blockalign, exdatalen, exdata);

static void func_VDAudio_SetCompression(IScriptInterpreter *isi, void *, CScriptValue *arglist, int arg_count) {
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
		EXT_SCRIPT_ERROR(OUT_OF_MEMORY);

	wfex->wFormatTag		= arglist[0].asInt();
	wfex->nSamplesPerSec	= arglist[1].asInt();
	wfex->nChannels			= arglist[2].asInt();
	wfex->wBitsPerSample	= arglist[3].asInt();
	wfex->nAvgBytesPerSec	= arglist[4].asInt();
	wfex->nBlockAlign		= arglist[5].asInt();
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

static void func_VDAudio_SetVolume(IScriptInterpreter *isi, void *, CScriptValue *arglist, int arg_count) {
	if (arg_count)
		g_dubOpts.audio.volume = arglist[0].asInt();		
	else
		g_dubOpts.audio.volume = 0;
}

static int func_VDAudio_GetVolume(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	return g_dubOpts.audio.volume;
}

static void func_VDAudio_EnableFilterGraph(IScriptInterpreter *isi, void *, CScriptValue *arglist, int arg_count) {
	g_dubOpts.audio.bUseAudioFilterGraph = (arglist[0].asInt() != 0);
}

static ScriptFunctionDef obj_VDAudio_functbl[]={
	{ (ScriptFunctionPtr)func_VDAudio_GetMode			, "GetMode"				, "i"		},
	{ (ScriptFunctionPtr)func_VDAudio_SetMode			, "SetMode"				, "0i"		},
	{ (ScriptFunctionPtr)func_VDAudio_GetInterleave		, "GetInterleave"		, "ii"		},
	{ (ScriptFunctionPtr)func_VDAudio_SetInterleave		, "SetInterleave"		, "0iiiii"	},
	{ (ScriptFunctionPtr)func_VDAudio_GetClipMode		, "GetClipMode"			, "ii"		},
	{ (ScriptFunctionPtr)func_VDAudio_SetClipMode		, "SetClipMode"			, "0ii"		},
	{ (ScriptFunctionPtr)func_VDAudio_GetConversion		, "GetConversion"		, "ii"		},
	{ (ScriptFunctionPtr)func_VDAudio_SetConversion		, "SetConversion"		, "0iii"	},
	{ (ScriptFunctionPtr)func_VDAudio_SetConversion		, NULL					, "0iiiii"	},
	{ (ScriptFunctionPtr)func_VDAudio_SetSource			, "SetSource"			, "0i"		},
	{ (ScriptFunctionPtr)func_VDAudio_SetSource			, NULL					, "0s"		},
	{ (ScriptFunctionPtr)func_VDAudio_SetCompressionPCM	, "SetCompressionPCM"	, "0iii"	},
	{ (ScriptFunctionPtr)func_VDAudio_SetCompression	, "SetCompression"		, "0"		},
	{ (ScriptFunctionPtr)func_VDAudio_SetCompression	, NULL					, "0iiiiii" },
	{ (ScriptFunctionPtr)func_VDAudio_SetCompression	, NULL					, "0iiiiiiis" },
	{ (ScriptFunctionPtr)func_VDAudio_SetVolume			, "SetVolume"			, "0" },
	{ (ScriptFunctionPtr)func_VDAudio_SetVolume			, NULL					, "0i" },
	{ (ScriptFunctionPtr)func_VDAudio_GetVolume			, "GetVolume"			, "i" },
	{ (ScriptFunctionPtr)func_VDAudio_EnableFilterGraph	, "EnableFilterGraph"	, "0i" },
	{ NULL }
};

static ScriptObjectDef obj_VDAudio_objtbl[]={
	{ "filters", &obj_VDAFilters },
	{ NULL }
};

static CScriptObject obj_VDAudio={
	NULL, obj_VDAudio_functbl, obj_VDAudio_objtbl
};

///////////////////////////////////////////////////////////////////////////
//
//	Object: VirtualDub.subset
//
///////////////////////////////////////////////////////////////////////////

static void func_VDSubset_Delete(IScriptInterpreter *isi, void *, CScriptValue *arglist, int arg_count) {
	g_project->ResetTimeline();
}

static void func_VDSubset_Clear(IScriptInterpreter *isi, void *, CScriptValue *arglist, int arg_count) {
	g_project->ResetTimeline();
	if (inputSubset)
		inputSubset->clear();
}

static void func_VDSubset_AddRange(IScriptInterpreter *isi, void *, CScriptValue *arglist, int arg_count) {
	if (inputSubset)
		inputSubset->addRange(arglist[0].asInt(), arglist[1].asInt(), false);
}

static void func_VDSubset_AddMaskedRange(IScriptInterpreter *isi, void *, CScriptValue *arglist, int arg_count) {
	if (inputSubset)
		inputSubset->addRange(arglist[0].asInt(), arglist[1].asInt(), true);
}

static ScriptFunctionDef obj_VDSubset_functbl[]={
	{ (ScriptFunctionPtr)func_VDSubset_Delete			, "Delete"				, "0"		},
	{ (ScriptFunctionPtr)func_VDSubset_Clear			, "Clear"				, "0"		},
	{ (ScriptFunctionPtr)func_VDSubset_AddRange			, "AddFrame"			, "0ii"		},	// DEPRECATED
	{ (ScriptFunctionPtr)func_VDSubset_AddRange			, "AddRange"			, "0ii"		},
	{ (ScriptFunctionPtr)func_VDSubset_AddMaskedRange	, "AddMaskedRange"		, "0ii"		},

	{ NULL }
};

static CScriptObject obj_VDSubset={
	NULL, obj_VDSubset_functbl
};


///////////////////////////////////////////////////////////////////////////
//
//	Object: VirtualDub
//
///////////////////////////////////////////////////////////////////////////

extern void CloseAVI();
extern void PreviewAVI(HWND, DubOptions *, int iPriority=0, bool fProp=false);

static void func_VirtualDub_SetStatus(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	guiSetStatus("%s", 255, *arglist[0].asString());
}

static void func_VirtualDub_OpenOld(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	VDStringW filename(VDTextAToW(*arglist[0].asString()));
	IVDInputDriver *pDriver = VDGetInputDriverForLegacyIndex(arglist[1].asInt());

	if (arg_count > 3) {
		long l = ((strlen(*arglist[3].asString())+3)/4)*3;
		char buf[64];

		memunbase64(buf, *arglist[3].asString(), l);

		g_project->Open(filename.c_str(), pDriver, !!arglist[2].asInt(), true, false, buf);
	} else
		g_project->Open(filename.c_str(), pDriver, !!arglist[2].asInt(), true, false);
}

static void func_VirtualDub_Open(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	VDStringW filename(VDTextU8ToW(VDStringA(*arglist[0].asString())));
	IVDInputDriver *pDriver = VDGetInputDriverByName(VDTextAToW(*arglist[1].asString()).c_str());

	if (arg_count > 3) {
		long l = ((strlen(*arglist[3].asString())+3)/4)*3;
		char buf[64];

		memunbase64(buf, *arglist[3].asString(), l);

		g_project->Open(filename.c_str(), pDriver, !!arglist[2].asInt(), true, false, buf);
	} else
		g_project->Open(filename.c_str(), pDriver, !!arglist[2].asInt(), true, false);
}

static void func_VirtualDub_Append(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	VDStringW filename(VDTextU8ToW(VDStringA(*arglist[0].asString())));

	AppendAVI(filename.c_str());
}

static void func_VirtualDub_Close(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	g_project->Close();
}

static void func_VirtualDub_Preview(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	PreviewAVI(g_hWnd, NULL, g_prefs.main.iPreviewPriority,true);
}

static void func_VirtualDub_SaveAVI(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	VDStringW filename(VDTextU8ToW(VDStringA(*arglist[0].asString())));

	if (g_fJobMode) {
		DubOptions opts(g_dubOpts);

		opts.fShowStatus			= false;
		opts.fMoveSlider			= true;
		opts.video.fShowInputFrame	= false;
		opts.video.fShowOutputFrame	= false;
		opts.video.fShowDecompressedFrame	= false;

		SaveAVI(filename.c_str(), true, &opts, false);
	} else
		SaveAVI(filename.c_str(), true, NULL, false);
}

static void func_VirtualDub_SaveCompatibleAVI(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	VDStringW filename(VDTextU8ToW(VDStringA(*arglist[0].asString())));

	if (g_fJobMode) {
		DubOptions opts(g_dubOpts);

		opts.fShowStatus			= false;
		opts.fMoveSlider			= true;
		opts.video.fShowInputFrame	= false;
		opts.video.fShowOutputFrame	= false;
		opts.video.fShowDecompressedFrame	= false;

		SaveAVI(filename.c_str(), true, &opts, true);
	} else
		SaveAVI(filename.c_str(), true, NULL, true);
}

static void func_VirtualDub_SaveSegmentedAVI(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	const VDStringW filename(VDTextU8ToW(VDStringA(*arglist[0].asString())));

	if (g_fJobMode) {
		DubOptions opts(g_dubOpts);

		opts.fShowStatus			= false;
		opts.fMoveSlider			= true;
		opts.video.fShowInputFrame	= false;
		opts.video.fShowOutputFrame	= false;
		opts.video.fShowDecompressedFrame	= false;

		SaveSegmentedAVI(filename.c_str(), true, &opts, arglist[1].asInt(), arglist[2].asInt());
	} else
		SaveSegmentedAVI(filename.c_str(), true, NULL, arglist[1].asInt(), arglist[2].asInt());
}

static void func_VirtualDub_SaveImageSequence(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	const VDStringW prefix(VDTextU8ToW(VDStringA(*arglist[0].asString())));
	const VDStringW suffix(VDTextU8ToW(VDStringA(*arglist[1].asString())));

	if (g_fJobMode) {
		DubOptions opts(g_dubOpts);

		opts.fShowStatus			= false;
		opts.fMoveSlider			= true;
		opts.video.fShowInputFrame	= false;
		opts.video.fShowOutputFrame	= false;
		opts.video.fShowDecompressedFrame	= false;

		SaveImageSequence(prefix.c_str(), suffix.c_str(), arglist[2].asInt(), true, &opts, arglist[3].asInt());
	} else
		SaveImageSequence(prefix.c_str(), suffix.c_str(), arglist[2].asInt(), true, NULL, arglist[3].asInt());
}

static void func_VirtualDub_SaveWAV(IScriptInterpreter *, CScriptObject *, CScriptValue *arglist, int arg_count) {
	const VDStringW filename(VDTextU8ToW(VDStringA(*arglist[0].asString())));

	SaveWAV(filename.c_str());
}

extern "C" unsigned long version_num;

static CScriptValue obj_VirtualDub_lookup(IScriptInterpreter *isi, CScriptObject *obj, void *lpVoid, char *szName) {
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

		return CScriptValue(handle);
	} else if (!strcmp(szName, "video"))
		return &obj_VDVideo;
	else if (!strcmp(szName, "audio"))
		return &obj_VDAudio;
	else if (!strcmp(szName, "subset"))
		return &obj_VDSubset;

	EXT_SCRIPT_ERROR(MEMBER_NOT_FOUND);
}

static ScriptFunctionDef obj_VirtualDub_functbl[]={
	{ (ScriptFunctionPtr)func_VirtualDub_SetStatus,			"SetStatus",			"0s" },
	{ (ScriptFunctionPtr)func_VirtualDub_OpenOld,			"Open",					"0sii" },
	{ (ScriptFunctionPtr)func_VirtualDub_OpenOld,			NULL,					"0siis" },
	{ (ScriptFunctionPtr)func_VirtualDub_Open,				NULL,					"0ssi" },
	{ (ScriptFunctionPtr)func_VirtualDub_Open,				NULL,					"0ssis" },
	{ (ScriptFunctionPtr)func_VirtualDub_Append,			"Append",				"0s" },
	{ (ScriptFunctionPtr)func_VirtualDub_Close,				"Close",				"0" },
	{ (ScriptFunctionPtr)func_VirtualDub_Preview,			"Preview",				"0" },
	{ (ScriptFunctionPtr)func_VirtualDub_SaveAVI,			"SaveAVI",				"0s" },
	{ (ScriptFunctionPtr)func_VirtualDub_SaveCompatibleAVI, "SaveCompatibleAVI",	"0s" },
	{ (ScriptFunctionPtr)func_VirtualDub_SaveSegmentedAVI,	"SaveSegmentedAVI",		"0sii" },
	{ (ScriptFunctionPtr)func_VirtualDub_SaveImageSequence,	"SaveImageSequence",	"0ssii" },
	{ (ScriptFunctionPtr)func_VirtualDub_SaveWAV,			"SaveWAV",				"0s" },
	{ NULL }
};

static CScriptObject obj_VirtualDub={
	&obj_VirtualDub_lookup, obj_VirtualDub_functbl
};

static CScriptValue RootHandler(IScriptInterpreter *isi, char *szName, void *lpData) {
	if (!strcmp(szName, "VirtualDub"))
		return &obj_VirtualDub;

	EXT_SCRIPT_ERROR(VAR_NOT_FOUND);
}


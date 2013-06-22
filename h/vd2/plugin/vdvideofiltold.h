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

#ifndef f_FILTER_H
#define f_FILTER_H

#include <stddef.h>
#include <windows.h>

// Copied from <vd2/system/vdtypes.h>.  Must be in sync.

#ifndef VD_STANDARD_TYPES_DECLARED
	#if defined(_MSC_VER)
		typedef signed __int64		sint64;
		typedef unsigned __int64	uint64;
	#elif defined(__GNUC__)
		typedef signed long long	sint64;
		typedef unsigned long long	uint64;
	#endif
	typedef signed int			sint32;
	typedef unsigned int		uint32;
	typedef signed short		sint16;
	typedef unsigned short		uint16;
	typedef signed char			sint8;
	typedef unsigned char		uint8;

	typedef sint64				int64;
	typedef sint32				int32;
	typedef sint16				int16;
	typedef sint8				int8;
#endif

//////////////////

struct CScriptObject;

//////////////////

enum {
	FILTERPARAM_SWAP_BUFFERS	= 0x00000001L,
	FILTERPARAM_NEEDS_LAST		= 0x00000002L,
};

#define FILTERPARAM_HAS_LAG(frames) ((int)(frames) << 16)

///////////////////

class VFBitmap;
class FilterActivation;
struct FilterFunctions;

typedef int  (*FilterInitProc     )(FilterActivation *fa, const FilterFunctions *ff);
typedef void (*FilterDeinitProc   )(FilterActivation *fa, const FilterFunctions *ff);
typedef int  (*FilterRunProc      )(const FilterActivation *fa, const FilterFunctions *ff);
typedef long (*FilterParamProc    )(FilterActivation *fa, const FilterFunctions *ff);
typedef int  (*FilterConfigProc   )(FilterActivation *fa, const FilterFunctions *ff, HWND hWnd);
typedef void (*FilterStringProc   )(const FilterActivation *fa, const FilterFunctions *ff, char *buf);
typedef int  (*FilterStartProc    )(FilterActivation *fa, const FilterFunctions *ff);
typedef int  (*FilterEndProc      )(FilterActivation *fa, const FilterFunctions *ff);
typedef bool (*FilterScriptStrProc)(FilterActivation *fa, const FilterFunctions *, char *, int);
typedef void (*FilterStringProc2  )(const FilterActivation *fa, const FilterFunctions *ff, char *buf, int maxlen);
typedef int  (*FilterSerialize    )(FilterActivation *fa, const FilterFunctions *ff, char *buf, int maxbuf);
typedef void (*FilterDeserialize  )(FilterActivation *fa, const FilterFunctions *ff, const char *buf, int maxbuf);
typedef void (*FilterCopy         )(FilterActivation *fa, const FilterFunctions *ff, void *dst);

typedef int (__cdecl *FilterModuleInitProc)(struct FilterModule *fm, const FilterFunctions *ff, int& vdfd_ver, int& vdfd_compat);
typedef void (__cdecl *FilterModuleDeinitProc)(struct FilterModule *fm, const FilterFunctions *ff);

//////////

typedef void (__cdecl *FilterPreviewButtonCallback)(bool fNewState, void *pData);
typedef void (__cdecl *FilterPreviewSampleCallback)(VFBitmap *, long lFrame, long lCount, void *pData);

class IFilterPreview {
public:
	virtual void SetButtonCallback(FilterPreviewButtonCallback, void *)=0;
	virtual void SetSampleCallback(FilterPreviewSampleCallback, void *)=0;

	virtual bool isPreviewEnabled()=0;
	virtual void Toggle(HWND)=0;
	virtual void Display(HWND, bool)=0;
	virtual void RedoFrame()=0;
	virtual void RedoSystem()=0;
	virtual void UndoSystem()=0;
	virtual void InitButton(HWND)=0;
	virtual void Close()=0;
	virtual bool SampleCurrentFrame()=0;
	virtual long SampleFrames()=0;
};

//////////

#define VIRTUALDUB_FILTERDEF_VERSION		(9)
#define	VIRTUALDUB_FILTERDEF_COMPATIBLE		(4)

// v3: added lCurrentSourceFrame to FrameStateInfo
// v4 (1.2): lots of additions (VirtualDub 1.2)
// v5 (1.3d): lots of bugfixes - stretchblt bilinear, and non-zero startproc
// v6 (1.4): added error handling functions
// v7 (1.4d): added frame lag, exception handling
// v8 (1.4.11): added string2 proc
// v9 (1.4.12): added (working) copy constructor

typedef struct FilterModule {
	struct FilterModule *next, *prev;
	HINSTANCE				hInstModule;
	FilterModuleInitProc	initProc;
	FilterModuleDeinitProc	deinitProc;
} FilterModule;

typedef struct FilterDefinition {

	struct FilterDefinition *next, *prev;
	FilterModule *module;

	const char *		name;
	const char *		desc;
	const char *		maker;
	void *				private_data;
	int					inst_data_size;

	FilterInitProc		initProc;
	FilterDeinitProc	deinitProc;
	FilterRunProc		runProc;
	FilterParamProc		paramProc;
	FilterConfigProc	configProc;
	FilterStringProc	stringProc;
	FilterStartProc		startProc;
	FilterEndProc		endProc;

	CScriptObject	*script_obj;

	FilterScriptStrProc	fssProc;

	// NEW - 1.4.11
	FilterStringProc2	stringProc2;
	FilterSerialize		serializeProc;
	FilterDeserialize	deserializeProc;
	FilterCopy			copyProc;
} FilterDefinition;

//////////

// FilterStateInfo: contains dynamic info about file being processed

class FilterStateInfo {
public:
	long	lCurrentFrame;				// current output frame
	long	lMicrosecsPerFrame;			// microseconds per output frame
	long	lCurrentSourceFrame;		// current source frame
	long	lMicrosecsPerSrcFrame;		// microseconds per source frame
	long	lSourceFrameMS;				// source frame timestamp
	long	lDestFrameMS;				// output frame timestamp
};

// VFBitmap: VBitmap extended to hold filter-specific information

class VFBitmap {
public:
	void *_vtable;
	uint32 *		data;
	uint32 *		palette;
	int				depth;
	sint32			w, h;
	ptrdiff_t		pitch;
	ptrdiff_t		modulo;
	uint32			size;
	uint32			offset;

	enum {
		NEEDS_HDC		= 0x00000001L,
	};

	DWORD	dwFlags;
	HDC		hdc;


	uint32 *Address32(int x, int y) const {
		return Address32i(x, h-y-1);
	}

	uint32 *Address32i(int x, int y) const {
		return (uint32 *)((char *)data + y*pitch + x*4);
	}

	void AlignTo4() {
		pitch = w << 2;
	}

	void AlignTo8() {
		pitch = ((w+1)&~1) << 2;
	}
};

// FilterActivation: This is what is actually passed to filters at runtime.

class FilterActivation {
public:
	FilterDefinition *filter;
	void *filter_data;
	VFBitmap &dst, &src;
	VFBitmap *__reserved0, *const last;
	unsigned long x1, y1, x2, y2;

	FilterStateInfo *pfsi;
	IFilterPreview *ifp;

	FilterActivation(VFBitmap& _dst, VFBitmap& _src, VFBitmap *_last) : dst(_dst), src(_src), last(_last) {}
	FilterActivation(const FilterActivation& fa, VFBitmap& _dst, VFBitmap& _src, VFBitmap *_last);
};

// These flags must match those in cpuaccel.h!

#ifndef f_VIRTUALDUB_CPUACCEL_H
#define CPUF_SUPPORTS_CPUID			(0x00000001L)
#define CPUF_SUPPORTS_FPU			(0x00000002L)
#define CPUF_SUPPORTS_MMX			(0x00000004L)
#define CPUF_SUPPORTS_INTEGER_SSE	(0x00000008L)
#define CPUF_SUPPORTS_SSE			(0x00000010L)
#define CPUF_SUPPORTS_SSE2			(0x00000020L)
#define CPUF_SUPPORTS_3DNOW			(0x00000040L)
#define CPUF_SUPPORTS_3DNOW_EXT		(0x00000080L)
#endif

struct FilterFunctions {
	FilterDefinition *(__cdecl *addFilter)(FilterModule *, FilterDefinition *, int fd_len);
	void (__cdecl *removeFilter)(FilterDefinition *);
	bool (__cdecl *isFPUEnabled)();
	bool (__cdecl *isMMXEnabled)();
	void (__cdecl *InitVTables)(struct FilterVTbls *);

	// These functions permit you to throw MyError exceptions from a filter.
	// YOU MUST ONLY CALL THESE IN runProc, initProc, and startProc.

	void (__cdecl *ExceptOutOfMemory)();						// ADDED: V6 (VirtualDub 1.4)
	void (__cdecl *Except)(const char *format, ...);			// ADDED: V6 (VirtualDub 1.4)

	// These functions are callable at any time.

	long (__cdecl *getCPUFlags)();								// ADDED: V6 (VirtualDub 1.4)
	long (__cdecl *getHostVersionInfo)(char *buffer, int len);	// ADDED: V7 (VirtualDub 1.4d)
};





///////////////////////////////////////////////////////////////////////////

#ifndef f_SYLIA_SCRIPTINTERPRETER_H
#define f_SYLIA_SCRIPTINTERPRETER_H

class CScriptValue;
class CScriptError;
struct CScriptObject;

class IScriptInterpreter {
public:
	virtual	void Destroy()										=0;

	virtual void SetRootHandler(void *, void *)	=0;

	virtual void ExecuteLine(char *s)							=0;

	virtual void ScriptError(int e)								=0;
	virtual const char* TranslateScriptError(CScriptError& cse)	=0;
	virtual char** AllocTempString(long l)						=0;

	virtual CScriptValue LookupObjectMember(CScriptObject *obj, void *, char *szIdent) = 0;
};

#define EXT_SCRIPT_ERROR(x)	(isi->ScriptError((CScriptError::x)))

#endif

#ifndef f_SYLIA_SCRIPTVALUE_H
#define f_SYLIA_SCRIPTVALUE_H

struct CScriptObject;
class CScriptValue;

typedef CScriptValue (*ScriptFunctionPtr)(IScriptInterpreter *, void *, CScriptValue *, int);
typedef void (*ScriptVoidFunctionPtr)(IScriptInterpreter *, void *, CScriptValue *, int);
typedef int (*ScriptIntFunctionPtr)(IScriptInterpreter *, void *, CScriptValue *, int);

typedef struct ScriptFunctionDef {
	ScriptFunctionPtr func_ptr;
	char *name;
	char *arg_list;
} ScriptFunctionDef;

typedef struct ScriptObjectDef {
	char *name;
	CScriptObject *obj;
} ScriptObjectDef;

typedef struct CScriptObject {
	void *_lookup;
	ScriptFunctionDef		*func_list;
	ScriptObjectDef			*obj_list;
} CScriptObject;

class CScriptValue {
public:
	enum { T_VOID, T_INT, T_PINT, T_STR, T_ARRAY, T_OBJECT, T_FNAME, T_FUNCTION, T_VARLV } type;
	CScriptObject *thisPtr;
	union {
		int i;
		char **s;
	} u;
	void *lpVoid;

	CScriptValue()						{ type = T_VOID; }
	CScriptValue(int i)					{ type = T_INT;			u.i = i; }
	CScriptValue(char **s)				{ type = T_STR;			u.s = s; }

	bool isVoid()			{ return type == T_VOID; }
	bool isInt()			{ return type == T_INT; }
	bool isString()			{ return type == T_STR; }

	int					asInt()			{ return u.i; }
	char **				asString()		{ return u.s; }
};

#endif



#endif

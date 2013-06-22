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


// This is really dumb, but necessary to support VTbls in C++.

struct FilterVTbls {
	void *pvtblVBitmap;
};

#ifdef VDEXT_MAIN
struct FilterVTbls g_vtbls;
#elif defined(VDEXT_NOTMAIN)
extern struct FilterVTbls g_vtbls;
#endif

#define INITIALIZE_VTBLS		ff->InitVTables(&g_vtbls)

#include "VBitmap.h"

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

class VFBitmap : public VBitmap {
public:
	enum {
		NEEDS_HDC		= 0x00000001L,
	};

	DWORD	dwFlags;
	HDC		hdc;
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

struct VDWaveFormat;

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

	VDWaveFormat *(__cdecl *AllocPCMWaveFormat)(unsigned sampling_rate, unsigned channels, unsigned bits, bool bFloat);
	VDWaveFormat *(__cdecl *AllocCustomWaveFormat)(unsigned extra_size);
	VDWaveFormat *(__cdecl *CopyWaveFormat)(const VDWaveFormat *);
	void (__cdecl *FreeWaveFormat)(const VDWaveFormat *);
};

///////////////////////////////////////////////////////////////////////////
//
//	Audio filter support

struct VDAudioFilterDefinition;
struct VDWaveFormat;

struct VDAudioFilterPin {
	unsigned	mGranularity;			// Block size a filter reads/writes this pin.
	unsigned	mDelay;					// Delay in samples on this input.
	unsigned	mBufferSize;			// The size, in samples, of the buffer.
	unsigned	mCurrentLevel;			// The number of samples currently in the buffer.
	sint64		mLength;				// Approximate length of this stream in us.
	const VDWaveFormat *mpFormat;
	bool		mbVBR;
	bool		mbEnded;

	uint32 (__cdecl *mpReadProc)(VDAudioFilterPin *pPin, void *dst, uint32 samples, bool bAllowFill, int format);

	// These helpers are non-virtual inlines and are compiled into filters.
	uint32 Read(void *dst, uint32 samples, bool bAllowFill, int format) {
		return mpReadProc(this, dst, samples, bAllowFill, format);
	}
};

struct VDAudioFilterContext {
	void *mpFilterData;
	VDAudioFilterPin	**mpInputs;
	VDAudioFilterPin	**mpOutputs;
	const FilterFunctions *mpServices;
	const VDAudioFilterDefinition *mpDefinition;
	uint32	mAPIVersion;
	uint32	mInputSamples;			// Number of input samples available on all pins.
	uint32	mInputGranules;			// Number of input granules available on all pins.
	uint32	mInputsEnded;			// Number of inputs that have ended.
};

// This structure is intentionally identical to WAVEFORMATEX, with one
// exception -- mExtraSize is *always* present, even for PCM.

struct VDWaveFormat {
	enum { kTagPCM = 1 };

	uint16		mTag;
	uint16		mChannels;
	uint32		mSamplingRate;
	uint32		mDataRate;
	uint16		mBlockSize;
	uint16		mSampleBits;
	uint16		mExtraSize;
};

struct VDFilterConfigEntry {
	enum Type {
		kTypeInvalid	= 0,
		kTypeU32		= 1,
		kTypeS32,
		kTypeU64,
		kTypeS64,
		kTypeDouble,
		kTypeAStr,
		kTypeWStr,
		kTypeBlock
	};

	const VDFilterConfigEntry *next;

	unsigned	idx;
	uint32		type;
	const wchar_t *name;
	const wchar_t *label;
	const wchar_t *desc;	
};

enum {
	kVFARun_OK				= 0,
	kVFARun_Finished		= 1,

	kVFAPrepare_OK			= 0,
	kVFAPrepare_BadFormat	= 1
};

enum {
	kVFARead_Native			= 0,
	kVFARead_PCM8			= 1,
	kVFARead_PCM16			= 2,
	kVFARead_PCM32F			= 3
};

typedef uint32		(__cdecl *VDAudioFilterRunProc			)(const VDAudioFilterContext *pContext);
typedef sint64		(__cdecl *VDAudioFilterSeekProc			)(const VDAudioFilterContext *pContext, sint64 microsecs);
typedef uint32		(__cdecl *VDAudioFilterPrepareProc		)(const VDAudioFilterContext *pContext);
typedef void		(__cdecl *VDAudioFilterStartProc		)(const VDAudioFilterContext *pContext);
typedef void		(__cdecl *VDAudioFilterStopProc			)(const VDAudioFilterContext *pContext);
typedef void		(__cdecl *VDAudioFilterInitProc			)(const VDAudioFilterContext *pContext);
typedef void		(__cdecl *VDAudioFilterDestroyProc		)(const VDAudioFilterContext *pContext);
typedef unsigned	(__cdecl *VDAudioFilterSerializeProc	)(const VDAudioFilterContext *pContext, void *dst, unsigned size);
typedef void		(__cdecl *VDAudioFilterDeserializeProc	)(const VDAudioFilterContext *pContext, const void *src, unsigned size);
typedef unsigned	(__cdecl *VDAudioFilterGetParamProc		)(const VDAudioFilterContext *pContext, unsigned idx, void *dst, unsigned size);
typedef void		(__cdecl *VDAudioFilterSetParamProc		)(const VDAudioFilterContext *pContext, unsigned idx, const void *src, unsigned variant_count);
typedef bool		(__cdecl *VDAudioFilterConfigProc		)(const VDAudioFilterContext *pContext, HWND hwnd);
typedef uint32		(__cdecl *VDAudioFilterReadProc			)(const VDAudioFilterContext *pContext, unsigned pin, void *dst, uint32 samples);

enum {
	kVFAF_Zero				= 0,
	kVFAF_HasConfig			= 1,				// Filter has a configuration dialog.

	kVFAF_Max				= 0xFFFFFFFF,
};

struct VDAudioFilterVtbl {
	VDAudioFilterDestroyProc			mpDestroy;
	VDAudioFilterPrepareProc			mpPrepare;
	VDAudioFilterStartProc				mpStart;
	VDAudioFilterStopProc				mpStop;
	VDAudioFilterRunProc				mpRun;
	VDAudioFilterReadProc				mpRead;
	VDAudioFilterSeekProc				mpSeek;
	VDAudioFilterSerializeProc			mpSerialize;
	VDAudioFilterDeserializeProc		mpDeserialize;
	VDAudioFilterGetParamProc			mpGetParam;
	VDAudioFilterSetParamProc			mpSetParam;
	VDAudioFilterConfigProc				mpConfig;
};

struct VDAudioFilterDefinition {
	uint32							mSize;				// size of this structure in bytes
	const wchar_t					*pszName;
	const wchar_t					*pszAuthor;
	const wchar_t					*pszDescription;
	uint32							mVersion;			// (major<<24) + (minor<<16) + build.  1.4.1000 would be 0x010403E8.
	uint32							mFlags;

	uint32							mFilterDataSize;
	uint32							mInputPins;
	uint32							mOutputPins;

	const VDFilterConfigEntry		*mpConfigInfo;

	VDAudioFilterInitProc			mpInit;
	const VDAudioFilterVtbl			*mpVtbl;
};

#endif

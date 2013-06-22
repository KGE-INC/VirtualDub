//	VirtualDub - Video processing and capture application
//	System library component
//	Copyright (C) 1998-2004 Avery Lee, All Rights Reserved.
//
//	Beginning with 1.6.0, the VirtualDub system library is licensed
//	differently than the remainder of VirtualDub.  This particular file is
//	thus licensed as follows (the "zlib" license):
//
//	This software is provided 'as-is', without any express or implied
//	warranty.  In no event will the authors be held liable for any
//	damages arising from the use of this software.
//
//	Permission is granted to anyone to use this software for any purpose,
//	including commercial applications, and to alter it and redistribute it
//	freely, subject to the following restrictions:
//
//	1.	The origin of this software must not be misrepresented; you must
//		not claim that you wrote the original software. If you use this
//		software in a product, an acknowledgment in the product
//		documentation would be appreciated but is not required.
//	2.	Altered source versions must be plainly marked as such, and must
//		not be misrepresented as being the original software.
//	3.	This notice may not be removed or altered from any source
//		distribution.

#ifndef f_VD2_SYSTEM_VDTYPES_H
#define f_VD2_SYSTEM_VDTYPES_H

#include <algorithm>
#include <stdio.h>
#include <stdarg.h>
#include <new>

#ifndef NULL
#define NULL 0
#endif

///////////////////////////////////////////////////////////////////////////
//
//	types
//
///////////////////////////////////////////////////////////////////////////

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

	#ifdef _M_AMD64
		typedef sint64 sintptr;
		typedef uint64 uintptr;
	#else
		typedef sint32 sintptr;
		typedef uint32 uintptr;
	#endif
#endif

#if defined(_MSC_VER)
	#define VD64(x) x##i64
#elif defined(__GNUC__)
	#define VD64(x) x##ll
#else
	#error Please add an entry for your compiler for 64-bit constant literals.
#endif

	
#define VDAPIENTRY			__cdecl

typedef int64 VDTime;
typedef int64 VDPosition;
typedef	struct __VDGUIHandle *VDGUIHandle;

// enforce wchar_t under Visual C++

#if defined(_MSC_VER) && !defined(_WCHAR_T_DEFINED)
	#include <ctype.h>
#endif

///////////////////////////////////////////////////////////////////////////
//
//	allocation
//
///////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER) && (_MSC_VER < 1300 || (_MSC_VER == 1400 && _MSC_FULL_VER <= 14002207))
#define new_nothrow new
#else
#define new_nothrow new(std::nothrow)
#endif

///////////////////////////////////////////////////////////////////////////
//
//	STL fixes
//
///////////////////////////////////////////////////////////////////////////

#if _MSC_VER < 1300 || (_MSC_VER == 1400 && _MSC_FULL_VER <= 14002207)
	// The VC6 STL was deliberately borked to avoid conflicting with
	// Windows min/max macros.  We work around this bogosity here.  Note
	// that NOMINMAX must be defined for these to compile properly.  Also,
	// there is a bug in the VC6 compiler that sometimes causes long
	// lvalues to "promote" to int, causing ambiguous override errors.
	// To avoid this, always explicitly declare which type you are using,
	// i.e. min<int>(x,0).  None of this is a problem with VC7 or later.
	namespace std {
		template<class T>
		inline const T& min(const T& x, const T& y) {
			return _cpp_min(x, y);
		}

		template<class T>
		inline const T& max(const T& x, const T& y) {
			return _cpp_max(x, y);
		}
	};
#endif

///////////////////////////////////////////////////////////////////////////
//
//	compiler fixes
//
///////////////////////////////////////////////////////////////////////////

#if _MSC_VER < 1400 || (_MSC_VER == 1400 && _MSC_FULL_VER <= 14002207)
	// swprintf() is not Standard, as it turns out. For now we map swprintf()
	// to _swprintf() under VC8; we'll fix it later when VC8 becomes the
	// baseline compiler.
	inline int _swprintf(wchar_t *dst, const wchar_t *format, ...) {
		va_list val;

		va_start(val, format);
		int r = vswprintf(dst, format, val);
		va_end(val);

		return r;
	}
#endif

///////////////////////////////////////////////////////////////////////////
//
//	attribute support
//
///////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
	#define VDINTERFACE		__declspec(novtable)
	#define VDNORETURN		__declspec(noreturn)
	#define VDPUREFUNC
#elif defined(__GNUC__)
	#define VDINTERFACE
	#define VDNORETURN		__attribute__((noreturn))
	#define VDPUREFUNC		__attribute__((pure))
#else
	#define VDINTERFACE
	#define VDNORETURN
	#define VDPUREFUNC
#endif

///////////////////////////////////////////////////////////////////////////
//
//	debug support
//
///////////////////////////////////////////////////////////////////////////

enum VDAssertResult {
	kVDAssertBreak,
	kVDAssertContinue,
	kVDAssertIgnore
};

extern VDAssertResult VDAssert(const char *exp, const char *file, int line);
extern VDAssertResult VDAssertPtr(const char *exp, const char *file, int line);
extern void VDDebugPrint(const char *format, ...);

#if defined(_MSC_VER)
	#if _MSC_VER >= 1300
		#define VDBREAK		__debugbreak()
	#else
		#define VDBREAK		__asm { int 3 }
	#endif
#elif defined(__GNUC__)
	#define VDBREAK		__asm__ volatile ("int3" : : )
#else
	#define VDBREAK		*(volatile char *)0 = *(volatile char *)0
#endif


#ifdef _DEBUG

	namespace {
		template<int line>
		struct VDAssertHelper {
			VDAssertHelper(const char *exp, const char *file) {
				if (!sbAssertDisabled)
					switch(VDAssert(exp, file, line)) {
					case kVDAssertBreak:
						VDBREAK;
						break;
					case kVDAssertIgnore:
						sbAssertDisabled = true;
						break;
					}
			}

			static bool sbAssertDisabled;
		};

		template<int lineno>
		bool VDAssertHelper<lineno>::sbAssertDisabled;
	}

	#define VDASSERT(exp)		if (static bool active = true) if (exp); else switch(VDAssert   (#exp, __FILE__, __LINE__)) { case kVDAssertBreak: VDBREAK; break; case kVDAssertIgnore: active = false; } else
	#define VDASSERTPTR(exp) 	if (static bool active = true) if (exp); else switch(VDAssertPtr(#exp, __FILE__, __LINE__)) { case kVDAssertBreak: VDBREAK; break; case kVDAssertIgnore: active = false; } else
	#define VDVERIFY(exp)		if (exp); else if (static bool active = true) switch(VDAssert   (#exp, __FILE__, __LINE__)) { case kVDAssertBreak: VDBREAK; break; case kVDAssertIgnore: active = false; } else
	#define VDVERIFYPTR(exp) 	if (exp); else if (static bool active = true) switch(VDAssertPtr(#exp, __FILE__, __LINE__)) { case kVDAssertBreak: VDBREAK; break; case kVDAssertIgnore: active = false; } else
	#define VDASSERTCT(exp)		(void)sizeof(int[(exp)?1:-1])

	#define VDINLINEASSERT(exp)			((exp)||(VDAssertHelper<__LINE__>(#exp, __FILE__),false))
	#define VDINLINEASSERTFALSE(exp)	((exp)&&(VDAssertHelper<__LINE__>("!("#exp")", __FILE__),true))

	#define NEVER_HERE			do { if (VDAssert( "[never here]", __FILE__, __LINE__ )) VDBREAK; __assume(false); } while(false)
	#define	VDNEVERHERE			do { if (VDAssert( "[never here]", __FILE__, __LINE__ )) VDBREAK; __assume(false); } while(false)

	#define VDDEBUG				VDDebugPrint

#else

	#if defined(_MSC_VER)
		#ifndef _M_AMD64
			#define VDASSERT(exp)		__assume(!!(exp))
			#define VDASSERTPTR(exp)	__assume(!!(exp))
		#else
			#define VDASSERT(exp)		__noop(exp)
			#define VDASSERTPTR(exp)	__noop(exp)
		#endif
	#elif defined(__GNUC__)
		#define VDASSERT(exp)		__builtin_expect(0 != (exp), 1)
		#define VDASSERTPTR(exp)	__builtin_expect(0 != (exp), 1)
	#endif

	#define VDVERIFY(exp)		(exp)
	#define VDVERIFYPTR(exp)	(exp)
	#define VDASSERTCT(exp)

	#define VDINLINEASSERT(exp)	(exp)
	#define VDINLINEASSERTFALSE(exp)	(exp)

	#define NEVER_HERE			VDASSERT(false)
	#define	VDNEVERHERE			VDASSERT(false)

	extern int VDDEBUG_Helper(const char *, ...);
	#define VDDEBUG				(void)sizeof VDDEBUG_Helper

#endif

#define VDDEBUG2			VDDebugPrint

// TODO macros
//
// These produce a diagnostic during compilation that indicate a TODO for
// later:
//
//		#pragma message(__TODO__ "Fix this.)
//		#vdpragma_TODO("Fix this.")

#define vdpragma_TODO2(x)	#x
#define vdpragma_TODO1(x)	vdpragma_TODO2(x)
#define vdpragma_TODO0		__FILE__ "(" vdpragma_TODO1(__LINE__) ") : TODO: "

#ifdef _MSC_VER
#define vdpragma_TODO(x)		message(vdpragma_TODO0 x)
#else
#define vdpragma_TODO(x)
#endif

///////////////////////////////////////////////////////////////////////////
//
// Object scope macros
//
// vdobjectscope() allows you to define a construct where an object is
// constructed and live only within the controlled statement.  This is
// used for vdsynchronized (thread.h) and protected scopes below.
// It relies on a strange quirk of C++ regarding initialized objects
// in the condition of a selection statement and also horribly abuses
// the switch statement, generating rather good code in release builds.
// The catch is that the controlled object must implement a conversion to
// bool returning false and must only be initialized with one argument (C
// syntax).
//
// Unfortunately, handy as this macro is, it is also damned good at
// breaking compilers.  For a start, declaring an object with a non-
// trivial destructor in a switch() kills both VC6 and VC7 with a C1001.
// The bug is fixed in VC8 (MSC 14.00).
//
// A somewhat safer alternative is the for() statement, along the lines
// of:
//
// switch(bool v=false) case 0: default: for(object_def; !v; v=true)
//
// This avoids the conversion operator but unfortunately usually generates
// an actual loop in the output.

#if defined(_MSC_VER) && (_MSC_VER < 1400 || (_MSC_VER == 1400 && _MSC_FULL_VER <= 14002207))
#define vdobjectscope(object_def) if(object_def) VDNEVERHERE; else
#else
#define vdobjectscope(object_def) switch(object_def) case 0: default:
#endif

///////////////////////////////////////////////////////////////////////////
//
// Protected scope macros
//
// These macros allow you to define a scope which is known to the crash
// handler -- that is, if the application crashes within a protected scope
// the handler will report the scope information in the crash output.
//

class VDProtectedAutoScope;

extern __declspec(thread) VDProtectedAutoScope *volatile g_protectedScopeLink;

// The reason for this function is a bug in the Intel compiler regarding
// construction optimization -- it stores VDProtectedAutoScope::'vtable'
// in the vtable slot instead of VDProtectedAutoScope1<T>::'vtable', thus
// killing the printf()s. "volatile" doesn't work to fix the problem, but
// calling an opaque global function does.  Oh well.

#ifdef __INTEL_COMPILER
void VDProtectedAutoScopeICLWorkaround();
#endif

class IVDProtectedScopeOutput {
public:
	virtual void write(const char *s) = 0;
	virtual void writef(const char *s, ...) = 0;
};

class VDProtectedAutoScope {
public:
	VDProtectedAutoScope(const char *file, int line, const char *action) : mpFile(file), mLine(line), mpAction(action), mpLink(g_protectedScopeLink) {
		// Note that the assignment to g_protectedScopeLink cannot occur here, as the
		// derived class has not been constructed yet.  Uninitialized objects in
		// the debugging chain are *bad*.
	}

	~VDProtectedAutoScope() {
		g_protectedScopeLink = mpLink;
	}

	operator bool() const { return false; }

	virtual void Write(IVDProtectedScopeOutput& out) {
		out.write(mpAction);
	}

	VDProtectedAutoScope *mpLink;
	const char *const mpFile;
	const int mLine;
	const char *const mpAction;
};

class VDProtectedAutoScopeData0 {
public:
	VDProtectedAutoScopeData0(const char *file, int line, const char *action) : mpFile(file), mLine(line), mpAction(action) {}
	const char *const mpFile;
	const int mLine;
	const char *const mpAction;
};

template<class T1>
class VDProtectedAutoScopeData1 {
public:
	VDProtectedAutoScopeData1(const char *file, int line, const char *action, const T1 a1) : mpFile(file), mLine(line), mpAction(action), mArg1(a1) {}
	const char *const mpFile;
	const int mLine;
	const char *const mpAction;
	const T1 mArg1;
};

template<class T1, class T2>
class VDProtectedAutoScopeData2 {
public:
	VDProtectedAutoScopeData2(const char *file, int line, const char *action, const T1 a1, const T2 a2) : mpFile(file), mLine(line), mpAction(action), mArg1(a1), mArg2(a2) {}
	const char *const mpFile;
	const int mLine;
	const char *const mpAction;
	const T1 mArg1;
	const T2 mArg2;
};

template<class T1, class T2, class T3>
class VDProtectedAutoScopeData3 {
public:
	VDProtectedAutoScopeData3(const char *file, int line, const char *action, const T1 a1, const T2 a2, const T3 a3) : mpFile(file), mLine(line), mpAction(action), mArg1(a1), mArg2(a2), mArg3(a3) {}
	const char *const mpFile;
	const int mLine;
	const char *const mpAction;
	const T1 mArg1;
	const T2 mArg2;
	const T3 mArg3;
};

template<class T1, class T2, class T3, class T4>
class VDProtectedAutoScopeData4 {
public:
	VDProtectedAutoScopeData4(const char *file, int line, const char *action, const T1 a1, const T2 a2, const T3 a3, const T4 a4) : mpFile(file), mLine(line), mpAction(action), mArg1(a1), mArg2(a2), mArg3(a3), mArg4(a4) {}
	const char *const mpFile;
	const int mLine;
	const char *const mpAction;
	const T1 mArg1;
	const T2 mArg2;
	const T3 mArg3;
	const T4 mArg4;
};

class VDProtectedAutoScope0 : public VDProtectedAutoScope {
public:
	VDProtectedAutoScope0(const VDProtectedAutoScopeData0& data) : VDProtectedAutoScope(data.mpFile, data.mLine, data.mpAction) {
		g_protectedScopeLink = this;
#ifdef __INTEL_COMPILER
		VDProtectedAutoScopeICLWorkaround();
#endif
	}
};

template<class T1>
class VDProtectedAutoScope1 : public VDProtectedAutoScope {
public:
	VDProtectedAutoScope1(const VDProtectedAutoScopeData1<T1>& data) : VDProtectedAutoScope(data.mpFile, data.mLine, data.mpAction), mArg1(data.mArg1) {
		g_protectedScopeLink = this;
#ifdef __INTEL_COMPILER
		VDProtectedAutoScopeICLWorkaround();
#endif
	}

	virtual void Write(IVDProtectedScopeOutput& out) {
		out.writef(mpAction, mArg1);
	}

	const T1 mArg1;
};

template<class T1, class T2>
class VDProtectedAutoScope2 : public VDProtectedAutoScope {
public:
	VDProtectedAutoScope2(const VDProtectedAutoScopeData2<T1,T2>& data) : VDProtectedAutoScope(data.mpFile, data.mLine, data.mpAction), mArg1(data.mArg1), mArg2(data.mArg2) {
		g_protectedScopeLink = this;
#ifdef __INTEL_COMPILER
		VDProtectedAutoScopeICLWorkaround();
#endif
	}

	virtual void Write(IVDProtectedScopeOutput& out) {
		out.writef(mpAction, mArg1, mArg2);
	}

	const T1 mArg1;
	const T2 mArg2;
};

template<class T1, class T2, class T3>
class VDProtectedAutoScope3 : public VDProtectedAutoScope {
public:
	VDProtectedAutoScope3(const VDProtectedAutoScopeData3<T1,T2,T3>& data) : VDProtectedAutoScope(data.mpFile, data.mLine, data.mpAction), mArg1(data.mArg1), mArg2(data.mArg2), mArg3(data.mArg3) {
		g_protectedScopeLink = this;
#ifdef __INTEL_COMPILER
		VDProtectedAutoScopeICLWorkaround();
#endif
	}

	virtual void Write(IVDProtectedScopeOutput& out) {
		out.writef(mpAction, mArg1, mArg2, mArg3);
	}

	const T1 mArg1;
	const T2 mArg2;
	const T3 mArg3;
};

template<class T1, class T2, class T3, class T4>
class VDProtectedAutoScope4 : public VDProtectedAutoScope {
public:
	VDProtectedAutoScope4(const VDProtectedAutoScopeData4<T1,T2,T3,T4>& data) : VDProtectedAutoScope(data.mpFile, data.mLine, data.mpAction), mArg1(data.mArg1), mArg2(data.mArg2), mArg3(data.mArg3), mArg4(data.mArg4) {
		g_protectedScopeLink = this;
#ifdef __INTEL_COMPILER
		VDProtectedAutoScopeICLWorkaround();
#endif
	}

	virtual void Write(IVDProtectedScopeOutput& out) {
		out.writef(mpAction, mArg1, mArg2, mArg3, mArg4);
	}

	const T1 mArg1;
	const T2 mArg2;
	const T3 mArg3;
	const T4 mArg4;
};


#define vdprotected(action) vdobjectscope(VDProtectedAutoScope0 autoscope = VDProtectedAutoScopeData0(__FILE__, __LINE__, action))
#define vdprotected1(actionf, type1, arg1) vdobjectscope(VDProtectedAutoScope1<type1> autoscope = VDProtectedAutoScopeData1<type1>(__FILE__, __LINE__, actionf, arg1))

// @&#(* preprocessor doesn't view template brackets as escaping commas, so we have a slight
// problem....

#ifdef _MSC_VER
#define vdprotected2(actionf, type1, arg1, type2, arg2) if(VDProtectedAutoScope2<type1, type2> autoscope = VDProtectedAutoScopeData2<type1, type2>(__FILE__, __LINE__, actionf, arg1, arg2)) VDNEVERHERE; else
#define vdprotected3(actionf, type1, arg1, type2, arg2, type3, arg3) if(VDProtectedAutoScope3<type1, type2, type3> autoscope = VDProtectedAutoScopeData3<type1, type2, type3>(__FILE__, __LINE__, actionf, arg1, arg2, arg3)) VDNEVERHERE; else
#define vdprotected4(actionf, type1, arg1, type2, arg2, type3, arg3, type4, arg4) if(VDProtectedAutoScope4<type1, type2, type3, type4> autoscope = VDProtectedAutoScopeData4<type1, type2, type3, type4>(__FILE__, __LINE__, actionf, arg1, arg2, arg3, arg4)) VDNEVERHERE; else
#else
#define vdprotected2(actionf, type1, arg1, type2, arg2) switch(VDProtectedAutoScope2<type1, type2> autoscope = VDProtectedAutoScopeData2<type1, type2>(__FILE__, __LINE__, actionf, arg1, arg2)) case 0: default:
#define vdprotected3(actionf, type1, arg1, type2, arg2, type3, arg3) switch(VDProtectedAutoScope3<type1, type2, type3> autoscope = VDProtectedAutoScopeData3<type1, type2, type3>(__FILE__, __LINE__, actionf, arg1, arg2, arg3)) case 0: default:
#define vdprotected4(actionf, type1, arg1, type2, arg2, type3, arg3, type4, arg4) switch(VDProtectedAutoScope4<type1, type2, type3, type4> autoscope = VDProtectedAutoScopeData4<type1, type2, type3, type4>(__FILE__, __LINE__, actionf, arg1, arg2, arg3, arg4)) case 0: default:
#endif

#endif

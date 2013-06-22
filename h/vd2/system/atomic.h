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

#ifndef f_VD2_SYSTEM_ATOMIC_H
#define f_VD2_SYSTEM_ATOMIC_H


// Intrinsics available in VC6.0
extern "C" long __cdecl _InterlockedDecrement(volatile long *p);
extern "C" long __cdecl _InterlockedIncrement(volatile long *p);
extern "C" long __cdecl _InterlockedCompareExchange(volatile long *p, long n, long p_compare);
extern "C" long __cdecl _InterlockedExchange(volatile long *p, long n);
extern "C" long __cdecl _InterlockedExchangeAdd(volatile long *p, long n);

#pragma intrinsic(_InterlockedDecrement)
#pragma intrinsic(_InterlockedIncrement)
#pragma intrinsic(_InterlockedCompareExchange)
#pragma intrinsic(_InterlockedExchange)
#pragma intrinsic(_InterlockedExchangeAdd)

// Intrinsics available in VC7.1. Note that the compiler is smart enough to
// use straight LOCK AND/OR/XOR if the return value is not needed; otherwise
// it uses a LOCK CMPXCHG loop.
#if _MSC_VER >= 1310
	extern "C" long __cdecl _InterlockedAnd(volatile long *p, long n);
	extern "C" long __cdecl _InterlockedOr(volatile long *p, long n);
	extern "C" long __cdecl _InterlockedXor(volatile long *p, long n);

	#pragma intrinsic(_InterlockedAnd)
	#pragma intrinsic(_InterlockedOr)
	#pragma intrinsic(_InterlockedXor)
#endif

// Intrinsics available with AMD64
#ifdef _M_AMD64
	extern "C" void *__cdecl _InterlockedExchangePointer(void *volatile *pp, void *p);
	#pragma intrinsic(_InterlockedExchangePointer)
#endif

class VDAtomicInt {
protected:
	volatile int n;

public:
	VDAtomicInt() {}
	VDAtomicInt(int v) : n(v) {}

	bool operator!() const { return !n; }
	bool operator!=(volatile int v) const  { return n!=v; }
	bool operator==(volatile int v) const { return n==v; }
	bool operator<=(volatile int v) const { return n<=v; }
	bool operator>=(volatile int v) const { return n>=v; }
	bool operator<(volatile int v) const { return n<v; }
	bool operator>(volatile int v) const { return n>v; }

	///////////////////////////////

	static inline int staticExchange(volatile int *dst, int v) {
		return (int)_InterlockedExchange((volatile long *)dst, v);
	}

	static inline void staticIncrement(volatile int *dst) {
		_InterlockedExchangeAdd((volatile long *)dst, 1);
	}

	static inline void staticDecrement(volatile int *dst) {
		_InterlockedExchangeAdd((volatile long *)dst, -1);
	}

	static inline bool staticDecrementTestZero(volatile int *dst) {
		return 1 == _InterlockedExchangeAdd((volatile long *)dst, -1);
	}

	static inline int staticAdd(volatile int *dst, int v) {
		return (int)_InterlockedExchangeAdd((volatile long *)dst, v) + v;
	}

	static inline int staticExchangeAdd(volatile int *dst, int v) {
		return _InterlockedExchangeAdd((volatile long *)dst, v);
	}

	static inline int staticCompareExchange(volatile int *dst, int v, int compare) {
		return _InterlockedCompareExchange((volatile long *)dst, v, compare);
	}

	///////////////////////////////

	int operator=(int v) { return n = v; }

	int operator++()		{ return staticAdd(&n, 1); }
	int operator--()		{ return staticAdd(&n, -1); }
	int operator++(int)		{ return staticExchangeAdd(&n, 1); }
	int operator--(int)		{ return staticExchangeAdd(&n, -1); }
	int operator+=(int v)	{ return staticAdd(&n, v); }
	int operator-=(int v)	{ return staticAdd(&n, -v); }

#if _MSC_VER >= 1310
	void operator&=(int v)	{ _InterlockedAnd((volatile long *)&n, v); }
	void operator|=(int v)	{ _InterlockedOr((volatile long *)&n, v); }
	void operator^=(int v)	{ _InterlockedXor((volatile long *)&n, v); }
#else
	void operator&=(int v) {
		__asm mov eax,v
		__asm mov ecx,this
		__asm lock and dword ptr [ecx],eax
	}

	void operator|=(int v) {
		__asm mov eax,v
		__asm mov ecx,this
		__asm lock or dword ptr [ecx],eax
	}

	void operator^=(int v) {
		__asm mov eax,v
		__asm mov ecx,this
		__asm lock xor dword ptr [ecx],eax
	}
#endif

	operator int() const {
		return n;
	}

	int xchg(int v) {
		return staticExchange(&n, v);
	}

	// 486 only, but much nicer.  They return the actual result.

	int inc()			{ return operator++(); }
	int dec()			{ return operator--(); }
	int add(int v)		{ return operator+=(v); }

	// These return the result before the operation, which is more inline with
	// what XADD allows us to do.

	int postinc()		{ return operator++(0); }
	int postdec()		{ return operator--(0); }
	int postadd(int v)	{ return staticExchangeAdd(&n, v); }

};

template<typename T>
class VDAtomicPtr {
protected:
	T *volatile ptr;

public:
	VDAtomicPtr() {}
	VDAtomicPtr(T *p) : ptr(p) { }

	operator T*() const { return ptr; }
	T* operator->() const { return ptr; }

	T* operator=(T* p) {
		return ptr = p;
	}

	T *xchg(T* p) {
#ifdef _M_AMD64
		return ptr == p ? p : (T *)_InterlockedExchangePointer((void *volatile *)&ptr, p);
#else
		return ptr == p ? p : (T *)_InterlockedExchange((volatile long *)&ptr, (long)p);
#endif
	}
};

#endif

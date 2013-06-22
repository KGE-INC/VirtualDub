#ifndef f_VD2_SYSTEM_ATOMIC_H
#define f_VD2_SYSTEM_ATOMIC_H

#pragma warning(push)
#pragma warning(disable: 4035)

class VDAtomicInt {
protected:
	volatile int n;

public:
	VDAtomicInt() {}
	VDAtomicInt(int v) : n(v) {}

	bool operator!() const { return !n; }
	bool operator!=(int v) const { return n!=v; }
	bool operator==(int v) const { return n==v; }
	bool operator<=(int v) const { return n<=v; }
	bool operator>=(int v) const { return n>=v; }
	bool operator<(int v) const { return n<v; }
	bool operator>(int v) const { return n>v; }

	///////////////////////////////

	static inline int staticExchange(volatile int *dst, int v) {
		__asm {
			mov eax,v
			mov edx,dst
			lock xchg [dst],eax
			mov v,eax
		}
		return v;
	}

	static inline void staticIncrement(volatile int *dst) {
		__asm {
			mov eax,dst
			lock inc dword ptr [eax]
		}
	}

	static inline void staticDecrement(volatile int *dst) {
		__asm {
			mov eax,dst
			lock dec dword ptr [eax]
		}
	}

	static inline bool staticDecrementTestZero(volatile int *dst) {
		bool b;

		__asm {
			mov eax,dst
			lock dec dword ptr [eax]
			setz al
			mov b,al
		}
	}

	static inline void staticAdd(volatile int *dst, int v) {
		__asm {
			mov eax,v
			mov	edx,dst
			lock add [edx],eax
		}
	}

	static inline int staticAddResult(volatile int *dst, int v) {
		__asm {
			mov eax,v
			mov	edx,dst
			lock xadd [edx],eax
			add	v,eax
		}
		return v;
	}

	///////////////////////////////

	int operator=(int v) { return n = v; }

	void operator++() {
		__asm mov ecx,this
		__asm lock inc dword ptr [ecx]
	}

	void operator--() {
		__asm mov ecx,this
		__asm lock dec dword ptr [ecx]
	}

	void operator++(int) { ++*this; }
	void operator--(int) { --*this; }

	void operator+=(int v) {
		__asm mov eax,v
		__asm mov ecx,this
		__asm lock add dword ptr [ecx],eax
	}

	void operator-=(int v) {
		__asm mov eax,v
		__asm mov ecx,this
		__asm lock sub dword ptr [ecx],eax
	}

	operator int() const {
		return n;
	}

	int xchg(int v) {
		__asm mov eax,v
		__asm mov ecx,this
		__asm xchg dword ptr [ecx], eax
	}

	bool inctestzero() {
		__asm mov ecx,this
		__asm lock inc dword ptr [ecx]
		__asm setz al
	}

	bool dectestzero() {
		__asm mov ecx,this
		__asm lock dec dword ptr [ecx]
		__asm setz al
	}

	bool addcarry(int v) {
		__asm mov eax,v
		__asm mov ecx,this
		__asm lock add dword ptr [ecx], eax
		__asm sbb eax,eax
		__asm and eax,1
	}

	bool subcarry(int v) {
		__asm mov eax,v
		__asm mov ecx,this
		__asm lock sub dword ptr [ecx], eax
		__asm sbb eax,eax
		__asm and eax,1
	}

	// 486 only, but much nicer.  They return the actual result.

	int inc() {
		__asm mov eax,1
		__asm mov ecx,this
		__asm lock xadd dword ptr [ecx],eax
		__asm inc eax
	}

	int dec() {
		__asm mov eax,-1
		__asm mov ecx,this
		__asm lock xadd dword ptr [ecx],eax
		__asm dec eax
	}

	int add(int v) {
		__asm mov eax,v
		__asm mov ecx,this
		__asm lock xadd dword ptr [ecx],eax
		__asm add eax,v
	}

	// These return the result before the operation, which is more inline with
	// what XADD allows us to do.

	int postinc() {
		__asm mov eax,1
		__asm mov ecx,this
		__asm lock xadd dword ptr [ecx],eax
	}

	int postdec() {
		__asm mov eax,-1
		__asm mov ecx,this
		__asm lock xadd dword ptr [ecx],eax
	}

	int postadd(int v) {
		__asm mov eax,v
		__asm mov ecx,this
		__asm lock xadd dword ptr [ecx],eax
	}

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

	T *operator()(T* p) {
		__asm mov eax,p
		__asm mov ecx,this
		__asm xchg [ecx],eax
		__asm mov p,eax

		return p;
	}
};

#pragma warning(pop)
#endif

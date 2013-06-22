#ifndef f_VD2_SYSTEM_ATOMIC_H
#define f_VD2_SYSTEM_ATOMIC_H


#ifdef _M_AMD64		// amd64

	extern "C" long __cdecl _InterlockedAnd(volatile long *p, long n);
	extern "C" long __cdecl _InterlockedOr(volatile long *p, long n);
	extern "C" long __cdecl _InterlockedXor(volatile long *p, long n);
	extern "C" long __cdecl _InterlockedExchange(volatile long *p, long n);
	extern "C" long __cdecl _InterlockedExchangeAdd(volatile long *p, long n);
	extern "C" void *__cdecl _InterlockedExchangePointer(void *volatile *pp, void *p);

	#pragma intrinsic(_InterlockedAnd)
	#pragma intrinsic(_InterlockedOr)
	#pragma intrinsic(_InterlockedXor)
	#pragma intrinsic(_InterlockedExchange)
	#pragma intrinsic(_InterlockedExchangeAdd)
	#pragma intrinsic(_InterlockedExchangePointer)

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
			return !_InterlockedExchangeAdd((volatile long *)dst, -1);
		}

		static inline void staticAdd(volatile int *dst, int v) {
			_InterlockedExchangeAdd((volatile long *)dst, v);
		}

		static inline int staticAddResult(volatile int *dst, int v) {
			return (int)_InterlockedExchangeAdd((volatile long *)dst, v) + v;
		}

		///////////////////////////////

		int operator=(int v) { return n = v; }

		void operator++() {
			staticIncrement(&n);
		}

		void operator--() {
			staticDecrement(&n);
		}

		void operator++(int) { ++*this; }
		void operator--(int) { --*this; }

		void operator+=(int v) {
			staticAdd(&n, v);
		}

		void operator-=(int v) {
			staticAdd(&n, -v);
		}

		void operator&=(int v) {
			_InterlockedAnd((volatile long *)&n, v);
		}

		void operator|=(int v) {
			_InterlockedOr((volatile long *)&n, v);
		}

		void operator^=(int v) {
			_InterlockedXor((volatile long *)&n, v);
		}

		operator int() const {
			return n;
		}

		int xchg(int v) {
			return staticExchange(&n, v);
		}

		bool inctestzero() {
			return !inc();
		}

		bool dectestzero() {
			return !dec();
		}

		bool addcarry(int v) {
			return (unsigned)add(v) < (unsigned)v;
		}

		bool subcarry(int v) {
			return (unsigned)add(-v) >= (unsigned)-v;
		}

		// 486 only, but much nicer.  They return the actual result.

		int inc() {
			return staticAddResult(&n, 1);
		}

		int dec() {
			return staticAddResult(&n, -1);
		}

		int add(int v) {
			return staticAddResult(&n, v);
		}

		// These return the result before the operation, which is more inline with
		// what XADD allows us to do.

		int postinc() {
			return staticAddResult(&n, 1) - 1;
		}

		int postdec() {
			return staticAddResult(&n, 1) + 1;
		}

		int postadd(int v) {
			return staticAddResult(&n, 1) - v;
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

		T *xchg(T* p) {
			return (T *)_InterlockedExchangePointer((void *volatile *)&ptr, p);
		}
	};

#else				// i386

	#pragma warning(push)
	#pragma warning(disable: 4035)

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
			__asm {
				mov eax,dst
				lock dec dword ptr [eax]
				setz al
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

		T *xchg(T* p) {
			__asm mov eax,p
			__asm mov ecx,this
			__asm xchg [ecx],eax
			__asm mov p,eax

			return p;
		}
	};

	#pragma warning(pop)
#endif

#endif

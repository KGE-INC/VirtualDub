#ifndef f_VD2_SYSTEM_VDALLOC_H
#define f_VD2_SYSTEM_VDALLOC_H

#include <stdlib.h>

// Why don't I use STL auto_ptr?  Two reasons.  First, auto_ptr has
// the overhead of an ownership flag, and second, auto_ptr can't
// be used with malloc() blocks.  So think of these as auto_ptr
// objects, but not quite....

#pragma warning(push)
#pragma warning(disable: 4284)		// operator-> must return pointer to UDT

class vdautoblockptr {
protected:
	void *ptr;

public:
	explicit vdautoblockptr(void *p = 0) : ptr(p) {}
	~vdautoblockptr() { free(ptr); }

	vdautoblockptr& operator=(void *src) { free(ptr); ptr = src; return *this; }

	operator void*() const { return ptr; }

	vdautoblockptr& from(vdautoblockptr& src) { free(ptr); ptr=src.ptr; src.ptr=0; }
	void *get() const { return ptr; }
	void *release() { void *v = ptr; ptr = NULL; return v; }
};

template<class T> class vdautoptr2 {
protected:
	T *ptr;

public:
	explicit vdautoptr2(T *p = 0) : ptr(p) {}
	~vdautoptr2() { free((void *)ptr); }

	vdautoptr2<T>& operator=(T *src) { free((void *)ptr); ptr = src; return *this; }

	operator T*() const { return ptr; }
	T& operator*() const { return *ptr; }
	T *operator->() const { return ptr; }

	vdautoptr2<T>& from(vdautoptr2<T>& src) { free((void *)ptr); ptr=src.ptr; src.ptr=0; }
	T *get() const { return ptr; }
	T *release() { T *v = ptr; ptr = NULL; return v; }
};

template<class T> class vdautoptr {
protected:
	T *ptr;

public:
	explicit vdautoptr(T *p = 0) : ptr(p) {}
	~vdautoptr() { delete ptr; }

	vdautoptr<T>& operator=(T *src) { delete ptr; ptr = src; return *this; }

	operator T*() const { return ptr; }
	T& operator*() const { return *ptr; }
	T *operator->() const { return ptr; }

	vdautoptr<T>& from(vdautoptr<T>& src) { delete ptr; ptr=src.ptr; src.ptr=0; }
	T *get() const { return ptr; }
	T *release() { T *v = ptr; ptr = NULL; return v; }
};

template<class T> class vdautoarrayptr {
protected:
	T *ptr;

public:
	explicit vdautoarrayptr(T *p = 0) : ptr(p) {}
	~vdautoarrayptr() { delete[] ptr; }

	vdautoarrayptr<T>& operator=(T *src) { delete[] ptr; ptr = src; return *this; }

	T& operator[](int offset) const { return ptr[offset]; }

	vdautoarrayptr<T>& from(vdautoarrayptr<T>& src) { delete[] ptr; ptr=src.ptr; src.ptr=0; }
	T *get() const { return ptr; }
	T *release() { T *v = ptr; ptr = NULL; return v; }
};

#pragma warning(pop)

#endif

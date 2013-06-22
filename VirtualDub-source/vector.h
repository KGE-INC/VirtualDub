#ifndef f_VD_VECTOR_H
#define f_VD_VECTOR_H

#include <stdlib.h>

#include "Error.h"

// Okay, so this is a poor man's STL vector.  I'd rather not drag in the STL
// vector until 2.0.  Besides, this vector has simpler semantics and throws
// my kind of errors... and I need the practice anyway.

template<class T>
class Vector {
private:
	// prohibit these functions
	Vector(const Vector&);
	const Vector& operator=(const Vector&);

public:
	Vector() : mBuf(0), mSize(0), mAllocated(0) {
	}

	~Vector() {
		free(mBuf);
	}

	const T& operator[](int x) const { return mBuf[x]; }
	T& operator[](int x) { return mBuf[x]; }

	void push_back(const T& v) {
		if (mSize >= mAllocated)
			reserve(mAllocated*2);

		mBuf[mSize++] = v;
	}

	void clear() {
		mSize = 0;
	}

	int size() const {
		return mSize;
	}

	void resize(int s) {
		reserve(s);
		mSize = s;
	}

	void reserve(int s) {
		if (s < mAllocated)
			return;

		if (!s)
			s = 1;

		s = (s+63) & ~63;

		T *newBuf = (T *)realloc(mBuf, sizeof(T) * s);

		if (!newBuf)
			throw MyMemoryError();

		mBuf = newBuf;
		mAllocated = s;
	}

private:
	T		*mBuf;
	int		mSize;
	int		mAllocated;
};

#endif

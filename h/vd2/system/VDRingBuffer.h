#ifndef f_SYSTEM_VDRINGBUFFER_H
#define f_SYSTEM_VDRINGBUFFER_H

#include <string.h>

#include <vd2/system/VDAtomic.h>

template<class T>
class VDRingBuffer {
protected:
	T				*pBuffer;
	int				 nSize;
	int				 nReadPoint;
	int				 nWritePoint;
	VDAtomicInt		 nLevel;

public:
	VDRingBuffer();
	VDRingBuffer(int size);
	~VDRingBuffer();

	void	 Init(int size);
	void	 Shutdown();

	int		 getSize() const { return nSize; }
	int		 getLevel() const { return nLevel; }
	int		 getSpace() const { return nSize - nLevel; }
	int		 getWriteSpace() const;
	T *		 getWritePtr() const { return pBuffer+nWritePoint; }

	bool	 empty() const { return !nLevel; }
	bool	 full() const { return nLevel == nSize; }

	void	 Flush() { nReadPoint = nWritePoint = nLevel = 0; }

	int		 Read(T *pBuffer, int bytes);
	const T	*LockRead(int requested, int& actual);
	const T *LockReadWrapped(int requested, int& actual, int& nReadPoint);
	int		 UnlockRead(int actual);

	int		 Write(const T *pData, int bytes);
	T		*LockWrite(int requested, int& actual);
	int		 UnlockWrite(int actual);
};

template<class T>
VDRingBuffer<T>::VDRingBuffer(int size)
: pBuffer(0)
{
	Init(size);
}

template<class T>
VDRingBuffer<T>::VDRingBuffer()
: nSize(0)
, nLevel(0)
, nReadPoint(0)
, nWritePoint(0)
, pBuffer(0)
{
}

template<class T>
VDRingBuffer<T>::~VDRingBuffer() {
	Shutdown();
}

template<class T>
void VDRingBuffer<T>::Init(int size) {
	Shutdown();
	pBuffer		= new T[nSize = size];
	nLevel		= 0;
	nReadPoint	= 0;
	nWritePoint	= 0;
}

template<class T>
void VDRingBuffer<T>::Shutdown() {
	delete[] pBuffer;
	pBuffer = NULL;
}

template<class T>
int VDRingBuffer<T>::getWriteSpace() const {
	volatile int tc = nSize - nWritePoint;
	volatile int space = nSize - nLevel;

	if (tc > space)
		tc = space;

	return tc;
}

template<class T>
int VDRingBuffer<T>::Read(T *pBuffer, int units) {
	VDASSERT(units >= 0);

	int actual = 0;
	const T *pSrc;

	while(units) {
		int tc;

		pSrc = LockRead(units, tc);

		if (!tc)
			break;

		memcpy(pBuffer, pSrc, tc * sizeof(T));

		UnlockRead(tc);

		actual += tc;
		units -= tc;
		pBuffer += tc;
	}

	return actual;
}

template<class T>
const T *VDRingBuffer<T>::LockRead(int requested, int& actual) {
	VDASSERT(requested >= 0);

	int nLevelNow = nLevel;

	if (requested > nLevelNow)
		requested = nLevelNow;

	if (requested + nReadPoint > nSize)
		requested = nSize - nReadPoint;

	actual = requested;

	return pBuffer + nReadPoint;
}

template<class T>
const T *VDRingBuffer<T>::LockReadWrapped(int requested, int& actual, int& readpt) {
	int nLevelNow = nLevel;

	if (requested > nLevelNow)
		requested = nLevelNow;

	actual = requested;
	readpt = nReadPoint;

	return pBuffer;
}

template<class T>
int VDRingBuffer<T>::UnlockRead(int actual) {
	VDASSERT(actual >= 0);
	VDASSERT(nLevel >= actual);

	int newpt = nReadPoint + actual;

	if (newpt >= nSize)
		newpt -= nSize;

	nReadPoint = newpt;

	return nLevel.add(-actual);
}

template<class T>
int VDRingBuffer<T>::Write(const T *pData, int bytes) {
	VDASSERT(bytes >= 0);

	int actual = 0;
	void *pDst;

	while(bytes) {
		int tc;

		pDst = LockWrite(bytes, tc);

		if (!actual)
			break;

		memcpy(pDst, pBuffer, tc);

		UnlockWrite(tc);

		actual += tc;
		bytes -= tc;
		pBuffer = (char *)pBuffer + tc;
	}

	return actual;
}

template<class T>
T *VDRingBuffer<T>::LockWrite(int requested, int& actual) {
	VDASSERT(requested >= 0);
	int nLevelNow = nSize - nLevel;

	if (requested > nLevelNow)
		requested = nLevelNow;

	if (requested + nWritePoint > nSize)
		requested = nSize - nWritePoint;

	actual = requested;

	return pBuffer + nWritePoint;
}

template<class T>
int VDRingBuffer<T>::UnlockWrite(int actual) {
	VDASSERT(actual >= 0);
	VDASSERT(nLevel + actual <= nSize);

	int newpt = nWritePoint + actual;

	if (newpt >= nSize)
		newpt = 0;

	nWritePoint = newpt;

	return nLevel.add(actual);
}



#endif

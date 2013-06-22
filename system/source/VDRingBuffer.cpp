#include <system/VDRingBuffer.h>

template<class T>
VDRingBuffer<T>::VDRingBuffer(int size)
: nSize(size)
, nLevel(0)
, nReadPoint(0)
, nWritePoint(0)
, pBuffer(new T[size])
{
}

template<class T>
VDRingBuffer<T>::~VDRingBuffer() {
	delete[] pBuffer;
}

template<class T>
int VDRingBuffer<T>::Read(T *pBuffer, int units) throw() {
	int actual = 0;
	void *pSrc;

	while(units) {
		int tc;

		pSrc = LockRead(units, tc);

		if (!actual)
			break;

		memcpy(pBuffer, pSrc, tc);

		UnlockRead(tc);

		actual += tc;
		units -= tc;
		pBuffer += tc;
	}

	return actual;
}

template<class T>
const T *VDRingBuffer<T>::LockRead(int requested, int& actual) throw() {
	int nLevelNow = nLevel;

	if (requested > nLevelNow)
		requested = nLevelNow;

	if (requested + nReadPoint > nSize)
		requested = nSize - nReadPoint;

	actual = requested;

	return pBuffer + nReadPoint;
}

template<class T>
void VDRingBuffer<T>::UnlockRead(int actual) throw() {
	int newpt = nReadPoint + actual;

	if (newpt >= nSize)
		newpt = 0;

	nReadPoint = newpt;

	__asm mov eax, actual
	__asm lock sub [ecx]VDRingBuffer<T>.nLevel, eax
}

template<class T>
int VDRingBuffer<T>::Write(const T *pData, int bytes) throw() {
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
T *VDRingBuffer<T>::LockWrite(int requested, int& actual) throw() {
	int nLevelNow = nLevel;

	if (requested > nLevelNow)
		requested = nLevelNow;

	if (requested + nWritePoint > nSize)
		requested = nSize - nWritePoint;

	actual = requested;

	return pBuffer + nWritePoint;
}

template<class T>
void VDRingBuffer<T>::UnlockWrite(int actual) throw() {
	int newpt = nWritePoint + actual;

	if (newpt >= nSize)
		newpt = 0;

	nWritePoint = newpt;

	__asm mov eax, actual
	__asm lock add [ecx]VDRingBuffer<T>.nLevel, eax
}


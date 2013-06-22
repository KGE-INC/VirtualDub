#include <windows.h>
#include <math.h>
#include "af_base.h"


#define VDASSERT(x) ((void)0)



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

	T *operator()(T* p) {
		__asm mov eax,p
		__asm mov ecx,this
		__asm xchg [ecx],eax
		__asm mov p,eax

		return p;
	}
};

#pragma warning(pop)








template<class T>
class VDAudioRingBuffer {
protected:
	T				*pBuffer;
	int				 nSize;
	int				 nReadPoint;
	int				 nWritePoint;
	int				 nObjectSize;
	VDAtomicInt		 nLevel;

public:
	VDAudioRingBuffer();
	VDAudioRingBuffer(int size, int objsize);
	~VDAudioRingBuffer();

	void	 Init(int size, int objsize);
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
	T		*LockWrite(int& actual);
	T		*LockWrite(int requested, int& actual);
	int		 UnlockWrite(int actual);
};

template<class T>
VDAudioRingBuffer<T>::VDAudioRingBuffer(int size, int objsize)
: pBuffer(0)
{
	Init(size, objsize);
}

template<class T>
VDAudioRingBuffer<T>::VDAudioRingBuffer()
: nSize(0)
, nLevel(0)
, nReadPoint(0)
, nWritePoint(0)
, pBuffer(0)
{
}

template<class T>
VDAudioRingBuffer<T>::~VDAudioRingBuffer() {
	Shutdown();
}

template<class T>
void VDAudioRingBuffer<T>::Init(int size, int objsize) {
	Shutdown();
	nSize		= size;
	pBuffer		= new T[size * objsize];
	nLevel		= 0;
	nReadPoint	= 0;
	nWritePoint	= 0;
	nObjectSize	= objsize;
}

template<class T>
void VDAudioRingBuffer<T>::Shutdown() {
	delete[] pBuffer;
	pBuffer = NULL;
}

template<class T>
int VDAudioRingBuffer<T>::getWriteSpace() const {
	volatile int tc = nSize - nWritePoint;
	volatile int space = nSize - nLevel;

	if (tc > space)
		tc = space;

	return tc;
}

template<class T>
int VDAudioRingBuffer<T>::Read(T *pBuffer, int units) {
	VDASSERT(units >= 0);

	int actual = 0;
	const T *pSrc;

	while(units) {
		int tc;

		pSrc = LockRead(units, tc);

		if (!tc)
			break;

		memcpy(pBuffer, pSrc, tc * sizeof(T) * nObjectSize);

		UnlockRead(tc);

		actual += tc;
		units -= tc;
		pBuffer += tc;
	}

	return actual;
}

template<class T>
const T *VDAudioRingBuffer<T>::LockRead(int requested, int& actual) {
	VDASSERT(requested >= 0);

	int nLevelNow = nLevel;

	if (requested > nLevelNow)
		requested = nLevelNow;

	if (requested + nReadPoint > nSize)
		requested = nSize - nReadPoint;

	actual = requested;

	return pBuffer + nReadPoint * nObjectSize;
}

template<class T>
const T *VDAudioRingBuffer<T>::LockReadWrapped(int requested, int& actual, int& readpt) {
	int nLevelNow = nLevel;

	if (requested > nLevelNow)
		requested = nLevelNow;

	actual = requested;
	readpt = nReadPoint;

	return pBuffer;
}

template<class T>
int VDAudioRingBuffer<T>::UnlockRead(int actual) {
	VDASSERT(actual >= 0);
	VDASSERT(nLevel >= actual);

	int newpt = nReadPoint + actual;

	if (newpt >= nSize)
		newpt -= nSize;

	nReadPoint = newpt;

	return nLevel.add(-actual);
}

template<class T>
int VDAudioRingBuffer<T>::Write(const T *pData, int units) {
	VDASSERT(units >= 0);

	int actual = 0;
	void *pDst;

	while(units) {
		int tc;

		pDst = LockWrite(units, tc);

		if (!actual)
			break;

		memcpy(pDst, pBuffer, tc * nObjSize);

		UnlockWrite(tc);

		actual += tc;
		units -= tc;
		pBuffer = (char *)pBuffer + tc;
	}

	return actual;
}

template<class T>
T *VDAudioRingBuffer<T>::LockWrite(int& actual) {
	int avail = nSize - nLevel;

	if (avail + nWritePoint > nSize)
		avail = nSize - nWritePoint;

	actual = avail;

	return pBuffer + nWritePoint * nObjectSize;
}

template<class T>
T *VDAudioRingBuffer<T>::LockWrite(int requested, int& actual) {
	VDASSERT(requested >= 0);
	int nLevelNow = nSize - nLevel;

	if (requested > nLevelNow)
		requested = nLevelNow;

	if (requested + nWritePoint > nSize)
		requested = nSize - nWritePoint;

	actual = requested;

	return pBuffer + nWritePoint * nObjectSize;
}

template<class T>
int VDAudioRingBuffer<T>::UnlockWrite(int actual) {
	VDASSERT(actual >= 0);
	VDASSERT(nLevel + actual <= nSize);

	int newpt = nWritePoint + actual;

	if (newpt >= nSize)
		newpt = 0;

	nWritePoint = newpt;

	return nLevel.add(actual);
}














class VDAudioFilterToneGenerator : public VDAudioFilterBase {
public:
	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	uint32 Prepare();
	uint32 Run();
	void Start();
	uint32 Read(unsigned pin, void *dst, uint32 samples);
	sint64 Seek(sint64);

	VDAudioRingBuffer<char> mOutputBuffer;

	unsigned		mPos;
	unsigned		mPad;
	unsigned		mLimit;
};

void __cdecl VDAudioFilterToneGenerator::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterToneGenerator;
}

uint32 VDAudioFilterToneGenerator::Prepare() {
	VDWaveFormat *pwf = mpContext->mpAudioCallbacks->AllocPCMWaveFormat(44100, 1, 16, false);

	if (!pwf)
		mpContext->mpServices->ExceptOutOfMemory();

	mpContext->mpOutputs[0]->mpFormat		= pwf;

	return 0;
}

void VDAudioFilterToneGenerator::Start() {
	VDAudioFilterPin& pin = *mpContext->mpOutputs[0];
	const VDWaveFormat& format = *pin.mpFormat;

	mOutputBuffer.Init(pin.mBufferSize, format.mBlockSize);

	mPos	= 0;
	mLimit	= 44100 * 10;
	mPad	= 0;

	pin.mLength = 10000000;
}

uint32 VDAudioFilterToneGenerator::Run() {
	int count;
	sint16 *dst = (sint16 *)mOutputBuffer.LockWrite(count);

	if (mPos + count > mLimit)
		count = mLimit - mPos;

	for(int i=0; i<count; ++i)
		dst[i] = 32767.0 * sin((i+mPos) * (3.1415926535897932 / 22100 * 440));

	mOutputBuffer.UnlockWrite(count);
	mpContext->mpOutputs[0]->mCurrentLevel = mOutputBuffer.getLevel();

	mPos += count;

	return mPos >= mLimit ? kVFARun_Finished : 0;
}

uint32 VDAudioFilterToneGenerator::Read(unsigned pinid, void *dst, uint32 samples) {
	VDAudioFilterPin& pin = *mpContext->mpOutputs[0];

	uint32 availspace = mOutputBuffer.getLevel();

	if (samples > availspace)
		samples = availspace;

	if (dst) {
		mOutputBuffer.Read((char *)dst, samples);
		pin.mCurrentLevel = mOutputBuffer.getLevel();
	}

	return samples;
}

sint64 VDAudioFilterToneGenerator::Seek(sint64 us) {
	mOutputBuffer.Flush();
	mpContext->mpOutputs[0]->mCurrentLevel = 0;

	mPos = 0;
	mPad = 0;

	return us;
}

extern const struct VDAudioFilterDefinition afilterDef_tonegenerator = {
	sizeof(VDAudioFilterDefinition),
	kVFAF_Zero,

	sizeof(VDAudioFilterToneGenerator),	0,	1,

	NULL,

	VDAudioFilterToneGenerator::InitProc,
	&VDAudioFilterBase::sVtbl,
};

extern const struct VDPluginInfo apluginDef_tonegenerator = {
	sizeof(VDPluginInfo),
	L"tone generator",
	NULL,
	L"Produces a constant 440Hz tone.",
	0,
	kVDPluginType_Audio,
	0,

	kVDPlugin_APIVersion,
	kVDPlugin_APIVersion,
	kVDPlugin_AudioAPIVersion,
	kVDPlugin_AudioAPIVersion,

	&afilterDef_tonegenerator
};



class VDAudioFilterRingModulator : public VDAudioFilterBase {
public:
	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	uint32 Prepare();
	uint32 Run();
	void Start();
	uint32 Read(unsigned pin, void *dst, uint32 samples);
	sint64 Seek(sint64);

	VDAudioRingBuffer<char> mOutputBuffer;

	unsigned		mPos;
	unsigned		mPad;
	unsigned		mLimit;
};

void __cdecl VDAudioFilterRingModulator::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterRingModulator;
}

uint32 VDAudioFilterRingModulator::Prepare() {
	VDAudioFilterPin& pin0 = *mpContext->mpInputs[0];
	VDAudioFilterPin& pin1 = *mpContext->mpInputs[1];

	// validate pin requirements
	if (pin0.mpFormat->mTag != VDWaveFormat::kTagPCM
		|| pin1.mpFormat->mTag != VDWaveFormat::kTagPCM
		|| pin0.mpFormat->mChannels != pin1.mpFormat->mChannels
		|| pin0.mpFormat->mSamplingRate != pin1.mpFormat->mSamplingRate
	)
		return kVFAPrepare_BadFormat;

	VDWaveFormat *pwf = mpContext->mpAudioCallbacks->AllocPCMWaveFormat(pin0.mpFormat->mSamplingRate, pin0.mpFormat->mChannels, 16, false);

	if (!pwf)
		mpContext->mpServices->ExceptOutOfMemory();

	mpContext->mpOutputs[0]->mpFormat		= pwf;

	return 0;
}

void VDAudioFilterRingModulator::Start() {
	VDAudioFilterPin& pin = *mpContext->mpOutputs[0];
	const VDWaveFormat& format = *pin.mpFormat;

	mOutputBuffer.Init(pin.mBufferSize, format.mBlockSize);
}

uint32 VDAudioFilterRingModulator::Run() {
	int count;

	if (mpContext->mpInputs[0]->mbEnded && mpContext->mpInputs[1]->mbEnded)
		return kVFARun_Finished;

	sint16 *dst = (sint16 *)mOutputBuffer.LockWrite(mpContext->mInputSamples, count);

	if (count > 512 / mpContext->mpOutputs[0]->mpFormat->mChannels)
		count = 512 / mpContext->mpOutputs[0]->mpFormat->mChannels;

	sint16 buf0[512];
	sint16 buf1[512];

	mpContext->mpInputs[0]->Read(buf0, count, true, kVFARead_PCM16);
	mpContext->mpInputs[1]->Read(buf1, count, true, kVFARead_PCM16);

	int samples = count * mpContext->mpOutputs[0]->mpFormat->mChannels;
	for(int i=0; i<samples; ++i) {
		int y = (buf0[i] * buf1[i] + 0x4000) >> 15;

		// take care of annoying -1.0 * -1.0 = +1.0 case
		if (y >= 0x7fff)
			y = 0x7fff;

		dst[i] = y;
	}

	mOutputBuffer.UnlockWrite(count);
	mpContext->mpOutputs[0]->mCurrentLevel = mOutputBuffer.getLevel();

	return 0;
}

uint32 VDAudioFilterRingModulator::Read(unsigned pinid, void *dst, uint32 samples) {
	VDAudioFilterPin& pin = *mpContext->mpOutputs[0];

	uint32 availspace = mOutputBuffer.getLevel();

	if (samples > availspace)
		samples = availspace;

	if (dst) {
		mOutputBuffer.Read((char *)dst, samples);
		pin.mCurrentLevel = mOutputBuffer.getLevel();
	}

	return samples;
}

sint64 VDAudioFilterRingModulator::Seek(sint64 us) {
	mOutputBuffer.Flush();
	mpContext->mpOutputs[0]->mCurrentLevel = 0;

	mPos = 0;
	mPad = 0;

	return us;
}

extern const struct VDAudioFilterDefinition afilterDef_ringmodulator = {
	sizeof(VDAudioFilterDefinition),
	kVFAF_Zero,

	sizeof(VDAudioFilterRingModulator),	2,	1,

	NULL,

	VDAudioFilterRingModulator::InitProc,
	&VDAudioFilterBase::sVtbl,
};

extern const struct VDPluginInfo apluginDef_ringmodulator = {
	sizeof(VDPluginInfo),
	L"ring modulator",
	NULL,
	L"Modulates one audio stream using another.",
	0,
	kVDPluginType_Audio,
	0,

	kVDPlugin_APIVersion,
	kVDPlugin_APIVersion,
	kVDPlugin_AudioAPIVersion,
	kVDPlugin_AudioAPIVersion,

	&afilterDef_ringmodulator
};




extern "C" __declspec(dllexport) const VDPluginInfo *const *VDGetPluginInfo() {
	static const VDPluginInfo *const sPlugins[]={
		&apluginDef_tonegenerator,
		&apluginDef_ringmodulator,
		NULL
	};

	return sPlugins;
}

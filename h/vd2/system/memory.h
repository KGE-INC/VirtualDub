#ifndef f_VD2_SYSTEM_MEMORY_H
#define f_VD2_SYSTEM_MEMORY_H

#include <vd2/system/vdtypes.h>

void *VDAlignedMalloc(size_t n, unsigned alignment);
void VDAlignedFree(void *p);

template<unsigned alignment>
struct VDAlignedObject {
	inline void *operator new(size_t n) { return VDAlignedMalloc(n, alignment); }
	inline void operator delete(void *p) { VDAlignedFree(p); }
};

void VDSwapMemory(void *p0, void *p1, unsigned bytes);
void VDInvertMemory(void *p, unsigned bytes);

void VDMemset8(void *dst, uint8 value, size_t count);
void VDMemset16(void *dst, uint16 value, size_t count);
void VDMemset32(void *dst, uint32 value, size_t count);

void VDMemset8Rect(void *dst, ptrdiff_t pitch, uint8 value, size_t w, size_t h);

#if defined(_WIN32) && defined(_M_IX86)
	extern void (__cdecl *VDFastMemcpyPartial)(void *dst, const void *src, size_t bytes);
	extern void (__cdecl *VDFastMemcpyFinish)();
	void VDFastMemcpyAutodetect();
#else
	void VDFastMemcpyPartial(void *dst, const void *src, size_t bytes);
	void VDFastMemcpyFinish();
	void VDFastMemcpyAutodetect();
#endif


void VDMemcpyRect(void *dst, ptrdiff_t dststride, const void *src, ptrdiff_t srcstride, size_t w, size_t h);

#endif

#include <vd2/system/memory.h>
#include <vd2/system/cpuaccel.h>

void VDSwapMemory(void *p0, void *p1, unsigned bytes) {
	long *dst0 = (long *)p0;
	long *dst1 = (long *)p1;

	while(bytes >= 4) {
		long a = *dst0;
		long b = *dst1;

		*dst0++ = b;
		*dst1++ = a;

		bytes -= 4;
	}

	char *dstb0 = (char *)dst0;
	char *dstb1 = (char *)dst1;

	while(bytes--) {
		char a = *dstb0;
		char b = *dstb1;

		*dstb0++ = b;
		*dstb1++ = a;
	}
}

void VDInvertMemory(void *p, unsigned bytes) {
	char *dst = (char *)p;

	if (!bytes)
		return;

	while((int)dst & 3) {
		*dst = ~*dst;
		++dst;

		if (!--bytes)
			return;
	}

	unsigned lcount = bytes >> 2;

	if (lcount)
		do {
			*(long *)dst = ~*(long *)dst;
			dst += 4;
		} while(--lcount);

	bytes &= 3;

	while(bytes--) {
		*dst = ~*dst;
		++dst;
	}
}

void VDMemset8(void *dst, uint8 value, size_t count) {
	if (count) {
		uint8 *dst2 = (uint8 *)dst;

		do {
			*dst2++ = value;
		} while(--count);
	}
}

void VDMemset16(void *dst, uint16 value, size_t count) {
	if (count) {
		uint16 *dst2 = (uint16 *)dst;

		do {
			*dst2++ = value;
		} while(--count);
	}
}

void VDMemset32(void *dst, uint32 value, size_t count) {
	if (count) {
		uint32 *dst2 = (uint32 *)dst;

		do {
			*dst2++ = value;
		} while(--count);
	}
}

#if defined(_WIN32) && defined(_M_IX86)
	extern "C" void __cdecl VDFastMemcpyPartialScalarAligned8(void *dst, const void *src, size_t bytes);
	extern "C" void __cdecl VDFastMemcpyPartialMMX(void *dst, const void *src, size_t bytes);
	extern "C" void __cdecl VDFastMemcpyPartialMMX2(void *dst, const void *src, size_t bytes);

	void VDFastMemcpyPartialScalar(void *dst, const void *src, size_t bytes) {
		if (!(((int)dst | (int)src | bytes) & 7))
			VDFastMemcpyPartialScalarAligned8(dst, src, bytes);
		else
			memcpy(dst, src, bytes);
	}

	void VDFastMemcpyFinishScalar() {
	}

	void __cdecl VDFastMemcpyFinishMMX() {
		__asm emms
	}

	void __cdecl VDFastMemcpyFinishMMX2() {
		__asm emms
		__asm sfence
	}

	void (__cdecl *VDFastMemcpyPartial)(void *dst, const void *src, size_t bytes) = VDFastMemcpyPartialScalar;
	void (__cdecl *VDFastMemcpyFinish)() = NULL;

	void VDFastMemcpyAutodetect() {
		long exts = CPUGetEnabledExtensions();

		if (exts & CPUF_SUPPORTS_INTEGER_SSE) {
			VDFastMemcpyPartial = VDFastMemcpyPartialMMX2;
			VDFastMemcpyFinish	= VDFastMemcpyFinishMMX2;
		} else if (exts & CPUF_SUPPORTS_MMX) {
			VDFastMemcpyPartial = VDFastMemcpyPartialMMX;
			VDFastMemcpyFinish	= VDFastMemcpyFinishMMX;
		} else {
			VDFastMemcpyPartial = VDFastMemcpyPartialScalar;
			VDFastMemcpyFinish	= VDFastMemcpyFinishScalar;
		}
	}

#else
	void VDFastMemcpyPartial(void *dst, const void *src, size_t bytes) {
		memcpy(dst, src, bytes);
	}

	void VDFastMemcpyFinish() {
	}

	void VDFastMemcpyAutodetect() {
	}
#endif

void VDMemcpyRect(void *dst, ptrdiff_t dststride, const void *src, ptrdiff_t srcstride, size_t w, size_t h) {
	if (w <= 0 || h <= 0)
		return;

	if (w == srcstride && w == dststride)
		VDFastMemcpyPartial(dst, src, w*h);
	else {
		char *dst2 = (char *)dst;
		const char *src2 = (const char *)src;

		do {
			VDFastMemcpyPartial(dst2, src2, w);
			dst2 += dststride;
			src2 += srcstride;
		} while(--h);
	}
	VDFastMemcpyFinish();
}


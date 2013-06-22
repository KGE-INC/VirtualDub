#include <vd2/system/memory.h>

void VDSwapMemory(void *p0, void *p1, unsigned bytes) throw() {
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

void VDInvertMemory(void *p, unsigned bytes) throw() {
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

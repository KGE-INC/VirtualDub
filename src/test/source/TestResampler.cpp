#include <vd2/system/cpuaccel.h>
#include <vd2/system/filesys.h>
#include <vd2/system/memory.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/resample.h>
#include "test.h"

DEFINE_TEST(Resampler) {
	CPUEnableExtensions(CPUCheckForExtensions());

	uint8 dst[32*32];
	uint8 src[32*32];

	VDPixmap pxdst;
	pxdst.pitch = 32;
	pxdst.format = nsVDPixmap::kPixFormat_Y8;

	VDPixmap pxsrc;
	pxsrc.pitch = 32;
	pxsrc.format = nsVDPixmap::kPixFormat_Y8;

	pxdst.h = 1;
	pxsrc.h = 1;
	for(int dalign = 0; dalign < 4; ++dalign) {
		for(int dx = 4; dx <= 16; ++dx) {
			pxdst.data = dst + 4*32 + 4 + dalign;
			pxdst.w = dx;

			for(int salign = 0; salign <= 4; ++salign) {
				for(int sx = 4; sx <= 16; ++sx) {
					VDMemset8(src, 0x40, sizeof src);
					VDMemset8(dst, 0xCD, sizeof dst);

					pxsrc.w = sx;
					pxsrc.data = src  + 4*32 + 4 + salign;

					VDMemset8(pxsrc.data, 0xA0, sx);
					VDPixmapResample(pxdst, pxsrc, IVDPixmapResampler::kFilterCubic);

					TEST_ASSERT(!VDMemCheck8(dst+32*3, 0xCD, 32));
					TEST_ASSERT(!VDMemCheck8(dst+32*4, 0xCD, dalign + 4));
					TEST_ASSERT(!VDMemCheck8(dst+32*4+dalign+4, 0xA0, dx));
					TEST_ASSERT(!VDMemCheck8(dst+32*4+dalign+4+dx, 0xCD, 32 - (4+dalign+dx)));
					TEST_ASSERT(!VDMemCheck8(dst+32*5, 0xCD, 32));
				}
			}
		}
	}
	return 0;
}

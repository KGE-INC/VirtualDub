#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/system/memory.h>
#include <tchar.h>
#include "test.h"

DEFINE_TEST(Pixmap) {
	using namespace nsVDPixmap;

	// test pal1
	for(int format=kPixFormat_Pal1; format<=kPixFormat_Pal8; ++format) {

		_tprintf(_T("    Testing format %hs\n"), VDPixmapGetInfo(format).name);

		int testw = 2048 >> (format - kPixFormat_Pal1);
		int teststep = 8 >> (format - kPixFormat_Pal1);

		VDPixmapBuffer srcbuf(testw, 2, format);

		int palcount = 1 << (1 << (format - kPixFormat_Pal1));
		for(int k=0; k<palcount; ++k) {
			uint32 v = 0;

			if (k & 1)
				v |= 0x000000ff;
			if (k & 2)
				v |= 0x0000ff00;
			if (k & 4)
				v |= 0x00ff0000;
			if (k & 8)
				v |= 0xff000000;

			((uint32 *)srcbuf.palette)[k] = v;
		}

		for(int q=0; q<256; ++q)
			((uint8 *)srcbuf.data)[q] = ((uint8 *)srcbuf.data)[srcbuf.pitch + q] = (uint8)q;

		VDInvertMemory(vdptroffset(srcbuf.data, srcbuf.pitch), 256);

		VDPixmapBuffer intbuf[4];
		
		intbuf[0].init(testw, 2, kPixFormat_XRGB1555);
		intbuf[1].init(testw, 2, kPixFormat_RGB565);
		intbuf[2].init(testw, 2, kPixFormat_RGB888);
		intbuf[3].init(testw, 2, kPixFormat_XRGB8888);

		VDPixmapBuffer dstbuf(testw, 2, kPixFormat_RGB888);

		for(int x1=0; x1<testw; x1+=teststep) {
			int xlimit = std::min<int>(testw, x1+64);
			for(int x2=x1+8; x2<xlimit; x2+=teststep) {
				for(int i=0; i<4; ++i) {
					VDMemset8Rect(intbuf[i].data, intbuf[i].pitch, 0, intbuf[i].w * VDPixmapGetInfo(intbuf[i].format).qsize, intbuf[i].h);
					VDVERIFY(VDPixmapBlt(intbuf[i], x1, 0, srcbuf, x1, 0, x2-x1, 2));
				}

				for(int j=0; j<3; ++j) {
					VDMemset8Rect(dstbuf.data, dstbuf.pitch, 0, 3*dstbuf.w, dstbuf.h);
					VDVERIFY(VDPixmapBlt(dstbuf, intbuf[j]));

					VDVERIFY(!VDCompareRect(intbuf[2].data, intbuf[2].pitch, dstbuf.data, dstbuf.pitch, 3*dstbuf.w, dstbuf.h));
				}
			}
		}

	}
	return 0;
}


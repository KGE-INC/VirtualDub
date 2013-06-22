#include "stdafx.h"
#include <list>
#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdvideofiltold.h>
#include <vd2/system/memory.h>
#include <malloc.h>
#include "vf_base.h"
#include "VBitmap.h"

extern FilterFunctions g_filterFuncs;

///////////////////////////////////////////////////////////////////////////

class VDVideoFilterIVTC : public VDVideoFilterBase {
public:
	static void *Create(const VDVideoFilterContext *pContext) {
		return new VDVideoFilterIVTC(pContext);
	}

	VDVideoFilterIVTC(const VDVideoFilterContext *pContext);
	~VDVideoFilterIVTC();

	sint32 Run();
	void Prefetch(sint64 frame);
	sint32 Prepare();
	void Start();
	void Stop();
	unsigned Suspend(void *dst, unsigned size);
	void Resume(const void *src, unsigned size);
	bool Config(HWND hwnd);
	size_t GetBlurb(wchar_t *buf, size_t bufsize);
};

VDVideoFilterIVTC::VDVideoFilterIVTC(const VDVideoFilterContext *pContext)
	: VDVideoFilterBase(pContext)
{
}

VDVideoFilterIVTC::~VDVideoFilterIVTC() {
}

namespace {
	uint64 ComputeSums(const VDPixmap& srce, const VDPixmap& srco) {
		const uint8 *p0 = (const uint8 *)srce.data;
		const uint8 *p1  = (const uint8 *)srco.data;
		const ptrdiff_t pitch0 = srce.pitch;
		const ptrdiff_t pitch1  = srco.pitch;
		const vdpixsize w = srce.w;
		const vdpixsize h = srce.h;

		VDASSERT(srce.w == srco.w);
		VDASSERT(srce.h == srco.h);

		uint64 error = 0;
		for(vdpixsize y=0; y<h; ++y) {
			const uint8 *q0, *q1, *q2;

			if (y & 1) {
				q0 = p0;
				q1 = p1;
				q2 = p0;

				if (y+1 < h)
					q2 += pitch0;
			} else {
				q0 = p1;
				q1 = p0;
				q2 = p1;

				if (y)
					q0 -= pitch1;
				if (y+1 < h)
					q2 += pitch1;
			}

			unsigned rowerror = 0;
			for(vdpixsize x=0; x<w; ++x) {
				int re = ((int)q0[0] + (int)q2[0]) - 2*(int)q1[0];
				int ge = ((int)q0[1] + (int)q2[1]) - 2*(int)q1[1];
				int be = ((int)q0[2] + (int)q2[2]) - 2*(int)q1[2];

				rowerror += re*re + ge*ge + be*be;

				q0 += 4;
				q1 += 4;
				q2 += 4;
			}

			error += rowerror;

			p0 += pitch0;
			p1 += pitch1;
		}

		return error;
	}
}

sint32 VDVideoFilterIVTC::Run() {
	sint64 frame = mpContext->mpOutput->mFrameNum;
	VDVideoFilterFrame *pFrame0 = mpContext->mpSrcFrames[0];

	if (!frame) {
		mpContext->CopyFrame(pFrame0);
		return kVFVRun_OK;
	}

	VDVideoFilterFrame *pFrame1 = mpContext->mpSrcFrames[1];

	uint64 error_0 = ComputeSums(*pFrame1->mpPixmap, *pFrame1->mpPixmap);
	uint64 error_e = ComputeSums(*pFrame0->mpPixmap, *pFrame1->mpPixmap);
	uint64 error_o = ComputeSums(*pFrame1->mpPixmap, *pFrame0->mpPixmap);

	VDVideoFilterFrame *pFrameEven = pFrame1;
	VDVideoFilterFrame *pFrameOdd = pFrame1;

	if (error_0 < error_e) {			// z < e
		if (error_o < error_0) {		// o < z < e
			// keep odd
			pFrameEven = pFrame0;
		} else {						// z < e, o
			// nothing to do.
		}
	} else {							// e < z
		if (error_o < error_e) {		// o < e < z
			// keep odd
			pFrameEven = pFrame0;
		} else {						// e < o, z
			// keep odd
			pFrameOdd = pFrame0;
		}
	}

	if (pFrameEven == pFrameOdd) {
		mpContext->CopyFrame(pFrame1);
		return kVFVRun_OK;
	}

	mpContext->AllocFrame();

	VDVideoFilterFrame *pDstFrame = mpContext->mpDstFrame;
	const VDPixmap& pxdst = *pDstFrame->mpPixmap;

	VDMemcpyRect(pxdst.data, pxdst.pitch*2, pFrameEven->mpPixmap->data, pFrameEven->mpPixmap->pitch * 2, pxdst.w * 4, (pxdst.h + 1) >> 1);
	VDMemcpyRect((char *)pxdst.data + pxdst.pitch, pxdst.pitch*2, (const char *)pFrameOdd->mpPixmap->data + pFrameOdd->mpPixmap->pitch, pFrameOdd->mpPixmap->pitch * 2, pxdst.w * 4, pxdst.h >> 1);

	return kVFVRun_OK;
}

void VDVideoFilterIVTC::Prefetch(sint64 frame) {
	if (frame >= 1)
		mpContext->Prefetch(0, frame-1, 0);
	mpContext->Prefetch(0, frame, 0);
}

sint32 VDVideoFilterIVTC::Prepare() {
	const VDVideoFilterPin& srcpin = *mpContext->mpInputs[0];
	const VDPixmap& src = *srcpin.mpFormat;
	VDPixmap& dst = *mpContext->mpOutput->mpFormat;

	dst.data		= NULL;
	dst.palette		= NULL;
	dst.format		= nsVDPixmap::kPixFormat_XRGB8888;
	dst.w			= src.w;
	dst.h			= src.h;
	dst.pitch		= -src.pitch;
	dst.data2		= NULL;
	dst.data3		= NULL;
	dst.pitch2		= 0;
	dst.pitch3		= 0;

	return 0;
}

void VDVideoFilterIVTC::Start() {
}

void VDVideoFilterIVTC::Stop() {
}

unsigned VDVideoFilterIVTC::Suspend(void *dst, unsigned size) {
	return 0;
}

void VDVideoFilterIVTC::Resume(const void *src, unsigned size) {
}

bool VDVideoFilterIVTC::Config(HWND hwnd) {
	return false;
}

size_t VDVideoFilterIVTC::GetBlurb(wchar_t *wbuf, size_t bufsize) {
	char buf[3072];
	buf[0] = 0;	// just in case
	return 0;
}

const VDPluginConfigEntry vfilterDef_IVTC_config={
	NULL, 0, VDPluginConfigEntry::kTypeWStr, L"config", L"Configuration string", L"Script configuration string for V1.x filter"
};

extern const struct VDVideoFilterDefinition vfilterDef_IVTC = {
	sizeof(VDVideoFilterDefinition),
	0,
	1,	1,
	&vfilterDef_IVTC_config,
	NULL,
	VDVideoFilterIVTC::Create,
	VDVideoFilterIVTC::MainProc,
};

extern const struct VDPluginInfo vpluginDef_IVTC = {
	sizeof(VDPluginInfo),
	L"IVTC",
	NULL,
	L"Removes 3:2 pulldown (telecine) from an interlaced video stream.",
	0,
	kVDPluginType_Video,
	0,

	kVDPlugin_APIVersion,
	kVDPlugin_APIVersion,
	kVDPlugin_VideoAPIVersion,
	kVDPlugin_VideoAPIVersion,

	&vfilterDef_IVTC
};

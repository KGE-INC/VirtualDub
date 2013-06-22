#include "stdafx.h"
#include <list>
#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdvideofiltold.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/system/memory.h>
#include <vd2/system/fraction.h>
#include <malloc.h>
#include "vf_base.h"
#include "VBitmap.h"

///////////////////////////////////////////////////////////////////////////

class VDVideoFilterSeparateFields : public VDVideoFilterBase {
public:
	static void *Create(const VDVideoFilterContext *pContext) {
		return new VDVideoFilterSeparateFields(pContext);
	}

	VDVideoFilterSeparateFields(const VDVideoFilterContext *pContext);
	~VDVideoFilterSeparateFields();

	sint32 Run();
	void Prefetch(sint64 frame);
	sint32 PrefetchSrcPrv(sint64 frame);
	sint32 Prepare();
	void Start();
	size_t GetBlurb(wchar_t *buf, size_t bufsize);
};

VDVideoFilterSeparateFields::VDVideoFilterSeparateFields(const VDVideoFilterContext *pContext)
	: VDVideoFilterBase(pContext)
{
}

VDVideoFilterSeparateFields::~VDVideoFilterSeparateFields() {
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

sint32 VDVideoFilterSeparateFields::Run() {
	sint64 frame = mpContext->mpOutput->mFrameNum;
	VDVideoFilterFrame *pFrame = mpContext->mpSrcFrames[0];

	pFrame->AddRef();
	pFrame = pFrame->WriteCopyRef();
	mpContext->CopyFrame(pFrame);
	pFrame->Release();

	VDPixmap& px = *mpContext->mpDstFrame->mpPixmap;

	if ((int)frame & 1)
		px.data = (char *)px.data + px.pitch;

	px.pitch += px.pitch;
	px.h >>= 1;

	return kVFVRun_OK;
}

void VDVideoFilterSeparateFields::Prefetch(sint64 frame) {
	mpContext->Prefetch(0, frame >> 1, 0);
}

sint32 VDVideoFilterSeparateFields::PrefetchSrcPrv(sint64 frame) {
	mpContext->Prefetch(0, frame >> 1, 0);
	return 0;
}

sint32 VDVideoFilterSeparateFields::Prepare() {
	const VDVideoFilterPin& srcpin = *mpContext->mpInputs[0];
	const VDPixmap& src = *srcpin.mpFormat;
	VDPixmap& dst = *mpContext->mpOutput->mpFormat;

	dst.data		= NULL;
	dst.palette		= NULL;
	dst.format		= nsVDPixmap::kPixFormat_XRGB8888;
	dst.w			= src.w;
	dst.h			= src.h >> 1;
	dst.pitch		= src.pitch * 2;
	dst.data2		= NULL;
	dst.data3		= NULL;
	dst.pitch2		= 0;
	dst.pitch3		= 0;

	return 0;
}

void VDVideoFilterSeparateFields::Start() {
	VDVideoFilterPin& outputPin = *mpContext->mpOutput;
	outputPin.mLength *= 2;

	VDFraction fr(outputPin.mFrameRateHi, outputPin.mFrameRateLo);
	fr = fr * 2;

	outputPin.mFrameRateHi = fr.getHi();
	outputPin.mFrameRateLo = fr.getLo();
}

size_t VDVideoFilterSeparateFields::GetBlurb(wchar_t *wbuf, size_t bufsize) {
	char buf[3072];
	buf[0] = 0;	// just in case
	return 0;
}

extern const struct VDVideoFilterDefinition vfilterDef_separateFields = {
	sizeof(VDVideoFilterDefinition),
	0,
	1,	1,
	NULL,
	NULL,
	VDVideoFilterSeparateFields::Create,
	VDVideoFilterSeparateFields::MainProc,
};

extern const VDPluginInfo vpluginDef_separateFields = {
	sizeof(VDPluginInfo),
	L"separate fields",
	NULL,
	L"Splits a frame-based video stream into a field-based stream at double rate.",
	0,
	kVDPluginType_Video,
	0,

	kVDPlugin_APIVersion,
	kVDPlugin_APIVersion,
	kVDPlugin_VideoAPIVersion,
	kVDPlugin_VideoAPIVersion,

	&vfilterDef_separateFields
};



///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////



class VDVideoFilterWeaveFields : public VDVideoFilterBase {
public:
	static void *Create(const VDVideoFilterContext *pContext) {
		return new VDVideoFilterWeaveFields(pContext);
	}

	VDVideoFilterWeaveFields(const VDVideoFilterContext *pContext);
	~VDVideoFilterWeaveFields();

	sint32 Run();
	void Prefetch(sint64 frame);
	sint32 PrefetchSrcPrv(sint64 frame);
	void Start();
	size_t GetBlurb(wchar_t *buf, size_t bufsize);
};

VDVideoFilterWeaveFields::VDVideoFilterWeaveFields(const VDVideoFilterContext *pContext)
	: VDVideoFilterBase(pContext)
{
}

VDVideoFilterWeaveFields::~VDVideoFilterWeaveFields() {
}

sint32 VDVideoFilterWeaveFields::Run() {
	sint64 frame = mpContext->mpOutput->mFrameNum;
	VDVideoFilterFrame *pFrame0 = mpContext->mpSrcFrames[0];

	if (mpContext->mSrcFrameCount < 2) {
		mpContext->CopyFrame(pFrame0);

#if 0
		mpContext->AllocFrame();
		VDPixmap& pxsrc0 = *pFrame0->mpPixmap;
		VDPixmap& pxdst = *mpContext->mpDstFrame->mpPixmap;

		VDMemcpyRect(pxdst.data, pxdst.pitch, pxsrc0.data, pxsrc0.pitch, pxdst.w*4, pxdst.h);
#endif
		return kVFVRun_OK;
	}

	VDVideoFilterFrame *pFrame1 = mpContext->mpSrcFrames[1];

VDDEBUG("generating frame %d from %d/%d\n", (int)frame, (int)pFrame0->mFrameNum, (int)pFrame1->mFrameNum);

	mpContext->AllocFrame();

	VDPixmap& pxsrc0 = *pFrame0->mpPixmap;
	VDPixmap& pxsrc1 = *pFrame1->mpPixmap;
	VDPixmap& pxdst = *mpContext->mpDstFrame->mpPixmap;

	VDMemcpyRect(pxdst.data, pxdst.pitch*2, pxsrc0.data, pxsrc0.pitch*2, pxdst.w*4, (pxdst.h + 1) >> 1);
	VDMemcpyRect(vdptroffset(pxdst.data, pxdst.pitch), pxdst.pitch*2, vdptroffset(pxsrc1.data, pxsrc1.pitch), pxsrc1.pitch*2, pxdst.w*4, pxdst.h >> 1);

	return kVFVRun_OK;
}

void VDVideoFilterWeaveFields::Prefetch(sint64 frame) {
	sint64 frame2 = (frame + 1) >> 1;

	if (frame & 1)
		mpContext->Prefetch(0, frame2-1, 0);

	mpContext->Prefetch(0, frame2, 0);
}

sint32 VDVideoFilterWeaveFields::PrefetchSrcPrv(sint64 frame) {
	mpContext->Prefetch(0, frame >> 1, 0);
	return 0;
}

void VDVideoFilterWeaveFields::Start() {
	VDVideoFilterPin& outputPin = *mpContext->mpOutput;
	outputPin.mLength *= 2;

	VDFraction fr(outputPin.mFrameRateHi, outputPin.mFrameRateLo);
	fr = fr * 2;

	outputPin.mFrameRateHi = fr.getHi();
	outputPin.mFrameRateLo = fr.getLo();
}

size_t VDVideoFilterWeaveFields::GetBlurb(wchar_t *wbuf, size_t bufsize) {
	char buf[3072];
	buf[0] = 0;	// just in case
	return 0;
}

const VDPluginConfigEntry vfilterDef_WeaveFields_config={
	NULL, 0, VDPluginConfigEntry::kTypeWStr, L"config", L"Configuration string", L"Script configuration string for V1.x filter"
};

extern const struct VDVideoFilterDefinition vfilterDef_WeaveFields = {
	sizeof(VDVideoFilterDefinition),
	0,
	1,	1,
	&vfilterDef_WeaveFields_config,
	NULL,
	VDVideoFilterWeaveFields::Create,
	VDVideoFilterWeaveFields::MainProc,
};

extern const VDPluginInfo vpluginDef_WeaveFields = {
	sizeof(VDPluginInfo),
	L"weave fields",
	NULL,
	L"Alternately interleaves fields from an interlaced video to produce a full-frame video at field rate.",
	0,
	kVDPluginType_Video,
	0,

	kVDPlugin_APIVersion,
	kVDPlugin_APIVersion,
	kVDPlugin_VideoAPIVersion,
	kVDPlugin_VideoAPIVersion,

	&vfilterDef_WeaveFields
};

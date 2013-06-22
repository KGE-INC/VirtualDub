#ifndef f_VD2_KASUMI_RESAMPLE_H
#define f_VD2_KASUMI_RESAMPLE_H

struct VDPixmap;

class IVDPixmapResampler {
public:
	enum FilterMode {
		kFilterPoint,
		kFilterLinear,
		kFilterCubic,
		kFilterLanczos3,
		kFilterCount
	};

	virtual ~IVDPixmapResampler() {}
	virtual void SetSplineFactor(double A) = 0;
	virtual bool Init(double dw, double dh, int dstformat, double sw, double sh, int srcformat, FilterMode hfilter, FilterMode vfilter, bool bInterpolationOnly) = 0;
	virtual void Shutdown() = 0;

	virtual void Process(const VDPixmap& dst, double dx1, double dy1, double dx2, double dy2, const VDPixmap& src, double sx, double sy) = 0;
};

IVDPixmapResampler *VDCreatePixmapResampler();
bool VDPixmapResample(const VDPixmap& dst, const VDPixmap& src, IVDPixmapResampler::FilterMode filter);

#endif

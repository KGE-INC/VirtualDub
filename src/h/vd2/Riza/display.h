#ifndef f_VD2_RIZA_DISPLAY_H
#define f_VD2_RIZA_DISPLAY_H

#include <vd2/system/vectors.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/refcount.h>
#include <vd2/system/atomic.h>
#include <vd2/Kasumi/pixmap.h>

VDGUIHandle VDCreateDisplayWindowW32(uint32 dwExFlags, uint32 dwFlags, int x, int y, int width, int height, VDGUIHandle hwndParent);

class IVDVideoDisplay;

class VDVideoDisplayFrame : public vdlist_node, public IVDRefCount {
public:
	VDVideoDisplayFrame();
	virtual ~VDVideoDisplayFrame();

	virtual int AddRef();
	virtual int Release();

	VDPixmap	mPixmap;
	uint32		mFlags;
	bool		mbInterlaced;
	bool		mbAllowConversion;

protected:
	VDAtomicInt	mRefCount;
};

class VDINTERFACE IVDVideoDisplayCallback {
public:
	virtual void DisplayRequestUpdate(IVDVideoDisplay *pDisp) = 0;
};

class VDINTERFACE IVDVideoDisplay {
public:
	enum {
		kFormatPal8			= nsVDPixmap::kPixFormat_Pal8,
		kFormatRGB1555		= nsVDPixmap::kPixFormat_XRGB1555,
		kFormatRGB565		= nsVDPixmap::kPixFormat_RGB565,
		kFormatRGB888		= nsVDPixmap::kPixFormat_RGB888,
		kFormatRGB8888		= nsVDPixmap::kPixFormat_XRGB8888,
		kFormatYUV422_YUYV	= nsVDPixmap::kPixFormat_YUV422_YUYV,
		kFormatYUV422_UYVY	= nsVDPixmap::kPixFormat_YUV422_UYVY
	};

	enum FieldMode {
		kEvenFieldOnly		= 0x01,
		kOddFieldOnly		= 0x02,
		kAllFields			= 0x03,
		kVSync				= 0x04,
		kFirstField			= 0x08,

		kVisibleOnly		= 0x40,
		kAutoFlipFields		= 0x80,

		kFieldModeMax		= 255
	};

	enum FilterMode {
		kFilterAnySuitable,
		kFilterPoint,
		kFilterBilinear,
		kFilterBicubic
	};

	virtual void Destroy() = 0;
	virtual void Reset() = 0;
	virtual void SetSourceMessage(const wchar_t *msg) = 0;
	virtual bool SetSource(bool bAutoUpdate, const VDPixmap& src, void *pSharedObject = 0, ptrdiff_t sharedOffset = 0, bool bAllowConversion = true, bool bInterlaced = false) = 0;
	virtual bool SetSourcePersistent(bool bAutoUpdate, const VDPixmap& src, bool bAllowConversion = true, bool bInterlaced = false) = 0;
	virtual void SetSourceSubrect(const vdrect32 *r) = 0;
	virtual void SetSourceSolidColor(uint32 color) = 0;

	virtual void SetFullScreen(bool fs) = 0;

	virtual void PostBuffer(VDVideoDisplayFrame *) = 0;
	virtual bool RevokeBuffer(bool allowFrameSkip, VDVideoDisplayFrame **ppFrame) = 0;
	virtual void FlushBuffers() = 0;

	virtual void Update(int mode = kAllFields) = 0;
	virtual void Cache() = 0;
	virtual void SetCallback(IVDVideoDisplayCallback *p) = 0;

	enum AccelerationMode {
		kAccelOnlyInForeground,
		kAccelResetInForeground,
		kAccelAlways
	};

	virtual void SetAccelerationMode(AccelerationMode mode) = 0;

	virtual FilterMode GetFilterMode() = 0;
	virtual void SetFilterMode(FilterMode) = 0;
	virtual float GetSyncDelta() const = 0;
};

void VDVideoDisplaySetFeatures(bool enableDirectX, bool enableOverlays, bool enableTermServ, bool enableOpenGL, bool enableDirect3D, bool enableD3DFX);
void VDVideoDisplaySetD3DFXFileName(const wchar_t *path);
void VDVideoDisplaySetDebugInfoEnabled(bool enable);

IVDVideoDisplay *VDGetIVideoDisplay(VDGUIHandle hwnd);
bool VDRegisterVideoDisplayControl();

#endif

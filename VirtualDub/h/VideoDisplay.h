#ifndef f_VIDEODISPLAY_H
#define f_VIDEODISPLAY_H

#include <windows.h>

#define VIDEODISPLAYCONTROLCLASS (g_szVideoDisplayControlName)
extern const char g_szVideoDisplayControlName[];

class IVDVideoDisplay;

class VDINTERFACE IVDVideoDisplayCallback {
public:
	virtual void DisplayRequestUpdate(IVDVideoDisplay *pDisp) = 0;
};

class VDINTERFACE IVDVideoDisplay {
public:
	enum {
		kFormatPal8,
		kFormatRGB1555,
		kFormatRGB565,
		kFormatRGB888,
		kFormatRGB8888,
		kFormatYUV422_YUYV,
		kFormatYUV422_UYVY
	};

	enum FieldMode {
		kAllFields,
		kEvenFieldOnly,
		kOddFieldOnly,

		kVisibleOnly		= 16,

		kFieldModeMax		= 255
	};

	virtual void Reset() = 0;
	virtual void SetSourcePalette(const uint32 *palette, int count) = 0;
	virtual bool SetSource(const void *data, ptrdiff_t stride, int w, int h, int format, void *pSharedObject = 0, ptrdiff_t sharedOffset = 0, bool bAllowConversion = true, bool bInterlaced = false) = 0;
	virtual void Update(int mode) = 0;
	virtual void Cache() = 0;
	virtual void SetCallback(IVDVideoDisplayCallback *p) = 0;
	virtual void LockAcceleration(bool) = 0;
};

IVDVideoDisplay *VDGetIVideoDisplay(HWND hwnd);
bool VDRegisterVideoDisplayControl();

#endif

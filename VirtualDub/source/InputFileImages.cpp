#include "stdafx.h"

#include <windows.h>

#include <vd2/system/error.h>
#include "VideoSourceImages.h"
#include "InputFileImages.h"

extern const char g_szError[];

InputFileImages::InputFileImages()
{
}

InputFileImages::~InputFileImages() {
}

void InputFileImages::Init(const wchar_t *szFile) {
	videoSrc = new VideoSourceImages(VDTextWToA(szFile).c_str());
}

void InputFileImages::setOptions(InputFileOptions *_ifo) {
}

InputFileOptions *InputFileImages::createOptions(const char *buf) {
	return NULL;
}

InputFileOptions *InputFileImages::promptForOptions(HWND hwnd) {
	return NULL;
}

void InputFileImages::setAutomated(bool fAuto) {
}

void InputFileImages::InfoDialog(HWND hwndParent) {
	MessageBox(hwndParent, "No file information is available for image sequences.", g_szError, MB_OK);
}

///////////////////////////////////////////////////////////////////////////

class VDInputDriverImages : public vdrefcounted<IVDInputDriver> {
public:
	const wchar_t *GetSignatureName() { return L"Image sequence input driver (internal)"; }

	int GetDefaultPriority() {
		return -1;
	}

	uint32 GetFlags() { return kF_Video; }

	const wchar_t *GetFilenamePattern() {
		return L"Image sequence (*.bmp,*.tga)\0*.bmp;*.tga\0";
	}

	bool DetectByFilename(const wchar_t *pszFilename) {
		size_t l = wcslen(pszFilename);

		if (l>4 && !wcsicmp(pszFilename + l - 4, L".tga"))
			return true;

		return false;
	}

	int DetectBySignature(const void *pHeader, sint32 nHeaderSize, const void *pFooter, sint32 nFooterSize, sint64 nFileSize) {
		if (nHeaderSize >= 32) {
			const uint8 *buf = (const uint8 *)pHeader;

			if (buf[0] == 'B' && buf[1] == 'M')
				return 1;
		}

		if (nFooterSize > 18) {
			if (!memcmp((const uint8 *)pFooter + nFooterSize - 18, "TRUEVISION-XFILE.", 18))
				return 1;
		}

		return -1;
	}

	InputFile *CreateInputFile(uint32 flags) {
		return new InputFileImages;
	}
};

extern IVDInputDriver *VDCreateInputDriverImages() { return new VDInputDriverImages; }

//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "stdafx.h"

#include <windows.h>

#include <vd2/system/error.h>
#include "VideoSourceImages.h"
#include "InputFileImages.h"
#include "image.h"

extern const char g_szError[];

InputFileImages::InputFileImages()
{
}

InputFileImages::~InputFileImages() {
}

void InputFileImages::Init(const wchar_t *szFile) {
	videoSrc = VDCreateVideoSourceImages(szFile);
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
		return L"Image sequence (*.png,*.bmp,*.tga,*.jpg,*.jpeg,*.iff)\0*.png;*.bmp;*.tga;*.jpg;*.jpeg;*.iff\0";
	}

	bool DetectByFilename(const wchar_t *pszFilename) {
		size_t l = wcslen(pszFilename);

		if (l>4 && !_wcsicmp(pszFilename + l - 4, L".tga"))
			return true;

		return false;
	}

	int DetectBySignature(const void *pHeader, sint32 nHeaderSize, const void *pFooter, sint32 nFooterSize, sint64 nFileSize) {
		if (nHeaderSize >= 32) {
			const uint8 *buf = (const uint8 *)pHeader;

			const uint8 kPNGSignature[8]={137,80,78,71,13,10,26,10};

			// Check for PNG
			if (!memcmp(buf, kPNGSignature, 8))
				return 1;

			// Check for BMP
			if (buf[0] == 'B' && buf[1] == 'M')
				return 1;

			// Check for MayaIFF (FOR4....CIMG)
			if (VDIsMayaIFFHeader(pHeader, nHeaderSize))
				return 1;

			if (buf[0] == 0xFF && buf[1] == 0xD8) {
				
				if (buf[2] == 0xFF && buf[3] == 0xE0) {		// x'FF' SOI x'FF' APP0
					// Hmm... might be a JPEG image.  Check for JFIF tag.

					if (buf[6] == 'J' && buf[7] == 'F' && buf[8] == 'I' && buf[9] == 'F')
						return 1;		// Looks like JPEG to me.
				}

				// Nope, see if it's an Exif file instead (used by digital cameras).

				if (buf[2] == 0xFF && buf[3] == 0xE1) {		// x'FF' SOI x'FF' APP1
					if (buf[6] == 'E' && buf[7] == 'x' && buf[8] == 'i' && buf[9] == 'f')
						return 1;		// Looks like JPEG to me.
				}

				// Look for a bare JPEG (start of second marker and x'FF' EOI at the end
				const uint8 *footer = (const uint8 *)pFooter;

				if (buf[2] == 0xFF && nFooterSize >= 2 && footer[nFooterSize - 2] == 0xFF && footer[nFooterSize - 1] == 0xD9)
					return 1;
			}
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

#include "stdafx.h"

#include <windows.h>

#include <vd2/system/error.h>
#include "VideoSourceImages.h"
#include "InputFileImages.h"

extern const char g_szError[];

InputFileImages::InputFileImages()
{
	audioSrc = NULL;
	videoSrc = NULL;
}

InputFileImages::~InputFileImages() {
	delete videoSrc;
}

void InputFileImages::Init(const char *szFile) {
	videoSrc = new VideoSourceImages(szFile);
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

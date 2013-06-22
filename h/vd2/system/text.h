#ifndef f_VD2_SYSTEM_TEXT_H
#define f_VD2_SYSTEM_TEXT_H

#include <ctype.h>
#include <stdarg.h>

template<class T> class VDBasicString;

// Fast text routines attempt to work in the per-thread area; if the
// per-thread area is not large enough, a buffer is allocated.  If
// the buffer cannot be allocated -- a very bad situation --a null
// string is used.

const char *VDFastTextWToA(const wchar_t *s);
const wchar_t *VDFastTextAToW(const char *s);
char *VDGetFastTextBufferA(int& maxchars);
wchar_t *VDGetFastTextBufferW(int& maxchars);

const char *VDFastTextVprintfA(const char *format, va_list val);
const wchar_t *VDFastTextVprintfW(const wchar_t *format, va_list val);

static inline const char *VDFastTextPrintfA(const char *format, ...) {
	va_list val;

	va_start(val, format);
	const char *pRet = VDFastTextVprintfA(format, val);
	va_end(val);

	return pRet;
}

static inline const wchar_t *VDFastTextPrintfW(const wchar_t *format, ...) {
	va_list val;

	va_start(val, format);
	const wchar_t *pRet = VDFastTextVprintfW(format, val);
	va_end(val);

	return pRet;
}

char *VDFastTextAllocA(size_t bytes);
wchar_t *VDFastTextAllocW(size_t bytes);
void VDFastTextFree();

// The max_dst value needs to include space for the NULL as well.  The number
// of characters produced is returned, minus the null terminator.

int VDTextWToA(char *dst, int max_dst, const wchar_t *src, int max_src = -1);
int VDTextAToW(wchar_t *dst, int max_dst, const char *src, int max_src = -1);

VDBasicString<char> VDTextWToA(const wchar_t *src, int length = -1);
VDBasicString<char> VDTextWToA(const VDBasicString<wchar_t>& sw);
VDBasicString<wchar_t> VDTextAToW(const char *src, int length = -1);
VDBasicString<wchar_t> VDTextAToW(const VDBasicString<char>& sw);

VDBasicString<char> VDTextWToU8(const VDBasicString<wchar_t>& s);
VDBasicString<char> VDTextWToU8(const wchar_t *s, int length);
VDBasicString<wchar_t> VDTextU8ToW(const VDBasicString<char>& s);
VDBasicString<wchar_t> VDTextU8ToW(const char *s, int length);

// The terminating NULL character is not included in these.

int VDTextWToALength(const wchar_t *s, int length=-1);
int VDTextAToWLength(const char *s, int length=-1);

VDBasicString<wchar_t> VDaswprintf(const wchar_t *format, int args, const void *const *argv);
VDBasicString<wchar_t> VDvswprintf(const wchar_t *format, int args, va_list val);
VDBasicString<wchar_t> VDswprintf(const wchar_t *format, int args, ...);

#endif

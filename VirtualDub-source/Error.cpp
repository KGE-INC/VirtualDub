//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2000 Avery Lee
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

#include "VirtualDub.h"

#include <stdio.h>
#include <stdarg.h>
#include <crtdbg.h>
#include <windows.h>

#include "Error.h"

MyError::MyError() {
	buf = NULL;
}

MyError::MyError(MyError& err) {
	buf = new char[strlen(err.buf)+1];
	if (buf)
		strcpy(buf,err.buf);
}

MyError::MyError(const char *f, ...) {
	va_list val;

	va_start(val, f);
	vsetf(f, val);
	va_end(val);
}

MyError::~MyError() {
	delete[] buf;
}

void MyError::setf(const char *f, ...) {
	va_list val;

	va_start(val, f);
	vsetf(f,val);
	va_end(val);
}

void MyError::vsetf(const char *f, va_list val) {
	buf = new char[256];
	if (buf) {
		buf[255] = 0;
		_vsnprintf(buf, 255, f, val);
	}
}

void MyError::post(HWND hWndParent, const char *title) {
	if (!buf || !*buf)
		return;

	_RPT2(0,"*** %s: %s\n", title, buf);
	MessageBox(hWndParent, buf, title, MB_OK | MB_ICONERROR);
}

void MyError::discard() {
	buf = NULL;
}

MyICError::MyICError(const char *s, DWORD icErr) {
	const char *err = "(Unknown)";

	switch(icErr) {
	case ICERR_OK:				err = "no error"; break;
	case ICERR_UNSUPPORTED:		err = "unsupported"; break;
	case ICERR_BADFORMAT:		err = "bad format"; break;
	case ICERR_MEMORY:			err = "out of memory"; break;
	case ICERR_INTERNAL:		err = "internal error"; break;
	case ICERR_BADFLAGS:		err = "bad flags"; break;
	case ICERR_BADPARAM:		err = "bad parameters"; break;
	case ICERR_BADSIZE:			err = "bad data size"; break;
	case ICERR_BADHANDLE:		err = "bad handle"; break;
	case ICERR_CANTUPDATE:		err = "can't update"; break;
	case ICERR_ABORT:			err = "aborted by user"; break;
	case ICERR_ERROR:			err = "unspecified error"; break;
	case ICERR_BADBITDEPTH:		err = "can't handle bit depth"; break;
	case ICERR_BADIMAGESIZE:	err = "bad image size"; break;
	default:
		if (icErr <= ICERR_CUSTOM) err = "<custom error>";
		break;
	}

	setf("%s error: %s (%ld)", s, err, icErr);
}

MyMMIOError::MyMMIOError(const char *s, DWORD mmioerr) {
	const char *err = "(Unknown)";

	switch(mmioerr) {
	case MMIOERR_FILENOTFOUND:		err = "file not found"; break;
	case MMIOERR_OUTOFMEMORY:		err = "out of memory"; break;
	case MMIOERR_CANNOTOPEN:		err = "couldn't open"; break;
	case MMIOERR_CANNOTCLOSE:		err = "couldn't close"; break;
	case MMIOERR_CANNOTREAD:		err = "couldn't read"; break;
	case MMIOERR_CANNOTWRITE:		err = "couldn't write"; break;
	case MMIOERR_CANNOTSEEK:		err = "couldn't seek"; break;
	case MMIOERR_CANNOTEXPAND:		err = "couldn't expand"; break;
	case MMIOERR_CHUNKNOTFOUND:		err = "chunk not found"; break;
	case MMIOERR_UNBUFFERED:		err = "unbuffered"; break;
	case MMIOERR_PATHNOTFOUND:		err = "path not found"; break;
	case MMIOERR_ACCESSDENIED:		err = "access denied"; break;
	case MMIOERR_SHARINGVIOLATION:	err = "sharing violation"; break;
	case MMIOERR_NETWORKERROR:		err = "network error"; break;
	case MMIOERR_TOOMANYOPENFILES:	err = "too many open files"; break;
	case MMIOERR_INVALIDFILE:		err = "invalid file"; break;
	}

	setf("%s error: %s (%ld)", s, err, mmioerr);
}

MyAVIError::MyAVIError(const char *s, DWORD avierr) {
	const char *err = "(Unknown)";

	switch(avierr) {
	case AVIERR_UNSUPPORTED:		err = "unsupported"; break;
	case AVIERR_BADFORMAT:			err = "bad format"; break;
	case AVIERR_MEMORY:				err = "out of memory"; break;
	case AVIERR_INTERNAL:			err = "internal error"; break;
	case AVIERR_BADFLAGS:			err = "bad flags"; break;
	case AVIERR_BADPARAM:			err = "bad parameters"; break;
	case AVIERR_BADSIZE:			err = "bad size"; break;
	case AVIERR_BADHANDLE:			err = "bad AVIFile handle"; break;
	case AVIERR_FILEREAD:			err = "file read error"; break;
	case AVIERR_FILEWRITE:			err = "file write error"; break;
	case AVIERR_FILEOPEN:			err = "file open error"; break;
	case AVIERR_COMPRESSOR:			err = "compressor error"; break;
	case AVIERR_NOCOMPRESSOR:		err = "compressor not available"; break;
	case AVIERR_READONLY:			err = "file marked read-only"; break;
	case AVIERR_NODATA:				err = "no data (?)"; break;
	case AVIERR_BUFFERTOOSMALL:		err = "buffer too small"; break;
	case AVIERR_CANTCOMPRESS:		err = "can't compress (?)"; break;
	case AVIERR_USERABORT:			err = "aborted by user"; break;
	case AVIERR_ERROR:				err = "error (?)"; break;
	}

	setf("%s error: %s (%08lx)", s, err, avierr);
}

MyMemoryError::MyMemoryError() {
	setf("Out of memory");
}

MyWin32Error::MyWin32Error(const char *format, DWORD err, ...) {
	char szError[128];
	char szTemp[256];
	va_list val;

	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			0,
			err,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			szError,
			sizeof szError,
			NULL);

	if (szError[0]) {
		long l = strlen(szError);

		if (l>1 && szError[l-2] == '\r')
			szError[l-2] = 0;
		else if (szError[l-1] == '\n')
			szError[l-1] = 0;
	}

	va_start(val, err);
	szTemp[sizeof szTemp-1] = 0;
	_vsnprintf(szTemp, sizeof szTemp, format, val);
	va_end(val);

	setf(szTemp, szError);
}

MyCrashError::MyCrashError(const char *format, DWORD dwExceptionCode) {
	const char *s = "(Unknown Exception)";

	switch(dwExceptionCode) {
	case EXCEPTION_ACCESS_VIOLATION:
		s = "Access Violation";
		break;
	case EXCEPTION_PRIV_INSTRUCTION:
		s = "Privileged Instruction";
		break;
	case EXCEPTION_INT_DIVIDE_BY_ZERO:
		s = "Integer Divide By Zero";
		break;
	case EXCEPTION_BREAKPOINT:
		s = "User Breakpoint";
		break;
	}

	setf(format, s);
}

MyUserAbortError::MyUserAbortError() {
	buf = strdup("");
}

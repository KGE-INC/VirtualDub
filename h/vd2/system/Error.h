//	VirtualDub - Video processing and capture application
//	System library component
//	Copyright (C) 1998-2004 Avery Lee, All Rights Reserved.
//
//	Beginning with 1.6.0, the VirtualDub system library is licensed
//	differently than the remainder of VirtualDub.  This particular file is
//	thus licensed as follows (the "zlib" license):
//
//	This software is provided 'as-is', without any express or implied
//	warranty.  In no event will the authors be held liable for any
//	damages arising from the use of this software.
//
//	Permission is granted to anyone to use this software for any purpose,
//	including commercial applications, and to alter it and redistribute it
//	freely, subject to the following restrictions:
//
//	1.	The origin of this software must not be misrepresented; you must
//		not claim that you wrote the original software. If you use this
//		software in a product, an acknowledgment in the product
//		documentation would be appreciated but is not required.
//	2.	Altered source versions must be plainly marked as such, and must
//		not be misrepresented as being the original software.
//	3.	This notice may not be removed or altered from any source
//		distribution.

#ifndef f_VD2_ERROR_H
#define f_VD2_ERROR_H

#include <windows.h>

class MyError {
private:
	const MyError& operator=(const MyError&);		// protect against accidents

protected:
	char *buf;

public:
	MyError() throw();
	MyError(const MyError& err) throw();
	MyError(const char *f, ...) throw();
	~MyError() throw();
	void assign(const MyError& e) throw();
	void setf(const char *f, ...) throw();
	void vsetf(const char *f, va_list val) throw();
	void post(HWND hWndParent, const char *title) const throw();
	char *gets() const throw() {
		return buf;
	}
	void discard() throw();
	void TransferFrom(MyError& err) throw();
};

class MyICError : public MyError {
public:
	MyICError(const char *s, DWORD icErr) throw();
	MyICError(DWORD icErr, const char *format, ...) throw();
};

class MyMMIOError : public MyError {
public:
	MyMMIOError(const char *s, DWORD icErr) throw();
};

class MyAVIError : public MyError {
public:
	MyAVIError(const char *s, DWORD aviErr) throw();
};

class MyMemoryError : public MyError {
public:
	MyMemoryError() throw();
};

class MyWin32Error : public MyError {
public:
	MyWin32Error(const char *format, DWORD err, ...) throw();
};

class MyCrashError : public MyError {
public:
	MyCrashError(const char *format, DWORD dwExceptionCode) throw();
};

class MyUserAbortError : public MyError {
public:
	MyUserAbortError() throw();
};

class MyInternalError : public MyError {
public:
	MyInternalError(const char *format, ...) throw();
};

#endif

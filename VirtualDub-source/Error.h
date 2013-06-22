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

#ifndef f_ERROR_H
#define f_ERROR_H

#include <windows.h>
#include <vfw.h>

class MyError {
protected:
	char *buf;

public:
	MyError(MyError& err);
	MyError();
	MyError(const char *f, ...);
	~MyError();
	void setf(const char *f, ...);
	void vsetf(const char *f, va_list val);
	void post(HWND hWndParent, const char *title);
	char *gets() {
		return buf;
	}
	void discard();
};

class MyICError : public MyError {
public:
	MyICError(const char *s, DWORD icErr);
};

class MyMMIOError : public MyError {
public:
	MyMMIOError(const char *s, DWORD icErr);
};

class MyAVIError : public MyError {
public:
	MyAVIError(const char *s, DWORD aviErr);
};

class MyMemoryError : public MyError {
public:
	MyMemoryError();
};

class MyWin32Error : public MyError {
public:
	MyWin32Error(const char *format, DWORD err, ...);
};

class MyCrashError : public MyError {
public:
	MyCrashError(const char *format, DWORD dwExceptionCode);
};

class MyUserAbortError : public MyError {
public:
	MyUserAbortError();
};

#endif

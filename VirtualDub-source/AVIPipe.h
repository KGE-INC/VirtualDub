//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee
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

#ifndef f_AVIPIPE_H
#define f_AVIPIPE_H

#include <windows.h>

class AVIPipe {
private:
	static char me[];

	HANDLE				hEventRead, hEventWrite;
	CRITICAL_SECTION	critsec;

	volatile struct AVIPipeBuffer {
		void	*data;
		long	size;
		long	len;
		long	sample;
		long	displayframe;
		long	id;
		int		iExdata;
		int		droptype;
	} *pBuffers;

	int		num_buffers;
	long	round_size;

	long	cur_read;
	long	cur_write;

	long	total_audio;

	volatile char	finalize_state;

	enum {
		FINALIZE_TRIGGERED		= 1,
		FINALIZE_ACKNOWLEDGED	= 2,
		FINALIZE_ABORTED		= 4,
		SYNCPOINT_TRIGGERED		= 8,
		SYNCPOINT_ACKNOWLEDGED	= 16,
	};

	// These are the same as in VideoSourceAVI

public:
	enum {
		kDroppable=0,
		kDependant,
		kIndependent
	};

	AVIPipe(int buffers, long roundup_size);
	~AVIPipe();

	BOOL isOkay();
	BOOL isFinalized();
	BOOL isNoMoreAudio();

	void *getWriteBuffer(long len, int *handle_ptr, DWORD timeout);
	void postBuffer(long len, long samples, long dframe, int exdata, int droptype, int handle);
	void *getReadBuffer(long *len_ptr, long *samples_ptr, long *dframe_ptr, int *exdata_ptr, int *droptype_ptr, int *handle_ptr, DWORD timeout);
	void releaseBuffer(int handle);
	void finalize();
	void abort();
	bool sync();
	void syncack();
	void getDropDistances(int& dependant, int& independent);
};

#endif

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

#ifndef f_MPEG_DECODE_H
#define f_MPEG_DECODE_H

#define MPEG_BUFFER_FORWARD			(0)
#define	MPEG_BUFFER_BACKWARD		(1)
#define	MPEG_BUFFER_BIDIRECTIONAL	(2)

#define MPEG_FRAME_TYPE_I			(1)
#define MPEG_FRAME_TYPE_P			(2)
#define MPEG_FRAME_TYPE_B			(3)
#define MPEG_FRAME_TYPE_D			(4)

void	mpeg_deinitialize();
void	mpeg_initialize(int width, int height, char *imatrix, char *nimatrix, BOOL fullpel);
void	mpeg_reset();
void	mpeg_convert_frame16(void *output_buffer, int buffer_ID);
void	mpeg_convert_frame24(void *output_buffer, int buffer_ID);
void	mpeg_convert_frame32(void *output_buffer, int buffer_ID);
void	mpeg_convert_frameUYVY16(void *output_buffer, int buffer_ID);
void	mpeg_convert_frameYUY216(void *output_buffer, int buffer_ID);
void	mpeg_decode_frame(void *input_data, int len, int frame_num);
void	mpeg_swap_buffers(int buffer1, int buffer2);
int		mpeg_lookup_frame(int frame);

#endif

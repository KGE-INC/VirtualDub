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

#ifndef f_MRULIST_H
#define f_MRULIST_H

class MRUList {
private:
	char *key_name;
	char *file_order;
	char **file_list;
	int max_files;
	long modified;

public:
	MRUList(int max_files, char *key);
	~MRUList();

	void add(char *file);
	int get(int index, char *lpBuffer, int cbBuffer);
	void move_to_top(int index);
	void clear();
	void load();
	void flush();
};

#endif

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

#include "stdafx.h"

#include <vd2/system/error.h>

#include "oshelper.h"

#include "MRUList.h"

MRUList::MRUList(int max_files, char *key_name) {
	this->max_files		= max_files;
	this->key_name		= key_name;

	file_order = new char[max_files+1];
	file_list = new char *[max_files];

	if (!file_order || !file_list) {
		delete file_order;
		delete file_list;

		throw MyMemoryError();
	}

	memset(file_order, 0, max_files+1);
	memset(file_list, 0, sizeof(char *)*max_files);
	modified = 0;

	load();
}

MRUList::~MRUList() {
	flush();

	for(int i=0; i<max_files; i++)
		delete file_list[i];

	delete file_order;
	delete file_list;
}

void MRUList::add(char *file) {
	int index;
	char *new_str;

	// Does this file already exist?  If not, don't add it!

	for(index=0; index<max_files; index++)
		if (file_list[index])
			if (!stricmp(file_list[index], file))
				return;

	// Add file to list

	new_str = strdup(file);

	if (!new_str) return;

	if (strlen(file_order) == max_files)
		index = file_order[max_files-1] - 'a';
	else {
		index=0;
		while(file_list[index]) ++index;
	}

	if (file_list[index]) freemem(file_list[index]);
	file_list[index] = new_str;

	memmove(file_order+1, file_order, max_files-1);
	file_order[0] = index+'a';

	modified |= (1L<<index) | 0x80000000L;
}

int MRUList::get(int index, char *lpBuffer, int cbBuffer) {
	int i, l;

	if (!file_order[index]) return -1;

	i = file_order[index] - 'a';

	l = strlen(file_list[i]) + 1;

	if (l > cbBuffer) return l;

	memcpy(lpBuffer, file_list[i], l);

	return 0;
}

void MRUList::move_to_top(int index) {
	// Move file to top of list

	if (index) {
		char c = file_order[index];
		memmove(file_order+1, file_order, index);
		file_order[0] = c;
	}

}

void MRUList::clear() {
	int i;

	memset(file_order, 0, max_files+1);
	for(i=0; i<max_files; i++) {
		delete file_list[i];
		file_list[i]=NULL;
	}

	modified |= 0x80000000L;
}

void MRUList::load() {
	HKEY hkey;
	char buf[128], buf2[2];
	DWORD dwType;
	DWORD cbData = sizeof buf;
	int i;

	clear();

	if (!(hkey = OpenConfigKey(key_name)))
		return;

	if (ERROR_SUCCESS == RegQueryValueEx(hkey, "MRUList", NULL, &dwType, (LPBYTE)buf, &cbData)
		&& dwType == REG_SZ) {

		for(i=0; i<max_files; i++) {
			buf2[0] = buf[i];
			buf2[1] = 0;

			if (!buf2[0]) break;

			if (ERROR_SUCCESS != RegQueryValueEx(hkey, buf2, NULL, &dwType, NULL, &cbData))
				break;

			if (dwType != REG_SZ) break;

			if (!(file_list[i] = (char *)allocmem(cbData))) break;

			if (ERROR_SUCCESS != RegQueryValueEx(hkey, buf2, NULL, NULL, (LPBYTE)file_list[i], &cbData)) {
				freemem(file_list[i]);
				break;
			}

			file_order[i] = 'a'+i;
		}
		file_order[i] = 0;
	}

	RegCloseKey(hkey);
}

void MRUList::flush() {
	HKEY hkey;
	int i;

	if (!modified) return;

	if (!(hkey = CreateConfigKey(key_name)))
		return;
#if 0
	for(i=0; i<max_files; i++) {
		if (modified & (1L<<i)) {
			char buf[2];

			buf[0] = 'a'+i;
			buf[1] = 0;
			RegSetValueEx(hkey, buf, 0, REG_SZ, (LPBYTE)file_list[i], strlen(file_list[i])+1);
		}
	}

	if (modified & 0x80000000L)
		RegSetValueEx(hkey, "MRUList", 0, REG_SZ, (LPBYTE)file_order, strlen(file_order)+1);
#else
	char buf[512];
	DWORD cbData;

	cbData = sizeof buf;
	if (ERROR_SUCCESS != RegQueryValueEx(hkey, "MRUList", NULL, NULL, (LPBYTE)buf, &cbData)
			|| strcmp(buf, file_order))

		RegSetValueEx(hkey, "MRUList", 0, REG_SZ, (LPBYTE)file_order, strlen(file_order)+1);

	for(i=0; i<max_files && file_order[i]; i++) {
		char name[2];
		char *s;

		name[0] = file_order[i];
		name[1] = 0;
		cbData = sizeof buf;

		s = file_list[name[0]-'a'];

		if (ERROR_SUCCESS != RegQueryValueEx(hkey, name, NULL, NULL, (LPBYTE)buf, &cbData)
			|| strcmp(buf, s))

			RegSetValueEx(hkey, name, 0, REG_SZ, (LPBYTE)s, strlen(s)+1);

	}

#endif

	RegCloseKey(hkey);

	modified = 0;
}

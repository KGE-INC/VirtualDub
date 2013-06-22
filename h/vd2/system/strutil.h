#ifndef f_VD2_SYSTEM_UTIL_H
#define f_VD2_SYSTEM_UTIL_H

#include <string.h>

char *strncpyz(char *strDest, const char *strSource, size_t count);
const char *strskipspace(const char *s) throw();

inline char *strskipspace(char *s) throw() {
	return const_cast<char *>(strskipspace(s));
}

#endif

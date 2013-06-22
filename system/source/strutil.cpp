#include <string.h>
#include <ctype.h>

#include <vd2/system/strutil.h>

char *strncpyz(char *strDest, const char *strSource, size_t count) {
	char *s;

	s = strncpy(strDest, strSource, count);
	strDest[count-1] = 0;

	return s;
}

const char *strskipspace(const char *s) {
	while(isspace((unsigned char)*s++))
		;

	return s-1;
}

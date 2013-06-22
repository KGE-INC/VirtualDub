#pragma warning(disable: 4786) // STFU

#include <vd2/system/vdtypes.h>

#include <windows.h>

#include <stdio.h>
#include <map>
#include <string>

#include "utils.h"

using namespace std;

typedef map<string, uint32> tVersionMap;
tVersionMap		g_versionMap;
int				g_version;
string		g_machineName;




void help() {
	exit(5);
}

void fail(const char *format, ...) {
	va_list val;
	va_start(val, format);
	fputs("Asuka: Failed: ", stdout);
	vprintf(format, val);
	fputc('\n', stdout);
	va_end(val);
	exit(10);
}



void canonicalize_name(string& name) {
	string::iterator it(name.begin());

	*it = toupper(*it);
	++it;
	transform(it, name.end(), it, tolower);
}

string get_name() {
	char buf[256];
	DWORD siz = sizeof buf;

	if (!GetComputerName(buf, &siz))		// hostname would probably work on a Unix platform
		buf[0] = 0;

	string name(buf);

	if (name.empty())
		name = "Anonymous";
	else
		canonicalize_name(name);

	return name;
}

int get_version() {
	return g_version;
}

bool read_version() {
	g_machineName = get_name();
	g_versionMap.clear();
	g_version = 0;

	FILE *f = fopen("version2.bin","r");

	if (!f) {
		printf("    warning: can't open version2.bin for read, starting new version series\n");
		return false;
	}

	char linebuf[2048];

	while(fgets(linebuf, sizeof linebuf, f)) {
		int local_builds, local_name_start, local_name_end;
		if (1==sscanf(linebuf, "host: \"%n%*[^\"]%n\" builds: %d", &local_name_start, &local_name_end, &local_builds)) {
			string name(linebuf+local_name_start, local_name_end - local_name_start);

			canonicalize_name(name);

			g_versionMap[name] = local_builds;

			g_version += local_builds;
		} else if (linebuf[0] != '\n')
			printf("    warning: line ignored: %s", linebuf);
	}

	return true;
}

void inc_version() {
	++g_version;
	++g_versionMap[g_machineName];
}

bool write_version() {
	printf("    incrementing to build %d (builds on '%s': %d)\n", g_version, g_machineName.c_str(), g_versionMap[g_machineName]);

	for(;;) {
		if (FILE *f = fopen("version2.bin","w")) {
			tVersionMap::const_iterator it(g_versionMap.begin()), itEnd(g_versionMap.end());

			for(; it!=itEnd; ++it) {
				const tVersionMap::value_type val(*it);
				int pad = 20-val.first.length();

				if (pad < 1)
					pad = 1;

				fprintf(f, "host: \"%s\"%*cbuilds: %d\n", val.first.c_str(), pad, ' ', val.second);
			}

			fclose(f);
			return true;
		} else {
			if (IDOK == MessageBox(NULL, "Can't open version2.bin.  Check out from Perforce?", "verinc error", MB_ICONEXCLAMATION|MB_OKCANCEL)) {
				system("p4 edit version2.bin");
				continue;
			}

			fail("Can't open version2.bin for write.");
			return false;
		}
	}
}

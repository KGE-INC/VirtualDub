#pragma warning(disable: 4786)		// SHUT UP

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <dos.h>
#include <time.h>

#include <string>
#include <map>
#include <algorithm>

using namespace std;

typedef unsigned uint32;

typedef map<std::string, uint32> tVersionMap;
tVersionMap		gVersionMap;

void canonicalize_name(std::string& name) {
	std::string::iterator it(name.begin());

	*it = toupper(*it);
	++it;
	std::transform(it, name.end(), it, tolower);
}

std::string get_name() {
	char buf[256];
	DWORD siz = sizeof buf;

	if (!GetComputerName(buf, &siz))		// hostname would probably work on a Unix platform
		buf[0] = 0;

	std::string name(buf);

	if (name.empty())
		name = "Anonymous";
	else
		canonicalize_name(name);

	return name;
}

int main(int argc, char **argv) {
	FILE *f;
	uint32 build=0;
	char s[25];
	time_t tm;

	//////////////

	bool amd64 = false;
	if (argc>=2 && !stricmp(argv[1], "amd64"))
		amd64 = true;

	if (f=fopen("version2.bin","r")) {
		char linebuf[2048];

		while(fgets(linebuf, sizeof linebuf, f)) {
			int local_builds, local_name_start, local_name_end;
			if (1==sscanf(linebuf, "host: \"%n%*[^\"]%n\" builds: %d", &local_name_start, &local_name_end, &local_builds)) {
				std::string name(linebuf+local_name_start, local_name_end - local_name_start);

				canonicalize_name(name);

				gVersionMap[name] = local_builds;

				build += local_builds;
			} else if (linebuf[0] != '\n')
				printf("    warning: line ignored: %s", linebuf);
		}
	} else {
		printf("    warning: can't open version2.bin for read, starting new version series\n");
	}

	std::string machine_name(get_name());

	++build;
	++gVersionMap[machine_name];
	printf("    incrementing to build %d (builds on '%s': %d)\n", build, machine_name.c_str(), gVersionMap[machine_name]);

	time(&tm);
	memcpy(s, asctime(localtime(&tm)), 24);
	s[24]=0;

	if (f=fopen("verstub.asm","w")) {
		if (amd64)
			fprintf(f,
				"\t"	".const\n"
				"\n"
				"\t"	"public\t"	"version_num\n"
				"\t"	"public\t"	"version_time\n"
				"\n"
				"version_num\t"	"dd\t"	"%ld\n"
				"version_time\t"	"db\t"	"\"%s\",0\n"
				"\n"
				"\t"	"end\n"
				,build
				,s);
		else
			fprintf(f,
				"\t"	".386\n"
				"\t"	".model\t"	"flat\n"
				"\t"	".const\n"
				"\n"
				"\t"	"public\t"	"_version_num\n"
				"\t"	"public\t"	"_version_time\n"
				"\n"
				"_version_num\t"	"dd\t"	"%ld\n"
				"_version_time\t"	"db\t"	"\"%s\",0\n"
				"\n"
				"\t"	"end\n"
				,build
				,s);
		fclose(f);
	}

	for(;;) {
		if (f=fopen("version2.bin","w")) {
			tVersionMap::const_iterator it(gVersionMap.begin()), itEnd(gVersionMap.end());

			for(; it!=itEnd; ++it) {
				const tVersionMap::value_type val(*it);
				int pad = 20-val.first.length();

				if (pad < 1)
					pad = 1;

				fprintf(f, "host: \"%s\"%*cbuilds: %d\n", val.first.c_str(), pad, ' ', val.second);
			}

			fclose(f);
			break;
		} else {
			if (IDOK == MessageBox(NULL, "Can't open version2.bin.  Check out from Perforce?", "verinc error", MB_ICONEXCLAMATION|MB_OKCANCEL)) {
				system("p4 edit version2.bin");
				continue;
			}

			printf("    can't open version2.bin for write\n");
			return 20;
		}
	}

	for(;;) {
		if (f=fopen("version.bin","wb")) {
			fwrite(&build, 4, 1, f);
			fclose(f);
			break;
		} else {
			if (IDOK == MessageBox(NULL, "Can't open version.bin.  Check out from Perforce?", "verinc error", MB_ICONEXCLAMATION|MB_OKCANCEL)) {
				system("p4 edit version.bin");
				continue;
			}

			printf("    can't open version.bin for write\n");
			return 20;
		}
	}

	return 0;
}

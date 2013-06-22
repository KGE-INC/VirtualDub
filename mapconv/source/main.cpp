#include <vector>
#include <algorithm>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CNAMBUF		(131072)
#define MAX_FNAMBUF		(131072)
#define MAX_SEGMENTS	(64)
#define MAX_GROUPS		(64)

// RVA stands for "Relative Virtual Address."  Strictly speaking, these
// aren't actually RVAs since they've already been relocated.  Oh well.
struct RVAEnt {
	long rva;
	char *line;
};

std::vector<RVAEnt> rvabuf;

char fnambuf[MAX_FNAMBUF];
char *fnamptr = fnambuf;

char cnambuf[MAX_CNAMBUF];
char *cnamptr = cnambuf;

long segbuf[MAX_SEGMENTS][2];
int segcnt=0;
int seggrp[MAX_SEGMENTS];
long grpstart[MAX_GROUPS];

char line[8192];
long codeseg_flags = 0;
FILE *f, *fo;

bool readline() {
	if (!fgets(line, sizeof line, f))
		return false;

	int l = strlen(line);

	if (l>0 && line[l-1]=='\n')
		line[l-1]=0;

	return true;
}

bool findline(const char *searchstr) {
	while(readline()) {
		if (strstr(line, searchstr))
			return true;
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////

void parsename(long rva, char *buf) {
	char *func_name = NULL;
	char *class_name = NULL;
	char c;
	int special_func = 0;

	if (*buf++ != '?') {
		func_name = buf;
	} else {

		if (*buf == '?') {
			// ??0
			// ??1
			// ??_G
			// ??_E

			++buf;
			c=*buf++;

			special_func = 31;
			if (c=='0')
				special_func = 1;		// constructor
			else if (c=='1')
				special_func = 2;		// destructor
			else if (c=='_') {
				c = *buf++;

				if (c == 'G')
					special_func = 3;		// scalar deleting destructor
				else if (c == 'E')
					special_func = 4;		// vector deleting destructor?
			}
		} else {
			func_name = buf;

			while(*buf != '@') {
				if (!*buf)
					throw "bad decorated name";

				++buf;
			}

			if (buf == func_name)
				printf("    warning: empty function name generated from: '%s'\n", func_name);

			*buf++ = 0;
		}

		// Look for a class name.

		if (*buf != '@') {
			if (!*buf)
				throw "bad decorated name";

			class_name = buf;

			while(*buf != '@') {
				if (!*buf)
					throw "bad decorated name (class)";

				++buf;
			}

			if (buf == class_name)
				printf("    warning: empty class name generated from: '%s'\n", class_name);

			*buf++ = 0;
		}
	}

	// write out to buffers

	if (class_name) {
		char *csptr = cnambuf;
		int idx = 0;

		while(csptr < cnamptr) {
			if (!strcmp(csptr, class_name)) {
				break;
			}
			while(*csptr++);
			++idx;
		}

		if (csptr >= cnamptr) {
			strcpy(cnamptr, class_name);
			while(*cnamptr++);
		}

		*fnamptr++ = 1 + (idx / 128);
		*fnamptr++ = 1 + (idx % 128);

		if (special_func)
			*fnamptr++ = special_func;
	}

	if (func_name) {
		if (!*func_name)
			printf("    warning: writing out empty function name\n");
		strcpy(fnamptr, func_name);
		while(*fnamptr++);
	} else {
		if (!class_name)
			printf("    warning: function name absent: '%s'\n", buf);
		*fnamptr++ = 0;
	}
}

struct RVASorter {
	bool operator()(const RVAEnt& e1, const RVAEnt& e2) {
		return e1.rva < e2.rva;
	}
};

int main(int argc, char **argv) {
	int ver=0;
	int i;

	if (argc<4) {
		printf("mapconv <listing-file> <output-name> <disassembler module>\n");
		return 0;
	}

	if (f=fopen("version.bin", "rb")) {
		fread(&ver,4,1,f);
		fclose(f);
	} else {
		printf("    can't read version file\n");
		return 20;
	}

	if (!(f=fopen(argv[1], "r"))) {
		printf("    can't open listing file \"%s\"\n", argv[1]);
		return 20;
	}

	if (!(fo=fopen(argv[2], "wb"))) {
		printf("    can't open output file \"%s\"\n", argv[2]);
		return 20;
	}

	int disasm_size = 0;
	{
		FILE *fd;

		if (!(fd=fopen(argv[3], "rb"))) {
			printf("    can't open disassembler module \"%s\"\n", argv[3]);
			return 20;
		}

		void *buf = malloc(32768);
		int act;

		while((act = fread(buf, 1, 32768, fd)) > 0) {
			disasm_size += act;
			fwrite(buf, act, 1, fo);
		}

		free(buf);
		fclose(fd);
	}

	// Begin parsing file

	try {
		line[0] = 0;

		if (!findline("Start         Length"))
			throw "can't find segment list";

		while(readline()) {
			long grp, start, len;

			if (3!=sscanf(line, "%lx:%lx %lx", &grp, &start, &len))
				break;

			if (strstr(line+49, "CODE")) {
				printf("        %04x:%08lx %08lx type code (%dKB)\n", grp, start, len, (len+1023)>>10);

				codeseg_flags |= 1<<grp;

				segbuf[segcnt][0] = start;
				segbuf[segcnt][1] = len;
				seggrp[segcnt] = grp;
				++segcnt;
			}
		}

		if (!findline("Publics by Value"))
			throw "Can't find public symbol list.";

		readline();

		while(readline()) {
			long grp, start, rva;
			char symname[2048];

			if (4!=sscanf(line, "%lx:%lx %s %lx", &grp, &start, symname, &rva))
				break;

			if (!(codeseg_flags & (1<<grp)))
				continue;

			RVAEnt entry = { rva, strdup(line) };

			rvabuf.push_back(entry);

//			parsename(rva,symname);
		}

		if (!findline("Static symbols"))
			printf("    warning: No static symbols found!\n");
		else {
			readline();

			while(readline()) {
				long grp, start, rva;
				char symname[4096];

				if (4!=sscanf(line, "%lx:%lx %s %lx", &grp, &start, symname, &rva))
					break;

				if (!(codeseg_flags & (1<<grp)))
					continue;

				RVAEnt entry = { rva, strdup(line) };

				rvabuf.push_back(entry);

	//			parsename(rva,symname);
			}
		}

		std::sort(rvabuf.begin(), rvabuf.end(), RVASorter());

		// delete all entries that take no space -- these often crop up with inline classes

		int j=0;
		for(i=0; i<rvabuf.size()-1; ++i) {
			if (rvabuf[i+1].rva - rvabuf[i].rva) {
				rvabuf[j++] = rvabuf[i];
			}
		}
		rvabuf[j++] = rvabuf[i];	// last entry is always copied

		printf("    Deleted %d zero-size entries from RVA table\n", i+1-j);
		rvabuf.resize(j);

		for(i=0; i<rvabuf.size(); i++) {
			long grp, start, rva;
			char symname[4096];

			sscanf(rvabuf[i].line, "%lx:%lx %s %lx", &grp, &start, symname, &rva);

			grpstart[grp] = rva - start;

			parsename(rva, symname);
		}
		
		for(i=0; i<segcnt; i++) {
			segbuf[i][0] += grpstart[seggrp[i]];
			printf("        #%-2d  %08lx-%08lx\n", i+1, segbuf[i][0], segbuf[i][0]+segbuf[i][1]-1);
		}

		printf("        Raw statistics:\n");
		printf("            Disassembler:     %ld bytes\n", disasm_size);
		printf("            RVA bytes:        %ld\n", rvabuf.size()*4);
		printf("            Class name bytes: %ld\n", cnamptr - cnambuf);
		printf("            Func name bytes:  %ld\n", fnamptr - fnambuf);

		printf("    Packing RVA data..."); fflush(stdout);

		std::vector<RVAEnt>::iterator itRVA = rvabuf.begin(), itRVAEnd = rvabuf.end();
		std::vector<char> rvaout;
		long firstrva = (*itRVA++).rva;
		long lastrva = firstrva;

		for(; itRVA != itRVAEnd; ++itRVA) {
			long rvadiff = (*itRVA).rva - lastrva;

			lastrva += rvadiff;

			if (rvadiff & 0xF0000000) rvaout.push_back((char)(0x80 | ((rvadiff>>28) & 0x7F)));
			if (rvadiff & 0xFFE00000) rvaout.push_back((char)(0x80 | ((rvadiff>>21) & 0x7F)));
			if (rvadiff & 0xFFFFC000) rvaout.push_back((char)(0x80 | ((rvadiff>>14) & 0x7F)));
			if (rvadiff & 0xFFFFFF80) rvaout.push_back((char)(0x80 | ((rvadiff>> 7) & 0x7F)));
			rvaout.push_back((char)(rvadiff & 0x7F));
		}

		printf("%ld bytes\n", rvaout.size());

		// dump data

		long t;

		static const char header[64]="[01|01] VirtualDub symbolic debug information\r\n\x1A";

		fwrite(header, 64, 1, fo);

		t = ver;
		fwrite(&t, 4, 1, fo);

		t = rvaout.size() + 4;
		fwrite(&t, 4, 1, fo);

		t = cnamptr - cnambuf;
		fwrite(&t, 4, 1, fo);

		t = fnamptr - fnambuf;
		fwrite(&t, 4, 1, fo);

		t = segcnt;
		fwrite(&t, 4, 1, fo);

		fwrite(&firstrva, 4, 1, fo);
		fwrite(&rvaout[0], rvaout.size(), 1, fo);
		fwrite(cnambuf, cnamptr - cnambuf, 1, fo);
		fwrite(fnambuf, fnamptr - fnambuf, 1, fo);
		fwrite(segbuf, segcnt*8, 1, fo);

		// really all done

		if (fclose(fo))
			throw "output file close failed";
		
	} catch(const char *s) {
		fprintf(stderr, "    fatal error: %s\n", s);
	}

	fclose(f);

	return 0;
}

#include <ctype.h>
#include <stdio.h>

#include <windows.h>
#include <commdlg.h>

#include "Error.h"
#include "File64.h"
#include "ProgressDialog.h"
#include "gui.h"

extern "C" unsigned long version_num;
extern const char g_szError[];

struct SparseAVIHeader {
	enum {
		kChunkID = 'VAPS',
		kChunkSize = 30
	};

	unsigned long		ckid;
	unsigned long		size;				// == size of this structure minus 8
	__int64				original_size;		// original file size, in bytes
	__int64				copied_size;		// amount of original data included (total size - sparsed data if no errors occur)
	__int64				error_point;		// number of source bytes processed before error or EOF occurred
	unsigned long		error_bytes;		// number of bytes copied beyond error point
	unsigned short		signature_length;	// length of name signature, without null terminating character

	// Here follows the null-terminated ANSI name of the application
	// that generated the sparsed file.

};

void CreateSparseAVI(const char *pszIn, const char *pszOut) {
	HANDLE h1 = INVALID_HANDLE_VALUE;
	HANDLE h2 = INVALID_HANDLE_VALUE;
	
	try {
		h1 = CreateFile(pszIn, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (h1 == INVALID_HANDLE_VALUE)
			throw GetLastError();

		h2 = CreateFile(pszOut, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

		if (h2 == INVALID_HANDLE_VALUE)
			throw GetLastError();

		File64 infile(h1, NULL);
		File64 outfile(h2, NULL);

		// Generate header

		char buf[4096]={0};
		SparseAVIHeader spah;
		__int64 insize = infile._sizeFile();
		unsigned long ckidbuf[2][256];
		int ckidcount = 0;
		__int64 ckidpos = 0;

		int l = sprintf(buf, "VirtualDub build %d/%s", version_num,
#ifdef _DEBUG
		"debug"
#else
		"release"
#endif
				);

		spah.ckid				= SparseAVIHeader::kChunkID;
		spah.size				= SparseAVIHeader::kChunkSize;
		spah.original_size		= insize;
		spah.copied_size		= 0;		// we will fill this in later
		spah.error_point		= 0;		// we will fill this in later
		spah.error_bytes		= 0;		// we will fill this in later
		spah.signature_length	= l;

		spah.size += l+1;

		outfile._writeFile2(&spah, 8 + SparseAVIHeader::kChunkSize);
		outfile._writeFile2(buf, (l+2)&~1);

		__int64 copy_start = outfile._sizeFile();

		// Sparse the AVI file!

		ProgressDialog pd(NULL, "Creating sparse AVI", "Processing source file", (long)((insize+1023)>>10), true);
		pd.setValueFormat("%ldK of %ldK");

		for(;;) {
			__int64 pos = infile._posFile();
			FOURCC fcc;
			DWORD dwLen;

			pd.advance((long)(pos>>10));
			pd.check();

			if (!infile._readChunkHeader(fcc, dwLen) || !isprint((unsigned char)(fcc>>24))
					|| !isprint((unsigned char)(fcc>>16))
					|| !isprint((unsigned char)(fcc>> 8))
					|| !isprint((unsigned char)(fcc    ))
					|| pos+dwLen+8 > insize) {

				if (ckidcount) {
					__int64 pos2 = outfile._posFile();

					outfile._seekFile2(ckidpos);
					outfile._writeFile2(ckidbuf, sizeof ckidbuf);
					outfile._seekFile2(pos2);
				}

				spah.copied_size = outfile._posFile() - copy_start;
				spah.error_point = pos;

				if (infile._posFile() < insize) {
					// error condition

					infile._seekFile2(pos);

					long actual = infile._readFile(buf, sizeof buf);

					if (actual > 0) {
						spah.error_bytes = actual;

						outfile._writeFile2(buf, actual);
					}
				}

				break;
			}

			if (!ckidcount) {
				ckidpos = outfile._posFile();

				memset(ckidbuf, 0, sizeof ckidbuf);
				outfile._writeFile2(ckidbuf, sizeof ckidbuf);
			}

			ckidbuf[0][ckidcount] = fcc;
			ckidbuf[1][ckidcount] = dwLen;

			if (++ckidcount >= 256) {
				__int64 pos2 = outfile._posFile();

				outfile._seekFile2(ckidpos);
				outfile._writeFile2(ckidbuf, sizeof ckidbuf);
				outfile._seekFile2(pos2);
				ckidcount = 0;
			}

			// Sparse any chunk that is of the form ##xx, or that are padding.

			if (fcc=='KNUJ' || (isdigit((unsigned char)fcc) && isdigit((unsigned char)(fcc>>8)))) {
				infile._skipFile2(dwLen+(dwLen&1));
				continue;
			}

			// Break into LIST and RIFF chunks.

			if (fcc == 'TSIL' || fcc == 'FFIR') {
				infile._readFile2(&fcc, 4);
				outfile._writeFile2(&fcc, 4);
				continue;

			}

			// Not sparsing, copy it.  Difference in 16-byte chunks at a time for better
			// compression on index blocks.

			char diffbuf[16]={0};
			int diffoffset = 0;

			dwLen = (dwLen+1)&~1;

			while(dwLen > 0) {
				unsigned long tc = 4096 - ((int)outfile._posFile()&4095);

				if (tc > dwLen)
					tc = dwLen;

				infile._readFile2(buf, tc);

				for(int i=0; i<tc; ++i) {
					char c = diffbuf[diffoffset];
					char d = buf[i];

					buf[i] = d - c;
					diffbuf[diffoffset] = d;
					diffoffset = (diffoffset+1)&15;
				}

				outfile._writeFile2(buf, tc);

				dwLen -= tc;
			}
		}

		// Rewrite the sparse file header.

		outfile._seekFile2(0);
		outfile._writeFile2(&spah, SparseAVIHeader::kChunkSize);

		CloseHandle(h1);
		CloseHandle(h2);

	} catch(DWORD err) {
		if (h1 != INVALID_HANDLE_VALUE)
			CloseHandle(h1);

		if (h2 != INVALID_HANDLE_VALUE)
			CloseHandle(h2);

		throw MyWin32Error("Error creating sparse AVI: %%s", err);
	} catch(const MyError&) {
		if (h1 != INVALID_HANDLE_VALUE)
			CloseHandle(h1);

		if (h2 != INVALID_HANDLE_VALUE)
			CloseHandle(h2);

		throw;
	}
}

void ExpandSparseAVI(HWND hwndParent, const char *pszIn, const char *pszOut) {
	HANDLE h1 = INVALID_HANDLE_VALUE;
	HANDLE h2 = INVALID_HANDLE_VALUE;
	
	try {
		h1 = CreateFile(pszIn, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (h1 == INVALID_HANDLE_VALUE)
			throw GetLastError();

		h2 = CreateFile(pszOut, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

		if (h2 == INVALID_HANDLE_VALUE)
			throw GetLastError();

		File64 infile(h1, NULL);
		File64 outfile(h2, NULL);

		// Read header

		char buf[4096]={0};
		SparseAVIHeader spah;
		__int64 insize = infile._sizeFile();
		unsigned long ckidbuf[2][256];
		int ckidcount = 0;
		__int64 ckidpos = 0;

		infile._readFile2(&spah, 8);
		if (spah.size < SparseAVIHeader::kChunkSize)
			throw MyError("Invalid sparse header.");

		infile._readFile2(&spah.original_size, SparseAVIHeader::kChunkSize);
		infile._skipFile2((spah.size - SparseAVIHeader::kChunkSize + 1)&~1);

		// Expand the sparse AVI file.

		{
			ProgressDialog pd(NULL, "Expanding sparse AVI", "Writing output file", (long)((spah.error_point + spah.error_bytes +1023)>>10), true);
			pd.setValueFormat("%ldK of %ldK");

			while(outfile._posFile() < spah.error_point) {
				FOURCC fcc;
				DWORD dwLen;

				pd.advance((long)(outfile._posFile()>>10));
				pd.check();

				if (!ckidcount)
					infile._readFile2(ckidbuf, sizeof ckidbuf);

				fcc = ckidbuf[0][ckidcount];
				dwLen = ckidbuf[1][ckidcount];
				ckidcount = (ckidcount+1)&255;

				outfile._writeFile2(&fcc, 4);
				outfile._writeFile2(&dwLen, 4);

				// Sparse any chunk that is of the form ##xx, or that are padding.

				if (fcc=='KNUJ' || (isdigit((unsigned char)fcc) && isdigit((unsigned char)(fcc>>8)))) {
					memset(buf, 0, sizeof buf);

					dwLen = (dwLen+1)&~1;

					while(dwLen > 0) {
						unsigned long tc = sizeof buf;
						if (tc > dwLen)
							tc = dwLen;

						outfile._writeFile2(buf, tc);
						dwLen -= tc;
					}

					continue;
				}

				// Break into LIST and RIFF chunks.

				if (fcc == 'TSIL' || fcc == 'FFIR') {
					infile._readFile2(&fcc, 4);
					outfile._writeFile2(&fcc, 4);
					continue;

				}

				// Not sparsed, copy it

				char diffbuf[16]={0};
				int diffoffset = 0;

				dwLen = (dwLen+1)&~1;

				while(dwLen > 0) {
					unsigned long tc = 4096 - ((int)outfile._posFile()&4095);

					if (tc > dwLen)
						tc = dwLen;

					infile._readFile2(buf, tc);

					for(int i=0; i<tc; ++i) {
						buf[i] = diffbuf[diffoffset] += buf[i];
						diffoffset = (diffoffset+1)&15;
					}

					outfile._writeFile2(buf, tc);

					dwLen -= tc;
				}
			}

			// Copy over the error bytes.

			unsigned long total = spah.error_bytes;

			while(total>0) {
				unsigned long tc = sizeof buf;

				if (tc > total)
					tc = total;

				infile._readFile2(buf, tc);
				outfile._writeFile2(buf, tc);

				total -= tc;
			}

			CloseHandle(h1);
			CloseHandle(h2);
		}

		guiMessageBoxF(hwndParent, "VirtualDub notice", MB_OK|MB_ICONINFORMATION,
			"Sparse file details:\n"
			"\n"
			"Original file size: %I64d\n"
			"Copied bytes: %I64d\n"
			"Error location: %I64d\n"
			"Error bytes: %ld\n"
			,spah.original_size
			,spah.copied_size
			,spah.error_point
			,spah.error_bytes);

	} catch(DWORD err) {
		if (h1 != INVALID_HANDLE_VALUE)
			CloseHandle(h1);

		if (h2 != INVALID_HANDLE_VALUE)
			CloseHandle(h2);

		throw MyWin32Error("Error creating sparse AVI: %%s", err);
	} catch(const MyError&) {
		if (h1 != INVALID_HANDLE_VALUE)
			CloseHandle(h1);

		if (h2 != INVALID_HANDLE_VALUE)
			CloseHandle(h2);

		throw;
	}
}

void CreateExtractSparseAVI(HWND hwndParent, bool bExtract) {
	char szFile1[512]={0};
	char szFile2[512]={0};
	OPENFILENAME ofn;

	static const char avifilter[]="Audio-video interleave (*.avi)\0*.avi\0All files (*.*)\0*.*\0";
	static const char sparsefilter[]="Sparsed AVI file (*.sparse)\0*.sparse\0";

	ofn.lStructSize			= sizeof(OPENFILENAME);
	ofn.hwndOwner			= hwndParent;
	ofn.lpstrFilter			= bExtract ? sparsefilter : avifilter;
	ofn.lpstrCustomFilter	= NULL;
	ofn.nFilterIndex		= 1;
	ofn.lpstrFile			= szFile1;
	ofn.nMaxFile			= sizeof szFile1;
	ofn.lpstrFileTitle		= NULL;
	ofn.nMaxFileTitle		= 0;
	ofn.lpstrInitialDir		= NULL;
	ofn.lpstrTitle			= bExtract ? "Select sparse AVI file" : "Select source AVI file";
	ofn.Flags				= OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLESIZING;
	ofn.lpstrDefExt			= bExtract ? "sparse" : "avi";

	if (GetOpenFileName(&ofn)) {
		ofn.lpstrFilter			= bExtract ? avifilter : sparsefilter;
		ofn.lpstrFile			= szFile2;
		ofn.nMaxFile			= sizeof szFile2;
		ofn.lpstrTitle			= "Select filename for output";
		ofn.Flags				= OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLESIZING | OFN_OVERWRITEPROMPT;
		ofn.lpstrDefExt			= bExtract ? "avi" : "sparseavi";

		if (GetSaveFileName(&ofn)) {
			try {
				if (bExtract)
					ExpandSparseAVI(hwndParent, szFile1, szFile2);
				else
					CreateSparseAVI(szFile1, szFile2);
				MessageBox(hwndParent, bExtract ? "Sparse AVI expansion complete." : "Sparse AVI creation complete.", "VirtualDub notice", MB_ICONINFORMATION);
			} catch(const MyError& e) {
				e.post(hwndParent, g_szError);
			}
		}
	}
}


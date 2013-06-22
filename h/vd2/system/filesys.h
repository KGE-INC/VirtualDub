#ifndef f_VD2_SYSTEM_FILESYS_H
#define f_VD2_SYSTEM_FILESYS_H

#include <wchar.h>

#include <vd2/system/vdtypes.h>
#include <vd2/system/VDString.h>

// VDFileSplitPath returns a pointer to the first character of the filename,
// or the beginning of the string if the path only contains one component.

const char *VDFileSplitPath(const char *);
const wchar_t *VDFileSplitPath(const wchar_t *);

static inline char *VDFileSplitPath(char *s) {
	return const_cast<char *>(VDFileSplitPath(const_cast<const char *>(s)));
}

static inline wchar_t *VDFileSplitPath(wchar_t *s) {
	return const_cast<wchar_t *>(VDFileSplitPath(const_cast<const wchar_t *>(s)));
}

VDString VDFileSplitPathLeft(const VDString&);
VDString VDFileSplitPathRight(const VDString&);
VDStringW VDFileSplitPathLeft(const VDStringW&);
VDStringW VDFileSplitPathRight(const VDStringW&);

// VDSplitRoot returns a pointer to the second component of the filename,
// or the beginning of the string if there is no second component.

const char *VDFileSplitRoot(const char *);
const wchar_t *VDFileSplitRoot(const wchar_t *);

static inline char *VDFileSplitRoot(char *s) {
	return const_cast<char *>(VDFileSplitRoot(const_cast<const char *>(s)));
}

static inline wchar_t *VDFileSplitRoot(wchar_t *s) {
	return const_cast<wchar_t *>(VDFileSplitRoot(const_cast<const wchar_t *>(s)));
}

VDString VDFileSplitRoot(const VDString&);
VDStringW VDFileSplitRoot(const VDStringW&);

// VDSplitExtension returns a pointer to the extension, including the period.
// The ending null terminator is returned if there is no extension.

const char *VDFileSplitExt(const char *);
const wchar_t *VDFileSplitExt(const wchar_t *);

static inline char *VDFileSplitExt(char *s) {
	return const_cast<char *>(VDFileSplitExt(const_cast<const char *>(s)));
}

static inline wchar_t *VDFileSplitExt(wchar_t *s) {
	return const_cast<wchar_t *>(VDFileSplitExt(const_cast<const wchar_t *>(s)));
}

VDString VDFileSplitExtLeft(const VDString&);
VDStringW VDFileSplitExtLeft(const VDStringW&);
VDString VDFileSplitExtRight(const VDString&);
VDStringW VDFileSplitExtRight(const VDStringW&);

/////////////////////////////////////////////////////////////////////////////

sint64 VDGetDiskFreeSpace(const VDStringW& path);
void VDCreateDirectory(const VDStringW& path);
bool VDDoesPathExist(const VDStringW& fileName);

VDStringW VDGetFullPath(const VDStringW& partialPath);

VDStringW VDMakePath(const VDStringW& base, const VDStringW& file);

#endif

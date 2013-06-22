#ifndef f_VD2_DITA_SERVICES_H
#define f_VD2_DITA_SERVICES_H

#include <wchar.h>

#include <vd2/system/vdtypes.h>
#include <vd2/system/VDString.h>

// editor

#define	VDFSPECKEY_LOADVIDEOFILE	101
#define	VDFSPECKEY_SAVEVIDEOFILE	102
#define	VDFSPECKEY_LOADAUDIOFILE	103
#define	VDFSPECKEY_SAVEAUDIOFILE	104

// capture

#define	VDFSPECKEY_CAPTURENAME		201

// configuration

#define	VDFSPECKEY_LOADPLUGIN		301
#define VDFSPECKEY_CONFIG			302

// tools

#define	VDFSPECKEY_HEXEDITOR		401
#define VDFSPECKEY_SCRIPT			402

class IVDUIContext;

IVDUIContext *VDGetUIContext();

const VDStringW VDGetLoadFileName			(long nKey, VDGUIHandle ctxParent, const wchar_t *pszTitle, const wchar_t *pszFilters, const wchar_t *pszExt);
const VDStringW VDGetLoadFileNameReadOnly	(long nKey, VDGUIHandle ctxParent, const wchar_t *pszTitle, const wchar_t *pszFilters, const wchar_t *pszExt, bool& bReadOnly);
const VDStringW VDGetSaveFileName			(long nKey, VDGUIHandle ctxParent, const wchar_t *pszTitle, const wchar_t *pszFilters, const wchar_t *pszExt);
const VDStringW VDGetDirectory(long nKey, VDGUIHandle ctxParent, const wchar_t *pszTitle);

#endif

#ifndef f_VD2_DITA_RESOURCES_H
#define f_VD2_DITA_RESOURCES_H

bool VDLoadResources(int moduleID, const void *src, int length);
void VDUnloadResources(int moduleID);
const wchar_t *VDLoadString(int moduleID, int table, int id);
const wchar_t *VDTryLoadString(int moduleID, int table, int id);
const unsigned char *VDLoadDialog(int moduleID, int id);
const unsigned char *VDLoadTemplate(int moduleID, int id);

#endif

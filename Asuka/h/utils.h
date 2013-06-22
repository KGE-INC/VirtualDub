#ifndef f_ASUKA_UTILS_H
#define f_ASUKA_UTILS_H

#include <vd2/system/vdtypes.h>
#include <string>

void VDNORETURN help();
void VDNORETURN fail(const char *format, ...);

void canonicalize_name(std::string& name);
std::string get_name();
int get_version();
bool read_version();
void inc_version();
bool write_version();

#endif

#ifndef f_HELP_H
#define f_HELP_H

#include <windows.h>

void HelpSetPath();
void HelpShowHelp(HWND hwnd);
void HelpPopup(HWND hwnd, DWORD helpID);
void HelpPopupByID(HWND hwnd, DWORD ctrlID, DWORD *lookup);

#endif

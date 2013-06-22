# Microsoft Developer Studio Project File - Name="Dita" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=Dita - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Dita.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Dita.mak" CFG="Dita - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Dita - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "Dita - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "Dita - Win32 Release ICL" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "Dita"
# PROP Scc_LocalPath ".."
CPP=xicl6.exe
RSC=rc.exe

!IF  "$(CFG)" == "Dita - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\lib\Release"
# PROP Intermediate_Dir "..\obj\Release\Dita"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /Zi /Oa /Og /Oi /Os /Oy /Ob1 /Gy /I "h" /I "win32" /I "..\h" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "WIN32_LEAN_AND_MEAN" /D "WIN32" /D "NOMINMAX" /YX /FD /GF /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=xilink6.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "Dita - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\lib\Debug"
# PROP Intermediate_Dir "..\obj\Debug\Dita"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "win32" /I "h" /I "..\h" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "WIN32_LEAN_AND_MEAN" /D "WIN32" /D "NOMINMAX" /YX /FD /GZ /GF /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=xilink6.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "Dita - Win32 Release ICL"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Dita___Win32_Release_ICL"
# PROP BASE Intermediate_Dir "Dita___Win32_Release_ICL"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\lib\ReleaseICL"
# PROP Intermediate_Dir "..\obj\ReleaseICL\Dita"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /Zi /Oa /Og /Oi /Os /Oy /Ob1 /Gy /I "h" /I "win32" /I "..\h" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "WIN32_LEAN_AND_MEAN" /D "WIN32" /D "NOMINMAX" /YX /FD /GF /c
# ADD CPP /nologo /MT /W3 /GX /Zi /Oa /Og /Oi /Os /Oy /Ob1 /Gy /I "h" /I "win32" /I "..\h" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "WIN32_LEAN_AND_MEAN" /D "WIN32" /D "NOMINMAX" /D "_USE_NON_INTEL_COMPILER" /YX /FD /GF /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=xilink6.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "Dita - Win32 Release"
# Name "Dita - Win32 Debug"
# Name "Dita - Win32 Release ICL"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\source\bytecode.cpp
# End Source File
# Begin Source File

SOURCE=.\source\constructor.cpp
# End Source File
# Begin Source File

SOURCE=.\source\control.cpp
# End Source File
# Begin Source File

SOURCE=.\source\ctl_file.cpp
# End Source File
# Begin Source File

SOURCE=.\source\ctl_grid.cpp
# End Source File
# Begin Source File

SOURCE=.\source\ctl_set.cpp
# End Source File
# Begin Source File

SOURCE=.\source\interface.cpp
# End Source File
# Begin Source File

SOURCE=.\source\resources.cpp
# End Source File
# Begin Source File

SOURCE=.\source\services.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\h\VD2\Dita\bytecode.h
# End Source File
# Begin Source File

SOURCE=.\h\control.h
# End Source File
# Begin Source File

SOURCE=.\h\ctl_file.h
# End Source File
# Begin Source File

SOURCE=.\h\ctl_grid.h
# End Source File
# Begin Source File

SOURCE=.\h\ctl_set.h
# End Source File
# Begin Source File

SOURCE=..\h\VD2\Dita\interface.h
# End Source File
# Begin Source File

SOURCE=..\h\VD2\Dita\resources.h
# End Source File
# Begin Source File

SOURCE=..\h\VD2\Dita\services.h
# End Source File
# End Group
# Begin Group "Win32 files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\win32\w32button.cpp
# End Source File
# Begin Source File

SOURCE=.\win32\w32button.h
# End Source File
# Begin Source File

SOURCE=.\win32\w32dialog.cpp
# End Source File
# Begin Source File

SOURCE=.\win32\w32dialog.h
# End Source File
# Begin Source File

SOURCE=.\win32\w32edit.cpp
# End Source File
# Begin Source File

SOURCE=.\win32\w32edit.h
# End Source File
# Begin Source File

SOURCE=.\win32\w32group.cpp
# End Source File
# Begin Source File

SOURCE=.\win32\w32group.h
# End Source File
# Begin Source File

SOURCE=.\win32\w32label.cpp
# End Source File
# Begin Source File

SOURCE=.\win32\w32label.h
# End Source File
# Begin Source File

SOURCE=.\win32\w32list.cpp
# End Source File
# Begin Source File

SOURCE=.\win32\w32list.h
# End Source File
# Begin Source File

SOURCE=.\win32\w32peer.cpp
# End Source File
# Begin Source File

SOURCE=.\win32\w32peer.h
# End Source File
# Begin Source File

SOURCE=.\win32\w32trackbar.cpp
# End Source File
# Begin Source File

SOURCE=.\win32\w32trackbar.h
# End Source File
# End Group
# End Target
# End Project

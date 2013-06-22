# Microsoft Developer Studio Project File - Name="system" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=system - Win32 Debug AMD64
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "system.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "system.mak" CFG="system - Win32 Debug AMD64"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "system - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "system - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "system - Win32 Release AMD64" (based on "Win32 (x86) Static Library")
!MESSAGE "system - Win32 Debug AMD64" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "system"
# PROP Scc_LocalPath "."
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "system - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\lib\Release"
# PROP Intermediate_Dir "..\obj\Release\system"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /Zi /O1 /Ob2 /I "..\h" /I ".\h" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "WIN32" /D "NOMINMAX" /D "WIN32_LEAN_AND_MEAN" /YX /FD /GF /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "system - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\lib\Debug"
# PROP Intermediate_Dir "..\obj\Debug\system"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "../h" /I ".\h" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "WIN32" /D "NOMINMAX" /D "WIN32_LEAN_AND_MEAN" /YX /FD /GZ /GF /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "system - Win32 Release AMD64"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "system___Win32_Release_AMD64"
# PROP BASE Intermediate_Dir "system___Win32_Release_AMD64"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\lib\ReleaseAMD64"
# PROP Intermediate_Dir "..\obj\ReleaseAMD64\system"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /Zi /O1 /Ob2 /I "..\h" /I ".\h" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "WIN32" /D "NOMINMAX" /D "WIN32_LEAN_AND_MEAN" /YX /FD /GF /c
# ADD CPP /nologo /MT /W3 /GX /Zi /O1 /Ob2 /I "..\h" /I ".\h" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "WIN32" /D "NOMINMAX" /D "WIN32_LEAN_AND_MEAN" /YX /FD /GF /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "system - Win32 Debug AMD64"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "system___Win32_Debug_AMD64"
# PROP BASE Intermediate_Dir "system___Win32_Debug_AMD64"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\lib\DebugAMD64"
# PROP Intermediate_Dir "..\obj\DebugAMD64\system"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "../h" /I ".\h" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "WIN32" /D "NOMINMAX" /D "WIN32_LEAN_AND_MEAN" /YX /FD /GZ /GF /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "../h" /I ".\h" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "WIN32" /D "NOMINMAX" /D "WIN32_LEAN_AND_MEAN" /YX /FD /GZ /GF /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "system - Win32 Release"
# Name "system - Win32 Debug"
# Name "system - Win32 Release AMD64"
# Name "system - Win32 Debug AMD64"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\source\cpuaccel.cpp
# End Source File
# Begin Source File

SOURCE=.\source\debug.cpp
# End Source File
# Begin Source File

SOURCE=.\source\debugx86.cpp
# End Source File
# Begin Source File

SOURCE=.\source\Error.cpp
# End Source File
# Begin Source File

SOURCE=.\source\file.cpp
# End Source File
# Begin Source File

SOURCE=.\source\fileasync.cpp
# End Source File
# Begin Source File

SOURCE=.\source\filesys.cpp
# End Source File
# Begin Source File

SOURCE=.\source\Fraction.cpp
# End Source File
# Begin Source File

SOURCE=.\source\int128.cpp
# End Source File
# Begin Source File

SOURCE=.\source\list.cpp
# End Source File
# Begin Source File

SOURCE=.\source\log.cpp
# End Source File
# Begin Source File

SOURCE=.\source\math.cpp
# End Source File
# Begin Source File

SOURCE=.\source\memory.cpp
# End Source File
# Begin Source File

SOURCE=.\source\profile.cpp
# End Source File
# Begin Source File

SOURCE=.\source\progress.cpp
# End Source File
# Begin Source File

SOURCE=.\source\registry.cpp
# End Source File
# Begin Source File

SOURCE=.\source\strutil.cpp
# End Source File
# Begin Source File

SOURCE=.\source\text.cpp
# End Source File
# Begin Source File

SOURCE=.\source\thread.cpp
# End Source File
# Begin Source File

SOURCE=.\source\time.cpp
# End Source File
# Begin Source File

SOURCE=.\source\tls.cpp
# End Source File
# Begin Source File

SOURCE=.\source\VDNamespace.cpp
# End Source File
# Begin Source File

SOURCE=.\source\VDScheduler.cpp
# End Source File
# Begin Source File

SOURCE=.\source\VDString.cpp
# End Source File
# Begin Source File

SOURCE=.\source\vectors.cpp
# End Source File
# Begin Source File

SOURCE=.\source\w32assist.cpp
# End Source File
# Begin Source File

SOURCE=.\source\zip.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\h\vd2\system\atomic.h
# End Source File
# Begin Source File

SOURCE=..\h\VD2\system\cpuaccel.h
# End Source File
# Begin Source File

SOURCE=..\h\vd2\system\debugx86.h
# End Source File
# Begin Source File

SOURCE=..\h\VD2\system\Error.h
# End Source File
# Begin Source File

SOURCE=..\h\vd2\system\file.h
# End Source File
# Begin Source File

SOURCE=..\h\vd2\system\fileasync.h
# End Source File
# Begin Source File

SOURCE=..\h\VD2\system\filesys.h
# End Source File
# Begin Source File

SOURCE=..\h\VD2\system\Fraction.h
# End Source File
# Begin Source File

SOURCE=..\h\VD2\system\int128.h
# End Source File
# Begin Source File

SOURCE=..\h\VD2\system\list.h
# End Source File
# Begin Source File

SOURCE=..\h\vd2\system\log.h
# End Source File
# Begin Source File

SOURCE=..\h\vd2\system\math.h
# End Source File
# Begin Source File

SOURCE=..\h\VD2\system\memory.h
# End Source File
# Begin Source File

SOURCE=..\h\vd2\system\profile.h
# End Source File
# Begin Source File

SOURCE=..\h\VD2\system\progress.h
# End Source File
# Begin Source File

SOURCE=..\h\vd2\system\refcount.h
# End Source File
# Begin Source File

SOURCE=..\h\VD2\system\registry.h
# End Source File
# Begin Source File

SOURCE=..\h\VD2\system\strutil.h
# End Source File
# Begin Source File

SOURCE=..\h\VD2\system\text.h
# End Source File
# Begin Source File

SOURCE=..\h\VD2\system\thread.h
# End Source File
# Begin Source File

SOURCE=..\h\vd2\system\time.h
# End Source File
# Begin Source File

SOURCE=..\h\VD2\system\tls.h
# End Source File
# Begin Source File

SOURCE=..\h\vd2\system\unknown.h
# End Source File
# Begin Source File

SOURCE=..\h\VD2\system\vdalloc.h
# End Source File
# Begin Source File

SOURCE=..\h\VD2\system\VDNamespace.h
# End Source File
# Begin Source File

SOURCE=..\h\VD2\system\VDQueue.h
# End Source File
# Begin Source File

SOURCE=..\h\VD2\system\VDRingBuffer.h
# End Source File
# Begin Source File

SOURCE=..\h\VD2\system\VDScheduler.h
# End Source File
# Begin Source File

SOURCE=..\h\vd2\system\vdstl.h
# End Source File
# Begin Source File

SOURCE=..\h\VD2\system\VDString.h
# End Source File
# Begin Source File

SOURCE=..\h\VD2\system\vdtypes.h
# End Source File
# Begin Source File

SOURCE=..\h\vd2\system\vectors.h
# End Source File
# Begin Source File

SOURCE=..\h\vd2\system\w32assist.h
# End Source File
# Begin Source File

SOURCE=..\h\vd2\system\zip.h
# End Source File
# End Group
# Begin Group "Assembly Files (x86)"

# PROP Default_Filter "asm"
# Begin Source File

SOURCE=.\source\a_memory.asm

!IF  "$(CFG)" == "system - Win32 Release"

# Begin Custom Build
IntDir=.\..\obj\Release\system
InputPath=.\source\a_memory.asm
InputName=a_memory

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "system - Win32 Debug"

# Begin Custom Build
IntDir=.\..\obj\Debug\system
InputPath=.\source\a_memory.asm
InputName=a_memory

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "system - Win32 Release AMD64"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "system - Win32 Debug AMD64"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# End Group
# Begin Group "Assembly Files (AMD64)"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\a64_int128.asm

!IF  "$(CFG)" == "system - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "system - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "system - Win32 Release AMD64"

# Begin Custom Build
IntDir=.\..\obj\ReleaseAMD64\system
InputPath=.\source\a64_int128.asm
InputName=a64_int128

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml64 /nologo /c /Zi /Fo"$(IntDir)\$(InputName).obj" "$(InputPath)"

# End Custom Build

!ELSEIF  "$(CFG)" == "system - Win32 Debug AMD64"

# PROP BASE Exclude_From_Build 1
# Begin Custom Build
IntDir=.\..\obj\DebugAMD64\system
InputPath=.\source\a64_int128.asm
InputName=a64_int128

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml64 /nologo /c /Zi /Fo"$(IntDir)\$(InputName).obj" "$(InputPath)"

# End Custom Build

!ENDIF 

# End Source File
# End Group
# Begin Group "Precompiled Header Support"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\stdafx.cpp

!IF  "$(CFG)" == "system - Win32 Release"

# ADD CPP /Yc"stdafx.h"

!ELSEIF  "$(CFG)" == "system - Win32 Debug"

!ELSEIF  "$(CFG)" == "system - Win32 Release AMD64"

!ELSEIF  "$(CFG)" == "system - Win32 Debug AMD64"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\h\stdafx.h
# End Source File
# End Group
# End Target
# End Project

# Microsoft Developer Studio Project File - Name="Priss" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=Priss - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Priss.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Priss.mak" CFG="Priss - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Priss - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "Priss - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "Priss - Win32 Release ICL" (based on "Win32 (x86) Static Library")
!MESSAGE "Priss - Win32 Release AMD64" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "Priss"
# PROP Scc_LocalPath "."
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Priss - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\lib\Release"
# PROP Intermediate_Dir "..\obj\Release\Priss"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /Zi /Ox /Ot /Oa /Og /Oi /Ob2 /I "h" /I "..\h" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "WIN32_LEAN_AND_MEAN" /YX /FD /GF /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "Priss - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\lib\Debug"
# PROP Intermediate_Dir "..\obj\Debug\Priss"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /ZI /Od /I "h" /I "..\h" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "WIN32_LEAN_AND_MEAN" /YX /FD /GZ /GF /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "Priss - Win32 Release ICL"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Priss___Win32_Release_ICL"
# PROP BASE Intermediate_Dir "Priss___Win32_Release_ICL"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\lib\ReleaseICL"
# PROP Intermediate_Dir "..\obj\ReleaseICL\Priss"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Zi /Ox /Ot /Oa /Og /Oi /Ob2 /I "h" /I "..\h" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "WIN32_LEAN_AND_MEAN" /YX /FD /GF /c
# ADD CPP /nologo /MT /W3 /Zi /Ox /Ot /Oa /Og /Oi /Ob2 /I "h" /I "..\h" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "WIN32_LEAN_AND_MEAN" /D "_USE_INTEL_COMPILER" /YX /FD /GF /QxW /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "Priss - Win32 Release AMD64"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Priss___Win32_Release_AMD64"
# PROP BASE Intermediate_Dir "Priss___Win32_Release_AMD64"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\lib\ReleaseAMD64"
# PROP Intermediate_Dir "..\obj\ReleaseAMD64\Priss"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /Zi /Ox /Ot /Oa /Og /Oi /Ob2 /I "h" /I "..\h" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "WIN32_LEAN_AND_MEAN" /YX /FD /GF /c
# ADD CPP /nologo /MT /W3 /Zi /Ox /Ot /Og /Oi /Ob2 /I "h" /I "..\h" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "WIN32_LEAN_AND_MEAN" /YX /FD /GF /c
# SUBTRACT CPP /Oa
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "Priss - Win32 Release"
# Name "Priss - Win32 Debug"
# Name "Priss - Win32 Release ICL"
# Name "Priss - Win32 Release AMD64"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\source\engine.cpp

!IF  "$(CFG)" == "Priss - Win32 Release"

!ELSEIF  "$(CFG)" == "Priss - Win32 Debug"

!ELSEIF  "$(CFG)" == "Priss - Win32 Release ICL"

!ELSEIF  "$(CFG)" == "Priss - Win32 Release AMD64"

# SUBTRACT CPP /Oa

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\layer1.cpp

!IF  "$(CFG)" == "Priss - Win32 Release"

!ELSEIF  "$(CFG)" == "Priss - Win32 Debug"

!ELSEIF  "$(CFG)" == "Priss - Win32 Release ICL"

!ELSEIF  "$(CFG)" == "Priss - Win32 Release AMD64"

# SUBTRACT CPP /Oa

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\layer2.cpp

!IF  "$(CFG)" == "Priss - Win32 Release"

!ELSEIF  "$(CFG)" == "Priss - Win32 Debug"

!ELSEIF  "$(CFG)" == "Priss - Win32 Release ICL"

!ELSEIF  "$(CFG)" == "Priss - Win32 Release AMD64"

# SUBTRACT CPP /Oa

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\layer3.cpp

!IF  "$(CFG)" == "Priss - Win32 Release"

!ELSEIF  "$(CFG)" == "Priss - Win32 Debug"

!ELSEIF  "$(CFG)" == "Priss - Win32 Release ICL"

!ELSEIF  "$(CFG)" == "Priss - Win32 Release AMD64"

# SUBTRACT CPP /Oa

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\layer3tables.cpp

!IF  "$(CFG)" == "Priss - Win32 Release"

!ELSEIF  "$(CFG)" == "Priss - Win32 Debug"

!ELSEIF  "$(CFG)" == "Priss - Win32 Release ICL"

!ELSEIF  "$(CFG)" == "Priss - Win32 Release AMD64"

# SUBTRACT CPP /Oa

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\polyphase.cpp

!IF  "$(CFG)" == "Priss - Win32 Release"

!ELSEIF  "$(CFG)" == "Priss - Win32 Debug"

!ELSEIF  "$(CFG)" == "Priss - Win32 Release ICL"

!ELSEIF  "$(CFG)" == "Priss - Win32 Release AMD64"

# SUBTRACT CPP /Oa

!ENDIF 

# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\h\bitreader.h
# End Source File
# Begin Source File

SOURCE=.\h\engine.h
# End Source File
# Begin Source File

SOURCE=.\h\polyphase.h
# End Source File
# End Group
# Begin Group "Interface Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\h\vd2\Priss\decoder.h
# End Source File
# End Group
# Begin Group "Assembly Files (AMD64)"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\a64_polyphase.asm

!IF  "$(CFG)" == "Priss - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Priss - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Priss - Win32 Release ICL"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Priss - Win32 Release AMD64"

# Begin Custom Build -
IntDir=.\..\obj\ReleaseAMD64\Priss
InputPath=.\source\a64_polyphase.asm
InputName=a64_polyphase

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml64 /nologo /c /Zi /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# End Group
# End Target
# End Project

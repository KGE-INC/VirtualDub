# Microsoft Developer Studio Project File - Name="Kasumi" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=Kasumi - Win32 Debug AMD64
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Kasumi.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Kasumi.mak" CFG="Kasumi - Win32 Debug AMD64"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Kasumi - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "Kasumi - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "Kasumi - Win32 Release AMD64" (based on "Win32 (x86) Static Library")
!MESSAGE "Kasumi - Win32 Debug AMD64" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "Kasumi"
# PROP Scc_LocalPath "."
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Kasumi - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\lib\Release"
# PROP Intermediate_Dir "..\obj\Release\Kasumi"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /G6 /W3 /Gm /GX /Zi /O2 /I "h" /I "..\h" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "WIN32_LEAN_AND_MEAN" /D "NOMINMAX" /YX /FD /GF /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\lib\Debug"
# PROP Intermediate_Dir "..\obj\Debug\Kasumi"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /GZ /c
# ADD CPP /nologo /G6 /MTd /W3 /Gm /GX /Zi /Od /I "h" /I "..\h" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "WIN32_LEAN_AND_MEAN" /D "NOMINMAX" /YX /FD /GZ /GF /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Release AMD64"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Kasumi___Win32_Release_AMD64"
# PROP BASE Intermediate_Dir "Kasumi___Win32_Release_AMD64"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\lib\ReleaseAMD64"
# PROP Intermediate_Dir "..\obj\ReleaseAMD64\Kasumi"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /W3 /Gm /GX /Zi /O2 /I "h" /I "..\h" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "WIN32_LEAN_AND_MEAN" /D "NOMINMAX" /YX /FD /GF /c
# ADD CPP /nologo /G6 /W3 /Gm /GX /Zi /O2 /I "h" /I "..\h" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "WIN32_LEAN_AND_MEAN" /D "NOMINMAX" /YX /FD /GF /GS- /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug AMD64"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Kasumi___Win32_Debug_AMD64"
# PROP BASE Intermediate_Dir "Kasumi___Win32_Debug_AMD64"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\lib\DebugAMD64"
# PROP Intermediate_Dir "..\obj\DebugAMD64\Kasumi"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MTd /W3 /Gm /GX /Zi /Od /I "h" /I "..\h" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "WIN32_LEAN_AND_MEAN" /D "NOMINMAX" /YX /FD /GZ /GF /c
# ADD CPP /nologo /G6 /MTd /W3 /Gm /GX /Zi /Od /I "h" /I "..\h" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "WIN32_LEAN_AND_MEAN" /D "NOMINMAX" /YX /FD /GF /c
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

# Name "Kasumi - Win32 Release"
# Name "Kasumi - Win32 Debug"
# Name "Kasumi - Win32 Release AMD64"
# Name "Kasumi - Win32 Debug AMD64"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\source\blt.cpp

!IF  "$(CFG)" == "Kasumi - Win32 Release"

# ADD CPP /MT

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug"

# ADD CPP /MTd

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Release AMD64"

# ADD BASE CPP /MT
# ADD CPP /GB /MT

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug AMD64"

# ADD BASE CPP /MTd
# ADD CPP /MTd

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\blt_reference.cpp

!IF  "$(CFG)" == "Kasumi - Win32 Release"

# ADD CPP /MT

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug"

# ADD CPP /MTd

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Release AMD64"

# ADD BASE CPP /MT
# ADD CPP /GB /MT

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug AMD64"

# ADD BASE CPP /MTd
# ADD CPP /MTd

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\blt_reference_pal.cpp

!IF  "$(CFG)" == "Kasumi - Win32 Release"

# ADD CPP /MT

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug"

# ADD CPP /MTd

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Release AMD64"

# ADD BASE CPP /MT
# ADD CPP /GB /MT

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug AMD64"

# ADD BASE CPP /MTd
# ADD CPP /MTd

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\blt_reference_rgb.cpp

!IF  "$(CFG)" == "Kasumi - Win32 Release"

# ADD CPP /MT

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug"

# ADD CPP /MTd

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Release AMD64"

# ADD BASE CPP /MT
# ADD CPP /GB /MT

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug AMD64"

# ADD BASE CPP /MTd
# ADD CPP /MTd

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\blt_reference_yuv.cpp

!IF  "$(CFG)" == "Kasumi - Win32 Release"

# ADD CPP /MT

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug"

# ADD CPP /MTd

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Release AMD64"

# ADD BASE CPP /MT
# ADD CPP /GB /MT

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug AMD64"

# ADD BASE CPP /MTd
# ADD CPP /MTd

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\blt_reference_yuv2yuv.cpp

!IF  "$(CFG)" == "Kasumi - Win32 Release"

# ADD CPP /MT

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug"

# ADD CPP /MTd

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Release AMD64"

# ADD CPP /GB /MT

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug AMD64"

# ADD BASE CPP /MTd
# ADD CPP /MTd

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\blt_reference_yuvrev.cpp
# End Source File
# Begin Source File

SOURCE=.\source\blt_spanutils.cpp

!IF  "$(CFG)" == "Kasumi - Win32 Release"

# ADD CPP /MT

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug"

# ADD CPP /MTd

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Release AMD64"

# ADD CPP /GB /MT

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug AMD64"

# ADD BASE CPP /MTd
# ADD CPP /MTd

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\pixmaputils.cpp

!IF  "$(CFG)" == "Kasumi - Win32 Release"

# ADD CPP /MT

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug"

# ADD CPP /MTd

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Release AMD64"

# ADD BASE CPP /MT
# ADD CPP /GB /MT

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug AMD64"

# ADD BASE CPP /MTd
# ADD CPP /MTd

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\resample.cpp
# End Source File
# Begin Source File

SOURCE=.\source\stretchblt_reference.cpp

!IF  "$(CFG)" == "Kasumi - Win32 Release"

# ADD CPP /MT

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug"

# ADD CPP /MTd

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Release AMD64"

# ADD BASE CPP /MT
# ADD CPP /GB /MT

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug AMD64"

# ADD BASE CPP /MTd
# ADD CPP /MTd

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\triblt.cpp

!IF  "$(CFG)" == "Kasumi - Win32 Release"

# ADD CPP /MT

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug"

# ADD CPP /MTd

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Release AMD64"

# ADD CPP /GB /MT

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug AMD64"

# ADD BASE CPP /MTd
# ADD CPP /MTd

!ENDIF 

# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\h\bitutils.h
# End Source File
# Begin Source File

SOURCE=.\h\blt_spanutils.h
# End Source File
# Begin Source File

SOURCE=..\h\vd2\Kasumi\pixmap.h
# End Source File
# Begin Source File

SOURCE=..\h\vd2\Kasumi\pixmapops.h
# End Source File
# Begin Source File

SOURCE=..\h\vd2\Kasumi\pixmaputils.h
# End Source File
# Begin Source File

SOURCE=..\h\vd2\Kasumi\resample.h
# End Source File
# Begin Source File

SOURCE=..\h\vd2\Kasumi\triblt.h
# End Source File
# End Group
# Begin Group "Assembly files (x86)"

# PROP Default_Filter "asm"
# Begin Source File

SOURCE=.\source\a_bltrgb.asm

!IF  "$(CFG)" == "Kasumi - Win32 Release"

# Begin Custom Build
IntDir=.\..\obj\Release\Kasumi
InputPath=.\source\a_bltrgb.asm
InputName=a_bltrgb

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug"

# Begin Custom Build
IntDir=.\..\obj\Debug\Kasumi
InputPath=.\source\a_bltrgb.asm
InputName=a_bltrgb

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Release AMD64"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug AMD64"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_bltrgb2yuv_mmx.asm

!IF  "$(CFG)" == "Kasumi - Win32 Release"

# Begin Custom Build
IntDir=.\..\obj\Release\Kasumi
InputPath=.\source\a_bltrgb2yuv_mmx.asm
InputName=a_bltrgb2yuv_mmx

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug"

# Begin Custom Build
IntDir=.\..\obj\Debug\Kasumi
InputPath=.\source\a_bltrgb2yuv_mmx.asm
InputName=a_bltrgb2yuv_mmx

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Release AMD64"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug AMD64"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_bltrgb_mmx.asm

!IF  "$(CFG)" == "Kasumi - Win32 Release"

# Begin Custom Build
IntDir=.\..\obj\Release\Kasumi
InputPath=.\source\a_bltrgb_mmx.asm
InputName=a_bltrgb_mmx

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug"

# Begin Custom Build
IntDir=.\..\obj\Debug\Kasumi
InputPath=.\source\a_bltrgb_mmx.asm
InputName=a_bltrgb_mmx

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Release AMD64"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug AMD64"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_resample_mmx.asm

!IF  "$(CFG)" == "Kasumi - Win32 Release"

# Begin Custom Build
IntDir=.\..\obj\Release\Kasumi
InputPath=.\source\a_resample_mmx.asm
InputName=a_resample_mmx

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug"

# Begin Custom Build
IntDir=.\..\obj\Debug\Kasumi
InputPath=.\source\a_resample_mmx.asm
InputName=a_resample_mmx

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Release AMD64"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug AMD64"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_stretchrgb_mmx.asm

!IF  "$(CFG)" == "Kasumi - Win32 Release"

# Begin Custom Build
IntDir=.\..\obj\Release\Kasumi
InputPath=.\source\a_stretchrgb_mmx.asm
InputName=a_stretchrgb_mmx

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug"

# Begin Custom Build
IntDir=.\..\obj\Debug\Kasumi
InputPath=.\source\a_stretchrgb_mmx.asm
InputName=a_stretchrgb_mmx

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Release AMD64"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug AMD64"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_stretchrgb_point.asm

!IF  "$(CFG)" == "Kasumi - Win32 Release"

# Begin Custom Build
IntDir=.\..\obj\Release\Kasumi
InputPath=.\source\a_stretchrgb_point.asm
InputName=a_stretchrgb_point

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug"

# Begin Custom Build
IntDir=.\..\obj\Debug\Kasumi
InputPath=.\source\a_stretchrgb_point.asm
InputName=a_stretchrgb_point

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Release AMD64"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug AMD64"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_triblt_mmx.asm

!IF  "$(CFG)" == "Kasumi - Win32 Release"

# Begin Custom Build
IntDir=.\..\obj\Release\Kasumi
InputPath=.\source\a_triblt_mmx.asm
InputName=a_triblt_mmx

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug"

# Begin Custom Build
IntDir=.\..\obj\Debug\Kasumi
InputPath=.\source\a_triblt_mmx.asm
InputName=a_triblt_mmx

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Release AMD64"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug AMD64"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_triblt_scalar.asm

!IF  "$(CFG)" == "Kasumi - Win32 Release"

# Begin Custom Build
IntDir=.\..\obj\Release\Kasumi
InputPath=.\source\a_triblt_scalar.asm
InputName=a_triblt_scalar

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug"

# Begin Custom Build
IntDir=.\..\obj\Debug\Kasumi
InputPath=.\source\a_triblt_scalar.asm
InputName=a_triblt_scalar

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Release AMD64"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug AMD64"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# End Group
# Begin Group "Source Files (x86)"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\blt_x86.cpp

!IF  "$(CFG)" == "Kasumi - Win32 Release"

# ADD CPP /MT

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug"

# ADD CPP /MTd

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Release AMD64"

# PROP Exclude_From_Build 1
# ADD BASE CPP /MT
# ADD CPP /MT

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug AMD64"

# ADD BASE CPP /MTd
# ADD CPP /MTd

!ENDIF 

# End Source File
# End Group
# Begin Group "Assembly files (AMD64)"

# PROP Default_Filter ".asm64"
# Begin Source File

SOURCE=.\source\a64_resample.asm64

!IF  "$(CFG)" == "Kasumi - Win32 Release"

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug"

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Release AMD64"

# Begin Custom Build
IntDir=.\..\obj\ReleaseAMD64\Kasumi
InputPath=.\source\a64_resample.asm64
InputName=a64_resample

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml64 /nologo /c /Zi /Fo"$(IntDir)\$(InputName).obj" "$(InputPath)"

# End Custom Build

!ELSEIF  "$(CFG)" == "Kasumi - Win32 Debug AMD64"

# Begin Custom Build
IntDir=.\..\obj\DebugAMD64\Kasumi
InputPath=.\source\a64_resample.asm64
InputName=a64_resample

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml64 /nologo /c /Zi /Fo"$(IntDir)\$(InputName).obj" "$(InputPath)"

# End Custom Build

!ENDIF 

# End Source File
# End Group
# Begin Source File

SOURCE=.\source\a_triblt.inc
# End Source File
# End Target
# End Project

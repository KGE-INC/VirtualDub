# Microsoft Developer Studio Project File - Name="Meia" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=Meia - Win32 Debug AMD64
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Meia.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Meia.mak" CFG="Meia - Win32 Debug AMD64"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Meia - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "Meia - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "Meia - Win32 Release AMD64" (based on "Win32 (x86) Static Library")
!MESSAGE "Meia - Win32 Debug AMD64" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "Meia"
# PROP Scc_LocalPath ".."
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Meia - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\lib\Release"
# PROP Intermediate_Dir "..\obj\Release\Meia"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /Zi /O2 /I "h" /I "..\h" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "WIN32_LEAN_AND_MEAN" /D "NOMINMAX" /YX /FD /GF /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\lib\Debug"
# PROP Intermediate_Dir "..\obj\Debug\Meia"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "h" /I "..\h" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "WIN32_LEAN_AND_MEAN" /D "NOMINMAX" /YX /FD /GF /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "Meia - Win32 Release AMD64"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Meia___Win32_Release_AMD64"
# PROP BASE Intermediate_Dir "Meia___Win32_Release_AMD64"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\lib\ReleaseAMD64"
# PROP Intermediate_Dir "..\obj\ReleaseAMD64\Meia"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /Zi /O2 /I "h" /I "..\h" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "WIN32_LEAN_AND_MEAN" /D "NOMINMAX" /YX /FD /GF /c
# ADD CPP /nologo /MT /W3 /GX /Zi /O2 /I "h" /I "..\h" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "WIN32_LEAN_AND_MEAN" /D "NOMINMAX" /YX /FD /GF /GS- /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug AMD64"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Meia___Win32_Debug_AMD64"
# PROP BASE Intermediate_Dir "Meia___Win32_Debug_AMD64"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\lib\DebugAMD64"
# PROP Intermediate_Dir "..\obj\DebugAMD64\Meia"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "h" /I "..\h" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "WIN32_LEAN_AND_MEAN" /D "NOMINMAX" /YX /FD /GF /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "h" /I "..\h" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "WIN32_LEAN_AND_MEAN" /D "NOMINMAX" /YX /FD /GF /c
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

# Name "Meia - Win32 Release"
# Name "Meia - Win32 Debug"
# Name "Meia - Win32 Release AMD64"
# Name "Meia - Win32 Debug AMD64"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\source\convert_reference.cpp
# End Source File
# Begin Source File

SOURCE=.\source\decode_dv.cpp
# End Source File
# Begin Source File

SOURCE=.\source\decode_png.cpp
# End Source File
# Begin Source File

SOURCE=.\source\idct_mmx.cpp
# End Source File
# Begin Source File

SOURCE=.\source\idct_reference.cpp
# End Source File
# Begin Source File

SOURCE=.\source\MPEGCache.cpp
# End Source File
# Begin Source File

SOURCE=.\source\MPEGDecoder.cpp
# End Source File
# Begin Source File

SOURCE=.\source\MPEGFile.cpp
# End Source File
# Begin Source File

SOURCE=.\source\predict_reference.cpp
# End Source File
# Begin Source File

SOURCE=.\source\tables.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\h\vd2\Meia\decode_dv.h
# End Source File
# Begin Source File

SOURCE=..\h\vd2\Meia\decode_png.h
# End Source File
# Begin Source File

SOURCE=.\source\idct_scalar_asm.inl
# End Source File
# Begin Source File

SOURCE=.\h\MPEGCache.h
# End Source File
# Begin Source File

SOURCE=.\h\tables.h
# End Source File
# End Group
# Begin Group "Interface Header Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\h\vd2\Meia\MPEGConvert.h
# End Source File
# Begin Source File

SOURCE=..\h\vd2\Meia\MPEGDecoder.h
# End Source File
# Begin Source File

SOURCE=..\h\vd2\Meia\MPEGFile.h
# End Source File
# Begin Source File

SOURCE=..\h\vd2\Meia\MPEGIDCT.h
# End Source File
# Begin Source File

SOURCE=..\h\vd2\Meia\MPEGPredict.h
# End Source File
# End Group
# Begin Group "Assembly Files (x86)"

# PROP Default_Filter ".asm"
# Begin Source File

SOURCE=.\source\a_predict_isse.asm

!IF  "$(CFG)" == "Meia - Win32 Release"

# Begin Custom Build
IntDir=.\..\obj\Release\Meia
InputPath=.\source\a_predict_isse.asm
InputName=a_predict_isse

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /nologo /c /coff /Zi /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug"

# Begin Custom Build
IntDir=.\..\obj\Debug\Meia
InputPath=.\source\a_predict_isse.asm
InputName=a_predict_isse

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /nologo /c /coff /Zi /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Meia - Win32 Release AMD64"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug AMD64"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_predict_mmx.asm

!IF  "$(CFG)" == "Meia - Win32 Release"

# Begin Custom Build
IntDir=.\..\obj\Release\Meia
InputPath=.\source\a_predict_mmx.asm
InputName=a_predict_mmx

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /nologo /c /coff /Zi /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug"

# Begin Custom Build
IntDir=.\..\obj\Debug\Meia
InputPath=.\source\a_predict_mmx.asm
InputName=a_predict_mmx

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /nologo /c /coff /Zi /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Meia - Win32 Release AMD64"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug AMD64"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_predict_scalar.asm

!IF  "$(CFG)" == "Meia - Win32 Release"

# Begin Custom Build
IntDir=.\..\obj\Release\Meia
InputPath=.\source\a_predict_scalar.asm
InputName=a_predict_scalar

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /nologo /c /coff /Zi /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug"

# Begin Custom Build
IntDir=.\..\obj\Debug\Meia
InputPath=.\source\a_predict_scalar.asm
InputName=a_predict_scalar

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /nologo /c /coff /Zi /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Meia - Win32 Release AMD64"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug AMD64"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_predict_sse2.asm

!IF  "$(CFG)" == "Meia - Win32 Release"

# Begin Custom Build
IntDir=.\..\obj\Release\Meia
InputPath=.\source\a_predict_sse2.asm
InputName=a_predict_sse2

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /nologo /c /coff /Zi /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug"

# Begin Custom Build
IntDir=.\..\obj\Debug\Meia
InputPath=.\source\a_predict_sse2.asm
InputName=a_predict_sse2

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /nologo /c /coff /Zi /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Meia - Win32 Release AMD64"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug AMD64"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_yuv2rgb.asm

!IF  "$(CFG)" == "Meia - Win32 Release"

# Begin Custom Build
IntDir=.\..\obj\Release\Meia
InputPath=.\source\a_yuv2rgb.asm
InputName=a_yuv2rgb

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /nologo /c /coff /Zi /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug"

# Begin Custom Build
IntDir=.\..\obj\Debug\Meia
InputPath=.\source\a_yuv2rgb.asm
InputName=a_yuv2rgb

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /nologo /c /coff /Zi /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Meia - Win32 Release AMD64"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug AMD64"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_yuvtable.asm

!IF  "$(CFG)" == "Meia - Win32 Release"

# Begin Custom Build
IntDir=.\..\obj\Release\Meia
InputPath=.\source\a_yuvtable.asm
InputName=a_yuvtable

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /nologo /c /coff /Zi /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug"

# Begin Custom Build
IntDir=.\..\obj\Debug\Meia
InputPath=.\source\a_yuvtable.asm
InputName=a_yuvtable

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /nologo /c /coff /Zi /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Meia - Win32 Release AMD64"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug AMD64"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\mpeg_idct_mmx.asm

!IF  "$(CFG)" == "Meia - Win32 Release"

# Begin Custom Build
IntDir=.\..\obj\Release\Meia
InputPath=.\source\mpeg_idct_mmx.asm
InputName=mpeg_idct_mmx

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /nologo /c /coff /Zi /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug"

# Begin Custom Build
IntDir=.\..\obj\Debug\Meia
InputPath=.\source\mpeg_idct_mmx.asm
InputName=mpeg_idct_mmx

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /nologo /c /coff /Zi /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Meia - Win32 Release AMD64"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug AMD64"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\mpeg_idct_sse2.asm

!IF  "$(CFG)" == "Meia - Win32 Release"

# Begin Custom Build
IntDir=.\..\obj\Release\Meia
InputPath=.\source\mpeg_idct_sse2.asm
InputName=mpeg_idct_sse2

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /nologo /c /coff /Zi /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug"

# Begin Custom Build
IntDir=.\..\obj\Debug\Meia
InputPath=.\source\mpeg_idct_sse2.asm
InputName=mpeg_idct_sse2

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /nologo /c /coff /Zi /Fo"$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Meia - Win32 Release AMD64"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug AMD64"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# End Group
# Begin Group "Source Files - IDCT Test"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\idct_test.cpp

!IF  "$(CFG)" == "Meia - Win32 Release"

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug"

# ADD CPP /O2 /Ob2
# SUBTRACT CPP /YX

!ELSEIF  "$(CFG)" == "Meia - Win32 Release AMD64"

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug AMD64"

# ADD BASE CPP /O2 /Ob2
# SUBTRACT BASE CPP /YX
# ADD CPP /O2 /Ob2
# SUBTRACT CPP /YX

!ENDIF 

# End Source File
# End Group
# Begin Group "Source Files (x86)"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\convert_isse.cpp

!IF  "$(CFG)" == "Meia - Win32 Release"

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug"

!ELSEIF  "$(CFG)" == "Meia - Win32 Release AMD64"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug AMD64"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\convert_mmx.cpp

!IF  "$(CFG)" == "Meia - Win32 Release"

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug"

!ELSEIF  "$(CFG)" == "Meia - Win32 Release AMD64"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug AMD64"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\convert_scalar.cpp

!IF  "$(CFG)" == "Meia - Win32 Release"

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug"

!ELSEIF  "$(CFG)" == "Meia - Win32 Release AMD64"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug AMD64"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\idct_scalar.cpp

!IF  "$(CFG)" == "Meia - Win32 Release"

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug"

!ELSEIF  "$(CFG)" == "Meia - Win32 Release AMD64"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug AMD64"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# End Group
# Begin Group "Assembly Files (AMD64)"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\a64_idct_sse2.asm

!IF  "$(CFG)" == "Meia - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Meia - Win32 Release AMD64"

# Begin Custom Build
IntDir=.\..\obj\ReleaseAMD64\Meia
InputPath=.\source\a64_idct_sse2.asm
InputName=a64_idct_sse2

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml64 /nologo /c /Zi /Fo"$(IntDir)\$(InputName).obj" "$(InputPath)"

# End Custom Build

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug AMD64"

# PROP BASE Exclude_From_Build 1
# Begin Custom Build
IntDir=.\..\obj\DebugAMD64\Meia
InputPath=.\source\a64_idct_sse2.asm
InputName=a64_idct_sse2

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml64 /nologo /c /Zi /Fo"$(IntDir)\$(InputName).obj" "$(InputPath)"

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a64_predict_sse2.asm

!IF  "$(CFG)" == "Meia - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Meia - Win32 Release AMD64"

# Begin Custom Build
IntDir=.\..\obj\ReleaseAMD64\Meia
InputPath=.\source\a64_predict_sse2.asm
InputName=a64_predict_sse2

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml64 /nologo /c /Zi /Fo"$(IntDir)\$(InputName).obj" "$(InputPath)"

# End Custom Build

!ELSEIF  "$(CFG)" == "Meia - Win32 Debug AMD64"

# PROP BASE Exclude_From_Build 1
# Begin Custom Build
IntDir=.\..\obj\DebugAMD64\Meia
InputPath=.\source\a64_predict_sse2.asm
InputName=a64_predict_sse2

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml64 /nologo /c /Zi /Fo"$(IntDir)\$(InputName).obj" "$(InputPath)"

# End Custom Build

!ENDIF 

# End Source File
# End Group
# End Target
# End Project

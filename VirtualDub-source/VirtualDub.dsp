# Microsoft Developer Studio Project File - Name="VirtualDub" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=VirtualDub - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "VirtualDub.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "VirtualDub.mak" CFG="VirtualDub - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "VirtualDub - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "VirtualDub - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "VirtualDub"
# PROP Scc_LocalPath "."
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Virtual0"
# PROP BASE Intermediate_Dir ".\Virtual0"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\Release"
# PROP Intermediate_Dir ".\Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /G5 /MT /W3 /GX /Zi /Ox /Ot /Oa /Og /Oi /I ".\vdsvrlnk" /I "..\..\nekoamp\main" /I "..\..\sylia\main" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "WIN32_LEAN_AND_MEAN" /YX /FD /GF /Zm1000 /c
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG" /d "WIN32_LEAN_AND_MEAN"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib winmm.lib vfw32.lib dxguid.lib msacm32.lib comctl32.lib $(IntDir)\verstub.obj ..\..\NekoAmp\main\release\amplib.lib /nologo /subsystem:windows /map /debug /machine:I386 /nodefaultlib:"libc" /opt:nowin98 /mapinfo:lines
# SUBTRACT LINK32 /pdb:none
# Begin Special Build Tool
IntDir=.\Release
SOURCE="$(InputPath)"
PreLink_Desc=Updating build number information...
PreLink_Cmds=verinc	ml /c /coff /nologo /Fo$(IntDir)\verstub.obj verstub.asm
PostBuild_Desc=Compiling function location database...
PostBuild_Cmds=i:\projwin\mapconv\release\mapconv release\VirtualDub.map release\VirtualDub.dbg
# End Special Build Tool

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\Virtual1"
# PROP BASE Intermediate_Dir ".\Virtual1"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\Debug"
# PROP Intermediate_Dir ".\Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I ".\vdsvrlnk" /I "..\..\nekoamp\main" /I "..\..\sylia\main" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "WIN32_LEAN_AND_MEAN" /YX /FD /GF /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib winmm.lib vfw32.lib dxguid.lib msacm32.lib comctl32.lib $(IntDir)\verstub.obj ..\..\NekoAmp\main\release\amplib.lib /nologo /subsystem:windows /incremental:no /map /debug /machine:I386 /nodefaultlib:"libc" /nodefaultlib:"libcmt"
# SUBTRACT LINK32 /profile /pdb:none /nodefaultlib
# Begin Special Build Tool
IntDir=.\Debug
SOURCE="$(InputPath)"
PreLink_Desc=Updating build number information...
PreLink_Cmds=verinc	ml /c /coff /nologo /Fo$(IntDir)\verstub.obj verstub.asm
PostBuild_Desc=Compiling function location database...
PostBuild_Cmds=i:\projwin\mapconv\release\mapconv debug\VirtualDub.map debug\VirtualDub.dbg
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "VirtualDub - Win32 Release"
# Name "VirtualDub - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\a_bitmap.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Assembling...
IntDir=.\Release
InputPath=.\a_bitmap.asm
InputName=a_bitmap

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build - Assembling...
IntDir=.\Debug
InputPath=.\a_bitmap.asm
InputName=a_bitmap

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\a_convert.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Assembling...
IntDir=.\Release
InputPath=.\a_convert.asm
InputName=a_convert

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build - Assembling...
IntDir=.\Debug
InputPath=.\a_convert.asm
InputName=a_convert

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\a_convertout.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Assembling...
IntDir=.\Release
InputPath=.\a_convertout.asm
InputName=a_convertout

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build - Assembling...
IntDir=.\Debug
InputPath=.\a_convertout.asm
InputName=a_convertout

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\a_histogram.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Assembling...
IntDir=.\Release
InputPath=.\a_histogram.asm
InputName=a_histogram

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build - Assembling...
IntDir=.\Debug
InputPath=.\a_histogram.asm
InputName=a_histogram

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\a_scene.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Assembling...
IntDir=.\Release
InputPath=.\a_scene.asm
InputName=a_scene

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build - Assembling...
IntDir=.\Debug
InputPath=.\a_scene.asm
InputName=a_scene

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\a_yuv422torgb.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Assembling...
IntDir=.\Release
InputPath=.\a_yuv422torgb.asm
InputName=a_yuv422torgb

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build - Assembling...
IntDir=.\Debug
InputPath=.\a_yuv422torgb.asm
InputName=a_yuv422torgb

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\about.cpp
# End Source File
# Begin Source File

SOURCE=.\AsyncBlitter.cpp
# End Source File
# Begin Source File

SOURCE=.\Audio.cpp
# End Source File
# Begin Source File

SOURCE=.\AudioSource.cpp
# End Source File
# Begin Source File

SOURCE=.\AVIAudioOutput.cpp
# End Source File
# Begin Source File

SOURCE=.\AVIIndex.cpp
# End Source File
# Begin Source File

SOURCE=.\AVIOutput.cpp
# End Source File
# Begin Source File

SOURCE=.\AVIOutputImages.cpp
# End Source File
# Begin Source File

SOURCE=.\AVIOutputPreview.cpp
# End Source File
# Begin Source File

SOURCE=.\AVIOutputStriped.cpp
# End Source File
# Begin Source File

SOURCE=.\AVIOutputWAV.cpp
# End Source File
# Begin Source File

SOURCE=.\AVIPipe.cpp
# End Source File
# Begin Source File

SOURCE=.\AVIReadHandler.cpp
# End Source File
# Begin Source File

SOURCE=.\AVIStripeSystem.cpp
# End Source File
# Begin Source File

SOURCE=.\ddrawsup.cpp
# End Source File
# Begin Source File

SOURCE=.\DubSource.cpp
# End Source File
# Begin Source File

SOURCE=.\dynacomp.inc
# End Source File
# Begin Source File

SOURCE=.\DynamicCode.cpp
# End Source File
# Begin Source File

SOURCE=.\e_blur.cpp
# End Source File
# Begin Source File

SOURCE=.\FastReadStream.cpp
# End Source File
# Begin Source File

SOURCE=.\FastWriteStream.cpp
# End Source File
# Begin Source File

SOURCE=.\fht.cpp
# End Source File
# Begin Source File

SOURCE=.\File64.cpp
# End Source File
# Begin Source File

SOURCE=.\FilterSystem.cpp
# End Source File
# Begin Source File

SOURCE=.\FrameSubset.cpp
# End Source File
# Begin Source File

SOURCE=.\Histogram.cpp
# End Source File
# Begin Source File

SOURCE=.\InputFile.cpp
# End Source File
# Begin Source File

SOURCE=.\InputFileImages.cpp
# End Source File
# Begin Source File

SOURCE=.\int128.cpp
# End Source File
# Begin Source File

SOURCE=.\list.cpp
# End Source File
# Begin Source File

SOURCE=.\memcheck.cpp
# End Source File
# Begin Source File

SOURCE=.\MonoBitmap.cpp
# End Source File
# Begin Source File

SOURCE=.\SceneDetector.cpp
# End Source File
# Begin Source File

SOURCE=.\sparseavi.cpp
# End Source File
# Begin Source File

SOURCE=.\VBitmap.cpp
# End Source File
# Begin Source File

SOURCE=.\VideoSource.cpp
# End Source File
# Begin Source File

SOURCE=.\VideoSourceImages.cpp
# End Source File
# Begin Source File

SOURCE=.\VideoTelecineRemover.cpp
# End Source File
# Begin Source File

SOURCE=.\VirtualDub.rc
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=.\AsyncBlitter.h
# End Source File
# Begin Source File

SOURCE=.\Audio.h
# End Source File
# Begin Source File

SOURCE=.\AudioSource.h
# End Source File
# Begin Source File

SOURCE=.\AVIAudioOutput.h
# End Source File
# Begin Source File

SOURCE=.\AVIIndex.h
# End Source File
# Begin Source File

SOURCE=.\AVIOutput.h
# End Source File
# Begin Source File

SOURCE=.\AVIOutputImages.h
# End Source File
# Begin Source File

SOURCE=.\AVIOutputPreview.h
# End Source File
# Begin Source File

SOURCE=.\AVIoutputStriped.h
# End Source File
# Begin Source File

SOURCE=.\AVIOutputWAV.h
# End Source File
# Begin Source File

SOURCE=.\AVIPipe.h
# End Source File
# Begin Source File

SOURCE=.\AVIReadHandler.h
# End Source File
# Begin Source File

SOURCE=.\AVIStripeSystem.h
# End Source File
# Begin Source File

SOURCE=.\Avisynth.h
# End Source File
# Begin Source File

SOURCE=.\convert.h
# End Source File
# Begin Source File

SOURCE=.\ddrawsup.h
# End Source File
# Begin Source File

SOURCE=.\DubSource.h
# End Source File
# Begin Source File

SOURCE=.\DynamicCode.h
# End Source File
# Begin Source File

SOURCE=.\e_blur.h
# End Source File
# Begin Source File

SOURCE=.\effect.h
# End Source File
# Begin Source File

SOURCE=.\f_convolute.h
# End Source File
# Begin Source File

SOURCE=.\FastReadStream.h
# End Source File
# Begin Source File

SOURCE=.\FastWriteStream.h
# End Source File
# Begin Source File

SOURCE=.\fht.h
# End Source File
# Begin Source File

SOURCE=.\File64.h
# End Source File
# Begin Source File

SOURCE=.\filtdlg.h
# End Source File
# Begin Source File

SOURCE=.\filter.h
# End Source File
# Begin Source File

SOURCE=.\FilterSystem.h
# End Source File
# Begin Source File

SOURCE=.\Fixes.h
# End Source File
# Begin Source File

SOURCE=.\FrameSubset.h
# End Source File
# Begin Source File

SOURCE=.\helpcoach.h
# End Source File
# Begin Source File

SOURCE=.\helpfile.h
# End Source File
# Begin Source File

SOURCE=.\Histogram.h
# End Source File
# Begin Source File

SOURCE=.\indeo_if.h
# End Source File
# Begin Source File

SOURCE=.\InputFile.h
# End Source File
# Begin Source File

SOURCE=.\InputFileImages.h
# End Source File
# Begin Source File

SOURCE=.\int128.h
# End Source File
# Begin Source File

SOURCE=.\list.h
# End Source File
# Begin Source File

SOURCE=.\MonoBitmap.h
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\SceneDetector.h
# End Source File
# Begin Source File

SOURCE=.\VBitmap.h
# End Source File
# Begin Source File

SOURCE=.\vector.h
# End Source File
# Begin Source File

SOURCE=.\VideoSource.h
# End Source File
# Begin Source File

SOURCE=.\VideoSourceImages.h
# End Source File
# Begin Source File

SOURCE=.\VideoTelecineRemover.h
# End Source File
# Begin Source File

SOURCE=.\VirtualDub.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;cnt;rtf;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\bitmap1.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00001.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00002.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00003.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00004.bmp
# End Source File
# Begin Source File

SOURCE=.\Changes.bin
# End Source File
# Begin Source File

SOURCE=.\ico00001.ico
# End Source File
# Begin Source File

SOURCE=.\icon1.ico
# End Source File
# Begin Source File

SOURCE=.\license.bin
# End Source File
# Begin Source File

SOURCE=.\pos_back.ico
# End Source File
# Begin Source File

SOURCE=.\pos_end.ico
# End Source File
# Begin Source File

SOURCE=.\pos_mark.ico
# End Source File
# Begin Source File

SOURCE=.\pos_markout.ico
# End Source File
# Begin Source File

SOURCE=.\pos_next.ico
# End Source File
# Begin Source File

SOURCE=.\pos_play.ico
# End Source File
# Begin Source File

SOURCE=.\pos_prev.ico
# End Source File
# Begin Source File

SOURCE=.\pos_star.ico
# End Source File
# Begin Source File

SOURCE=.\pos_stop.ico
# End Source File
# Begin Source File

SOURCE=.\rel_notes.bin
# End Source File
# Begin Source File

SOURCE=.\scenefwd.ico
# End Source File
# Begin Source File

SOURCE=.\scenerev.ico
# End Source File
# Begin Source File

SOURCE=.\toolbar_.bmp
# End Source File
# Begin Source File

SOURCE=.\virtuald.ico
# End Source File
# End Group
# Begin Group "Source - Capture"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\capaccel.cpp
# End Source File
# Begin Source File

SOURCE=.\capaccel.h
# End Source File
# Begin Source File

SOURCE=.\capbt848.cpp
# End Source File
# Begin Source File

SOURCE=.\capclip.cpp
# End Source File
# Begin Source File

SOURCE=.\capclip.h
# End Source File
# Begin Source File

SOURCE=.\caphisto.cpp
# End Source File
# Begin Source File

SOURCE=.\caphisto.h
# End Source File
# Begin Source File

SOURCE=.\caplog.cpp
# End Source File
# Begin Source File

SOURCE=.\caplog.h
# End Source File
# Begin Source File

SOURCE=.\capspill.cpp
# End Source File
# Begin Source File

SOURCE=.\capspill.h
# End Source File
# Begin Source File

SOURCE=.\capture.cpp
# End Source File
# Begin Source File

SOURCE=.\capture.h
# End Source File
# Begin Source File

SOURCE=.\capvumeter.cpp
# End Source File
# Begin Source File

SOURCE=.\capwarn.cpp
# End Source File
# End Group
# Begin Group "Source - Filters"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\a_average.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Assembling...
IntDir=.\Release
InputPath=.\a_average.asm
InputName=a_average

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build - Assembling...
IntDir=.\Debug
InputPath=.\a_average.asm
InputName=a_average

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\a_brightcont.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Assembling...
IntDir=.\Release
InputPath=.\a_brightcont.asm
InputName=a_brightcont

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build - Assembling...
IntDir=.\Debug
InputPath=.\a_brightcont.asm
InputName=a_brightcont

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\a_cmult.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Assembling...
IntDir=.\Release
InputPath=.\a_cmult.asm
InputName=a_cmult

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build - Assembling...
IntDir=.\Debug
InputPath=.\a_cmult.asm
InputName=a_cmult

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\a_convolute.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Assembling...
IntDir=.\Release
InputPath=.\a_convolute.asm
InputName=a_convolute

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build - Assembling...
IntDir=.\Debug
InputPath=.\a_convolute.asm
InputName=a_convolute

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\a_grayscale.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Assembling...
IntDir=.\Release
InputPath=.\a_grayscale.asm
InputName=a_grayscale

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build - Assembling...
IntDir=.\Debug
InputPath=.\a_grayscale.asm
InputName=a_grayscale

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\a_reduce.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Assembling...
IntDir=.\Release
InputPath=.\a_reduce.asm
InputName=a_reduce

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build - Assembling...
IntDir=.\Debug
InputPath=.\a_reduce.asm
InputName=a_reduce

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\A_resize.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Assembling...
IntDir=.\Release
InputPath=.\A_resize.asm
InputName=A_resize

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build - Assembling...
IntDir=.\Debug
InputPath=.\A_resize.asm
InputName=A_resize

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\a_rotate.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Assembling...
IntDir=.\Release
InputPath=.\a_rotate.asm
InputName=a_rotate

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build - Assembling...
IntDir=.\Debug
InputPath=.\a_rotate.asm
InputName=a_rotate

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\a_sharpen.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Assembling...
IntDir=.\Release
InputPath=.\a_sharpen.asm
InputName=a_sharpen

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build - Assembling...
IntDir=.\Debug
InputPath=.\a_sharpen.asm
InputName=a_sharpen

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\a_threshold.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Assembling...
IntDir=.\Release
InputPath=.\a_threshold.asm
InputName=a_threshold

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build - Assembling...
IntDir=.\Debug
InputPath=.\a_threshold.asm
InputName=a_threshold

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\a_tv.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Assembling...
IntDir=.\Release
InputPath=.\a_tv.asm
InputName=a_tv

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build - Assembling...
IntDir=.\Debug
InputPath=.\a_tv.asm
InputName=a_tv

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\f_averag.cpp
# End Source File
# Begin Source File

SOURCE=.\F_blur.cpp
# End Source File
# Begin Source File

SOURCE=.\f_boxblur.cpp
# End Source File
# Begin Source File

SOURCE=.\f_brightcont.cpp
# End Source File
# Begin Source File

SOURCE=.\f_convolute.cpp
# End Source File
# Begin Source File

SOURCE=.\f_deinterlace.cpp
# End Source File
# Begin Source File

SOURCE=.\f_emboss.cpp
# End Source File
# Begin Source File

SOURCE=.\f_fieldbob.cpp
# End Source File
# Begin Source File

SOURCE=.\f_fieldswap.cpp
# End Source File
# Begin Source File

SOURCE=.\f_fill.cpp
# End Source File
# Begin Source File

SOURCE=.\f_flipv.cpp
# End Source File
# Begin Source File

SOURCE=.\f_grayscale.cpp
# End Source File
# Begin Source File

SOURCE=.\f_invert.cpp
# End Source File
# Begin Source File

SOURCE=.\f_levels.cpp
# End Source File
# Begin Source File

SOURCE=.\f_list.cpp
# End Source File
# Begin Source File

SOURCE=.\f_null.cpp
# End Source File
# Begin Source File

SOURCE=.\f_reduce2.cpp
# End Source File
# Begin Source File

SOURCE=.\f_reducehq.cpp
# End Source File
# Begin Source File

SOURCE=.\F_resize.cpp
# End Source File
# Begin Source File

SOURCE=.\f_rotate.cpp
# End Source File
# Begin Source File

SOURCE=.\f_rotate2.cpp
# End Source File
# Begin Source File

SOURCE=.\f_sharpen.cpp
# End Source File
# Begin Source File

SOURCE=.\f_smoother.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ox /Ot /Oa /Og /Oi /Oy

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\f_threshold.cpp
# End Source File
# Begin Source File

SOURCE=.\f_timesmooth.cpp
# End Source File
# Begin Source File

SOURCE=.\f_tsoften.cpp
# End Source File
# Begin Source File

SOURCE=.\f_tv.cpp
# End Source File
# Begin Source File

SOURCE=.\resample.cpp
# End Source File
# Begin Source File

SOURCE=.\resample.h
# End Source File
# End Group
# Begin Group "Source - MPEG/MJPEG support"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\a_mjpgdec.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Assembling...
IntDir=.\Release
InputPath=.\a_mjpgdec.asm
InputName=a_mjpgdec

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build - Assembling...
IntDir=.\Debug
InputPath=.\a_mjpgdec.asm
InputName=a_mjpgdec

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\a_predict.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Assembling...
IntDir=.\Release
InputPath=.\a_predict.asm
InputName=a_predict

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build - Assembling...
IntDir=.\Debug
InputPath=.\a_predict.asm
InputName=a_predict

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\a_predict_isse.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Assembling...
IntDir=.\Release
InputPath=.\a_predict_isse.asm
InputName=a_predict_isse

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build - Assembling...
IntDir=.\Debug
InputPath=.\a_predict_isse.asm
InputName=a_predict_isse

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\a_predict_mmx.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Assembling...
IntDir=.\Release
InputPath=.\a_predict_mmx.asm
InputName=a_predict_mmx

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build - Assembling...
IntDir=.\Debug
InputPath=.\a_predict_mmx.asm
InputName=a_predict_mmx

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\a_yuv2rgb.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Assembling...
IntDir=.\Release
InputPath=.\a_yuv2rgb.asm
InputName=a_yuv2rgb

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build - Assembling...
IntDir=.\Debug
InputPath=.\a_yuv2rgb.asm
InputName=a_yuv2rgb

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\a_yuv2rgbhq.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Assembling...
IntDir=.\Release
InputPath=.\a_yuv2rgbhq.asm
InputName=a_yuv2rgbhq

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build - Assembling...
IntDir=.\Debug
InputPath=.\a_yuv2rgbhq.asm
InputName=a_yuv2rgbhq

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\a_yuvtable.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Assembling...
IntDir=.\Release
InputPath=.\a_yuvtable.asm
InputName=a_yuvtable

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build - Assembling...
IntDir=.\Debug
InputPath=.\a_yuvtable.asm
InputName=a_yuvtable

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\CMemoryBitInput.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ox /Ot /Oa /Og /Oi /Oy

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\MJPEGDecoder.cpp
# End Source File
# Begin Source File

SOURCE=.\Mpeg.cpp
# End Source File
# Begin Source File

SOURCE=.\mpeg_decode.cpp
# End Source File
# Begin Source File

SOURCE=.\mpeg_idct.cpp
# End Source File
# Begin Source File

SOURCE=.\mpeg_idct_mmx.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Assembling...
IntDir=.\Release
InputPath=.\mpeg_idct_mmx.asm
InputName=mpeg_idct_mmx

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build
IntDir=.\Debug
InputPath=.\mpeg_idct_mmx.asm
InputName=mpeg_idct_mmx

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\mpeg_tables.cpp
# End Source File
# End Group
# Begin Group "Headers - MPEG/MJPEG support"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\CMemoryBitInput.h
# End Source File
# Begin Source File

SOURCE=.\mjpeg_color.inl
# End Source File
# Begin Source File

SOURCE=.\MJPEGDecoder.h
# End Source File
# Begin Source File

SOURCE=.\mpeg.h
# End Source File
# Begin Source File

SOURCE=.\mpeg_decode.h
# End Source File
# Begin Source File

SOURCE=.\mpeg_idct.h
# End Source File
# Begin Source File

SOURCE=.\mpeg_tables.h
# End Source File
# End Group
# Begin Group "Text resources"

# PROP Default_Filter ".txt"
# Begin Source File

SOURCE=.\Changes.txt

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Building resource text...
InputPath=.\Changes.txt
InputName=Changes

"$(InputName).bin" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy /b $(InputName).txt+null.bin $(InputName).bin

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build - Building resource text...
InputPath=.\Changes.txt
InputName=Changes

"$(InputName).bin" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy /b $(InputName).txt+null.bin $(InputName).bin

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\credits.txt
# End Source File
# Begin Source File

SOURCE=.\rel_notes.txt

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Building resource text...
InputPath=.\rel_notes.txt
InputName=rel_notes

"$(InputName).bin" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy /b $(InputName).txt+null.bin $(InputName).bin

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build - Building resource text...
InputPath=.\rel_notes.txt
InputName=rel_notes

"$(InputName).bin" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy /b $(InputName).txt+null.bin $(InputName).bin

# End Custom Build

!ENDIF 

# End Source File
# End Group
# Begin Group "Source - coach"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\coach.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Os /Oy-
# SUBTRACT CPP /Ot

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\coach.txt
# End Source File
# End Group
# Begin Group "Source - User Interface"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\ClippingControl.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Os /Oy-
# SUBTRACT CPP /Ot

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\HexViewer.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Oa /Oi /Os /Oy-
# SUBTRACT CPP /Ot /Ow

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\LevelControl.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Os /Oy-
# SUBTRACT CPP /Ot

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\PositionControl.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Os /Oy-
# SUBTRACT CPP /Ot

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\ProgressDialog.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Os /Oy-
# SUBTRACT CPP /Ot

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# End Group
# Begin Group "Headers - User Interface"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\ClippingControl.h
# End Source File
# Begin Source File

SOURCE=.\HexViewer.h
# End Source File
# Begin Source File

SOURCE=.\LevelControl.h
# End Source File
# Begin Source File

SOURCE=.\PositionControl.h
# End Source File
# Begin Source File

SOURCE=.\ProgressDialog.h
# End Source File
# End Group
# Begin Group "Files - YUV codec"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\CVideoCompressor.cpp
# End Source File
# Begin Source File

SOURCE=.\CVideoCompressor.h
# End Source File
# Begin Source File

SOURCE=.\icdriver.cpp
# End Source File
# Begin Source File

SOURCE=.\IVideoCompressor.h
# End Source File
# Begin Source File

SOURCE=.\IVideoDriver.h
# End Source File
# Begin Source File

SOURCE=.\yuvcodec.cpp
# End Source File
# Begin Source File

SOURCE=.\yuvcodec.h
# End Source File
# End Group
# Begin Group "Source - Really not time critical"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\acompchoose.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Oa /Os /Oy- /Ob1
# SUBTRACT CPP /Ox /Ot /Ow

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\autodetect.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Oa /Os /Oy- /Ob1
# SUBTRACT CPP /Ox /Ot /Ow

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\auxdlg.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Oa /Os /Oy- /Ob1
# SUBTRACT CPP /Ox /Ot /Ow

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\Command.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Oa /Os /Oy- /Ob1
# SUBTRACT CPP /Ox /Ot /Ow

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\compchoose.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Oa /Os /Oy- /Ob1
# SUBTRACT CPP /Ox /Ot /Ow

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\cpuaccel.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Oa /Os /Oy- /Ob1
# SUBTRACT CPP /Ox /Ot /Ow

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\Crash.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Oa /Os /Oy- /Ob1
# SUBTRACT CPP /Ox /Ot /Ow

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\Disasm.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Oa /Os /Oy- /Ob1
# SUBTRACT CPP /Ox /Ot /Ow

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\Error.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Oa /Os /Oy- /Ob1
# SUBTRACT CPP /Ox /Ot /Ow

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\Filtdlg.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Oa /Os /Oy- /Ob1
# SUBTRACT CPP /Ox /Ot /Ow

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gui.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Oa /Os /Oy- /Ob1
# SUBTRACT CPP /Ox /Ot /Ow

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\Init.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Oa /Os /Oy- /Ob1
# SUBTRACT CPP /Ox /Ot /Ow

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\Job.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Oa /Os /Oy- /Ob1
# SUBTRACT CPP /Ox /Ot /Ow

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\license.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Oa /Os /Oy- /Ob1
# SUBTRACT CPP /Ox /Ot /Ow

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\Main.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Oa /Os /Oy- /Ob1
# SUBTRACT CPP /Ox /Ot /Ow

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\misc.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Oa /Os /Oy- /Ob1
# SUBTRACT CPP /Ox /Ot /Ow

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\MRUList.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Oa /Os /Oy- /Ob1
# SUBTRACT CPP /Ox /Ot /Ow

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\optdlg.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Oa /Os /Oy- /Ob1
# SUBTRACT CPP /Ox /Ot /Ow

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\prefs.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Oa /Os /Oy- /Ob1
# SUBTRACT CPP /Ox /Ot /Ow

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\Script.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Oa /Os /Oy- /Ob1
# SUBTRACT CPP /Ox /Ot /Ow

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\tls.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Oa /Os /Oy- /Ob1
# SUBTRACT CPP /Ox /Ot /Ow

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# End Group
# Begin Group "Headers - Really not time critical"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\autodetect.h
# End Source File
# Begin Source File

SOURCE=.\auxdlg.h
# End Source File
# Begin Source File

SOURCE=.\command.h
# End Source File
# Begin Source File

SOURCE=.\cpuaccel.h
# End Source File
# Begin Source File

SOURCE=.\crash.h
# End Source File
# Begin Source File

SOURCE=.\disasm.h
# End Source File
# Begin Source File

SOURCE=.\Error.h
# End Source File
# Begin Source File

SOURCE=.\gui.h
# End Source File
# Begin Source File

SOURCE=.\job.h
# End Source File
# Begin Source File

SOURCE=.\misc.h
# End Source File
# Begin Source File

SOURCE=.\MRUList.h
# End Source File
# Begin Source File

SOURCE=.\optdlg.h
# End Source File
# Begin Source File

SOURCE=.\prefs.h
# End Source File
# Begin Source File

SOURCE=.\script.h
# End Source File
# Begin Source File

SOURCE=.\tls.h
# End Source File
# End Group
# Begin Group "dTV headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\dTVdrv.h
# End Source File
# End Group
# Begin Group "Source - Not time critical"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Dub.cpp
# End Source File
# Begin Source File

SOURCE=.\DubStatus.cpp
# End Source File
# Begin Source File

SOURCE=.\Filters.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ob1
# SUBTRACT CPP /Ox /Ot /Ow

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\oshelper.cpp
# End Source File
# Begin Source File

SOURCE=.\server.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ob1
# SUBTRACT CPP /Ox /Ot /Ow

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\VideoSequenceCompressor.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ob1
# SUBTRACT CPP /Ox /Ot /Ow

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\wrappedMMIO.cpp
# End Source File
# End Group
# Begin Group "Headers - Not time critical"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Dub.h
# End Source File
# Begin Source File

SOURCE=.\DubStatus.h
# End Source File
# Begin Source File

SOURCE=.\filters.h
# End Source File
# Begin Source File

SOURCE=.\oshelper.h
# End Source File
# Begin Source File

SOURCE=.\server.h
# End Source File
# Begin Source File

SOURCE=.\VideoSequenceCompressor.h
# End Source File
# Begin Source File

SOURCE=.\wrappedMMIO.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\hello.txt
# End Source File
# Begin Source File

SOURCE=.\dist\readme.doc
# End Source File
# Begin Source File

SOURCE=.\dist\source.doc
# End Source File
# Begin Source File

SOURCE=.\todo.txt
# End Source File
# End Target
# End Project

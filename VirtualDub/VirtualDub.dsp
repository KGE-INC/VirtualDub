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
!MESSAGE "VirtualDub - Win32 Release ICL" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "VirtualDub"
# PROP Scc_LocalPath "."
CPP=xicl6.exe
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
# PROP Output_Dir "../out/Release"
# PROP Intermediate_Dir "../obj/Release/VirtualDub"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /G6 /MT /W3 /GX /Zi /Ox /Ot /Oa /Og /Oi /Ob2 /I ".\h" /I ".\res" /I "..\h" /I "..\vdsvrlnk" /I "..\sylia" /D "NDEBUG" /D "_USE_NON_INTEL_COMPILER" /D "WIN32" /D "_WINDOWS" /D "WIN32_LEAN_AND_MEAN" /D "NOMINMAX" /FD /GF /c
# SUBTRACT CPP /YX /Yc /Yu
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG" /d "WIN32_LEAN_AND_MEAN"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib winmm.lib vfw32.lib dxguid.lib msacm32.lib comctl32.lib $(IntDir)\verstub.obj priss.lib sylia.lib system.lib dita.lib meia.lib /nologo /entry:"VeedubWinMain" /subsystem:windows /map:"../out/Release/VirtualDub.map" /debug /machine:I386 /nodefaultlib:"libc" /libpath:"../lib/Release" /opt:nowin98 /opt:ref /mapinfo:lines
# SUBTRACT LINK32 /pdb:none
# Begin Special Build Tool
IntDir=.\../obj/Release/VirtualDub
SOURCE="$(InputPath)"
PreLink_Desc=Updating build number information...
PreLink_Cmds=..\out\Release\verinc	ml /c /coff /nologo /Fo$(IntDir)\verstub.obj verstub.asm
PostBuild_Desc=Compiling function location database...
PostBuild_Cmds=..\out\Release\mapconv ..\out\release\VirtualDub.map ..\out\release\VirtualDub.vdi res\ia32.vdi
# End Special Build Tool

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\Virtual1"
# PROP BASE Intermediate_Dir ".\Virtual1"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "../out/Debug"
# PROP Intermediate_Dir "../obj/Debug/VirtualDub"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I ".\h" /I ".\res" /I "..\h" /I "..\vdsvrlnk" /I "..\sylia" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "WIN32_LEAN_AND_MEAN" /D "NOMINMAX" /Yu"stdafx.h" /FD /GF /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib winmm.lib vfw32.lib dxguid.lib msacm32.lib comctl32.lib $(IntDir)\verstub.obj priss.lib sylia.lib system.lib dita.lib meia.lib /nologo /subsystem:windows /verbose /map:"../out/Debug/VirtualDub.map" /debug /machine:I386 /nodefaultlib:"libc" /nodefaultlib:"libcmt" /libpath:"../lib/Debug"
# SUBTRACT LINK32 /pdb:none
# Begin Special Build Tool
IntDir=.\../obj/Debug/VirtualDub
SOURCE="$(InputPath)"
PreLink_Desc=Updating build number information...
PreLink_Cmds=..\out\Debug\verinc	ml /c /coff /nologo /Fo$(IntDir)\verstub.obj verstub.asm
PostBuild_Desc=Compiling function location database...
PostBuild_Cmds=..\out\Debug\mapconv ..\out\debug\VirtualDub.map ..\out\debug\VirtualDub.vdi res\ia32.vdi
# End Special Build Tool

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "VirtualDub___Win32_Release_ICL"
# PROP BASE Intermediate_Dir "VirtualDub___Win32_Release_ICL"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\out\ReleaseICL"
# PROP Intermediate_Dir "..\obj\ReleaseICL\VirtualDub"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MT /W3 /GX /Zi /Ox /Ot /Oa /Og /Oi /Ob2 /I ".\h" /I ".\res" /I "..\h" /I "..\vdsvrlnk" /I "..\sylia" /D "NDEBUG" /D "_USE_NON_INTEL_COMPILER" /D "WIN32" /D "_WINDOWS" /D "WIN32_LEAN_AND_MEAN" /D "NOMINMAX" /FD /GF /c
# SUBTRACT BASE CPP /YX /Yc /Yu
# ADD CPP /nologo /G6 /MT /W3 /GX /Zi /Ox /Ot /Oa /Og /Oi /Ob2 /I ".\h" /I ".\res" /I "..\h" /I "..\vdsvrlnk" /I "..\sylia" /D "NDEBUG" /D "_USE_INTEL_COMPILER" /D "WIN32" /D "_WINDOWS" /D "WIN32_LEAN_AND_MEAN" /D "NOMINMAX" /FD /GF /QxW /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG" /d "WIN32_LEAN_AND_MEAN"
# ADD RSC /l 0x409 /d "NDEBUG" /d "WIN32_LEAN_AND_MEAN"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib winmm.lib vfw32.lib dxguid.lib msacm32.lib comctl32.lib $(IntDir)\verstub.obj priss.lib sylia.lib system.lib dita.lib /nologo /entry:"VeedubWinMain" /subsystem:windows /map:"../out/Release/VirtualDub.map" /debug /machine:I386 /nodefaultlib:"libc" /libpath:"../lib/Release" /opt:nowin98 /opt:ref /mapinfo:lines
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib winmm.lib vfw32.lib dxguid.lib msacm32.lib comctl32.lib $(IntDir)\verstub.obj priss.lib sylia.lib system.lib dita.lib meia.lib /nologo /entry:"VeedubWinMain" /subsystem:windows /map:"../out/ReleaseICL/VeedubP4.map" /debug /machine:I386 /nodefaultlib:"libc" /out:"..\out\ReleaseICL/VeedubP4.exe" /libpath:"../lib/ReleaseICL" /opt:nowin98 /opt:ref /mapinfo:lines
# SUBTRACT LINK32 /pdb:none
# Begin Special Build Tool
IntDir=.\..\obj\ReleaseICL\VirtualDub
SOURCE="$(InputPath)"
PreLink_Desc=Updating build number information...
PreLink_Cmds=..\out\Release\verinc	ml /c /coff /nologo /Fo$(IntDir)\verstub.obj verstub.asm
PostBuild_Desc=Compiling function location database...
PostBuild_Cmds=..\out\Release\mapconv ..\out\ReleaseICL\VeedubP4.map ..\out\ReleaseICL\VeedubP4.vdi res\ia32.vdi
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "VirtualDub - Win32 Release"
# Name "VirtualDub - Win32 Debug"
# Name "VirtualDub - Win32 Release ICL"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\source\about.cpp
# End Source File
# Begin Source File

SOURCE=.\source\AsyncBlitter.cpp
# End Source File
# Begin Source File

SOURCE=.\source\Audio.cpp
# End Source File
# Begin Source File

SOURCE=.\source\audioutil.cpp
# End Source File
# Begin Source File

SOURCE=.\source\DynamicCode.cpp
# End Source File
# Begin Source File

SOURCE=.\source\e_blur.cpp
# End Source File
# Begin Source File

SOURCE=.\source\fht.cpp
# End Source File
# Begin Source File

SOURCE=.\source\FrameSubset.cpp
# End Source File
# Begin Source File

SOURCE=.\source\Histogram.cpp
# End Source File
# Begin Source File

SOURCE=.\source\image.cpp
# End Source File
# Begin Source File

SOURCE=.\source\leaks.cpp
# End Source File
# Begin Source File

SOURCE=.\source\plugins.cpp
# End Source File
# Begin Source File

SOURCE=.\source\SceneDetector.cpp
# End Source File
# Begin Source File

SOURCE=.\source\VBitmap.cpp
# End Source File
# Begin Source File

SOURCE=.\source\VideoDisplay.cpp
# End Source File
# Begin Source File

SOURCE=.\source\VideoDisplayDrivers.cpp
# End Source File
# Begin Source File

SOURCE=.\source\VideoSourceAdapter.cpp
# End Source File
# Begin Source File

SOURCE=.\source\VideoTelecineRemover.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=.\h\AsyncBlitter.h
# End Source File
# Begin Source File

SOURCE=.\h\Audio.h
# End Source File
# Begin Source File

SOURCE=.\h\audioutil.h
# End Source File
# Begin Source File

SOURCE=.\h\Avisynth.h
# End Source File
# Begin Source File

SOURCE=.\h\convert.h
# End Source File
# Begin Source File

SOURCE=.\h\DynamicCode.h
# End Source File
# Begin Source File

SOURCE=.\h\e_blur.h
# End Source File
# Begin Source File

SOURCE=.\h\effect.h
# End Source File
# Begin Source File

SOURCE=.\h\f_convolute.h
# End Source File
# Begin Source File

SOURCE=.\h\fht.h
# End Source File
# Begin Source File

SOURCE=.\h\filter.h
# End Source File
# Begin Source File

SOURCE=.\h\Fixes.h
# End Source File
# Begin Source File

SOURCE=.\h\FrameSubset.h
# End Source File
# Begin Source File

SOURCE=.\h\helpcoach.h
# End Source File
# Begin Source File

SOURCE=.\h\helpfile.h
# End Source File
# Begin Source File

SOURCE=.\h\Histogram.h
# End Source File
# Begin Source File

SOURCE=.\h\image.h
# End Source File
# Begin Source File

SOURCE=.\h\indeo_if.h
# End Source File
# Begin Source File

SOURCE=.\h\plugins.h
# End Source File
# Begin Source File

SOURCE=.\res\resource.h
# End Source File
# Begin Source File

SOURCE=.\h\SceneDetector.h
# End Source File
# Begin Source File

SOURCE=.\h\VBitmap.h
# End Source File
# Begin Source File

SOURCE=..\h\vd2\plugin\vdaudiofilt.h
# End Source File
# Begin Source File

SOURCE=..\h\vd2\plugin\vdplugin.h
# End Source File
# Begin Source File

SOURCE=.\h\VideoDisplay.h
# End Source File
# Begin Source File

SOURCE=.\h\VideoDisplayDrivers.h
# End Source File
# Begin Source File

SOURCE=.\h\VideoSourceAdapter.h
# End Source File
# Begin Source File

SOURCE=.\h\VideoTelecineRemover.h
# End Source File
# Begin Source File

SOURCE=.\h\VirtualDub.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;cnt;rtf;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\res\Changes.bin
# End Source File
# Begin Source File

SOURCE=.\res\ico00001.ico
# End Source File
# Begin Source File

SOURCE=.\res\icon1.ico
# End Source File
# Begin Source File

SOURCE=.\res\license.bin
# End Source File
# Begin Source File

SOURCE=.\res\pos_back.ico
# End Source File
# Begin Source File

SOURCE=.\res\pos_end.ico
# End Source File
# Begin Source File

SOURCE=.\res\pos_mark.ico
# End Source File
# Begin Source File

SOURCE=.\res\pos_markout.ico
# End Source File
# Begin Source File

SOURCE=.\res\pos_next.ico
# End Source File
# Begin Source File

SOURCE=.\res\pos_play.ico
# End Source File
# Begin Source File

SOURCE=.\res\pos_prev.ico
# End Source File
# Begin Source File

SOURCE=.\res\pos_star.ico
# End Source File
# Begin Source File

SOURCE=.\res\pos_stop.ico
# End Source File
# Begin Source File

SOURCE=.\res\rel_notes.bin
# End Source File
# Begin Source File

SOURCE=.\res\scenefwd.ico
# End Source File
# Begin Source File

SOURCE=.\res\scenerev.ico
# End Source File
# Begin Source File

SOURCE=.\res\virtuald.ico
# End Source File
# Begin Source File

SOURCE=.\res\VirtualDub.rc
# End Source File
# End Group
# Begin Group "Source - Capture"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\capaccel.cpp
# End Source File
# Begin Source File

SOURCE=.\h\capaccel.h
# End Source File
# Begin Source File

SOURCE=.\source\capbt848.cpp
# End Source File
# Begin Source File

SOURCE=.\source\capclip.cpp
# End Source File
# Begin Source File

SOURCE=.\h\capclip.h
# End Source File
# Begin Source File

SOURCE=.\source\caphisto.cpp
# End Source File
# Begin Source File

SOURCE=.\h\caphisto.h
# End Source File
# Begin Source File

SOURCE=.\source\caplog.cpp
# End Source File
# Begin Source File

SOURCE=.\h\caplog.h
# End Source File
# Begin Source File

SOURCE=.\source\capspill.cpp
# End Source File
# Begin Source File

SOURCE=.\h\capspill.h
# End Source File
# Begin Source File

SOURCE=.\source\capture.cpp
# End Source File
# Begin Source File

SOURCE=.\h\capture.h
# End Source File
# Begin Source File

SOURCE=.\source\capvumeter.cpp
# End Source File
# Begin Source File

SOURCE=.\source\capwarn.cpp
# End Source File
# End Group
# Begin Group "Source - Filters"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\F_blur.cpp
# End Source File
# Begin Source File

SOURCE=.\source\f_boxblur.cpp
# End Source File
# Begin Source File

SOURCE=.\source\f_brightcont.cpp
# End Source File
# Begin Source File

SOURCE=.\source\f_chromasmoother.cpp
# End Source File
# Begin Source File

SOURCE=.\source\f_convolute.cpp
# End Source File
# Begin Source File

SOURCE=.\source\f_deinterlace.cpp
# End Source File
# Begin Source File

SOURCE=.\source\f_emboss.cpp
# End Source File
# Begin Source File

SOURCE=.\source\f_fieldbob.cpp
# End Source File
# Begin Source File

SOURCE=.\source\f_fieldswap.cpp
# End Source File
# Begin Source File

SOURCE=.\source\f_fill.cpp
# End Source File
# Begin Source File

SOURCE=.\source\f_flipv.cpp
# End Source File
# Begin Source File

SOURCE=.\source\f_grayscale.cpp
# End Source File
# Begin Source File

SOURCE=.\source\f_hsv.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /G6

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /G6
# ADD CPP /G6

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\f_invert.cpp
# End Source File
# Begin Source File

SOURCE=.\source\f_levels.cpp
# End Source File
# Begin Source File

SOURCE=.\source\f_list.cpp
# End Source File
# Begin Source File

SOURCE=.\source\f_logo.cpp
# End Source File
# Begin Source File

SOURCE=.\source\f_null.cpp
# End Source File
# Begin Source File

SOURCE=.\source\f_reduce2.cpp
# End Source File
# Begin Source File

SOURCE=.\source\f_reducehq.cpp
# End Source File
# Begin Source File

SOURCE=.\source\F_resize.cpp
# End Source File
# Begin Source File

SOURCE=.\source\f_rotate.cpp
# End Source File
# Begin Source File

SOURCE=.\source\f_rotate2.cpp
# End Source File
# Begin Source File

SOURCE=.\source\f_sharpen.cpp
# End Source File
# Begin Source File

SOURCE=.\source\f_smoother.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ox /Ot /Oa /Og /Oi /Oy

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Ox /Ot /Oa /Og /Oi /Oy
# ADD CPP /Ox /Ot /Oa /Og /Oi /Oy

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\f_threshold.cpp
# End Source File
# Begin Source File

SOURCE=.\source\f_timesmooth.cpp
# End Source File
# Begin Source File

SOURCE=.\source\f_tsoften.cpp
# End Source File
# Begin Source File

SOURCE=.\source\f_tv.cpp
# End Source File
# Begin Source File

SOURCE=.\source\resample.cpp
# End Source File
# Begin Source File

SOURCE=.\h\resample.h
# End Source File
# End Group
# Begin Group "Source - MPEG/MJPEG support"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\MJPEGDecoder.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ob2

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Ob2
# ADD CPP /Ob2

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\Mpeg.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ob2

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Ob2
# ADD CPP /Ob2

!ENDIF 

# End Source File
# End Group
# Begin Group "Headers - MPEG/MJPEG support"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\h\mjpeg_color.inl
# End Source File
# Begin Source File

SOURCE=.\h\MJPEGDecoder.h
# End Source File
# Begin Source File

SOURCE=.\h\mpeg.h
# End Source File
# End Group
# Begin Group "Text resources"

# PROP Default_Filter ".txt"
# Begin Source File

SOURCE=.\res\Changes.txt

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Building resource text...
InputDir=.\res
InputPath=.\res\Changes.txt
InputName=Changes

"$(InputDir)\$(InputName).bin" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy /b $(InputDir)\$(InputName).txt+null.bin $(InputDir)\$(InputName).bin

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build - Building resource text...
InputDir=.\res
InputPath=.\res\Changes.txt
InputName=Changes

"$(InputDir)\$(InputName).bin" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy /b $(InputDir)\$(InputName).txt+null.bin $(InputDir)\$(InputName).bin

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# Begin Custom Build - Building resource text...
InputDir=.\res
InputPath=.\res\Changes.txt
InputName=Changes

"$(InputDir)\$(InputName).bin" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy /b $(InputDir)\$(InputName).txt+null.bin $(InputDir)\$(InputName).bin

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\res\credits.txt
# End Source File
# Begin Source File

SOURCE=.\res\rel_notes.txt

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build - Building resource text...
InputDir=.\res
InputPath=.\res\rel_notes.txt
InputName=rel_notes

"$(InputDir)\$(InputName).bin" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy /b $(InputDir)\$(InputName).txt+null.bin $(InputDir)\$(InputName).bin

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build - Building resource text...
InputDir=.\res
InputPath=.\res\rel_notes.txt
InputName=rel_notes

"$(InputDir)\$(InputName).bin" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy /b $(InputDir)\$(InputName).txt+null.bin $(InputDir)\$(InputName).bin

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# Begin Custom Build - Building resource text...
InputDir=.\res
InputPath=.\res\rel_notes.txt
InputName=rel_notes

"$(InputDir)\$(InputName).bin" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy /b $(InputDir)\$(InputName).txt+null.bin $(InputDir)\$(InputName).bin

# End Custom Build

!ENDIF 

# End Source File
# End Group
# Begin Group "Source - User Interface"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\AudioDisplay.cpp
# End Source File
# Begin Source File

SOURCE=.\source\ClippingControl.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Os
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\FilterGraph.cpp
# End Source File
# Begin Source File

SOURCE=.\source\HexViewer.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Os
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\LevelControl.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Os
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\LogWindow.cpp
# End Source File
# Begin Source File

SOURCE=.\source\PositionControl.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Os
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\ProgressDialog.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Os
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\RTProfileDisplay.cpp
# End Source File
# Begin Source File

SOURCE=.\source\VideoWindow.cpp
# End Source File
# End Group
# Begin Group "Headers - User Interface"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\h\AudioDisplay.h
# End Source File
# Begin Source File

SOURCE=.\h\ClippingControl.h
# End Source File
# Begin Source File

SOURCE=.\h\FilterGraph.h
# End Source File
# Begin Source File

SOURCE=.\h\HexViewer.h
# End Source File
# Begin Source File

SOURCE=.\h\LevelControl.h
# End Source File
# Begin Source File

SOURCE=.\source\LogWindow.h
# End Source File
# Begin Source File

SOURCE=.\h\PositionControl.h
# End Source File
# Begin Source File

SOURCE=.\h\ProgressDialog.h
# End Source File
# Begin Source File

SOURCE=.\h\RTProfileDisplay.h
# End Source File
# Begin Source File

SOURCE=.\h\VideoWindow.h
# End Source File
# End Group
# Begin Group "Files - YUV codec"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\CVideoCompressor.cpp
# End Source File
# Begin Source File

SOURCE=.\h\CVideoCompressor.h
# End Source File
# Begin Source File

SOURCE=.\source\icdriver.cpp
# End Source File
# Begin Source File

SOURCE=.\h\IVideoCompressor.h
# End Source File
# Begin Source File

SOURCE=.\h\IVideoDriver.h
# End Source File
# Begin Source File

SOURCE=.\source\yuvcodec.cpp
# End Source File
# Begin Source File

SOURCE=.\h\yuvcodec.h
# End Source File
# End Group
# Begin Group "Headers - dTV"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\h\dTVdrv.h
# End Source File
# End Group
# Begin Group "Source - Size optimized"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\acompchoose.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0 /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\afiltdlg.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Ob0

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\AudioFilterSystem.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Ob0

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\autodetect.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0 /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\auxdlg.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0 /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\AVIAudioOutput.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\AVIPipe.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\AVIStripeSystem.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Ob0

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\Command.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0 /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\compchoose.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0 /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\Crash.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0 /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\ddrawsup.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\Disasm.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0 /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\Dub.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0 /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\DubIO.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Ob0

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\DubOutput.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Ob0

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\DubStatus.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0 /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\DubUtils.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Ob0

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\FastReadStream.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\FastWriteStream.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\Filtdlg.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0 /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\Filters.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0 /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\FilterSystem.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\gui.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0 /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\Init.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0 /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\Job.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0 /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\license.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0 /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\Main.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0 /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\memcheck.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\misc.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0 /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\MonoBitmap.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\MRUList.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0 /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\optdlg.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0 /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\oshelper.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0 /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\prefs.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0 /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\project.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Ob0

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\projectui.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Ob0

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\Script.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0 /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\server.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0 /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\sparseavi.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\stringtables.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Ob0

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\VideoSequenceCompressor.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od /Ob0 /Yu

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot /YX /Yc /Yu
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\VideoSourceImages.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Ob0

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

!ENDIF 

# End Source File
# End Group
# Begin Group "Headers - Size optimized"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\h\afiltdlg.h
# End Source File
# Begin Source File

SOURCE=.\h\AudioFilterSystem.h
# End Source File
# Begin Source File

SOURCE=.\h\autodetect.h
# End Source File
# Begin Source File

SOURCE=.\h\auxdlg.h
# End Source File
# Begin Source File

SOURCE=.\h\AVIAudioOutput.h
# End Source File
# Begin Source File

SOURCE=.\h\AVIPipe.h
# End Source File
# Begin Source File

SOURCE=.\h\AVIStripeSystem.h
# End Source File
# Begin Source File

SOURCE=.\h\command.h
# End Source File
# Begin Source File

SOURCE=.\h\crash.h
# End Source File
# Begin Source File

SOURCE=.\h\ddrawsup.h
# End Source File
# Begin Source File

SOURCE=.\h\disasm.h
# End Source File
# Begin Source File

SOURCE=.\h\Dub.h
# End Source File
# Begin Source File

SOURCE=.\h\DubIO.h
# End Source File
# Begin Source File

SOURCE=.\h\DubOutput.h
# End Source File
# Begin Source File

SOURCE=.\h\DubStatus.h
# End Source File
# Begin Source File

SOURCE=.\h\DubUtils.h
# End Source File
# Begin Source File

SOURCE=.\h\FastReadStream.h
# End Source File
# Begin Source File

SOURCE=.\h\FastWriteStream.h
# End Source File
# Begin Source File

SOURCE=.\h\filtdlg.h
# End Source File
# Begin Source File

SOURCE=.\h\filters.h
# End Source File
# Begin Source File

SOURCE=.\h\FilterSystem.h
# End Source File
# Begin Source File

SOURCE=.\h\gui.h
# End Source File
# Begin Source File

SOURCE=.\h\job.h
# End Source File
# Begin Source File

SOURCE=.\h\misc.h
# End Source File
# Begin Source File

SOURCE=.\h\MonoBitmap.h
# End Source File
# Begin Source File

SOURCE=.\h\MRUList.h
# End Source File
# Begin Source File

SOURCE=.\h\optdlg.h
# End Source File
# Begin Source File

SOURCE=.\h\oshelper.h
# End Source File
# Begin Source File

SOURCE=.\h\prefs.h
# End Source File
# Begin Source File

SOURCE=.\h\project.h
# End Source File
# Begin Source File

SOURCE=.\h\projectui.h
# End Source File
# Begin Source File

SOURCE=.\h\script.h
# End Source File
# Begin Source File

SOURCE=.\h\server.h
# End Source File
# Begin Source File

SOURCE=.\h\VideoSequenceCompressor.h
# End Source File
# Begin Source File

SOURCE=.\h\VideoSourceImages.h
# End Source File
# End Group
# Begin Group "Precompiled Header Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\stdafx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# Begin Source File

SOURCE=.\h\stdafx.h
# End Source File
# End Group
# Begin Group "Assembly Files"

# PROP Default_Filter ".asm"
# Begin Source File

SOURCE=.\source\a_bitmap.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build
IntDir=.\../obj/Release/VirtualDub
InputPath=.\source\a_bitmap.asm
InputName=a_bitmap

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build
IntDir=.\../obj/Debug/VirtualDub
InputPath=.\source\a_bitmap.asm
InputName=a_bitmap

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# Begin Custom Build
IntDir=.\..\obj\ReleaseICL\VirtualDub
InputPath=.\source\a_bitmap.asm
InputName=a_bitmap

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_chromasmoother.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build
IntDir=.\../obj/Release/VirtualDub
InputPath=.\source\a_chromasmoother.asm
InputName=a_chromasmoother

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build
IntDir=.\../obj/Debug/VirtualDub
InputPath=.\source\a_chromasmoother.asm
InputName=a_chromasmoother

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# Begin Custom Build
IntDir=.\..\obj\ReleaseICL\VirtualDub
InputPath=.\source\a_chromasmoother.asm
InputName=a_chromasmoother

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_convert.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build
IntDir=.\../obj/Release/VirtualDub
InputPath=.\source\a_convert.asm
InputName=a_convert

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build
IntDir=.\../obj/Debug/VirtualDub
InputPath=.\source\a_convert.asm
InputName=a_convert

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# Begin Custom Build
IntDir=.\..\obj\ReleaseICL\VirtualDub
InputPath=.\source\a_convert.asm
InputName=a_convert

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_convertout.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build
IntDir=.\../obj/Release/VirtualDub
InputPath=.\source\a_convertout.asm
InputName=a_convertout

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build
IntDir=.\../obj/Debug/VirtualDub
InputPath=.\source\a_convertout.asm
InputName=a_convertout

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# Begin Custom Build
IntDir=.\..\obj\ReleaseICL\VirtualDub
InputPath=.\source\a_convertout.asm
InputName=a_convertout

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_fastdisplay.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build
IntDir=.\../obj/Release/VirtualDub
InputPath=.\source\a_fastdisplay.asm
InputName=a_fastdisplay

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build
IntDir=.\../obj/Debug/VirtualDub
InputPath=.\source\a_fastdisplay.asm
InputName=a_fastdisplay

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# Begin Custom Build
IntDir=.\..\obj\ReleaseICL\VirtualDub
InputPath=.\source\a_fastdisplay.asm
InputName=a_fastdisplay

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_histogram.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build
IntDir=.\../obj/Release/VirtualDub
InputPath=.\source\a_histogram.asm
InputName=a_histogram

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build
IntDir=.\../obj/Debug/VirtualDub
InputPath=.\source\a_histogram.asm
InputName=a_histogram

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# Begin Custom Build
IntDir=.\..\obj\ReleaseICL\VirtualDub
InputPath=.\source\a_histogram.asm
InputName=a_histogram

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_scene.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build
IntDir=.\../obj/Release/VirtualDub
InputPath=.\source\a_scene.asm
InputName=a_scene

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build
IntDir=.\../obj/Debug/VirtualDub
InputPath=.\source\a_scene.asm
InputName=a_scene

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# Begin Custom Build
IntDir=.\..\obj\ReleaseICL\VirtualDub
InputPath=.\source\a_scene.asm
InputName=a_scene

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_yuv422torgb.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build
IntDir=.\../obj/Release/VirtualDub
InputPath=.\source\a_yuv422torgb.asm
InputName=a_yuv422torgb

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build
IntDir=.\../obj/Debug/VirtualDub
InputPath=.\source\a_yuv422torgb.asm
InputName=a_yuv422torgb

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# Begin Custom Build
IntDir=.\..\obj\ReleaseICL\VirtualDub
InputPath=.\source\a_yuv422torgb.asm
InputName=a_yuv422torgb

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# End Group
# Begin Group "Assembly Files - Filters"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\a_brightcont.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build
IntDir=.\../obj/Release/VirtualDub
InputPath=.\source\a_brightcont.asm
InputName=a_brightcont

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build
IntDir=.\../obj/Debug/VirtualDub
InputPath=.\source\a_brightcont.asm
InputName=a_brightcont

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# Begin Custom Build
IntDir=.\..\obj\ReleaseICL\VirtualDub
InputPath=.\source\a_brightcont.asm
InputName=a_brightcont

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_cmult.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build
IntDir=.\../obj/Release/VirtualDub
InputPath=.\source\a_cmult.asm
InputName=a_cmult

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build
IntDir=.\../obj/Debug/VirtualDub
InputPath=.\source\a_cmult.asm
InputName=a_cmult

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# Begin Custom Build
IntDir=.\..\obj\ReleaseICL\VirtualDub
InputPath=.\source\a_cmult.asm
InputName=a_cmult

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_convolute.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build
IntDir=.\../obj/Release/VirtualDub
InputPath=.\source\a_convolute.asm
InputName=a_convolute

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build
IntDir=.\../obj/Debug/VirtualDub
InputPath=.\source\a_convolute.asm
InputName=a_convolute

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# Begin Custom Build
IntDir=.\..\obj\ReleaseICL\VirtualDub
InputPath=.\source\a_convolute.asm
InputName=a_convolute

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_grayscale.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build
IntDir=.\../obj/Release/VirtualDub
InputPath=.\source\a_grayscale.asm
InputName=a_grayscale

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build
IntDir=.\../obj/Debug/VirtualDub
InputPath=.\source\a_grayscale.asm
InputName=a_grayscale

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# Begin Custom Build
IntDir=.\..\obj\ReleaseICL\VirtualDub
InputPath=.\source\a_grayscale.asm
InputName=a_grayscale

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_reduce.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build
IntDir=.\../obj/Release/VirtualDub
InputPath=.\source\a_reduce.asm
InputName=a_reduce

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build
IntDir=.\../obj/Debug/VirtualDub
InputPath=.\source\a_reduce.asm
InputName=a_reduce

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# Begin Custom Build
IntDir=.\..\obj\ReleaseICL\VirtualDub
InputPath=.\source\a_reduce.asm
InputName=a_reduce

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\A_resize.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build
IntDir=.\../obj/Release/VirtualDub
InputPath=.\source\A_resize.asm
InputName=A_resize

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build
IntDir=.\../obj/Debug/VirtualDub
InputPath=.\source\A_resize.asm
InputName=A_resize

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# Begin Custom Build
IntDir=.\..\obj\ReleaseICL\VirtualDub
InputPath=.\source\A_resize.asm
InputName=A_resize

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_rotate.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build
IntDir=.\../obj/Release/VirtualDub
InputPath=.\source\a_rotate.asm
InputName=a_rotate

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build
IntDir=.\../obj/Debug/VirtualDub
InputPath=.\source\a_rotate.asm
InputName=a_rotate

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# Begin Custom Build
IntDir=.\..\obj\ReleaseICL\VirtualDub
InputPath=.\source\a_rotate.asm
InputName=a_rotate

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_sharpen.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build
IntDir=.\../obj/Release/VirtualDub
InputPath=.\source\a_sharpen.asm
InputName=a_sharpen

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build
IntDir=.\../obj/Debug/VirtualDub
InputPath=.\source\a_sharpen.asm
InputName=a_sharpen

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# Begin Custom Build
IntDir=.\..\obj\ReleaseICL\VirtualDub
InputPath=.\source\a_sharpen.asm
InputName=a_sharpen

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_threshold.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build
IntDir=.\../obj/Release/VirtualDub
InputPath=.\source\a_threshold.asm
InputName=a_threshold

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build
IntDir=.\../obj/Debug/VirtualDub
InputPath=.\source\a_threshold.asm
InputName=a_threshold

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# Begin Custom Build
IntDir=.\..\obj\ReleaseICL\VirtualDub
InputPath=.\source\a_threshold.asm
InputName=a_threshold

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_tv.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build
IntDir=.\../obj/Release/VirtualDub
InputPath=.\source\a_tv.asm
InputName=a_tv

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build
IntDir=.\../obj/Debug/VirtualDub
InputPath=.\source\a_tv.asm
InputName=a_tv

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# Begin Custom Build
IntDir=.\..\obj\ReleaseICL\VirtualDub
InputPath=.\source\a_tv.asm
InputName=a_tv

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# End Group
# Begin Group "Assembly Files - MPEG/MJPEG"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\a_mjpgdec.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build
IntDir=.\../obj/Release/VirtualDub
InputPath=.\source\a_mjpgdec.asm
InputName=a_mjpgdec

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build
IntDir=.\../obj/Debug/VirtualDub
InputPath=.\source\a_mjpgdec.asm
InputName=a_mjpgdec

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# Begin Custom Build
IntDir=.\..\obj\ReleaseICL\VirtualDub
InputPath=.\source\a_mjpgdec.asm
InputName=a_mjpgdec

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_yuv2rgbhq.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build
IntDir=.\../obj/Release/VirtualDub
InputPath=.\source\a_yuv2rgbhq.asm
InputName=a_yuv2rgbhq

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build
IntDir=.\../obj/Debug/VirtualDub
InputPath=.\source\a_yuv2rgbhq.asm
InputName=a_yuv2rgbhq

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# Begin Custom Build
IntDir=.\..\obj\ReleaseICL\VirtualDub
InputPath=.\source\a_yuv2rgbhq.asm
InputName=a_yuv2rgbhq

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\a_yuvtable.asm

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# Begin Custom Build
IntDir=.\../obj/Release/VirtualDub
InputPath=.\source\a_yuvtable.asm
InputName=a_yuvtable

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# Begin Custom Build
IntDir=.\../obj/Debug/VirtualDub
InputPath=.\source\a_yuvtable.asm
InputName=a_yuvtable

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# Begin Custom Build
IntDir=.\..\obj\ReleaseICL\VirtualDub
InputPath=.\source\a_yuvtable.asm
InputName=a_yuvtable

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /c /Zi /coff /nologo /Fo$(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# End Group
# Begin Group "Source - Audio Filters"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\af_base.cpp
# End Source File
# Begin Source File

SOURCE=.\source\af_centercut.cpp
# End Source File
# Begin Source File

SOURCE=.\source\af_centermix.cpp
# End Source File
# Begin Source File

SOURCE=.\source\af_chorus.cpp
# End Source File
# Begin Source File

SOURCE=.\source\af_gain.cpp
# End Source File
# Begin Source File

SOURCE=.\source\af_input.cpp
# End Source File
# Begin Source File

SOURCE=.\source\af_list.cpp
# End Source File
# Begin Source File

SOURCE=.\source\af_mix.cpp
# End Source File
# Begin Source File

SOURCE=.\source\af_newrate.cpp
# End Source File
# Begin Source File

SOURCE=.\source\af_pitchshift.cpp
# End Source File
# Begin Source File

SOURCE=.\source\af_polyphase.cpp
# End Source File
# Begin Source File

SOURCE=.\source\af_sink.cpp
# End Source File
# Begin Source File

SOURCE=.\source\af_split.cpp
# End Source File
# End Group
# Begin Group "Headers - Audio Filters"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\h\af_base.h
# End Source File
# Begin Source File

SOURCE=.\h\af_polyphase.h
# End Source File
# Begin Source File

SOURCE=.\h\af_sink.h
# End Source File
# End Group
# Begin Group "Source - I/O drivers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\source\AudioSource.cpp
# End Source File
# Begin Source File

SOURCE=.\source\AVIIndex.cpp
# End Source File
# Begin Source File

SOURCE=.\source\AVIOutput.cpp
# End Source File
# Begin Source File

SOURCE=.\source\AVIOutputImages.cpp
# End Source File
# Begin Source File

SOURCE=.\source\AVIOutputPreview.cpp
# End Source File
# Begin Source File

SOURCE=.\source\AVIOutputStriped.cpp
# End Source File
# Begin Source File

SOURCE=.\source\AVIOutputWAV.cpp
# End Source File
# Begin Source File

SOURCE=.\source\AVIReadHandler.cpp
# End Source File
# Begin Source File

SOURCE=.\source\DubSource.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

# ADD CPP /Od

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

# ADD BASE CPP /Os /Oy /Ob1
# SUBTRACT BASE CPP /Ox /Ot
# ADD CPP /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\InputFile.cpp

!IF  "$(CFG)" == "VirtualDub - Win32 Release"

# ADD CPP /Ow /Os /Oy /Ob1
# SUBTRACT CPP /Ox /Ot /Oa

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Debug"

!ELSEIF  "$(CFG)" == "VirtualDub - Win32 Release ICL"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\source\InputFileASF.cpp
# End Source File
# Begin Source File

SOURCE=.\source\InputFileAVI.cpp
# End Source File
# Begin Source File

SOURCE=.\source\InputFileImages.cpp
# End Source File
# Begin Source File

SOURCE=.\source\VideoSource.cpp
# End Source File
# End Group
# Begin Group "Headers - I/O drivers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\h\AudioSource.h
# End Source File
# Begin Source File

SOURCE=.\h\AVIIndex.h
# End Source File
# Begin Source File

SOURCE=.\h\AVIOutput.h
# End Source File
# Begin Source File

SOURCE=.\h\AVIOutputImages.h
# End Source File
# Begin Source File

SOURCE=.\h\AVIOutputPreview.h
# End Source File
# Begin Source File

SOURCE=.\h\AVIoutputStriped.h
# End Source File
# Begin Source File

SOURCE=.\h\AVIOutputWAV.h
# End Source File
# Begin Source File

SOURCE=.\h\AVIReadHandler.h
# End Source File
# Begin Source File

SOURCE=.\h\DubSource.h
# End Source File
# Begin Source File

SOURCE=.\h\InputFile.h
# End Source File
# Begin Source File

SOURCE=.\h\InputFileAVI.h
# End Source File
# Begin Source File

SOURCE=.\h\InputFileImages.h
# End Source File
# Begin Source File

SOURCE=.\h\VideoSource.h
# End Source File
# End Group
# End Target
# End Project

# Microsoft Developer Studio Project File - Name="helpfile" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=helpfile - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "helpfile.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "helpfile.mak" CFG="helpfile - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "helpfile - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "helpfile - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "Perforce Project"
# PROP Scc_LocalPath "."

!IF  "$(CFG)" == "helpfile - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f helpfile.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "helpfile.exe"
# PROP BASE Bsc_Name "helpfile.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "nmake /nologo /c SRC=source BUILD=Release"
# PROP Rebuild_Opt ""
# PROP Target_File "..\out\Release\help.zip"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "helpfile - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f helpfile.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "helpfile.exe"
# PROP BASE Bsc_Name "helpfile.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line "nmake /nologo /c SRC=source BUILD=Debug"
# PROP Rebuild_Opt "/a"
# PROP Target_File "..\out\Debug\help.zip"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "helpfile - Win32 Release"
# Name "helpfile - Win32 Debug"

!IF  "$(CFG)" == "helpfile - Win32 Release"

!ELSEIF  "$(CFG)" == "helpfile - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "lina"
# Begin Source File

SOURCE=.\source\audiofilters.lina
# End Source File
# Begin Source File

SOURCE=.\source\avi.lina
# End Source File
# Begin Source File

SOURCE=.\source\capture.lina
# End Source File
# Begin Source File

SOURCE=.\source\crash.lina
# End Source File
# Begin Source File

SOURCE=.\source\dialogs.lina
# End Source File
# Begin Source File

SOURCE=.\source\fxvideo.lina
# End Source File
# Begin Source File

SOURCE=.\source\render.lina
# End Source File
# Begin Source File

SOURCE=.\source\root.lina
# End Source File
# Begin Source File

SOURCE=.\source\videofilters.lina
# End Source File
# End Group
# Begin Source File

SOURCE=.\Makefile
# End Source File
# End Target
# End Project

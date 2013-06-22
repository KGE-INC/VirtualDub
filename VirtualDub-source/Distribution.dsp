# Microsoft Developer Studio Project File - Name="Distribution" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Generic Project" 0x010a

CFG=Distribution - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Distribution.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Distribution.mak" CFG="Distribution - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Distribution - Win32 Release" (based on "Win32 (x86) Generic Project")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
MTL=midl.exe
# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Distribution___Win32_Release"
# PROP BASE Intermediate_Dir "Distribution___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "i:\shared\virtualdub\current"
# PROP Intermediate_Dir "dist"
# PROP Target_Dir ""
# Begin Target

# Name "Distribution - Win32 Release"
# Begin Source File

SOURCE=.\binary.lst
# PROP Ignore_Default_Tool 1
USERDEP__BINAR="i:\projwin\virtualdub\release\virtualdub.exe"	
# Begin Custom Build - Constructing binary archive...
InputPath=.\binary.lst

"j:\istore\shared\virtualdub\current\VirtualDub.zip" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	del j:\istore\shared\virtualdub\current\VirtualDub.zip 
	zip j:\istore\shared\virtualdub\current\VirtualDub.zip -9 -j -@ < binary.lst 
	zip -9 -j j:\istore\shared\virtualdub\current\linkmap.zip release\virtualdub.map 
	
# End Custom Build
# End Source File
# Begin Source File

SOURCE=.\source.lst
# PROP Ignore_Default_Tool 1
# Begin Custom Build - Constructing source archive...
InputPath=.\source.lst

"j:\istore\shared\virtualdub\current\VirtualDub_source.zip" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	del j:\istore\shared\virtualdub\current\VirtualDub_source.zip 
	zip j:\istore\shared\virtualdub\current\VirtualDub_source.zip -9 -@ < source.lst 
	
# End Custom Build
# End Source File
# Begin Source File

SOURCE=.\source2.lst
# PROP Ignore_Default_Tool 1
# Begin Custom Build - Constructing source2 archive...
InputPath=.\source2.lst

"j:\istore\shared\virtualdub\current\VirtualDub_auxsrc.zip" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	del j:\istore\shared\virtualdub\current\VirtualDub_auxsrc.zip 
	zip j:\istore\shared\virtualdub\current\VirtualDub_auxsrc.zip -9 -@ < source2.lst 
	
# End Custom Build
# End Source File
# End Target
# End Project

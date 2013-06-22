@echo off
setlocal enableextensions

set _ddk=c:\winddk\3790
set _vc6=c:\vc6\vc98
set _dx9=c:\dx9sdk

set _amd64_bin=%_ddk%\bin\win64\x86\amd64;%_ddk%\bin\x86
set _amd64_include=%_dx9%\include;%_ddk%\inc\crt;%_ddk%\inc\wnet;%_vc6%\include
set _amd64_lib=%_dx9%\lib\x64;%_ddk%\lib\wnet\amd64


set _incremental=false
set _build_release=false
set _build_releaseP4=false
set _build_releaseAMD64=false
set _build_help=false
set _builds_set=false
set _check=false

rem ---parse command line arguments

:arglist
if "%1"=="" goto endargs

if "%1"=="/inc" (
	set _incremental=true
) else if "%1"=="/full" (
	set _incremental=false
) else if "%1"=="/release" (
	set _build_release=true
	set _builds_set=true
) else if "%1"=="/check" (
	set _check=true
) else if "%1"=="/p4" (
	set _build_releaseP4=true
	set _builds_set=true
) else if "%1"=="/amd64" (
	set _build_releaseAMD64=true
	set _builds_set=true
) else if "%1"=="/helpfile" (
	set _build_help=true
	set _builds_set=true
) else if "%1"=="/packonly" (
	set _builds_set=true
) else (
	echo.
	echo syntax: release [switches]
	echo     /inc          do incremental build
	echo     /full         do full build
	echo     /check        do check build [allow version increment]
        echo     /release      build release build
	echo     /p4           build P4 build [requires Intel C/C++ compiler]
	echo     /amd64        build AMD64 build [requires prerelease VC8 compiler from DDK]
	echo     /helpfile     build helpfile
	echo     /packonly     skip builds and only package
	exit /b 5
)

shift /1
goto arglist

:endargs

rem --- initialize build parameters

if "%_builds_set%"=="false" (
	set _build_release=true
	set _build_releasep4=true
	set _build_releaseamd64=true
	set _build_helpfile=true
)

set _project_switches=
if "%_incremental%"=="false" set _project_switches=/rebuild

if "%_build_helpfile"=="true" (
	echo --- Building helpfile
	msdev VirtualDub.dsw /make "Helpfile - Win32 Release" %_project_switches%
)

if "%_build_release%"=="true" call :build "Release" Release Release
if "%_build_releaseP4%"=="true" call :build "Release ICL" ReleaseICL P4
if "%_build_releaseAMD64%"=="true" call :build "Release AMD64" ReleaseAMD64 AMD64

if "%_check%"=="false" (
	echo --- building final archives

	rd /s /q out\Distribution
	md out\Distribution
	zip -0 -X -r out\Distribution\src.zip * -x lib\* obj\* out\* *.ncb *.opt *.old *.vcproj *.vspscc *.sln *.plg *.aps *.pch *.pdb *.obj *.tmp
	bzip2 -9 out\Distribution\src.zip
	md out\Distribution\bindist
	copy out\Release\VirtualDub.exe out\Distribution\bindist
	copy out\Release\VirtualDub.vdi out\Distribution\bindist
	copy out\Release\vdicmdrv.dll out\Distribution\bindist
	copy out\Release\vdsvrlnk.dll out\Distribution\bindist
	copy out\Release\vdremote.dll out\Distribution\bindist
	copy out\Release\auxsetup.exe out\Distribution\bindist
	copy out\Release\VirtualDub.vdhelp out\Distribution\bindist
	copy out\ReleaseICL\VeedubP4.exe out\Distribution\bindist
	copy out\ReleaseICL\VeedubP4.vdi out\Distribution\bindist
	upx -9 out\Distribution\bindist\*.exe out\Distribution\bindist\*.dll
	copy out\ReleaseAMD64\Veedub64.exe out\Distribution\bindist
	copy out\ReleaseAMD64\Veedub64.vdi out\Distribution\bindist
	xcopy VirtualDub\dist\* out\Distribution\bindist /s/e/i
	copy copying out\Distribution\bindist
	cd out\Distribution\bindist
	zip -9 -X -r ..\bin.zip VirtualDub.exe VirtualDub.vdi VirtualDub.vdhelp *.dll auxsetup.exe aviproxy\* plugins\* copying
	zip -9 -X ..\bin-p4.zip VeedubP4.* copying
	zip -9 -X ..\bin-amd64.zip Veedub64.* copying
	cd ..\..\..
	zip -9 -X -j out\Distribution\linkmaps.zip out\Release\VirtualDub.map out\ReleaseICL\VeedubP4.map out\ReleaseAMD64\Veedub64.map
)

exit /b

:build
echo --- Building release: %3

if "%_check%"=="false" (
	attrib -r VirtualDub\version.bin
	attrib -r VirtualDub\version2.bin
)

if "%3"=="AMD64" (
	rem ---these must be built separately as we can't run AMD64 tools on Win32
	msdev VirtualDub.dsw /make "verinc - Win32 Release" %_project_switches%
	msdev VirtualDub.dsw /make "mapconv - Win32 Release" %_project_switches%

	setlocal
	set path=%_amd64_bin%;%path%
	set include=%_amd64_include%;%include%
	set lib=%_amd64_lib%;%lib%
	msdev VirtualDub.dsw /make "VirtualDub - Win32 %~1" %_project_switches% /useenv
	endlocal
) else (
	msdev VirtualDub.dsw /make "VirtualDub - Win32 %~1" %_project_switches%
)

if "%_check%"=="false" (
	p4 sync -f VirtualDub/version.bin
	p4 sync -f VirtualDub/version2.bin
)

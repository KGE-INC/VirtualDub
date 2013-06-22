@echo off
setlocal enableextensions enabledelayedexpansion

if "%COMPUTERNAME%"=="ATHENA64" (
	set _ddk=c:\winddk\3790
	set _vc6=c:\vc6\vc98
	set _vc71=c:\vs.net\vc7
	set _vc8=d:\vs8\vc
	set _dx9=d:\dx9sdk
) else if "%COMPUTERNAME%"=="ATHENA" (
	set _ddk=c:\winddk\3790
	set _vc6=c:\vc6\vc98
	set _vc71=c:\vs.net\vc7
	set _vc8=c:\vs8\vc
	set _dx9=i:\dx9sdk
) else (
	set _ddk=c:\winddk\3790
	set _vc6=c:\vc6\vc98
	set _vc71=c:\vs.net\vc7
	set _vc8=
	set _dx9=c:\dx9sdk
)

set _amd64_bin=%_ddk%\bin\win64\x86\amd64;%_ddk%\bin\x86
set _amd64_include=%_dx9%\include;%_ddk%\inc\crt;%_ddk%\inc\wnet;%_vc6%\include
set _amd64_lib=%_dx9%\lib\x64;%_ddk%\lib\wnet\amd64


set _incremental=false
set _build_debug=false
set _build_debugAMD64=false
set _build_release=false
set _build_releaseAMD64=false
set _build_help=false
set _builds_set=false
set _check=false
set _use_vc71=false
set _use_vc8=false
set _vc8_flags=/D_CRT_SECURE_NO_DEPRECATE /wd4018 /wd4571

rem ---parse command line arguments

:arglist
if "%1"=="" goto endargs

if "%1"=="/inc" (
	set _incremental=true
) else if "%1"=="/full" (
	set _incremental=false
) else if "%1"=="/debug" (
	set _build_debug=true
	set _builds_set=true
) else if "%1"=="/release" (
	set _build_release=true
	set _builds_set=true
) else if "%1"=="/check" (
	set _check=true
) else if "%1"=="/debugamd64" (
	set _build_debugAMD64=true
	set _builds_set=true
) else if "%1"=="/amd64" (
	set _build_releaseAMD64=true
	set _builds_set=true
) else if "%1"=="/helpfile" (
	set _build_help=true
	set _builds_set=true
) else if "%1"=="/packonly" (
	set _builds_set=true
) else if "%1"=="/vc71" (
	set _use_vc71=true
) else if "%1"=="/vc8" (
	set _use_vc8=true
	if "!_vc8!"=="" (
		echo Error: Path is not set for Visual Studio .NET 2005 for this machine
		exit /b 5
	)
) else (
	echo.
	echo syntax: release [switches]
	echo     /inc          do incremental build
	echo     /full         do full build
	echo     /check        do check build [allow version increment]
	echo     /debug        build debug build
        echo     /release      build release build
	echo     /debugamd64   build debug AMD64 build [requires DDK]
	echo     /amd64        build AMD64 build [requires DDK]
	echo     /helpfile     build helpfile
	echo     /packonly     skip builds and only package
	echo     /vc71         use Visual Studio .NET 2003 compiler for x86 build
	echo     /vc8          use Visual Studio .NET 2005 compiler for x86 and AMD64 builds
	exit /b 5
)

shift /1
goto arglist

:endargs

rem --- initialize build parameters

if exist VirtualDub\autobuild.lock del VirtualDub\autobuild.lock /q
if not "%_check%"=="true" (
	echo. >VirtualDub\autobuild.lock
)

if "%_builds_set%"=="false" (
	set _build_release=true
	set _build_releaseamd64=true
	set _build_help=true
)

set _project_switches=
if "%_incremental%"=="false" set _project_switches=/rebuild

echo --- Starting build

if "%_build_help%"=="true" (
	echo --- Building helpfile
	msdev VirtualDub.dsw /make "Helpfile - Win32 Release" %_project_switches%
)

if "%_build_debug%"=="true" (
	call :build "Debug" Debug Debug
	if errorlevel 1 goto abort
)

if "%_build_debugAMD64%"=="true" (
	call :build "Debug AMD64" DebugAMD64 AMD64
	if errorlevel 1 goto abort
)

if "%_build_release%"=="true" (
	call :build "Release" Release Release
	if errorlevel 1 goto abort
)

if "%_build_releaseAMD64%"=="true" (
	call :build "Release AMD64" ReleaseAMD64 AMD64
	if errorlevel 1 goto abort
)

if "%_check%"=="false" (
	echo --- building final archives

	rd /s /q out\Distribution
	md out\Distribution
	zip -0 -X -r out\Distribution\src.zip * -x lib\* obj\* out\* *.ncb *.opt *.old *.vcproj *.vspscc *.sln *.plg *.aps *.pch *.pdb *.obj *.tmp disasm2\*
	bzip2 -9 out\Distribution\src.zip
	md out\Distribution\bindist
	copy out\Release\VirtualDub.exe out\Distribution\bindist
	copy out\Release\VirtualDub.vdi out\Distribution\bindist
	copy out\Release\vdub.exe out\Distribution\bindist
	copy out\Release\vdicmdrv.dll out\Distribution\bindist
	copy out\Release\vdsvrlnk.dll out\Distribution\bindist
	copy out\Release\vdremote.dll out\Distribution\bindist
	copy out\Release\auxsetup.exe out\Distribution\bindist
	copy out\Release\VirtualDub.vdhelp out\Distribution\bindist
	upx -9 out\Distribution\bindist\*.exe out\Distribution\bindist\*.dll
	copy out\ReleaseAMD64\Veedub64.exe out\Distribution\bindist
	copy out\ReleaseAMD64\Veedub64.vdi out\Distribution\bindist
	copy out\ReleaseAMD64\vdub64.exe out\Distribution\bindist
	xcopy VirtualDub\dist\* out\Distribution\bindist /s/e/i
	copy copying out\Distribution\bindist
	cd out\Distribution\bindist
	zip -9 -X -r ..\bin.zip VirtualDub.exe VirtualDub.vdi VirtualDub.vdhelp vdub.exe *.dll auxsetup.exe aviproxy\* plugins\* copying
	zip -9 -X ..\bin-amd64.zip Veedub64.* vdub64.exe copying
	cd ..\..\..
	zip -9 -X -j out\Distribution\linkmaps.zip out\Release\VirtualDub.map out\ReleaseAMD64\Veedub64.map
)

:cleanup
if exist VirtualDub\autobuild.lock del VirtualDub\autobuild.lock /q
endlocal
exit /b

:abort
if exist VirtualDub\autobuild.lock del /q VirtualDub\autobuild.lock
endlocal
exit /b 5

:build
echo --- Building release: %2

if "%3"=="AMD64" (
	rem ---these must be built separately as we can't run AMD64 tools on Win32
	if "%_incremental%"=="false" (
		msdev VirtualDub.dsw /make "VirtualDub - Win32 %~1" /clean
	)
	if "%2"=="DebugAMD64" (
		if not exist out\Debug\Asuka.exe (
			msdev VirtualDub.dsw /make "Asuka - Win32 Debug" %_project_switches%
		)
	) else (
		if not exist out\Release\Asuka.exe (
			msdev VirtualDub.dsw /make "Asuka - Win32 Release" %_project_switches%
		)
	)

	setlocal
	if "%_use_vc8%"=="true" (
		echo ---Using Visual Studio .NET 2005 compiler for AMD64.
		set include=
		set lib=
		call %_vc8%\bin\vcvars32.bat
		call %_vc8%\bin\x86_amd64\vcvarsamd64.bat
		set cl=%_vc8_flags% !cl!
	) else (
		echo ---Using prerelease DDK compiler for AMD64.
		set path=!_amd64_bin!;!path!
		set include=%_amd64_include%;!include!
		set lib=%_amd64_lib%;!lib!
		set ml=/Dxmmword=qword !ml!
	)
	if "%2"=="DebugAMD64" (
		set cl=/homeparams !cl!
	)
	set include=%_dx9%\include;!include!
	set lib=%_dx9%\lib\x64;!lib!
	msdev VirtualDub.dsw /make "VirtualDub - Win32 %~1" /useenv
	endlocal
	if errorlevel 1 set _build_abort=true
) else (
	setlocal
	if "!_use_vc8!"=="true" (
		echo ---Using Visual Studio .NET 2005 compiler for x86.
		set include=
		set lib=
		call !_vc8!\bin\vcvars32.bat
		set include=!_dx9!\include;!include!
		set lib=!_dx9!\lib;!lib!
		set cl=!_vc8_flags! !cl!
		set _project_switches=!_project_switches! /useenv
	) else if "!_use_vc71!"=="true" (
		echo ---Using Visual Studio .NET 2003 compiler.
		set include=
		set lib=
		call !_vc71!\bin\vcvars32.bat
		set include=!_dx9!\include;!include!
		set lib=!_dx9!\lib;!lib!
	)
	msdev VirtualDub.dsw /make "vdsvrlnk - Win32 %~1" !_project_switches!
	msdev VirtualDub.dsw /make "vdicmdrv - Win32 %~1" !_project_switches!
	msdev VirtualDub.dsw /make "vdremote - Win32 %~1" !_project_switches!
	msdev VirtualDub.dsw /make "Setup - Win32 %~1" !_project_switches!
	msdev VirtualDub.dsw /make "VirtualDub - Win32 %~1" !_project_switches!
	endlocal
	if errorlevel 1 set _build_abort=true
)

if "%_build_abort%"=="true" (
	echo.
	echo ---Build failed!
	goto abort
)

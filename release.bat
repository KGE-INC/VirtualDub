if not "%1"=="norebuild" (
	attrib -r VirtualDub/version.bin
	attrib -r VirtualDub/version2.bin
	msdev VirtualDub.dsw /make "VirtualDub - Win32 Release" /rebuild
	msdev VirtualDub.dsw /make "Helpfile - Win32 Release" /rebuild
	p4 sync -f VirtualDub/version.bin
	p4 sync -f VirtualDub/version2.bin
	attrib -r VirtualDub/version.bin
	attrib -r VirtualDub/version2.bin
	msdev VirtualDub.dsw /make "VirtualDub - Win32 Release ICL" /rebuild
	p4 sync -f VirtualDub/version.bin
	p4 sync -f VirtualDub/version2.bin
)
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
xcopy VirtualDub\dist\* out\Distribution\bindist /s/e/i
copy copying out\Distribution\bindist
cd out\Distribution\bindist
zip -9 -X -r ..\bin.zip VirtualDub.exe VirtualDub.vdi VirtualDub.vdhelp *.dll auxsetup.exe aviproxy\* plugins\* copying
zip -9 -X ..\bin-p4.zip VeedubP4.* copying
cd ..\..\..
zip -9 -X -j out\Distribution\linkmaps.zip out\Release\VirtualDub.map out\ReleaseICL\VeedubP4.map

OUTDIR = ..\out\Helpfile

..\$(BUILD)\VirtualDub.vdhelp: $(SRC)/*.lina $(SRC)\pics\*
	-rd /q /s $(OUTDIR)
	md $(OUTDIR)
	..\out\$(BUILD)\Lina $(SRC)/root.lina
	cd $(OUTDIR)
	zip -0 -X help0.zip *.html *.css pics/*
	-del ..\$(BUILD)\VirtualDub.vdhelp
	zip -9 -X -j ..\$(BUILD)\VirtualDub.vdhelp help0.zip
	del help0.zip

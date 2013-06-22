OUTDIR = ..\out\filtsdk2

$(OUTDIR)\index.html: $(SRC)\*.lina
	-rd /q /s $(OUTDIR)
	md $(OUTDIR)
	..\out\$(BUILD)\Lina $(SRC)/root.lina

span	struct
u		dd		?
v		dd		?
span	ends

mipspan	struct
u		dd		?
v		dd		?
lambda	dd		?
mipspan	ends

mipmap	struct
bits	dd		?
pitch	dd		?
uvmul	dd		?
		dd		?
mipmap	ends

texinfo	struct
mips	mipmap	16 dup (<?,?>)
dst		dd		?
src		dd		?
w		dd		?
texinfo	ends

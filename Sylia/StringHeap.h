#ifndef f_STRINGHEAP_H
#define f_STRINGHEAP_H

class CStringHeap {
private:
	struct StringDescriptor {
		char **handle;
		long len;
	};

	StringDescriptor *lpHeap;
	char **lpHandleTable;
	long lHandles;
	long lQuads;
	long lQuadsFree;

	bool _Allocate(char **, int, bool);

public:
	CStringHeap(long, long);
	~CStringHeap();

	void Clear();
	void Compact();
	char **Allocate(int, bool);
	void Free(char **, bool);
};

#endif

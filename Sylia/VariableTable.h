#ifndef f_SYLIA_VARIABLETABLE_H
#define f_SYLIA_VARIABLETABLE_H

#include "ScriptValue.h"
#include "VectorHeap.h"

class VDScriptStringHeap;

class VariableTableEntry {
public:
	VariableTableEntry *next;
	VDScriptValue v;
	char szName[1];
};

class VariableTable {
private:
	long				lHashTableSize;
	VariableTableEntry	**lpHashTable;
	VectorHeap			varheap;

	long Hash(char *szName);
	VariableTableEntry *Allocate(long lNameLen);

public:
	VariableTable(int);
	~VariableTable();

	void MarkStrings(VDScriptStringHeap& heap);

	VariableTableEntry *Lookup(char *szName);
	VariableTableEntry *Declare(char *szName);
};

#endif

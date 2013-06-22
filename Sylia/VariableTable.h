#ifndef f_SYLIA_VARIABLETABLE_H
#define f_SYLIA_VARIABLETABLE_H

#include "ScriptValue.h"
#include "VectorHeap.h"

class VariableTableEntry {
public:
	VariableTableEntry *next;
	CScriptValue v;
	char szName[];
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

	VariableTableEntry *Lookup(char *szName);
	VariableTableEntry *Declare(char *szName);
};

#endif

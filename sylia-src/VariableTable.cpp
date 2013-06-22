#include <string.h>

#include "ScriptError.h"

#include "VariableTable.h"

VariableTable::VariableTable(int ht_size) : varheap(16384) {
	long i;

	lHashTableSize	= ht_size;
	lpHashTable		= new VariableTableEntry *[ht_size];

	for(i=0; i<lHashTableSize; i++)
		lpHashTable[i] = 0;
}

VariableTable::~VariableTable() {
	delete[] lpHashTable;
}


long VariableTable::Hash(char *szName) {
	long hc = 0;
	char c;

	while(c=*szName++)
		hc = (hc + 17) * (int)(unsigned char)c;

	return hc % lHashTableSize;
}

VariableTableEntry *VariableTable::Lookup(char *szName) {
	long lHashVal = Hash(szName);
	VariableTableEntry *vte = lpHashTable[lHashVal];

	while(vte) {
		if (!strcmp(vte->szName, szName))
			return vte;

		vte = vte->next;
	}

	return NULL;
}

VariableTableEntry *VariableTable::Declare(char *szName) {
	VariableTableEntry *vte;
	long lHashVal = Hash(szName);
	long lNameLen;

	lNameLen	= strlen(szName);

	vte			= Allocate(lNameLen);
	vte->next	= lpHashTable[lHashVal];
	vte->v		= CScriptValue();
	strcpy(vte->szName, szName);

	lpHashTable[lHashVal] = vte;

	return vte;
}

VariableTableEntry *VariableTable::Allocate(long lNameLen) {
	VariableTableEntry *vte = (VariableTableEntry *)varheap.Allocate(sizeof(VariableTableEntry) + lNameLen);

	if (!vte) SCRIPT_ERROR(OUT_OF_MEMORY);

	return vte;
}

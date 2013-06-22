#include <crtdbg.h>
#include <string.h>
#include <stdlib.h>

#include <vd2/system/Props.h>

Props::Props(const PropDef *_pDef) : pDef(_pDef) {
	while(_pDef->type)
		++_pDef;
	
	nItems = _pDef - pDef;
	pData = new PropVal[nItems];

	memset(pData, 0, sizeof(PropVal)*nItems);
}

Props::Props(const Props &src)
: nItems(src.nItems)
, pDef(src.pDef)
, pData(new PropVal[src.nItems])
{
	for(int i=0; i<nItems; i++) {
		pData[i] = src.pData[i];

		if (getType(i) == PropDef::kString)
			pData[i].s = strdup(pData[i].s);
	}
}

Props::~Props() {
	int i;

	for(i=0; i<nItems; i++)
		if (getType(i) == PropDef::kString)
			free((char *)pData[i].s);

	delete[] pData;
}

const IProps& Props::operator=(const IProps& src) {
	for(int i=0; i<nItems; i++)
		switch(getType(i)) {
		case PropDef::kInt:			setInt(i, src[i].i); break;
		case PropDef::kDouble:		setDbl(i, src[i].d); break;
		case PropDef::kString:		setStr(i, src[i].s); break;
		case PropDef::kBool:		setBool(i, src[i].f); break;
		}

	return *this;
}

IProps *Props::clone() const {
	return new Props(*this);
}

const PropVal& Props::operator[](int id) const throw() {
	return pData[id];
}

void Props::setInt(int id, int i) throw() {
	_ASSERT(getType(id) == PropDef::kInt);

	pData[id].i = i;
}

void Props::setDbl(int id, double d) throw() {
	_ASSERT(getType(id) == PropDef::kDouble);

	pData[id].d = d;
}

void Props::setStr(int id, const char *s) throw() {
	_ASSERT(getType(id) == PropDef::kString);

	free((char *)pData[id].s);
	pData[id].s = strdup(s);
}

void Props::setBool(int id, bool f) throw() {
	_ASSERT(getType(id) == PropDef::kBool);

	pData[id].f = f;
}

const PropDef *Props::getDef(int id) const throw() {
	_ASSERT(id>=0 && id<nItems);

	return &pDef[id];
}

int Props::getType(int id) const throw() {
	_ASSERT(id>=0 && id<nItems);

	return pDef[id].type;
}

int Props::getCount() const throw() {
	return nItems;
}

int Props::lookup(const char *s) const throw() {
	int i;

	for(i=0; i<nItems; i++)
		if (!strcmp(pDef[i].vname, s))
			return i;

	return -1;
}

char *Props::serialize(long& len) const throw() {
	int bytes = 0;

	for(int i=0; i<nItems; i++) {
		switch(getType(i)) {
		case PropDef::kInt:
			bytes += 4 + (-bytes & 3);
			break;

		case PropDef::kDouble:
			bytes += 8 + (-bytes & 7);
			break;

		case PropDef::kBool:
			++bytes;
			break;

		case PropDef::kString:
			bytes += 1+strlen(pData[i].s);
			break;
		}
	}

	char *mem0 = (char *)malloc(bytes), *s = mem0;

	if (!mem0)
		return NULL;

	for(i=0; i<nItems; i++) {
		switch(getType(i)) {
		case PropDef::kInt:
			s += -(s-mem0)&3;
			*(int *)s = pData[i].i;
			s += sizeof(int);
			break;

		case PropDef::kDouble:
			s += -(s-mem0)&7;
			*(double *)s = pData[i].d;
			s += sizeof(double);
			break;

		case PropDef::kBool:
			*s++ = pData[i].f;
			break;

		case PropDef::kString:
			strcpy(s, pData[i].s);
			while(*s++);
			break;
		}
	}

	len = bytes;

	return mem0;
}

void Props::deserialize(const char *s) throw() {
	const char *const base = s;
	int i;

	for(i=0; i<nItems; i++)
		switch(getType(i)) {
		case PropDef::kInt:
			s += -(s-base)&3;
			setInt(i, *(int *)s);
			s += sizeof(int);
			break;

		case PropDef::kDouble:
			s += -(s-base)&7;
			setDbl(i, *(double *)s);
			s += sizeof(double);
			break;

		case PropDef::kBool:
			setBool(i, 0 != *s++);
			break;

		case PropDef::kString:
			setStr(i, s);
			while(*s++);
			break;
		}
}

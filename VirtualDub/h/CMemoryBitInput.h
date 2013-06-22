//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifndef f_CMEMORYBITINPUT_H
#define f_CMEMORYBITINPUT_H

#define XFORCEINLINE inline

#if 1

class CMemoryBitInput {
public:
	unsigned char *buf;
	int bitcnt;
	unsigned char *buf_start;
	unsigned long bitheap;

public:
	CMemoryBitInput();
	CMemoryBitInput(char *buffer);
	CMemoryBitInput(CMemoryBitInput& cmbi) {
		buf = cmbi.buf;
		buf_start = cmbi.buf_start;
		bitcnt = cmbi.bitcnt;
		bitheap = cmbi.bitheap;
	}

	bool XFORCEINLINE get_flag() {
		bool rv = (signed long)bitheap < 0;

		bitheap <<= 1;
		if (++bitcnt==0) {
			bitheap += (unsigned long)*buf++;
			bitcnt = -8;
		}

		return rv;
	}

	long XFORCEINLINE get() {
		long rv = bitheap>>31;

		bitheap <<= 1;

		if (++bitcnt==0) {
			bitheap += (unsigned long)*buf++;
			bitcnt = -8;
		}

		return rv;
	}
	long XFORCEINLINE get(unsigned char bits) {
		long rv = bitheap >> (32-bits);

		bitcnt += bits;

		bitheap <<= bits;

		refill();

		return rv;
	}
	long XFORCEINLINE get8(unsigned char bits) {
		long rv = bitheap >> (32-bits);

		bitcnt += bits;

		bitheap <<= bits;

		refill8();

		return rv;
	}
	long XFORCEINLINE get2(unsigned char bits) {
		long rv = bitheap >> (32-bits);

		bitcnt += bits;
		bitheap <<= bits;

		return rv;
	}
	long XFORCEINLINE getconst(const unsigned char bits) {
		long rv = bitheap >> (32-bits);

		bitcnt += bits;
		bitheap <<= bits;

		if (bits >= 8) {
			bitheap += ((unsigned long)*buf++) << bitcnt;
			bitcnt -= 8;
		}
		if (bits >= 16) {
			bitheap += ((unsigned long)*buf++) << bitcnt;
			bitcnt -= 8;
		}

		if (bits & 7)
			refill8();

		return rv;
	}
	long XFORCEINLINE get_signed(unsigned char bits) {
		long rv = ((signed long)bitheap) >> (32-bits);

		bitcnt += bits;
		bitheap <<= bits;

		refill();

		return rv;
	}
	long XFORCEINLINE get_signed2(unsigned char bits) {
		long rv = ((signed long)bitheap) >> (32-bits);

		bitcnt += bits;
		bitheap <<= bits;

		return rv;
	}
	long XFORCEINLINE get_signed_const(unsigned char bits) {
		long rv = (signed long)bitheap >> (32-bits);

		bitcnt += bits;
		bitheap <<= bits;

		if (bits >= 8) {
			bitheap += ((unsigned long)*buf++) << bitcnt;
			bitcnt -= 8;
		}
		if (bits >= 16) {
			bitheap += ((unsigned long)*buf++) << bitcnt;
			bitcnt -= 8;
		}
		if (bits >= 24) {
			bitheap += ((unsigned long)*buf++) << bitcnt;
			bitcnt -= 8;
		}

		if (bits & 7)
			refill8();

		return rv;
	}

	unsigned long XFORCEINLINE peek() const {
		return bitheap;
	}

	long XFORCEINLINE peek(unsigned char bits) const {
		return bitheap >> (32-bits);
	}
	long XFORCEINLINE peek8(unsigned char bits) const {
		return bitheap >> (32-bits);
	}
	void XFORCEINLINE refill() {
		while(bitcnt >= 0) {
			bitheap += ((unsigned long)*buf++) << bitcnt;
			bitcnt -= 8;
		}
	}
	void XFORCEINLINE refill8() {
		if(bitcnt >= 0) {
			bitheap += ((unsigned long)*buf++) << bitcnt;
			bitcnt -= 8;
		}
	}

	long XFORCEINLINE next() const {
		return bitheap >> 31;
	}

	bool XFORCEINLINE next(unsigned char bits, long compare) const {
		return (long)(bitheap >> (32-bits)) == compare;
	}

	void XFORCEINLINE skip() {
		bitheap <<= 1;
		if (++bitcnt==0) {
			bitheap += (unsigned long)*buf++;
			bitcnt = -8;
		}
	}
	void XFORCEINLINE skip(unsigned char bits) {
		bitcnt += bits;
		bitheap <<= bits;

		refill();
	}
	void XFORCEINLINE skip8(unsigned char bits) {
		bitcnt += bits;
		bitheap <<= bits;

		refill8();
	}
	void XFORCEINLINE skip2(unsigned char bits) {
		bitcnt += bits;
		bitheap <<= bits;
	}
	void XFORCEINLINE skipconst(const unsigned char bits) {
		bitcnt += bits;
		bitheap <<= bits;

		if (bits >= 8) {
			bitheap += ((unsigned long)*buf++) << bitcnt;
			bitcnt -= 8;
		}
		if (bits >= 16) {
			bitheap += ((unsigned long)*buf++) << bitcnt;
			bitcnt -= 8;
		}

		if (bits & 7)
			refill8();
	}

	void bytealign() {
		while(bitcnt&7) {
			bitheap <<= 1;
			bitcnt ++;
		}
		while(bitcnt >= 0) {
			bitheap += ((unsigned long)*buf++) << bitcnt;
			bitcnt -= 8;
		}
	}

	long bytecount() {
		return buf - buf_start - (24-bitcnt)/8;
	}
};

#elif 1

class CMemoryBitInput {
private:
	unsigned char *buf;
	int bitcnt;
	unsigned char *buf_start;
	unsigned long bitheap;

public:
	CMemoryBitInput();
	CMemoryBitInput(char *buffer);
	CMemoryBitInput(CMemoryBitInput& cmbi) {
		buf = cmbi.buf;
		buf_start = cmbi.buf_start;
		bitcnt = cmbi.bitcnt;
		bitheap = cmbi.bitheap;
	}

	long XFORCEINLINE get_flag() {
		if (bitcnt == 24)
			while(bitcnt >= 0) {
				bitheap += ((unsigned long)*buf++) << bitcnt;
				bitcnt -= 8;
			}

		const unsigned long rv = bitheap;

		bitheap <<= 1;
		++bitcnt;

		return (signed long)rv;
	}

	char XFORCEINLINE get() {
		char rv;
		
		if (bitcnt == 24)
			while(bitcnt >= 0) {
				bitheap += ((unsigned long)*buf++) << bitcnt;
				bitcnt -= 8;
			}

		rv = bitheap>>31;

		bitheap <<= 1;

		++bitcnt;

		return rv;
	}
	long XFORCEINLINE get(unsigned char bits) {
		long rv;

		if (bitcnt >= 24-(int)bits)
			while(bitcnt >= 0) {
				bitheap += ((unsigned long)*buf++) << bitcnt;
				bitcnt -= 8;
			}
		
		rv = bitheap >> (32-bits);

		bitcnt += bits;

		bitheap <<= bits;

		return rv;
	}
	long XFORCEINLINE get_signed(unsigned char bits) {
		long rv;

		if (bitcnt >= 24-(int)bits)
			while(bitcnt >= 0) {
				bitheap += ((unsigned long)*buf++) << bitcnt;
				bitcnt -= 8;
			}
		
		rv = (signed long)bitheap >> (32-bits);

		bitcnt += bits;

		bitheap <<= bits;

		return rv;
	}
	long XFORCEINLINE peek(unsigned char bits) {
		long rv;

		if (bitcnt >= 24-(int)bits)
			while(bitcnt >= 0) {
				bitheap += ((unsigned long)*buf++) << bitcnt;
				bitcnt -= 8;
			}
		
		return bitheap >> (32-bits);
	}
	long XFORCEINLINE peek8(unsigned char bits) {
		long rv;

		if (bitcnt >= 24-(int)bits)
			while(bitcnt >= 0) {
				bitheap += ((unsigned long)*buf++) << bitcnt;
				bitcnt -= 8;
			}
		
		return bitheap >> (32-bits);
	}

	long XFORCEINLINE next() {
		long rv;

		if (bitcnt == 24)
			while(bitcnt >= 0) {
				bitheap += ((unsigned long)*buf++) << bitcnt;
				bitcnt -= 8;
			}
		
		return bitheap >> 31;
	}

	bool XFORCEINLINE next(unsigned char bits, long compare) {
		long rv;

		if (bitcnt >= 24-(int)bits)
			while(bitcnt >= 0) {
				bitheap += ((unsigned long)*buf++) << bitcnt;
				bitcnt -= 8;
			}
		
		return (long)(bitheap >> (32-bits)) == compare;
	}

	void XFORCEINLINE skip(unsigned char bits) {
		bitcnt += bits;
		bitheap <<= bits;

	}

	void bytealign() {
		while(bitcnt&7) {
			bitheap <<= 1;
			bitcnt ++;
		}
		while(bitcnt >= 0) {
			bitheap += ((unsigned long)*buf++) << bitcnt;
			bitcnt -= 8;
		}
	}

	long bytecount() {
		return buf - buf_start - (24-bitcnt)/8;
	}
};

#else

class CMemoryBitInput {
private:
	unsigned char *buf;
	int bitcnt;
	unsigned char *buf_start;

public:
	CMemoryBitInput();
	CMemoryBitInput(char *buffer);
	CMemoryBitInput(CMemoryBitInput& cmbi) {
		buf = cmbi.buf;
		buf_start = cmbi.buf_start;
		bitcnt = cmbi.bitcnt;
	}

	char get_flag() {
		char rv = buf[0] & (0x80 >> bitcnt);

		if (++bitcnt > 7) {
			bitcnt = 0;
			++buf;
		}

		return rv;
	}

	char get() {
		char rv = (buf[0]>>(7-bitcnt)) & 1;

		if (++bitcnt > 7) {
			bitcnt = 0;
			++buf;
		}

		return rv;
	}
	long get(unsigned char bits) {
		unsigned long rv;
		unsigned char *buf2 = buf;

		_asm {
			mov eax,[buf2]
			mov eax,[eax]
			bswap eax
			mov rv,eax

		}
//		rv = (unsigned long)(((unsigned long)buf[0] << 24) | ((unsigned long)buf[1] << 16) | ((unsigned long)buf[2] << 8) | (unsigned long)buf[3]);
//		if (bitcnt) rv = (rv<<bitcnt) | (buf[4]>>(8-bitcnt));
		rv = (rv<<bitcnt) | ((unsigned long)buf[4]>>(8-bitcnt));

		rv >>= (32-bits);

		bitcnt += bits;
		buf += bitcnt>>3;
		bitcnt &= 7;

		return rv;
	}
	long peek(unsigned char bits) const {
		unsigned long rv;
		unsigned char *buf2 = buf;

		_asm {
			mov eax,buf2
			mov eax,[eax]
			bswap eax
			mov rv,eax
		}

		//rv = (unsigned long)(((unsigned long)buf[0] << 24) | ((unsigned long)buf[1] << 16) | ((unsigned long)buf[2] << 8) | (unsigned long)buf[3]);
//		if (bitcnt) rv = (rv<<bitcnt) | (buf[4]>>(8-bitcnt));
//		rv<<=bitcnt;

		return (rv >> ((32-bits)-bitcnt)) & ((1L<<bits)-1);
	}
	long peek8(unsigned char bits) const {
		unsigned long rv;

		rv = (unsigned long)(((unsigned long)buf[0] << 8) | (unsigned long)buf[1]);

		return (rv >> ((16-bits)-bitcnt)) & ((1L<<bits)-1);
	}

	long next() const {
		return (buf[0]>>(7-(bitcnt&7))) & 1;
	}

	long next(unsigned char bits, long compare) const {
		unsigned long rv;
		unsigned char *buf2 = buf;

		_asm {
			mov eax,buf2
			mov eax,[eax]
			bswap eax
			mov rv,eax
		}

//		rv = (unsigned long)(((unsigned long)buf[0] << 24) | ((unsigned long)buf[1] << 16) | ((unsigned long)buf[2] << 8) | (unsigned long)buf[3]);
		if (bitcnt) rv = (rv<<bitcnt) | (buf[4]>>(8-bitcnt));

		return (long)(rv >> (32-bits)) == compare;
	}
	void skip(unsigned char bits) {
		bitcnt += bits;
		buf += bitcnt>>3;
		bitcnt &= 7;
	}
	long bytecount() {
		return buf - buf_start;
	}
};

#endif

#undef INLINE

#endif

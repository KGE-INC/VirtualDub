#ifndef f_VD2_KASUMI_PIXMAPUTILS_H
#define f_VD2_KASUMI_PIXMAPUTILS_H

#include <vd2/Kasumi/pixmap.h>

struct VDPixmapFormatInfo {
	const char *name;		// debugging name
	int qw, qh;				// width, height of a quantum
	int	qwbits, qhbits;		// width and height of a quantum as shifts
	int qsize;				// size of a pixel in bytes
	int auxbufs;			// number of auxiliary buffers (0 for chunky formats, usually 2 for planar)
	int	auxwbits, auxhbits;	// subsampling factors for auxiliary buffers in shifts
	int palsize;			// entries in palette
	int subformats;			// number of subformats for this format
};

extern VDPixmapFormatInfo g_vdPixmapFormats[];

inline const VDPixmapFormatInfo& VDPixmapGetInfo(sint32 format) {
	VDASSERT((uint32)format < nsVDPixmap::kPixFormat_Max_Standard);
	return g_vdPixmapFormats[format];
}

#ifdef _DEBUG
	bool VDAssertValidPixmap(const VDPixmap& px);
#else
	inline bool VDAssertValidPixmap(const VDPixmap& px) { return true; }
#endif

inline VDPixmap VDPixmapFromLayout(const VDPixmapLayout& layout, void *p) {
	VDPixmap px;

	px.data		= (char *)p + layout.data;
	px.data2	= (char *)p + layout.data2;
	px.data3	= (char *)p + layout.data3;
	px.format	= layout.format;
	px.w		= layout.w;
	px.h		= layout.h;
	px.palette	= layout.palette;
	px.pitch	= layout.pitch;
	px.pitch2	= layout.pitch2;
	px.pitch3	= layout.pitch3;

	return px;
}

inline VDPixmapLayout VDPixmapToLayout(const VDPixmap& px, void *&p) {
	VDPixmapLayout layout;
	p = px.data;
	layout.data		= 0;
	layout.data2	= (char *)px.data2 - (char *)px.data;
	layout.data3	= (char *)px.data3 - (char *)px.data;
	layout.format	= px.format;
	layout.w		= px.w;
	layout.h		= px.h;
	layout.palette	= px.palette;
	layout.pitch	= px.pitch;
	layout.pitch2	= px.pitch2;
	layout.pitch3	= px.pitch3;
	return layout;
}

uint32 VDPixmapCreateLinearLayout(VDPixmapLayout& layout, int format, vdpixsize w, vdpixsize h, int alignment);

VDPixmap VDPixmapOffset(const VDPixmap& src, vdpixpos x, vdpixpos y);
VDPixmapLayout VDPixmapLayoutOffset(const VDPixmapLayout& src, vdpixpos x, vdpixpos y);

void VDPixmapFlipV(VDPixmap& layout);
void VDPixmapLayoutFlipV(VDPixmapLayout& layout);


#ifndef VDPTRSTEP_DECLARED
	template<class T>
	void vdptrstep(T *&p, ptrdiff_t offset) {
		p = (T *)((char *)p + offset);
	}
#endif
#ifndef VDPTROFFSET_DECLARED
	template<class T>
	T *vdptroffset(T *p, ptrdiff_t offset) {
		return (T *)((char *)p + offset);
	}
#endif


typedef void *(*tpVDPixBltTable)[nsVDPixmap::kPixFormat_Max_Standard];

tpVDPixBltTable VDGetPixBltTableReference();
tpVDPixBltTable VDGetPixBltTableX86Scalar();
tpVDPixBltTable VDGetPixBltTableX86MMX();



class VDPixmapBuffer : public VDPixmap {
public:
	VDPixmapBuffer() : pBuffer(NULL) {}
	VDPixmapBuffer(const VDPixmap& src);
	VDPixmapBuffer(sint32 w, sint32 h, int format) : pBuffer(NULL) {
		init(w, h, format);
	}

	~VDPixmapBuffer() {
		delete[] pBuffer;
	}

	void clear() {
		delete[] pBuffer;
		pBuffer = NULL;
		format = nsVDPixmap::kPixFormat_Null;
	}
	void init(sint32 w, sint32 h, int format);

	void assign(const VDPixmap& src);

protected:
	char *pBuffer;
};


#endif

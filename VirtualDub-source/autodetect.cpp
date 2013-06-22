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

#include <math.h>
#include <windows.h>

#include "VideoSource.h"
#include "command.h"

///////////////////////////////////////////////////////////////////////////

#define R(x) ((((x)&0xFF000000)>>24) | (((x)&0x00FF0000)>>8) | (((x)&0x0000FF00)<<8) | (((x)&0x000000FF)<<24))

const struct CodecEntry {
	FOURCC fcc;
	const char *name;
} codec_entries[]={
	{ R('VCR1'), "ATI video 1" },
	{ R('VCR2'), "ATI video 2" },
	{ R('TR20'), "Duck TrueMotion 2.0" },
	{ R('dvsd'), "DV" },
	{ R('HFYU'), "Huffyuv" },
	{ R('I263'), "Intel H.263" },
	{ R('I420'), "LifeView YUV12 codec" },
	{ R('IR21'), "Indeo Video 2.1" },
	{ R('IV31'), "Indeo Video 3.1" },
	{ R('IV32'), "Indeo Video 3.2" },
	{ R('IV41'), "Indeo Video 4.1" },
	{ R('IV50'), "Indeo Video 5.x" },
	{ R('UCOD'), "Iterated Systems' ClearVideo" },
	{ R('mjpg'), "Motion JPEG" },
	{ R('MJPG'), "Motion JPEG" },
	{ R('dmb1'), "Motion JPEG (Matrox)" },
	{ R('MPG4'), "Microsoft High-Speed MPEG-4 " },
	{ R('MP42'), "Microsoft High-Speed MPEG-4 V2" },
	{ R('MP43'), "Microsoft High-Speed MPEG-4 V3" },
	{ R('DIV3'), "Microsoft High-Speed MPEG-4 V3 [Hack: DivX Low-Motion]" },
	{ R('DIV4'), "Microsoft High-Speed MPEG-4 V3 [Hack: DivX Fast-Motion]" },
	{ R('AP41'), "Microsoft High-Speed MPEG-4 V3 [Hack: AngelPotion Definitive]" },
	{ R('MRLE'), "Microsoft RLE" },
	{ R('MSVC'), "Microsoft Video 1" },
	{ R('CRAM'), "Microsoft Video 1" },
	{ R('DIVX'), "OpenDIVX" },
	{ R('CVID'), "Radius Cinepak" },
	{ R('VIVO'), "VivoActive" },

	{ R('VDST'), "VirtualDub frameclient driver" },
};

#undef R

const char *LookupVideoCodec(FOURCC fccType) {
	int i;

	for(i=0; i<3; i++) {
		int c = (int)((fccType>>(8*i)) & 255);

		if (isalpha(c))
			fccType = (fccType & ~(FOURCC)(0xff << (i*8))) | (toupper(c) << (i*8));
	}

	for(i=0; i<sizeof codec_entries/sizeof codec_entries[0]; i++)
		if (codec_entries[i].fcc == fccType)
			return codec_entries[i].name;

	return NULL;
}

///////////////////////////////////////////////////////////////////////////

#if 0
struct AutodetectEntry {
	int iWidth, iHeight;
	long lMicroSecsPerFrame;
	long lFrames;
	char *lpszName;
} ad_entries[]={

	{ 240, 180, 33333, 3524, "AD Police opening"							},
	{ 320, 240, 33333, 2657, "Ah My Goddess! opening"						},
	{ 160, 128, 33333, 2755, "Ah My Goddess! opening"						},
	{ 240, 180, 33333, 2790, "Ah My Goddess! opening"						},
	{ 320, 240, 33333, 2774, "Ah My Goddess! ending"						},
	{ 160, 128, 41667, 1792, "Armitage III opening"							},
	{ 320, 240, 33333, 2718, "Bakuretsu Hunter opening"						},
	{ 320, 240, 66667, 1354, "Blue Seed opening"							},
	{ 352, 240, 33367, 6903, "Bubblegum Crisis - Rock Me"					},
	{ 352, 240, 33367, 5728, "Bubblegum Crisis - Touchdown to Tomorrow"		},
	{ 352, 240, 33367, 3606, "Bubblegum Crash opening"						},
	{ 160, 120, 66667, 1487, "Cat Girl Nuku Nuku opening #1"				},
	{ 160, 120, 66667, 1536, "Cat Girl Nuku Nuku opening #2"				},
	{ 160, 120, 33333, 3063, "Cat Girl Nuku Nuku opening #2"				},
	{ 320, 240, 66738, 1491, "DNA^2 opening"								},
	{ 160, 128, 33333, 2765, "DNA^2 opening"								},
	{ 160, 128, 41667, 2488, "El-Hazard opening #2"							},
	{ 320, 240, 41667, 2736, "El-Hazard ending"								},
	{ 160, 128, 41667, 2301, "El-Hazard opening #1"							},
	{ 352, 240, 33367, 2988, "El-Hazard TV opening #1"						},
	{ 320, 240, 33333, 2955, "El-Hazard TV opening #1"						},
	{ 160, 120, 99887, 1180, "El-Hazard TV opening #2"						},
	{ 240, 180, 33333, 3405, "El-Hazard TV opening #2"						},
	{ 320, 240, 33367, 4292, "Final Fantasy VIII preview"					},
	{ 352, 240, 33367, 2763, "Fushigi Yuugi opening"						},
	{ 320, 240, 33333, 2709, "Fushigi Yuugi opening"						},
	{ 320, 240, 33367, 2402, "Fushigi Yuugi ending"							},
	{ 320, 240, 66667, 1381, "Idol Project opening"							},
	{ 288, 224, 41667, 2378, "Iria opening"									},
	{ 352, 240, 33333, 3033, "Key the Metal Idol opening"					},
	{ 352, 224, 33367, 2748, "Magical Knights Rayearth opening #1"			},
	{ 320, 240, 33333, 2772, "Magical Knights Rayearth opening #1"			},
	{ 320, 240, 33333, 2711, "Magical Knights Rayearth opening #2"			},
	{ 160, 120, 99971, 2325, "Magical Knights Rayearth OAV opening"			},
	{ 320, 240, 33333, 2805, "Maison Ikkoku opening"						},
	{ 320, 240, 66667, 1350, "Marmalade Boy opening"						},
	{ 316, 236, 33333, 2701, "Martian Successor Nadesico opening"			},
	{ 320, 240, 66667, 1376, "Martian Successor Nadesico opening"			},
	{ 316, 240, 66667,  897, "Martian Successor Nadesico ending"			},
	{ 320, 240, 66667, 2159, "Martian Successor Nadesico - Yurika song"		},
	{ 352, 240, 33367, 2698, "Neon Genesis Evangelion opening #1"			},
	{ 160, 120, 33405, 2733, "Neon Genesis Evangelion opening #1"			},
	{ 240, 180, 33333, 2709, "Neon Genesis Evangelion opening #1"			},
	{ 320, 240, 55444, 1643, "Neon Genesis Evangelion opening #1 (subtitled)" },
	{ 320, 240, 33333, 2708, "Neon Genesis Evangelion opening #2"			},
	{ 160, 120, 66667, 1431, "Nurse Angel Ririka opening"					},
	{ 320, 240, 33333, 2722, "Pretty Sammy opening"							},
	{ 320, 240, 41667, 2176, "Rurouni Kenshin opening"						},
	{ 352, 240, 33367, 3080, "Rurouni Kenshin opening"						},
	{ 316, 236,100000,  913, "Sailor Moon SuperS movie opening"				},
	{ 240, 180, 33333, 2529, "Tenchi Muyo TV opening"						},
	{ 352, 240, 33367, 2088, "Visions of Escaflowne ending"					},
	{ 320, 227,100000,  693, "Visions of Escaflowne ending"					},
	{ 160, 120, 33405, 2081, "Visions of Escaflowne ending"					},
	{ 320, 240, 33367, 2648, "Visions of Escaflowne opening"				},
	{ 320, 240, 66740, 1361, "Visions of Escaflowne opening"				},
	{ 316, 240, 66667, 1047, "Wedding Peach ending"							},
	{ 316, 240, 66667, 1333, "Wedding Peach opening"						},
	{ 240, 180, 33333, 3010, "You're Under Arrest opening"					},
	{ 320, 240, 33367, 2560, "Yu Yu Hakusho opening"						},
	{ 320, 240, 33333, 2610, "Yu Yu Hakusho opening"						},

	{ 0 }

};

char *AutodetectFile(VideoSource *pvs) {
	AutodetectEntry *pae = ad_entries;

	while(pae->iWidth) {
		if (pae->iWidth == pvs->getImageFormat()->biWidth
			&& pae->iHeight == pvs->getImageFormat()->biHeight
			&& abs(pae->lMicroSecsPerFrame - MulDiv(pvs->streamInfo.dwScale, 1000000, pvs->streamInfo.dwRate))<100
			&& pae->lFrames == (pvs->lSampleLast - pvs->lSampleFirst)
			)

			return pae->lpszName;

		++pae;
	}

	return NULL;
}
#else
char *AutodetectFile(VideoSource *pvs) {
	return NULL;
}
#endif

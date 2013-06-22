#include "stdafx.h"

#include <list>
#include <vector>
#include <utility>
#include <vd2/system/cache.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/error.h>
#include <vd2/system/protscope.h>
#include <vd2/system/thread.h>
#include <vd2/system/time.h>
#include <vd2/system/refcount.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/VDScheduler.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/w32assist.h>
#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdvideofilt.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Kasumi/region.h>
#include "VideoFilterSystem.h"
#include "VideoFilterFrame.h"
#include "backface.h"

///////////////////////////////////////////////////////////////////////////

#pragma warning(disable: 4355)		// warning C4355: 'this' : used in base member initializer list

class VDVideoFrameRequest;

//#define VDDEBUG_FILT	VDDEBUG
#define VDDEBUG_FILT	(void)sizeof printf

//#define DEBUG_ENABLE_FRAME_NUMBERS

///////////////////////////////////////////////////////////////////////////

namespace {
	const sint32 kPoints_30[][2]={
		{ 0x00004e66, 0xffff32cd },	{ 0x00007a22, 0xffff32cd },	{ 0x00009000, 0xffff55bc },	{ 0x00009000, 0xffff9b9a },
		{ 0x00009000, 0xffffe0ef },	{ 0x00007a22, 0x00000399 },	{ 0x00004e66, 0x00000399 },	{ 0x000022aa, 0x00000399 },
		{ 0x00000ccc, 0xffffe0ef },	{ 0x00000ccc, 0xffff9b9a },	{ 0x00000ccc, 0xffff55bc },	{ 0x000022aa, 0xffff32cd },
		{ 0x00004e66, 0xffff32cd },	{ 0x00004e66, 0xffffef34 },	{ 0x00005aef, 0xffffef34 },	{ 0x00006488, 0xffffe911 },
		{ 0x00006b33, 0xffffdccd },	{ 0x000071dd, 0xffffd089 },	{ 0x00007533, 0xffffbacd },	{ 0x00007533, 0xffff9b9a },
		{ 0x00007533, 0xffff7c23 },	{ 0x000071dd, 0xffff6634 },	{ 0x00006b33, 0xffff59cd },	{ 0x00006488, 0xffff4d67 },
		{ 0x00005aef, 0xffff4734 },	{ 0x00004e66, 0xffff4734 },	{ 0x00004199, 0xffff4734 },	{ 0x000037ef, 0xffff4d67 },
		{ 0x00003166, 0xffff59cd },	{ 0x00002add, 0xffff6634 },	{ 0x00002799, 0xffff7c23 },	{ 0x00002799, 0xffff9b9a },
		{ 0x00002799, 0xffffbacd },	{ 0x00002add, 0xffffd089 },	{ 0x00003166, 0xffffdccd },	{ 0x000037ef, 0xffffe911 },
		{ 0x00004199, 0xffffef34 },	{ 0x00004e66, 0xffffef34 },
	};

	const uint8 kCommands_30[]={
		1,4,4,4,4,2,1,4,4,4,4,4,4,4,4,2,0
	};

	const sint32 kPoints_31[][2]={
		{ 0x00006999, 0x00000000 },	{ 0x00004f99, 0x00000000 },	{ 0x00004f99, 0xffff619a },	{ 0x00004533, 0xffff6889 },
		{ 0x00003511, 0xffff6c00 },	{ 0x00001f33, 0xffff6c00 },	{ 0x00001f33, 0xffff579a },	{ 0x00003eef, 0xffff579a },
		{ 0x00005111, 0xffff4b56 },	{ 0x00005599, 0xffff32cd },	{ 0x00006999, 0xffff32cd },
	};

	const uint8 kCommands_31[]={
		1,3,3,4,3,4,3,2,0
	};

	const sint32 kPoints_32[][2]={
		{ 0x00008e00, 0x00000000 },	{ 0x000008cc, 0x00000000 },	{ 0x000008cc, 0xfffff3bc },	{ 0x00000c99, 0xffffe800 },
		{ 0x00001433, 0xffffdccd },	{ 0x00001bcc, 0xffffd19a },	{ 0x000029bb, 0xffffc3cd },	{ 0x00003e00, 0xffffb367 },
		{ 0x00005244, 0xffffa300 },	{ 0x00006022, 0xffff95ef },	{ 0x00006799, 0xffff8c34 },	{ 0x00006f11, 0xffff8278 },
		{ 0x000072cc, 0xffff7778 },	{ 0x000072cc, 0xffff6b34 },	{ 0x000072cc, 0xffff6000 },	{ 0x00006f99, 0xffff5734 },
		{ 0x00006933, 0xffff50cd },	{ 0x000062cc, 0xffff4a67 },	{ 0x000059dd, 0xffff4734 },	{ 0x00004e66, 0xffff4734 },
		{ 0x00004444, 0xffff4734 },	{ 0x00003b99, 0xffff4a67 },	{ 0x00003466, 0xffff50cd },	{ 0x00002d33, 0xffff5734 },
		{ 0x00002933, 0xffff60ab },	{ 0x00002866, 0xffff6d34 },	{ 0x00000d99, 0xffff6d34 },	{ 0x00000eef, 0xffff5b9a },
		{ 0x00001566, 0xffff4d78 },	{ 0x00002100, 0xffff42cd },	{ 0x00002c99, 0xffff3823 },	{ 0x00003bdd, 0xffff32cd },
		{ 0x00004ecc, 0xffff32cd },	{ 0x000062cc, 0xffff32cd },	{ 0x00007244, 0xffff3834 },	{ 0x00007d33, 0xffff4300 },
		{ 0x00008822, 0xffff4dcd },	{ 0x00008d99, 0xffff5b56 },	{ 0x00008d99, 0xffff6b9a },	{ 0x00008d99, 0xffff7a45 },
		{ 0x00008999, 0xffff8734 },	{ 0x00008199, 0xffff9267 },	{ 0x00007999, 0xffff9d9a },	{ 0x00006999, 0xffffad34 },
		{ 0x00005199, 0xffffc134 },	{ 0x00003999, 0xffffd534 },	{ 0x00002d55, 0xffffe223 },	{ 0x00002ccc, 0xffffe800 },
		{ 0x00008e00, 0xffffe800 },
	};

	const uint8 kCommands_32[]={
		1,3,4,4,4,4,4,4,4,4,3,4,4,4,4,4,4,4,3,2,0
	};

	const sint32 kPoints_33[][2]={
		{ 0x00006a00, 0xffff9134 },	{ 0x00007644, 0xffff9467 },	{ 0x00007fbb, 0xffff9aab },	{ 0x00008666, 0xffffa400 },
		{ 0x00008d11, 0xffffad56 },	{ 0x00009066, 0xffffb823 },	{ 0x00009066, 0xffffc467 },	{ 0x00009066, 0xffffd711 },
		{ 0x00008a11, 0xffffe645 },	{ 0x00007d66, 0xfffff200 },	{ 0x000070bb, 0xfffffdbc },	{ 0x000060ef, 0x00000399 },
		{ 0x00004e00, 0x00000399 },	{ 0x00003b11, 0x00000399 },	{ 0x00002b99, 0xfffffe67 },	{ 0x00001f99, 0xfffff400 },
		{ 0x00001399, 0xffffe99a },	{ 0x00000d33, 0xffffdbde },	{ 0x00000c66, 0xffffcacd },	{ 0x00002733, 0xffffcacd },
		{ 0x00002733, 0xffffd4ab },	{ 0x00002a88, 0xffffdd34 },	{ 0x00003133, 0xffffe467 },	{ 0x000037dd, 0xffffeb9a },
		{ 0x00004177, 0xffffef34 },	{ 0x00004e00, 0xffffef34 },	{ 0x00005a00, 0xffffef34 },	{ 0x00006399, 0xffffeb45 },
		{ 0x00006acc, 0xffffe367 },	{ 0x00007200, 0xffffdb89 },	{ 0x00007599, 0xffffd178 },	{ 0x00007599, 0xffffc534 },
		{ 0x00007599, 0xffffb867 },	{ 0x000071dd, 0xffffae9a },	{ 0x00006a66, 0xffffa7cd },	{ 0x000062ef, 0xffffa100 },
		{ 0x00005822, 0xffff9d9a },	{ 0x00004a00, 0xffff9d9a },	{ 0x00004000, 0xffff9d9a },	{ 0x00004000, 0xffff899a },
		{ 0x00004600, 0xffff899a },	{ 0x00005f11, 0xffff899a },	{ 0x00006b99, 0xffff7e00 },	{ 0x00006b99, 0xffff66cd },
		{ 0x00006b99, 0xffff5cab },	{ 0x000068dd, 0xffff54de },	{ 0x00006366, 0xffff4f67 },	{ 0x00005def, 0xffff49ef },
		{ 0x00005622, 0xffff4734 },	{ 0x00004c00, 0xffff4734 },	{ 0x00003777, 0xffff4734 },	{ 0x00002c00, 0xffff51de },
		{ 0x00002999, 0xffff6734 },	{ 0x00000ecc, 0xffff6734 },	{ 0x000010ef, 0xffff5623 },	{ 0x00001788, 0xffff4923 },
		{ 0x00002299, 0xffff4034 },	{ 0x00002daa, 0xffff3745 },	{ 0x00003b55, 0xffff32cd },	{ 0x00004b99, 0xffff32cd },
		{ 0x00005d77, 0xffff32cd },	{ 0x00006bbb, 0xffff37bc },	{ 0x00007666, 0xffff419a },	{ 0x00008111, 0xffff4b78 },
		{ 0x00008666, 0xffff5800 },	{ 0x00008666, 0xffff6734 },	{ 0x00008666, 0xffff7000 },	{ 0x00008411, 0xffff7834 },
		{ 0x00007f66, 0xffff7fcd },	{ 0x00007abb, 0xffff8767 },	{ 0x00007399, 0xffff8d34 },	{ 0x00006a00, 0xffff9134 },
	};

	const uint8 kCommands_33[]={
		1,4,4,4,4,4,4,3,4,4,4,4,4,4,3,3,3,4,4,4,4,3,4,4,4,4,4,4,2,0
	};

	const sint32 kPoints_34[][2]={
		{ 0x00008f99, 0xffffd000 },	{ 0x00007466, 0xffffd000 },	{ 0x00007466, 0x00000000 },	{ 0x00005a66, 0x00000000 },
		{ 0x00005a66, 0xffffd000 },	{ 0x00000466, 0xffffd000 },	{ 0x00000466, 0xffffb934 },	{ 0x00006000, 0xffff3400 },
		{ 0x00007466, 0xffff3400 },	{ 0x00007466, 0xffffb934 },	{ 0x00008f99, 0xffffb934 },	{ 0x00005a66, 0xffffb934 },
		{ 0x00005a66, 0xffff5ccd },	{ 0x00001c66, 0xffffb934 },
	};

	const uint8 kCommands_34[]={
		1,3,3,3,3,3,3,3,3,3,3,2,1,3,3,2,0
	};

	const sint32 kPoints_35[][2]={
		{ 0x00002d33, 0xffff8667 },	{ 0x00003755, 0xffff7e67 },	{ 0x000043dd, 0xffff7a67 },	{ 0x000052cc, 0xffff7a67 },
		{ 0x00006399, 0xffff7a67 },	{ 0x00007233, 0xffff8056 },	{ 0x00007e99, 0xffff8c34 },	{ 0x00008b00, 0xffff9811 },
		{ 0x00009133, 0xffffa7de },	{ 0x00009133, 0xffffbb9a },	{ 0x00009133, 0xffffd023 },	{ 0x00008b00, 0xffffe145 },
		{ 0x00007e99, 0xffffef00 },	{ 0x00007233, 0xfffffcbc },	{ 0x00006177, 0x00000399 },	{ 0x00004c66, 0x00000399 },
		{ 0x00003b99, 0x00000399 },	{ 0x00002d22, 0xfffffede },	{ 0x00002100, 0xfffff567 },	{ 0x000014dd, 0xffffebef },
		{ 0x00000ddd, 0xffffddde },	{ 0x00000c00, 0xffffcb34 },	{ 0x000026cc, 0xffffcb34 },	{ 0x00002822, 0xffffd734 },
		{ 0x00002c55, 0xffffe034 },	{ 0x00003366, 0xffffe634 },	{ 0x00003a77, 0xffffec34 },	{ 0x000042cc, 0xffffef34 },
		{ 0x00004c66, 0xffffef34 },	{ 0x000058ef, 0xffffef34 },	{ 0x00006311, 0xffffea78 },	{ 0x00006acc, 0xffffe100 },
		{ 0x00007288, 0xffffd789 },	{ 0x00007666, 0xffffcbde },	{ 0x00007666, 0xffffbe00 },	{ 0x00007666, 0xffffafde },
		{ 0x00007277, 0xffffa4bc },	{ 0x00006a99, 0xffff9c9a },	{ 0x000062bb, 0xffff9478 },	{ 0x00005888, 0xffff9067 },
		{ 0x00004c00, 0xffff9067 },	{ 0x00003d99, 0xffff9067 },	{ 0x00003177, 0xffff95de },	{ 0x00002799, 0xffffa0cd },
		{ 0x00001333, 0xffffa0cd },	{ 0x00001b99, 0xffff3667 },	{ 0x00008799, 0xffff3667 },	{ 0x00008799, 0xffff4e67 },
		{ 0x00003199, 0xffff4e67 },
	};

	const uint8 kCommands_35[]={
		1,4,4,4,4,4,4,4,3,4,4,4,4,4,4,4,3,3,3,3,3,2,0
	};

	const sint32 kPoints_36[][2]={
		{ 0x000026cc, 0xffff9334 },	{ 0x00003422, 0xffff8489 },	{ 0x00004377, 0xffff7d34 },	{ 0x000054cc, 0xffff7d34 },
		{ 0x00006599, 0xffff7d34 },	{ 0x000073cc, 0xffff8356 },	{ 0x00007f66, 0xffff8f9a },	{ 0x00008b00, 0xffff9bde },
		{ 0x000090cc, 0xffffab78 },	{ 0x000090cc, 0xffffbe67 },	{ 0x000090cc, 0xffffd19a },	{ 0x00008b11, 0xffffe1ef },
		{ 0x00007f99, 0xffffef67 },	{ 0x00007422, 0xfffffcde },	{ 0x000064cc, 0x00000399 },	{ 0x00005199, 0x00000399 },
		{ 0x00003bbb, 0x00000399 },	{ 0x00002acc, 0xfffffb23 },	{ 0x00001ecc, 0xffffea34 },	{ 0x000012cc, 0xffffd945 },
		{ 0x00000ccc, 0xffffc111 },	{ 0x00000ccc, 0xffffa19a },	{ 0x00000ccc, 0xffff57bc },	{ 0x00002488, 0xffff32cd },
		{ 0x00005400, 0xffff32cd },	{ 0x000062ef, 0xffff32cd },	{ 0x00006faa, 0xffff3711 },	{ 0x00007a33, 0xffff3f9a },
		{ 0x000084bb, 0xffff4823 },	{ 0x00008b11, 0xffff54ab },	{ 0x00008d33, 0xffff6534 },	{ 0x000072cc, 0xffff6534 },
		{ 0x00007066, 0xffff5134 },	{ 0x00006599, 0xffff4734 },	{ 0x00005266, 0xffff4734 },	{ 0x00004622, 0xffff4734 },
		{ 0x00003c22, 0xffff4dbc },	{ 0x00003466, 0xffff5acd },	{ 0x00002caa, 0xffff67de },	{ 0x00002822, 0xffff7aab },
		{ 0x000026cc, 0xffff9334 },	{ 0x000026cc, 0xffffa734 },	{ 0x00002711, 0xffffbe67 },	{ 0x00002b00, 0xffffd034 },
		{ 0x00003299, 0xffffdc9a },	{ 0x00003a33, 0xffffe900 },	{ 0x00004466, 0xffffef34 },	{ 0x00005133, 0xffffef34 },
		{ 0x00005c66, 0xffffef34 },	{ 0x00006555, 0xffffeaab },	{ 0x00006c00, 0xffffe19a },	{ 0x000072aa, 0xffffd889 },
		{ 0x00007600, 0xffffcd56 },	{ 0x00007600, 0xffffc000 },	{ 0x00007600, 0xffffb267 },	{ 0x00007299, 0xffffa745 },
		{ 0x00006bcc, 0xffff9e9a },	{ 0x00006500, 0xffff95ef },	{ 0x00005bdd, 0xffff919a },	{ 0x00005066, 0xffff919a },
		{ 0x00004200, 0xffff919a },	{ 0x00003422, 0xffff98cd },	{ 0x000026cc, 0xffffa734 },
	};

	const uint8 kCommands_36[]={
		1,4,4,4,4,4,4,4,4,4,4,3,4,4,4,2,1,4,4,4,4,4,4,4,2,0
	};

	const sint32 kPoints_37[][2]={
		{ 0x00009000, 0xffff48cd },	{ 0x000046cc, 0x00000000 },	{ 0x00002999, 0x00000000 },	{ 0x000074cc, 0xffff4e67 },
		{ 0x00000d99, 0xffff4e67 },	{ 0x00000d99, 0xffff3667 },	{ 0x00009000, 0xffff3667 },
	};

	const uint8 kCommands_37[]={
		1,3,3,3,3,3,3,2,
	};

	const sint32 kPoints_38[][2]={
		{ 0x000066cc, 0xffff9600 },	{ 0x000082cc, 0xffffa311 },	{ 0x000090cc, 0xffffb489 },	{ 0x000090cc, 0xffffca67 },
		{ 0x000090cc, 0xffffdb78 },	{ 0x00008aaa, 0xffffe945 },	{ 0x00007e66, 0xfffff3cd },	{ 0x00007222, 0xfffffe56 },
		{ 0x00006222, 0x00000399 },	{ 0x00004e66, 0x00000399 },	{ 0x00003a66, 0x00000399 },	{ 0x00002a55, 0xfffffe56 },
		{ 0x00001e33, 0xfffff3cd },	{ 0x00001211, 0xffffe945 },	{ 0x00000c00, 0xffffdb78 },	{ 0x00000c00, 0xffffca67 },
		{ 0x00000c00, 0xffffb489 },	{ 0x000019dd, 0xffffa311 },	{ 0x00003599, 0xffff9600 },	{ 0x00001eef, 0xffff88ef },
		{ 0x00001399, 0xffff7889 },	{ 0x00001399, 0xffff64cd },	{ 0x00001399, 0xffff5667 },	{ 0x00001900, 0xffff4a78 },
		{ 0x000023cc, 0xffff4100 },	{ 0x00002e99, 0xffff3789 },	{ 0x00003ccc, 0xffff32cd },	{ 0x00004e66, 0xffff32cd },
		{ 0x00006000, 0xffff32cd },	{ 0x00006e22, 0xffff3789 },	{ 0x000078cc, 0xffff4100 },	{ 0x00008377, 0xffff4a78 },
		{ 0x000088cc, 0xffff5667 },	{ 0x000088cc, 0xffff64cd },	{ 0x000088cc, 0xffff7889 },	{ 0x00007d77, 0xffff88ef },
		{ 0x000066cc, 0xffff9600 },	{ 0x00004e66, 0xffff8b34 },	{ 0x00006377, 0xffff81de },	{ 0x00006e00, 0xffff7511 },
		{ 0x00006e00, 0xffff64cd },	{ 0x00006e00, 0xffff5bbc },	{ 0x00006b33, 0xffff5489 },	{ 0x00006599, 0xffff4f34 },
		{ 0x00006000, 0xffff49de },	{ 0x00005844, 0xffff4734 },	{ 0x00004e66, 0xffff4734 },	{ 0x00004444, 0xffff4734 },
		{ 0x00003c66, 0xffff49de },	{ 0x000036cc, 0xffff4f34 },	{ 0x00003133, 0xffff5489 },	{ 0x00002e66, 0xffff5bbc },
		{ 0x00002e66, 0xffff64cd },	{ 0x00002e66, 0xffff7511 },	{ 0x00003911, 0xffff81de },	{ 0x00004e66, 0xffff8b34 },
		{ 0x00007600, 0xffffca67 },	{ 0x00007600, 0xffffc1de },	{ 0x000072dd, 0xffffba78 },	{ 0x00006c99, 0xffffb434 },
		{ 0x00006655, 0xffffadef },	{ 0x00005c44, 0xffffa79a },	{ 0x00004e66, 0xffffa134 },	{ 0x00004088, 0xffffa79a },
		{ 0x00003677, 0xffffadef },	{ 0x00003033, 0xffffb434 },	{ 0x000029ef, 0xffffba78 },	{ 0x000026cc, 0xffffc1de },
		{ 0x000026cc, 0xffffca67 },	{ 0x000026cc, 0xffffd5de },	{ 0x00002a66, 0xffffdede },	{ 0x00003199, 0xffffe567 },
		{ 0x000038cc, 0xffffebef },	{ 0x00004266, 0xffffef34 },	{ 0x00004e66, 0xffffef34 },	{ 0x00005a22, 0xffffef34 },
		{ 0x000063aa, 0xffffebef },	{ 0x00006b00, 0xffffe567 },	{ 0x00007255, 0xffffdede },	{ 0x00007600, 0xffffd5de },
		{ 0x00007600, 0xffffca67 },
	};

	const uint8 kCommands_38[]={
		1,4,4,4,4,4,4,4,4,4,4,4,4,2,1,4,4,4,4,4,4,2,1,4,4,4,4,4,4,4,4,2,0
	};

	const sint32 kPoints_39[][2]={
		{ 0x00007599, 0xffffa334 },	{ 0x00006844, 0xffffb1de },	{ 0x000058ef, 0xffffb934 },	{ 0x00004799, 0xffffb934 },
		{ 0x00003711, 0xffffb934 },	{ 0x00002900, 0xffffb311 },	{ 0x00001d66, 0xffffa6cd },	{ 0x000011cc, 0xffff9a89 },
		{ 0x00000c00, 0xffff8aef },	{ 0x00000c00, 0xffff7800 },	{ 0x00000c00, 0xffff64cd },	{ 0x000011bb, 0xffff5478 },
		{ 0x00001d33, 0xffff4700 },	{ 0x000028aa, 0xffff3989 },	{ 0x000037dd, 0xffff32cd },	{ 0x00004acc, 0xffff32cd },
		{ 0x000060aa, 0xffff32cd },	{ 0x000071aa, 0xffff3b34 },	{ 0x00007dcc, 0xffff4c00 },	{ 0x000089ef, 0xffff5ccd },
		{ 0x00009000, 0xffff7511 },	{ 0x00009000, 0xffff94cd },	{ 0x00009000, 0xffffdeab },	{ 0x00007844, 0x00000399 },
		{ 0x000048cc, 0x00000399 },	{ 0x00003999, 0x00000399 },	{ 0x00002cbb, 0xffffff56 },	{ 0x00002233, 0xfffff6cd },
		{ 0x000017aa, 0xffffee45 },	{ 0x00001155, 0xffffe1bc },	{ 0x00000f33, 0xffffd134 },	{ 0x00002a00, 0xffffd134 },
		{ 0x00002c22, 0xffffe534 },	{ 0x000036cc, 0xffffef34 },	{ 0x00004a00, 0xffffef34 },	{ 0x00005688, 0xffffef34 },
		{ 0x00006099, 0xffffe8ab },	{ 0x00006833, 0xffffdb9a },	{ 0x00006fcc, 0xffffce89 },	{ 0x00007444, 0xffffbbbc },
		{ 0x00007599, 0xffffa334 },	{ 0x00007600, 0xffff8f34 },	{ 0x000075bb, 0xffff7800 },	{ 0x000071cc, 0xffff6634 },
		{ 0x00006a33, 0xffff59cd },	{ 0x00006299, 0xffff4d67 },	{ 0x00005866, 0xffff4734 },	{ 0x00004b99, 0xffff4734 },
		{ 0x00004066, 0xffff4734 },	{ 0x00003777, 0xffff4bbc },	{ 0x000030cc, 0xffff54cd },	{ 0x00002a22, 0xffff5dde },
		{ 0x000026cc, 0xffff6911 },	{ 0x000026cc, 0xffff7667 },	{ 0x000026cc, 0xffff8400 },	{ 0x00002a33, 0xffff8f23 },
		{ 0x00003100, 0xffff97cd },	{ 0x000037cc, 0xffffa078 },	{ 0x000040ef, 0xffffa4cd },	{ 0x00004c66, 0xffffa4cd },
		{ 0x00005a88, 0xffffa4cd },	{ 0x00006866, 0xffff9d9a },	{ 0x00007600, 0xffff8f34 },
	};

	const uint8 kCommands_39[]={
		1,4,4,4,4,4,4,4,4,4,4,3,4,4,4,2,1,4,4,4,4,4,4,4,2,0
	};

	const struct VDGlyphInfo {
		const sint32 (*mpPoints)[2];
		const uint8 *mpCommands;
		float mOffset;
		float mAdvance;
	} kGlyphs[]={
		{ kPoints_30, kCommands_30, 0.f, 0.6f },
		{ kPoints_31, kCommands_31, 0.1f, 0.6f },
		{ kPoints_32, kCommands_32, 0.f, 0.6f },
		{ kPoints_33, kCommands_33, 0.f, 0.6f },
		{ kPoints_34, kCommands_34, 0.f, 0.6f },
		{ kPoints_35, kCommands_35, 0.f, 0.6f },
		{ kPoints_36, kCommands_36, 0.f, 0.6f },
		{ kPoints_37, kCommands_37, 0.f, 0.6f },
		{ kPoints_38, kCommands_38, 0.f, 0.6f },
		{ kPoints_39, kCommands_39, 0.f, 0.6f },
	};
}

///////////////////////////////////////////////////////////////////////////

class VDVideoFilterFiberScheduler : protected VDThread {
public:
	VDVideoFilterFiberScheduler();
	~VDVideoFilterFiberScheduler();

	void Start();
	void Stop();

	VDScheduler& Scheduler() { return mScheduler; }

protected:
	void ThreadRun();

	VDAtomicInt		mExit;
	VDScheduler		mScheduler;
	VDSignal		mWakeup;
};

VDVideoFilterFiberScheduler::VDVideoFilterFiberScheduler()
	: VDThread("video fibers")
	, mExit(0)
{
	mScheduler.setSignal(&mWakeup);
}

VDVideoFilterFiberScheduler::~VDVideoFilterFiberScheduler() {
}

void VDVideoFilterFiberScheduler::Start() {
	if (!isThreadAttached()) {
		mExit = 0;

		ThreadStart();
	}
}

void VDVideoFilterFiberScheduler::Stop() {
	mExit = 1;
	mWakeup.signal();

	ThreadWait();
}

void VDVideoFilterFiberScheduler::ThreadRun() {
	VDConvertThreadToFiberW32(NULL);

	while(!mExit) {
		if (!mScheduler.Run())
			mScheduler.IdleWait();
	}
}

///////////////////////////////////////////////////////////////////////////

class VDVideoFilterPixmapAllocator : public IVDPoolAllocator {
public:
	void SetFormat(int w, int h, int format);

	VDPooledObject *OnPoolAllocate();

protected:
	int mWidth;
	int mHeight;
	int mFormat;
};

void VDVideoFilterPixmapAllocator::SetFormat(int w, int h, int format) {
	mWidth = w;
	mHeight = h;
	mFormat = format;
}

VDPooledObject *VDVideoFilterPixmapAllocator::OnPoolAllocate() {
	VDVideoFilterPixmap *p = new VDVideoFilterPixmap;

	p->mBuffer.init(mWidth, mHeight, mFormat);
	return p;
}

///////////////////////////////////////////////////////////////////////////
//
//	VDVideoFilterSystem
//
///////////////////////////////////////////////////////////////////////////

class VDVideoFilterInstance;

class VDVideoFilterSystem : public vdrefcounted<IVDVideoFilterSystem> {
	friend VDVideoFilterInstance;
public:
	VDVideoFilterSystem();
	~VDVideoFilterSystem();

	void SetScheduler(VDScheduler *pScheduler);
	void SetErrorCallback(IVDAsyncErrorCallback *pCB);

	bool IsRunning() { return mbIsRunning; }

	void Clear();
	IVDVideoFilterInstance *CreateFilter(const VDPluginInfo *pDef, void *pCreationData);
	void Connect(IVDVideoFilterInstance *src, IVDVideoFilterInstance *dst, int dstpin);
	void Prepare(bool allowPartial);
	void Start();
	void Stop();

protected:
	VDScheduler *mpScheduler;
	IVDAsyncErrorCallback *mpErrorCB;

	bool	mbIsRunning;

	typedef std::list<VDVideoFilterInstance *> tFilters;
	tFilters	mFilters;
	tFilters	mStartedFilters;

	VDVideoFilterFiberScheduler		mFiberScheduler;
};

///////////////////////////////////////////////////////////////////////////
//
//	VDVideoFilterInstance
//
///////////////////////////////////////////////////////////////////////////

class VDVideoFilterInstance : public vdrefcounted<IVDVideoFilterInstance>
							, public VDSchedulerNode
							, protected IVDVideoFrameRequestScheduler
							, protected IVDCacheAllocator
							, public IVDPluginCallbacks
							, public VDBackfaceObject<VDVideoFilterInstance>
{
public:
	VDVideoFilterInstance(const VDPluginInfo *pInfo, void *pCreationData);
	~VDVideoFilterInstance();

	void SetParent(VDVideoFilterSystem *pVFS);

	void Connect(int pin, VDVideoFilterInstance *pSource);

	int GetSourceCount() const { return mSources.size(); }
	VDVideoFilterInstance *GetSource(int n) const { return mSources[n]; }

	bool IsPrepared() { return mState == kStatePrepared || mState == kStateRunning; }

	bool IsFiberDependent() const { return mbScheduleAsFiber; }

	bool Config(VDGUIHandle);
	void Prepare();
	void Start();
	void Stop();

	VDStringW GetBlurb();

	IVDVideoFrameRequest *RequestPreviewSourceFrame(sint64 frame, IVDVideoFrameRequestCompleteCallback *pCallback, void *pToken);
	IVDVideoFrameRequest *RequestFrame(sint64 frame, IVDVideoFrameRequestCompleteCallback *pCallback, void *pToken);

	const VDPixmap& GetFormat() {
		return *mContext.mpOutput->mpFormat;
	}

	VDVideoFilterOutputInfo GetOutputInfo() {
		const VDVideoFilterPin& pin = *mContext.mpOutput;
		VDVideoFilterOutputInfo info;

		info.mpFormat = pin.mpFormat;
		info.mStart = pin.mStart;
		info.mLength = pin.mLength;
		info.mFrameRateHi = pin.mFrameRateHi;
		info.mFrameRateLo = pin.mFrameRateLo;

		return info;
	}

	void SerializeConfig(VDPluginConfig& config);
	void DeserializeConfig(const VDPluginConfig& config);
	void SetConfigVal(unsigned idx, const VDPluginConfigVariant& var);

	int GetDirectRange(sint64& pos, sint32& len);

	bool Service();

#if VD_BACKFACE_ENABLED
	static const char *BackfaceGetShortName();
	static const char *BackfaceGetLongName();
	void BackfaceDumpObject(IVDBackfaceOutput& out);
	void BackfaceDumpBlurb(IVDBackfaceOutput& out);
#endif

protected:
	void NotifyFrameRequestReady(VDVideoFrameRequest *pRequest, bool bSuccessful);

	VDCachedObject *OnCacheAllocate() {
		return new VDVideoFrameRequest(this);
	}

	void ExtPrefetch(int pin, sint64 frameno, uint32 flags);
	void ExtAllocFrame();
	void ExtCopyFrame(VDVideoFilterFrame *pFrame);

	static inline VDVideoFilterInstance *AsThis(const VDVideoFilterContext *pContext) {
		return (VDVideoFilterInstance *)((char *)pContext - offsetof(VDVideoFilterInstance, mContext));
	}

	static void ExtEntryPrefetch(const VDVideoFilterContext *pContext, int pin, sint64 frameno, uint32 flags) {
		AsThis(pContext)->ExtPrefetch(pin, frameno, flags);
	}

	static void ExtEntryAllocFrame(const VDVideoFilterContext *pContext) {
		AsThis(pContext)->ExtAllocFrame();
	}

	static void ExtEntryCopyFrame(const VDVideoFilterContext *pContext, VDVideoFilterFrame *pFrame) {
		AsThis(pContext)->ExtCopyFrame(pFrame);
	}

	void DumpStatus();

	void VDXAPIENTRYV SetError(const char *format, ...);
	void VDXAPIENTRY SetErrorOutOfMemory();
	void * VDXAPIENTRY GetExtendedAPI(const char *pExtendedAPI);
	uint32 VDXAPIENTRY GetCPUFeatureFlags();

	const VDPluginInfo			*mpInfo;
	const VDVideoFilterDefinition	*mpDef;

	VDVideoFilterSystem *mpParent;

	VDCache			mRequestCache;

	volatile uint32	mFlags;
	bool			mbSerialized;			// if set, we must serialize requests
	bool			mbScheduleAsFiber;
	VDAtomicInt		mRequestedState;
	VDAtomicInt		mState;

	enum {
		kStateIdle,
		kStatePrepared,
		kStateRunning,
		kStateError
	};

	VDVideoFrameRequest		*mpPrefetchRequest;
	int		mPrefetchPin;
	sint64	mPrefetchFrame;

	vdblock<VDVideoFilterInstance *>	mSources;
	vdblock<VDVideoFilterFrame *>		mSourceFramePtrs;

	std::vector<VDVideoFilterPin>			mPins;
	vdfastvector<VDVideoFilterPin *>		mPinPtrs;
	std::vector<VDPixmap>					mPinFormats;

	VDCriticalSection					mLock;
	VDVideoFilterContext				mContext;

	VDVideoFilterFrameCache	mCache;
	VDPool					mFramePool;
	VDVideoFilterPixmapAllocator	mFrameAllocator;

	VDSignal				mFiberSignal;

	MyError					mError;

	static const VDVideoFilterCallbacks	sVideoCallbacks;
};

const VDVideoFilterCallbacks VDVideoFilterInstance::sVideoCallbacks = {
	ExtEntryPrefetch,
	ExtEntryAllocFrame,
	ExtEntryCopyFrame,
};

IVDVideoFilterInstance *VDCreateVideoFilterInstance(const VDPluginInfo *pDef, void *pCreationData) {
	return new VDVideoFilterInstance(pDef, pCreationData);
}

VDVideoFilterInstance::VDVideoFilterInstance(const VDPluginInfo *pInfo, void *pCreationData)
	: mpInfo(pInfo)
	, mpDef(reinterpret_cast<const VDVideoFilterDefinition *>(pInfo->mpTypeSpecificInfo))
	, mpParent(NULL)
	, mRequestCache(this)
	, mPins(mpDef->mInputPins + mpDef->mOutputPins)
	, mPinPtrs(mpDef->mInputPins + mpDef->mOutputPins)
	, mPinFormats(mpDef->mInputPins + mpDef->mOutputPins)
	, mSources(mpDef->mInputPins)
	, mRequestedState(kStateIdle)
	, mState(kStateIdle)
	, mFramePool(&mFrameAllocator)
{
	for(int i=0; i<mpDef->mInputPins + mpDef->mOutputPins; ++i) {
		mPins[i].mpFormat = &mPinFormats[i];
		mPinPtrs[i] = &mPins[i];
	}

	if (pCreationData)
		mContext.mpFilterData		= pCreationData;
	else
		mContext.mpFilterData		= mpDef->mpDefaultCreationData;

	mContext.mpDefinition		= mpDef;
	mContext.mpFilterData		= mpDef->mpCreate(&mContext);
	mContext.mpServices			= this;
	mContext.mpVideoCallbacks	= &sVideoCallbacks;
	mContext.mpInputs			= mPinPtrs.data();
	mContext.mpOutput			= &mPins[mpDef->mInputPins];

	mCache.SetLimit(16);

	mbScheduleAsFiber = false;
	if (mpDef->mFlags & kVFVDef_RunAsFiber)
		mbScheduleAsFiber = true;
}

VDVideoFilterInstance::~VDVideoFilterInstance() {
	mContext.mpDefinition->mpMain(&mContext, kVFVCmd_Destroy, 0, 0, 0);

	// force request cache to free frames
	mRequestCache.Shutdown();
}

void VDVideoFilterInstance::SetParent(VDVideoFilterSystem *pVFS) {
	VDASSERT(!mpParent);
	mpParent = pVFS;
}

void VDVideoFilterInstance::Connect(int pin, VDVideoFilterInstance *pSource) {
	VDASSERT((unsigned)pin <= mSources.size());
	mSources[pin] = pSource;
}

bool VDVideoFilterInstance::Config(VDGUIHandle h) {
	HWND hwnd = (HWND)h;

	return 0 != mContext.mpDefinition->mpMain(&mContext, kVFVCmd_Config, 0, &hwnd, sizeof hwnd);
}

void VDVideoFilterInstance::Prepare() {
	const int inpins = mContext.mpDefinition->mInputPins;
	const int outpins = mContext.mpDefinition->mOutputPins;

	for(int i=0; i<mContext.mpDefinition->mInputPins; ++i) {
		VDVideoFilterPin& outpin = *mSources[i]->mContext.mpOutput;
		VDVideoFilterPin& inpin = *mContext.mpInputs[i];

		*inpin.mpFormat		= *outpin.mpFormat;
		inpin.mFrameRateLo	= outpin.mFrameRateLo;
		inpin.mFrameRateHi	= outpin.mFrameRateHi;
		inpin.mLength		= outpin.mLength;
		inpin.mStart		= outpin.mStart;
	}

	if (inpins && outpins) {
		VDVideoFilterPin& inpin = *mContext.mpInputs[0];
		VDVideoFilterPin& outpin = *mContext.mpOutput;

		outpin.mFrameRateLo = inpin.mFrameRateLo;
		outpin.mFrameRateHi = inpin.mFrameRateHi;
		outpin.mLength		= inpin.mLength;
		outpin.mStart		= inpin.mStart;
		*outpin.mpFormat	= *inpin.mpFormat;
	}

	mbSerialized = false;

	mRequestedState = kStatePrepared;
	if (mbScheduleAsFiber) {
		while(mState != kStatePrepared) {
			Reschedule();
			mFiberSignal.wait();

			if (mState == kStateError)
				throw mError;
		}
	} else {
		mError.clear();
		mFlags = mContext.mpDefinition->mpMain(&mContext, kVFVCmd_Prepare, 0, 0, 0);
		if (mError.gets())
			throw mError;
		mState = kStatePrepared;
	}

	if (mFlags & kVFVPrepare_Serialize)
		mbSerialized = true;

	if (outpins) {
		VDVideoFilterPin& outpin = *mContext.mpOutput;

		mFrameAllocator.SetFormat(outpin.mpFormat->w, outpin.mpFormat->h, outpin.mpFormat->format);
	}
}

void VDVideoFilterInstance::Start() {
	mRequestedState = kStateRunning;
	if (mbScheduleAsFiber) {
		while(mState != kStateRunning) {
			Reschedule();
			mFiberSignal.wait();

			if (mState == kStateError)
				throw mError;
		}
	} else {
		mError.clear();
		mContext.mpDefinition->mpMain(&mContext, kVFVCmd_Start, 0, 0, 0);
		if (mError.gets())
			throw mError;
		mState = kStateRunning;
		Reschedule();
	}
}

void VDVideoFilterInstance::Stop() {
	mRequestedState = kStateIdle;
	if (mbScheduleAsFiber) {
		while(mState != kStateIdle) {
			Reschedule();
			mFiberSignal.wait();

			if (mState == kStateError) {
				VDASSERT(false);		// Stop() isn't supposed to throw!
				mState = kStateIdle;
				break;
			}
		}
	} else {
		mState = kStateIdle;
		mContext.mpDefinition->mpMain(&mContext, kVFVCmd_Stop, 0, 0, 0);
	}

	mFramePool.Shutdown();		// must do this so we don't ever keep old frames around with a different format
}

VDStringW VDVideoFilterInstance::GetBlurb() {
	wchar_t buf[1024];
	sint32 chars = mContext.mpDefinition->mpMain(&mContext, kVFVCmd_Blurb, 0, buf, 1024);
	VDStringW s;

	if (chars > 0) {
		if (chars < 1024) {
			s.assign(buf, chars);
		} else {
			vdblock<wchar_t> buf2(chars + 1);
			sint32 size = (sint32)chars;

			chars = mContext.mpDefinition->mpMain(&mContext, kVFVCmd_Blurb, 0, buf2.data(), size+1);

			if (chars <= size)
				s.assign(buf2.data(), chars);
		}
	}

	return s;
}

void VDVideoFilterInstance::SerializeConfig(VDPluginConfig& config) {
	config.clear();

	const VDPluginConfigEntry *pEnt = mContext.mpDefinition->mpConfigInfo;

	if (pEnt) {
		vdprotected1("retrieving config for video filter \"%ls\"", const wchar_t *, mpInfo->mpName) {
			for(; pEnt->next; pEnt = pEnt->next) {
				switch(pEnt->type) {
				case VDPluginConfigEntry::kTypeU32:
					{
						uint32 v;
						mContext.mpDefinition->mpMain(&mContext, kVFVCmd_GetParam, pEnt->idx, &v, sizeof(uint32));
						config[pEnt->idx].SetU32(v);
					}
					break;
				case VDPluginConfigEntry::kTypeS32:
					{
						uint32 v;
						mContext.mpDefinition->mpMain(&mContext, kVFVCmd_GetParam, pEnt->idx, &v, sizeof(sint32));
						config[pEnt->idx].SetS32(v);
					}
					break;
				case VDPluginConfigEntry::kTypeU64:
					{
						uint32 v;
						mContext.mpDefinition->mpMain(&mContext, kVFVCmd_GetParam, pEnt->idx, &v, sizeof(uint64));
						config[pEnt->idx].SetU64(v);
					}
					break;
				case VDPluginConfigEntry::kTypeS64:
					{
						uint32 v;
						mContext.mpDefinition->mpMain(&mContext, kVFVCmd_GetParam, pEnt->idx, &v, sizeof(sint64));
						config[pEnt->idx].SetS64(v);
					}
					break;
				case VDPluginConfigEntry::kTypeDouble:
					{
						double v;
						mContext.mpDefinition->mpMain(&mContext, kVFVCmd_GetParam, pEnt->idx, &v, sizeof(double));
						config[pEnt->idx].SetDouble(v);
					}
					break;
				case VDPluginConfigEntry::kTypeAStr:
					{
						uint32 l = mContext.mpDefinition->mpMain(&mContext, kVFVCmd_GetParam, pEnt->idx, NULL, 0);
						std::vector<char> tmp(l);
						mContext.mpDefinition->mpMain(&mContext, kVFVCmd_GetParam, pEnt->idx, &tmp.front(), l);
						config[pEnt->idx].SetAStr(&tmp.front());
					}
					break;
				case VDPluginConfigEntry::kTypeWStr:
					{
						uint32 l = mContext.mpDefinition->mpMain(&mContext, kVFVCmd_GetParam, pEnt->idx, NULL, 0);
						std::vector<char> tmp(l);
						mContext.mpDefinition->mpMain(&mContext, kVFVCmd_GetParam, pEnt->idx, &tmp.front(), l);
						config[pEnt->idx].SetWStr((const wchar_t *)&tmp.front());
					}
					break;
				case VDPluginConfigEntry::kTypeBlock:
					{
						uint32 l = mContext.mpDefinition->mpMain(&mContext, kVFVCmd_GetParam, pEnt->idx, NULL, 0);
						std::vector<char> tmp(l);
						mContext.mpDefinition->mpMain(&mContext, kVFVCmd_GetParam, pEnt->idx, &tmp.front(), l);
						config[pEnt->idx].SetBlock(&tmp.front(), l);
					}
					break;
				}
			}
		}
	}
}

void VDVideoFilterInstance::DeserializeConfig(const VDPluginConfig& config) {
	vdprotected1("restoring config for video filter \"%ls\"", const wchar_t *, mpInfo->mpName) {
		VDPluginConfig::const_iterator it(config.begin()), itEnd(config.end());

		mError.clear();
		for(; it!=itEnd; ++it) {
			const unsigned idx = (*it).first;
			const VDPluginConfigVariant& var = (*it).second;

			SetConfigVal(idx, var);
			if (mError.gets())
				throw mError;
		}
	}
}

void VDVideoFilterInstance::SetConfigVal(unsigned idx, const VDPluginConfigVariant& var) {
	const VDPluginConfigEntry *pEnt = mContext.mpDefinition->mpConfigInfo;

	for(; pEnt; pEnt = pEnt->next) {
		if (pEnt->idx == idx) {
			if (pEnt->type != var.GetType()) {
				VDASSERT(false);
				return;
			}

			mError.clear();
			switch(var.GetType()) {
			case VDPluginConfigVariant::kTypeU32:		mContext.mpDefinition->mpMain(&mContext, kVFVCmd_SetParam, idx, (void *)&var.GetU32(), sizeof(uint32)); break;
			case VDPluginConfigVariant::kTypeS32:		mContext.mpDefinition->mpMain(&mContext, kVFVCmd_SetParam, idx, (void *)&var.GetS32(), sizeof(sint32)); break;
			case VDPluginConfigVariant::kTypeU64:		mContext.mpDefinition->mpMain(&mContext, kVFVCmd_SetParam, idx, (void *)&var.GetU64(), sizeof(uint64)); break;
			case VDPluginConfigVariant::kTypeS64:		mContext.mpDefinition->mpMain(&mContext, kVFVCmd_SetParam, idx, (void *)&var.GetS64(), sizeof(sint64)); break;
			case VDPluginConfigVariant::kTypeDouble:	mContext.mpDefinition->mpMain(&mContext, kVFVCmd_SetParam, idx, (void *)&var.GetDouble(), sizeof(double)); break;
			case VDPluginConfigVariant::kTypeAStr:		mContext.mpDefinition->mpMain(&mContext, kVFVCmd_SetParam, idx, (void *)var.GetAStr(), strlen(var.GetAStr())+1); break;
			case VDPluginConfigVariant::kTypeWStr:		mContext.mpDefinition->mpMain(&mContext, kVFVCmd_SetParam, idx, (void *)var.GetWStr(), (wcslen(var.GetWStr())+1)*sizeof(wchar_t)); break;
			case VDPluginConfigVariant::kTypeBlock:		mContext.mpDefinition->mpMain(&mContext, kVFVCmd_SetParam, idx, (void *)var.GetBlockPtr(), var.GetBlockLen()); break;
			default:
				VDASSERT(false);
			}
			if (mError.gets())
				throw mError;
			return;
		}
	}

	VDASSERT(false);
}

int VDVideoFilterInstance::GetDirectRange(sint64& pos, sint32& len) {
	VFVCmdInfo_GetDirectRange data = { pos, len };

	int directSource = mContext.mpDefinition->mpMain(&mContext, kVFVCmd_GetDirectRange, 0, &data, sizeof data);

	if (directSource <= 0)
		return 0;

	pos = data.pos;
	len = data.len;

	return directSource;
}

bool VDVideoFilterInstance::Service() {
	if (mRequestedState != mState) {
		mError.clear();

		switch(mRequestedState) {
		case kStateRunning:
			mContext.mpDefinition->mpMain(&mContext, kVFVCmd_Start, 0, 0, 0);
			break;
		case kStateIdle:
			mContext.mpDefinition->mpMain(&mContext, kVFVCmd_Stop, 0, 0, 0);
			break;
		case kStatePrepared:
			mFlags = mContext.mpDefinition->mpMain(&mContext, kVFVCmd_Prepare, 0, 0, 0);
			break;
		}

		if (mError.gets())
			mState = kStateError;
		else
			mState = mRequestedState;

		mFiberSignal.signal();

		return false;
	}

	if (mState != kStateRunning)
		return false;

	VDVideoFrameRequest *pReq = static_cast<VDVideoFrameRequest *>(mRequestCache.GetNextReady());

	if (!pReq)
		return false;

	bool bMoreRequestsReady;
	VDVideoFrameRequest& req = *pReq;

	// lock request so it can't retrigger if prefetches occur from run()
	req.Lock();

	// prepare context and source frames
	mContext.mpDstFrame				= 0;
	mContext.mpOutput->mFrameNum	= req.GetFrameNumber();

	mSourceFramePtrs.resize(req.GetSourceFrameCount());
	req.GetSourceFrames(mSourceFramePtrs.data());
	mContext.mpSrcFrames			= mSourceFramePtrs.data();
	mContext.mSrcFrameCount			= mSourceFramePtrs.size();

	// launch filter
	sint32 result;
	mError.clear();
	result = mContext.mpDefinition->mpMain(&mContext, kVFVCmd_Run, 0, 0, 0);
	if (mError.gets()) {
		if (mpParent->mpErrorCB)
			mpParent->mpErrorCB->OnAsyncError(mError);
		mState = kStateError;
		return false;
	}

	if (result == kVFVRun_NeedMoreFrames) {
		VDDEBUG_FILT("Video filter[%-16ls]: recycling frame %ld for additional source frames\n", mpInfo->mpName, (long)req.GetFrameNumber());

		mRequestCache.Schedule(pReq);
		bMoreRequestsReady = true;

		req.Unlock();
	} else {
		VDVideoFilterFrameRef *pFrameRef = (VDVideoFilterFrameRef *)((char *)mContext.mpDstFrame - offsetof(VDVideoFilterFrameRef, mFrame));

#ifdef DEBUG_ENABLE_FRAME_NUMBERS
		const VDPixmap& px = *mContext.mpDstFrame->mpPixmap;
		uint32 num = (uint32)mContext.mpOutput->mFrameNum;

		// render text
		int posx = px.w - 10;
		int posy = px.h - 10;

		if (mpDef->mInputPins)
			posy -= 60;

		VDPixmapPathRasterizer pathrast;
		VDPixmapRegion rgn;

		while(num) {
			int digit = num % 10;
			num /= 10;

			const VDGlyphInfo& glyphInfo = kGlyphs[digit];

			const sint32 (*points)[2] = glyphInfo.mpPoints;
			const uint8 *cmd = glyphInfo.mpCommands;

			vdvector2i basept;
			vdvector2i pts[4];

			while(uint8 c = *cmd++) {
				switch(c) {
				case 1:
					basept.x = pts[3].x = points[0][0] / 205;
					basept.y = pts[3].y = points[0][1] / 205;
					++points;
					break;
				case 2:
					pathrast.Line(pts[3], basept);
					break;
				case 3:
					pts[2] = pts[3];
					pts[3].x = points[0][0] / 205;
					pts[3].y = points[0][1] / 205;
					++points;
					pathrast.Line(pts[2], pts[3]);
					break;
				case 4:
					pts[0] = pts[3];
					pts[1].x = points[0][0] / 205;
					pts[1].y = points[0][1] / 205;
					pts[2].x = points[1][0] / 205;
					pts[2].y = points[1][1] / 205;
					pts[3].x = points[2][0] / 205;
					pts[3].y = points[2][1] / 205;
					points += 3;
					pathrast.Bezier(pts);
					break;
				}
			}

			pathrast.ScanConvert(rgn);

			VDPixmapFillRegion(px, rgn, posx + VDRoundToInt(65536.0f/1638.0f * (glyphInfo.mOffset - glyphInfo.mAdvance)), posy, 0xC0E0FF);

			posx -= VDRoundToInt(65536.0f/1638.0f * glyphInfo.mAdvance);
		}
#endif

		VDDEBUG_FILT("Video filter[%-16ls]: frame %ld complete (frame=%p, pixmap=%p)\n", mpInfo->mpName, (long)req.GetFrameNumber(), mContext.mpDstFrame, mContext.mpDstFrame->mpPixmap);

		pFrameRef = mCache.AddFrame(pFrameRef, req.GetFrameNumber());		// must requery as cache may alias the frame to keep track of a copied upstream frame

		VDDEBUG_FILT("Video filter[%-16ls]: frame %ld being marked done with pixmap %p\n", mpInfo->mpName, (long)req.GetFrameNumber(), &*pFrameRef->mFrame.mpPixmap);
		req.SetFrame(&pFrameRef->mFrame);
		req.MarkDone();

		mRequestCache.MarkCompleted(pReq);
		bMoreRequestsReady = true;
	}

	pReq->Release();

	return bMoreRequestsReady;
}

IVDVideoFrameRequest *VDVideoFilterInstance::RequestPreviewSourceFrame(sint64 frame, IVDVideoFrameRequestCompleteCallback *pCallback, void *pToken) {
	int srcpin;
	sint64 srcframe;

	vdsynchronized(mLock) {
		mpPrefetchRequest = NULL;
		mPrefetchFrame = -1;
		mPrefetchPin = -1;

		sint32 rv = mContext.mpDefinition->mpMain(&mContext, kVFVCmd_PrefetchSrcPrv, frame, 0, 0);
		if (rv < 0) {
			if (mpDef->mInputPins) {
				mPrefetchFrame = frame;
				mPrefetchPin = 0;
			} else {
				mPrefetchPin = -2;
				mPrefetchFrame = -1;
			}
		} else if (rv > 0) {
			mPrefetchPin = -2;
			mPrefetchFrame = -1;
		}

		srcpin = mPrefetchPin;
		srcframe = mPrefetchFrame;
	}

	if (srcpin == -2)
		return RequestFrame(frame, pCallback, pToken);

	if (srcpin < 0)
		return NULL;

	return mSources[srcpin]->RequestPreviewSourceFrame(srcframe, pCallback, pToken);
}

IVDVideoFrameRequest *VDVideoFilterInstance::RequestFrame(sint64 frame, IVDVideoFrameRequestCompleteCallback *pCallback, void *pToken) {
	bool is_new;
	VDVideoFrameRequest *pReq = static_cast<VDVideoFrameRequest *>(mRequestCache.Create(frame, is_new));

	IVDVideoFrameRequest *pReqRet = pReq;
	pReqRet->AddRef();

	if (is_new) {
		VDVideoFilterFrameRef *pFrame = mCache.LookupFrame(frame);

		if (pFrame) {
			VDDEBUG_FILT("Video filter[%-16ls]: returning cached request for frame %ld (pixmap=%p)\n", mpInfo->mpName, (long)pReq->GetFrameNumber(), &*pFrame->mFrame.mpPixmap);

			pReq->SetFrame(&pFrame->mFrame);
			pReq->MarkDone();

			if (pCallback)
				pCallback->NotifyVideoFrameRequestComplete(pReq, pReq->IsSuccessful(), false, pToken);

			if (pCallback) {
				pReqRet->Release();
				pReqRet = NULL;
			}

			mRequestCache.MarkCompleted(pReq);
		} else {
			if (pCallback) {
				// We must return the rv of AddNotifyTarget() as that is customized for the callback
				// used.
				pReqRet->Release();
				pReqRet = pReq->AddNotifyTarget(pCallback, pToken);
			}

			VDASSERT(!pReq->IsWaiting());

			pReq->Lock();
			vdsynchronized(mLock) {
				mpPrefetchRequest = pReq;
				VDASSERT(pReq->GetSourceFrameCount() == 0);
				mContext.mpDefinition->mpMain(&mContext, kVFVCmd_Prefetch, frame, 0, 0);
			}
			pReq->Unlock();
		}
	} else {
		if (pCallback) {
			if (pReq->IsReady()) {
				pCallback->NotifyVideoFrameRequestComplete(pReq, pReq->IsSuccessful(), false, pToken);
				pReqRet->Release();
				pReqRet = NULL;
			} else {
				// We must return the rv of AddNotifyTarget() as that is customized for the callback
				// used.
				pReqRet->Release();
				pReqRet = pReq->AddNotifyTarget(pCallback, pToken);
			}
		}
	}

	if (pReq)
		pReq->Release();

	return pReqRet;
}

void VDVideoFilterInstance::NotifyFrameRequestReady(VDVideoFrameRequest *pRequest, bool bSuccessful) {
	VDDEBUG_FILT("Video filter[%-16ls]: frame %ld ready -- scheduling for processing.\n", mpInfo->mpName, (long)pRequest->GetFrameNumber());

	vdsynchronized(mLock) {
		mRequestCache.Schedule(pRequest);

		Reschedule();

#pragma vdpragma_TODO("must handle serialization here")
	}
}

void VDVideoFilterInstance::ExtPrefetch(int pin, sint64 frameno, uint32 flags) {
	VDASSERT((unsigned)pin < mSources.size());

	if (!mpPrefetchRequest) {
		mPrefetchPin = pin;
		mPrefetchFrame = frameno;
		return;
	}

	VDVideoFrameRequest& req = *mpPrefetchRequest;
	VDSourceFrameRequest& srcreq = req.NewSourceRequest();
	VDVideoFilterInstance *pSrc = mSources[pin];
	VDVideoFilterPin& outpin = *pSrc->mContext.mpOutput;

	if (frameno >= outpin.mStart + outpin.mLength)
		frameno = outpin.mStart + outpin.mLength - 1;

	if (frameno < outpin.mStart)
		frameno = outpin.mStart;

	srcreq.mpFrame = NULL;
	req.WeakAddRef();
	srcreq.mpRequest = pSrc->RequestFrame(frameno, &req, &srcreq);
}

void VDVideoFilterInstance::ExtAllocFrame() {
	if (mContext.mpDstFrame)
		mContext.mpDstFrame->Release();

	const VDPixmap& dstformat = *mContext.mpOutput->mpFormat;

	vdrefptr<VDVideoFilterFrameRef> frameref(mCache.AllocFrame());

	if (!frameref->mpPixmapRef) {
		frameref->mpPixmapRef.set(static_cast<VDVideoFilterPixmap *>(mFramePool.Allocate()));
		frameref->mpPixmapRef->mBuffer.init(dstformat.w, dstformat.h, dstformat.format);
	}

#if VD_BACKFACE_ENABLED
	frameref->mpPixmapRef->mLastAllocTime = VDGetCurrentTick();
	frameref->mpPixmapRef->mAllocFrame = (uint32)mContext.mpOutput->mFrameNum;
	frameref->mLastAllocTime = VDGetCurrentTick();
#endif

	frameref->mPixmap = frameref->mpPixmapRef->mBuffer;

	mContext.mpDstFrame = &frameref.release()->mFrame;
}

void VDVideoFilterInstance::ExtCopyFrame(VDVideoFilterFrame *pFrame) {
	if (mContext.mpDstFrame != pFrame) {
		if (mContext.mpDstFrame)
			mContext.mpDstFrame->Release();

		mContext.mpDstFrame = pFrame;
		pFrame->AddRef();
	}
}

void VDVideoFilterInstance::DumpStatus() {
	vdsynchronized(mLock) {
		VDDEBUG("       %p: Video filter [%-16ls]: %d pending, %d ready, %d active, %d complete, %d idle\n"
			, this
			, mpInfo->mpName
			, mRequestCache.GetStateCount(kVDCacheStatePending)
			, mRequestCache.GetStateCount(kVDCacheStateReady)
			, mRequestCache.GetStateCount(kVDCacheStateActive)
			, mRequestCache.GetStateCount(kVDCacheStateComplete)
			, mRequestCache.GetStateCount(kVDCacheStateIdle)
			);

		mRequestCache.DumpListStatus(kVDCacheStatePending);
		mRequestCache.DumpListStatus(kVDCacheStateComplete);
	}
}

void VDXAPIENTRYV VDVideoFilterInstance::SetError(const char *format, ...) {
	va_list val;

	va_start(val, format);
	mError.vsetf(format, val);
	va_end(val);
}

void VDXAPIENTRY VDVideoFilterInstance::SetErrorOutOfMemory() {
	MyMemoryError e;

	mError.TransferFrom(e);
}

void * VDXAPIENTRY VDVideoFilterInstance::GetExtendedAPI(const char *pExtendedAPI) {
	return NULL;
}

uint32 VDXAPIENTRY VDVideoFilterInstance::GetCPUFeatureFlags() {
	return CPUGetEnabledExtensions();
}

#if VD_BACKFACE_ENABLED
const char *VDVideoFilterInstance::BackfaceGetShortName() {
	return "vfilt";
}

const char *VDVideoFilterInstance::BackfaceGetLongName() {
	return "VDVideoFilterInstance";
}

void VDVideoFilterInstance::BackfaceDumpObject(IVDBackfaceOutput& out) {
}

void VDVideoFilterInstance::BackfaceDumpBlurb(IVDBackfaceOutput& out) {
	out("Video filter [%ls]: cache[%s]", mpInfo->mpName, out.GetTag(&mCache).c_str());
}
#endif

///////////////////////////////////////////////////////////////////////////

IVDVideoFilterSystem *VDCreateVideoFilterSystem() {
	return new VDVideoFilterSystem;
}

VDVideoFilterSystem::VDVideoFilterSystem()
	: mbIsRunning(false)
	, mpErrorCB(NULL)
{
}

VDVideoFilterSystem::~VDVideoFilterSystem() {
	Clear();
}

void VDVideoFilterSystem::SetScheduler(VDScheduler *pScheduler) {
	mpScheduler = pScheduler;

	mFiberScheduler.Scheduler().setErrorCallback(pScheduler->getErrorCallback());
}

void VDVideoFilterSystem::SetErrorCallback(IVDAsyncErrorCallback *pCB) {
	mpErrorCB = pCB;
}

void VDVideoFilterSystem::Clear() {
	Stop();

	while(!mFilters.empty()) {
		VDVideoFilterInstance *pInst = mFilters.back();
		mFilters.pop_back();

		pInst->Release();
	}
}

IVDVideoFilterInstance *VDVideoFilterSystem::CreateFilter(const VDPluginInfo *pDef, void *pCreationData) {
	vdrefptr<VDVideoFilterInstance> pFilt(new VDVideoFilterInstance(pDef, pCreationData));
	mFilters.push_back(pFilt);

	pFilt->SetParent(this);

	return pFilt.release();
}

void VDVideoFilterSystem::Connect(IVDVideoFilterInstance *src, IVDVideoFilterInstance *dst, int dstpin) {
	static_cast<VDVideoFilterInstance *>(dst)->Connect(dstpin, static_cast<VDVideoFilterInstance *>(src));
}

void VDVideoFilterSystem::Prepare(bool allowPartial) {
	typedef std::list<std::pair<VDVideoFilterInstance *, int> > tSortedFilters;
	tSortedFilters filters;

	for(tFilters::iterator it(mFilters.begin()), itEnd(mFilters.end()); it != itEnd; ++it) {
		VDVideoFilterInstance *pInst = *it;

		int nSources = pInst->GetSourceCount();

		if (nSources)
			filters.push_back(std::make_pair(pInst, nSources));
		else
			filters.push_front(std::make_pair(pInst, nSources));
	}

	while(!filters.empty()) {
		int unconnectedSources = filters.front().second;
		if (unconnectedSources) {
			if (allowPartial) {
				filters.pop_front();
				continue;
			} else {
				throw MyError("Video filter \"%ls\" has %d unconnected sources.\n", unconnectedSources);
			}
		}

		VDASSERT(!filters.front().second);
		VDVideoFilterInstance *pInst = filters.front().first;

		if (pInst->IsFiberDependent()) {
			mFiberScheduler.Start();
			mFiberScheduler.Scheduler().Add(pInst);
			pInst->Prepare();
			mFiberScheduler.Scheduler().Remove(pInst);
		} else {
			pInst->Prepare();
		}

		filters.pop_front();

		tSortedFilters::iterator it(filters.begin()), itEnd(filters.end());

		while(it != itEnd) {
			VDVideoFilterInstance *pInst2 = (*it).first;
			int srcs = pInst2->GetSourceCount();

			for(int i=0; i<srcs; ++i) {
				if (pInst2->GetSource(i) == pInst)
					--(*it).second;
			}

			tSortedFilters::iterator itNext(it);
			++itNext;

			if (!(*it).second)
				filters.splice(filters.begin(), filters, it);

			it = itNext;
		}
	}
}

void VDVideoFilterSystem::Start() {
	for(tFilters::iterator it(mFilters.begin()), itEnd(mFilters.end()); it != itEnd; ++it) {
		VDVideoFilterInstance *pInst = *it;

		if (pInst->IsFiberDependent()) {
			mFiberScheduler.Start();
			mFiberScheduler.Scheduler().Add(pInst);
		} else {
			mpScheduler->Add(pInst);
		}

		pInst->Start();
		mStartedFilters.push_back(pInst);
	}

	mbIsRunning = true;
}

void VDVideoFilterSystem::Stop() {
	mbIsRunning = false;

	while(!mStartedFilters.empty()) {
		VDVideoFilterInstance *pInst = mStartedFilters.back();

		pInst->Stop();
		if (pInst->IsFiberDependent()) {
			mFiberScheduler.Scheduler().Remove(pInst);
		} else {
			mpScheduler->Remove(pInst);
		}

		mStartedFilters.pop_back();
	}

	mFiberScheduler.Stop();
}

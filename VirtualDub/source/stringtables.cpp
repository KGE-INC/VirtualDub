#include <stdafx.h>
#include <vd2/Dita/resources.h>

namespace {

	// The IDs below are deliberately not exposed via a header file to avoid having to recompile
	// the entire app when editing strings.

	enum { kVDST_Dub = 1 };

	const char *const g_strtab_Dub[]={
		"Dub: Output segment overflow occurred -- segment byte size may be too low.",
		"Dub: Switching to new segment.",
		"Dub: I/O thread has not cycled for ten seconds -- possible livelock. (Thread location: %p)",
		"Dub: Processing thread has not cycled for ten seconds -- possible livelock. (Thread location: %p)",
		"Dub: Video codec produced delay frame while trying to flush codec B-frame delay! Ignoring to avert possible infinite loop.",
		"Dub: Video codec is requesting B-frame delay %d frames longer than end of video! Stopping video stream to avert possible infinite loop.",
		0
	};

	enum { kVDST_AVIReadHandler = 2 };

	const char *const g_strtab_AVIReadHandler[]={
		"AVI: Avisynth detected. Extended error handling enabled.",
		"AVI: OpenDML hierarchical index detected on stream %d.",
		"AVI: Index not found or damaged -- reconstructing via file scan.",
		"AVI: Invalid chunk detected at %lld. Enabling aggressive recovery mode.",
		"AVI: Invalid block found at %lld -- disabling streaming.",
		"AVI: Stream %d has an invalid sample rate. Substituting %lg samples/sec as a placeholder.",
		0
	};

	enum { kVDST_VideoSource = 3 };

	const char *const g_strtab_VideoSource[]={
		"AVI: Resuming normal decoding (concealment off) at frame %u",
		"AVI: Decoding error on frame %u -- attempting to bypass.",
		"AVI: Frame %u is too short (%d < %d bytes) but decoding anyway.",
		"Video codec \"%.64hs\" is buggy and returned to VirtualDub with MMX active. Applying workaround.",
		"AVI: Video format structure in file is abnormally large (%d bytes > 16K). Truncating to %d bytes.",
		"One of the video codecs installed in the system modified the video format passed to it by VirtualDub.  This indicates a codec "
			"bug that may cause the Windows video codec system to malfunction.",
		0
	};

	enum { kVDST_InputFileAVI = 4 };

	const char *const g_strtab_InputFileAVI[]={
		"AVI: Opening file \"%ls\"",
		"AVI: Keyframe flag reconstruction was not specified in open options and the video stream "
							"is not a known keyframe-only type.  Seeking in the video stream may be extremely slow.",
		"AVI: Type-1 DV file detected -- VirtualDub cannot extract audio from this type of interleaved stream.",
		0
	};

	enum { kVDST_Mpeg = 5 };

	const char *const g_strtab_Mpeg[]={
		"MPEGAudio: Concealing decoding error on frame %lu: %hs.",
		"MPEG: Opening file \"%ls\"",
		"MPEG: Anachronistic or discontinuous timestamp found in %ls stream %d at byte position %lld (may indicate improper join)",
		0
	};

	enum { kVDST_AudioSource = 6 };

	const char *const g_strtab_AudioSource[]={
		"AVI: Truncated or invalid MP3 audio format detected (%d bytes, should be %d). Attempting to fix.",
		0
	};

	enum { kVDST_ProjectUI = 7 };

	const char *const g_strtab_ProjectUI[]={
		"%s",
		"%s - [%s]",
		"%s - [%s] (render in progress)",
		0
	};
}

void VDInitAppStringTables() {
	VDLoadStaticStringTableA(0, kVDST_Dub, g_strtab_Dub);
	VDLoadStaticStringTableA(0, kVDST_AVIReadHandler, g_strtab_AVIReadHandler);
	VDLoadStaticStringTableA(0, kVDST_VideoSource, g_strtab_VideoSource);
	VDLoadStaticStringTableA(0, kVDST_InputFileAVI, g_strtab_InputFileAVI);
	VDLoadStaticStringTableA(0, kVDST_Mpeg, g_strtab_Mpeg);
	VDLoadStaticStringTableA(0, kVDST_AudioSource, g_strtab_AudioSource);
	VDLoadStaticStringTableA(0, kVDST_ProjectUI, g_strtab_ProjectUI);
}

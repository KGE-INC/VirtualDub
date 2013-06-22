//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
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

#include "stdafx.h"
#include "AudioFilterSystem.h"

extern const VDAudioFilterDefinition
	afilterDef_input,
	afilterDef_lowpass,
	afilterDef_highpass,
	afilterDef_butterfly,
	afilterDef_stereosplit,
	afilterDef_stereomerge,
	afilterDef_playback,
	afilterDef_resample,
	afilterDef_output,
	afilterDef_sink,
	afilterDef_pitchshift,
	afilterDef_stretch,
	afilterDef_discard,
	afilterDef_centercut,
	afilterDef_centermix,
	afilterDef_gain,
	afilterDef_stereochorus;

static const VDAudioFilterDefinition *const g_builtin_audio_filters[]={
	&afilterDef_input,
	&afilterDef_lowpass,
	&afilterDef_highpass,
	&afilterDef_butterfly,
	&afilterDef_stereosplit,
	&afilterDef_stereomerge,
	&afilterDef_playback,
	&afilterDef_resample,
	&afilterDef_output,
	&afilterDef_sink,
	&afilterDef_pitchshift,
	&afilterDef_stretch,
	&afilterDef_discard,
	&afilterDef_centercut,
	&afilterDef_centermix,
	&afilterDef_gain,
	&afilterDef_stereochorus
};

void VDInitBuiltinAudioFilters() {
	for(int i=0; i<sizeof g_builtin_audio_filters / sizeof g_builtin_audio_filters[0]; ++i)
		VDAddAudioFilter(g_builtin_audio_filters[i]);
}

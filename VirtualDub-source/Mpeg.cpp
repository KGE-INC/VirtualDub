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

#include "VirtualDub.h"
#include <stdio.h>
#include <crtdbg.h>
#include <process.h>
#include <windows.h>
#include <commctrl.h>
#include <fcntl.h>
#include <io.h>

#include "AudioSource.h"
#include "VideoSource.h"
#include "FastReadStream.h"
#include "Error.h"

#include "misc.h"
#include "mpeg.h"
#include "mpeg_decode.h"
#include "resource.h"
#include "gui.h"
#include "cpuaccel.h"

#include "IAMPDecoder.h"

//////////////////////////////////////////////////////////////////////////

extern HINSTANCE g_hInst;
extern char g_msgBuf[128];

//////////////////////////////////////////////////////////////////////////

#define ENABLE_AUDIO_SUPPORT

#define VIDPKT_TYPE_SEQUENCE_START		(0xb3)
#define	VIDPKT_TYPE_SEQUENCE_END		(0xb7)
#define VIDPKT_TYPE_GROUP_START			(0xb8)
#define VIDPKT_TYPE_PICTURE_START		(0x00)
#define VIDPKT_TYPE_SLICE_START_MIN		(0x01)
#define	VIDPKT_TYPE_SLICE_START_MAX		(0xaf)
#define VIDPKT_TYPE_EXT_START			(0xb5)
#define VIDPKT_TYPE_USER_START			(0xb2)

//////////////////////////////////////////////////////////////////////////
//
//
//							DataVector
//
//
//////////////////////////////////////////////////////////////////////////

class DataVectorBlock {
public:
	enum { BLOCK_SIZE = 4096 };

	DataVectorBlock *next;

	char heap[BLOCK_SIZE];
	int index;

	DataVectorBlock() {
		next = NULL;
		index = 0;
	}
};

class DataVector {
private:
	DataVectorBlock *first, *last;
	int item_size;
	int count;

	void _Add(void *);

public:
	DataVector(int item_size);
	~DataVector();

	void Add(void *pp) {
		if (!last || last->index >= DataVectorBlock::BLOCK_SIZE - item_size) {
			_Add(pp);
			return;
		}

		memcpy(last->heap + last->index, pp, item_size);
		last->index += item_size;
		++count;
	}

	void *MakeArray();

	int Length() { return count; }
};

DataVector::DataVector(int _item_size) : item_size(_item_size) {
	first = last = NULL;
	count = 0;
}

DataVector::~DataVector() {
	DataVectorBlock *i, *j;

	j = first;
	while(i=j) {
		j = i->next;
		delete i;
	}
}

void DataVector::_Add(void *pp) {
	if (!last || last->index > DataVectorBlock::BLOCK_SIZE - item_size) {
		DataVectorBlock *ib = new DataVectorBlock();

		if (!ib) throw MyMemoryError();

		if (last)		last->next = ib;
		else			first = ib;

		last = ib;
	}

	memcpy(last->heap + last->index, pp, item_size);
	last->index += item_size;
	++count;
}

void *DataVector::MakeArray() {
	char *array = new char[count * item_size], *ptr = array;
	DataVectorBlock *dvb = first;

	if (!array) throw MyMemoryError();

	while(dvb) {
		memcpy(ptr, dvb->heap, dvb->index);
		ptr += dvb->index;

		dvb=dvb->next;
	}

	return array;
}



//////////////////////////////////////////////////////////////////////////
//
//
//					InputFileMPEGOptions
//
//
//////////////////////////////////////////////////////////////////////////

class InputFileMPEGOptions : public InputFileOptions {
public:
	enum {
		DECODE_NO_B	= 1,
		DECODE_NO_P	= 2,
	};

	struct InputFileMPEGOpts {
		int len;
		int iDecodeMode;
		bool fAcceptPartial;
	} opts;

		
	~InputFileMPEGOptions();

	bool read(const char *buf);
	int write(char *buf, int buflen);

	static BOOL APIENTRY SetupDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam);
};

InputFileMPEGOptions::~InputFileMPEGOptions() {
}

bool InputFileMPEGOptions::read(const char *buf) {
	const InputFileMPEGOpts *pp = (const InputFileMPEGOpts *)buf;

	if (pp->len != sizeof(InputFileMPEGOpts))
		return false;

	opts = *pp;

	return true;
}

int InputFileMPEGOptions::write(char *buf, int buflen) {
	InputFileMPEGOpts *pp = (InputFileMPEGOpts *)buf;

	if (buflen<sizeof(InputFileMPEGOpts))
		return 0;

	opts.len = sizeof(InputFileMPEGOpts);
	*pp = opts;

	return sizeof(InputFileMPEGOpts);
}

///////

BOOL APIENTRY InputFileMPEGOptions::SetupDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam) {
	InputFileMPEGOptions *thisPtr = (InputFileMPEGOptions *)GetWindowLong(hDlg, DWL_USER);

	switch(message) {
		case WM_INITDIALOG:
			SetWindowLong(hDlg, DWL_USER, lParam);
			CheckDlgButton(hDlg, IDC_MPEG_ALL_FRAMES, TRUE);
			return TRUE;

		case WM_COMMAND:
			if (LOWORD(wParam) == IDCANCEL) {
				if (IsDlgButtonChecked(hDlg, IDC_MPEG_I_FRAMES_ONLY	))
					thisPtr->opts.iDecodeMode = InputFileMPEGOptions::DECODE_NO_B | InputFileMPEGOptions::DECODE_NO_P;

				if (IsDlgButtonChecked(hDlg, IDC_MPEG_IP_FRAMES_ONLY))
					thisPtr->opts.iDecodeMode = InputFileMPEGOptions::DECODE_NO_B;

				if (IsDlgButtonChecked(hDlg, IDC_MPEG_ALL_FRAMES	))
					thisPtr->opts.iDecodeMode = 0;

				thisPtr->opts.fAcceptPartial = !!IsDlgButtonChecked(hDlg, IDC_MPEG_ACCEPTPARTIAL);

				EndDialog(hDlg, 0);
				return TRUE;
			}
			break;
	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
//
//
//					MPEGPacketInfo/MPEGSampleInfo
//
//
//////////////////////////////////////////////////////////////////////////


struct MPEGPacketInfo {
	__int64		file_pos;
	__int64		stream_pos;
};

struct MPEGSampleInfo {
	__int64			stream_pos;
	int				size;
	union {
		struct {
			char			frame_type;
			char			subframe_num;		// cannot have >132 (exact?) frames per GOP
			char			_pad[2];
		};

		struct {
			long			header;
		};
	};
};

//////////////////////////////////////////////////////////////////////////
//
//
//					InputFileMPEG
//
//
//////////////////////////////////////////////////////////////////////////

class AudioSourceMPEG;
class VideoSourceMPEG;

class InputFileMPEG : public InputFile {
friend VideoSourceMPEG;
friend AudioSourceMPEG;
private:
	__int64 file_len, file_cpos;
	char *video_packet_buffer;
	char *audio_packet_buffer;
	MPEGPacketInfo *video_packet_list;
	MPEGSampleInfo *video_sample_list;
	MPEGPacketInfo *audio_packet_list;
	MPEGSampleInfo *audio_sample_list;
	int packets, apackets;
	int frames, aframes;
	int last_packet[2];
	int width, height;
	long frame_rate;
	bool fInterleaved, fHasAudio, fIsVCD;
	bool fAbort;

	long audio_first_header;

	int iDecodeMode;
	bool fAcceptPartial;
	bool fAudioBadPad;

	FastReadStream *pFastRead;
	int fh;

	static const char szME[];

	enum {
		RESERVED_STREAM		= 0xbc,
		PRIVATE_STREAM1		= 0xbd,
		PADDING_STREAM		= 0xbe,
		PRIVATE_STREAM2		= 0xbf,
	};

	enum {
		SCAN_BUFFER_SIZE	= 65536,
	};

	char *	pScanBuffer;
	char *	pScan;
	char *	pScanLimit;
	__int64	i64ScanCpos;

	void	StartScan();
	bool	NextStartCode();
	void	Skip(int bytes);

	int		Read() {
		return pScan < pScanLimit ? (unsigned char)*pScan++ : _Read();
	}
	int		Read(void *, int, bool);

	int		_Read();
	void	UnRead() {
		--pScan;
	}
	void	EndScan();
	__int64	Tell();

	void	ReadStream(void *buffer, __int64 pos, long len, bool fAudio);

	static BOOL CALLBACK ParseDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

	void setOptions(InputFileOptions *);
	InputFileOptions *createOptions(const char *buf);
	InputFileOptions *promptForOptions(HWND);
public:
	InputFileMPEG();
	~InputFileMPEG();

	void Init(char *szFile);
	static void _InfoDlgThread(void *pvInfo);
	static BOOL APIENTRY _InfoDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam);
	void InfoDialog(HWND hwndParent);
};


//////////////////////////////////////////////////////////////////////////
//
//
//						VideoSourceMPEG
//
//
//////////////////////////////////////////////////////////////////////////

class VideoSourceMPEG : public VideoSource {
private:
	InputFileMPEG *parentPtr;

	long frame_forw, frame_back, frame_bidir;
	long frame_type;

	BOOL fFBValid;

	LONG renumber_frame(LONG lSample);
	LONG translate_frame(LONG lSample);
	long prev_IP(long f);
	long prev_I(long f);
	bool is_I(long lSample);

public:
	VideoSourceMPEG(InputFileMPEG *);
	~VideoSourceMPEG();

	BOOL init();
	char getFrameTypeChar(long lFrameNum);
	BOOL _isKey(LONG lSample);
	virtual LONG nearestKey(LONG lSample);
	virtual LONG prevKey(LONG lSample);
	virtual LONG nextKey(LONG lSample);
	bool setDecompressedFormat(int depth);
	bool setDecompressedFormat(BITMAPINFOHEADER *pbih);
	void invalidateFrameBuffer();
	BOOL isFrameBufferValid();
	void streamBegin(bool fRealTime);
	void streamSetDesiredFrame(long frame_num);
	long streamGetNextRequiredFrame(BOOL *is_preroll);
	int streamGetRequiredCount(long *);
	void *streamGetFrame(void *inputBuffer, long data_len, BOOL is_key, BOOL is_preroll, long frame_num);
	void *getFrame(LONG frameNum);
	eDropType getDropType(long);
	int _read(LONG lStart, LONG lCount, LPVOID lpBuffer, LONG cbBuffer, LONG *lSamplesRead, LONG *lBytesRead);

	long streamToDisplayOrder(long sample_num) {
		if (sample_num<lSampleFirst || sample_num >= lSampleLast)
			return sample_num;

		long gopbase = sample_num;

		if (!is_I(gopbase))
			gopbase = prev_I(gopbase);

		return gopbase + parentPtr->video_sample_list[sample_num].subframe_num;
	}

	long displayToStreamOrder(long display_num) {
		return (display_num<lSampleFirst || display_num >= lSampleLast)
			? display_num
			: translate_frame(renumber_frame(display_num));
	}

	bool isDecodable(long sample_num);
};

VideoSourceMPEG::VideoSourceMPEG(InputFileMPEG *parent) {
	parentPtr = parent;
}

BOOL VideoSourceMPEG::init() {
	BITMAPINFOHEADER *bmih;
	int w, h;

	fFBValid = FALSE;

	lSampleFirst = 0;
	lSampleLast = parentPtr->frames;

	w = (parentPtr->width+15) & -16;
	h = (parentPtr->height+1) & -2;

	if (!AllocFrameBuffer(w * h * 4 + 4))
		throw MyMemoryError();

	if (!(bmih = (BITMAPINFOHEADER *)allocFormat(sizeof(BITMAPINFOHEADER))))
		throw MyMemoryError();

	if (!(bmihDecompressedFormat = (BITMAPINFOHEADER *)allocmem(getFormatLen()))) throw MyMemoryError();

	bmih->biSize		= sizeof(BITMAPINFOHEADER);
	bmih->biWidth		= w;
	bmih->biHeight		= h;
	bmih->biPlanes		= 1;
	bmih->biBitCount	= 32;
	bmih->biCompression	= 0xffffffff;
	bmih->biSizeImage	= 0;
	bmih->biXPelsPerMeter	= 0;
	bmih->biYPelsPerMeter	= 0;
	bmih->biClrUsed		= 0;
	bmih->biClrImportant	= 0;

	streamInfo.fccType					= streamtypeVIDEO;
	streamInfo.fccHandler				= 0;
	streamInfo.dwFlags					= 0;
	streamInfo.wPriority				= 0;
	streamInfo.wLanguage				= 0;
	streamInfo.dwScale					= 1000;
	streamInfo.dwRate					= parentPtr->frame_rate;
	streamInfo.dwStart					= 0;
	streamInfo.dwLength					= parentPtr->frames;
	streamInfo.dwInitialFrames			= 0;
	streamInfo.dwSuggestedBufferSize	= 0;
	streamInfo.dwQuality				= 0xffffffffL;
	streamInfo.dwSampleSize				= 0;
	streamInfo.rcFrame.left				= 0;
	streamInfo.rcFrame.top				= 0;
	streamInfo.rcFrame.right			= w;
	streamInfo.rcFrame.bottom			= h;

	return TRUE;
}

VideoSourceMPEG::~VideoSourceMPEG() {
}

bool VideoSourceMPEG::setDecompressedFormat(int depth) {
	if (depth != 32 && depth != 24 && depth != 16) return FALSE;

//#pragma warning("don't ship with this!!!!!!");
//	if (depth != 32 && depth != 16) return FALSE;

	return VideoSource::setDecompressedFormat(depth);
}

bool VideoSourceMPEG::setDecompressedFormat(BITMAPINFOHEADER *pbih) {
	if (pbih->biCompression == BI_RGB)
		return setDecompressedFormat(pbih->biBitCount);

	// Sanity-check format.

	BITMAPINFOHEADER *pbihInput = getImageFormat();

	if (pbih->biWidth != pbihInput->biWidth
		|| pbih->biHeight != pbihInput->biHeight
		|| pbih->biPlanes != 1)

		return false;

	do {
		// Accept UYVY, YUYV, or YUY2 at 16-bits per pel.

		if (pbih->biBitCount == 16 && (isEqualFOURCC(pbih->biCompression, 'YVYU')
			|| isEqualFOURCC(pbih->biCompression, 'VYUY') || isEqualFOURCC(pbih->biCompression, '2YUY')))

			break;

		// Damn.

		return false;

	} while(0);

	// Looks good!

	memcpy(bmihDecompressedFormat, pbih, sizeof(BITMAPINFOHEADER));
	invalidateFrameBuffer();

	return true;
}

void VideoSourceMPEG::invalidateFrameBuffer() {
	fFBValid = FALSE;
}

BOOL VideoSourceMPEG::isFrameBufferValid() {
	return fFBValid;
}

char VideoSourceMPEG::getFrameTypeChar(long lFrameNum) {
	if (lFrameNum<lSampleFirst || lFrameNum >= lSampleLast)
		return ' ';

	lFrameNum = translate_frame(renumber_frame(lFrameNum));

	switch(parentPtr->video_sample_list[lFrameNum].frame_type) {
	case MPEG_FRAME_TYPE_I:	return 'I';
	case MPEG_FRAME_TYPE_P: return 'P';
	case MPEG_FRAME_TYPE_B: return 'B';
	default:
		return ' ';
	}
}

VideoSource::eDropType VideoSourceMPEG::getDropType(long lFrameNum) {
	if (lFrameNum<lSampleFirst || lFrameNum >= lSampleLast)
		return kDroppable;

	switch(parentPtr->video_sample_list[translate_frame(renumber_frame(lFrameNum))].frame_type) {
	case MPEG_FRAME_TYPE_I:	return kIndependent;
	case MPEG_FRAME_TYPE_P: return kDependant;
	case MPEG_FRAME_TYPE_B: return kDroppable;
	default:
		return kDroppable;
	}
}

bool VideoSourceMPEG::isDecodable(long sample_num) {
	if (sample_num<lSampleFirst || sample_num >= lSampleLast)
		return false;

	long dep;

	switch(parentPtr->video_sample_list[sample_num].frame_type) {
	case MPEG_FRAME_TYPE_B:
		dep = prev_IP(sample_num);
		if (dep>=0) {
			if (mpeg_lookup_frame(dep)<0)
			return false;
			sample_num = dep;
		}
	case MPEG_FRAME_TYPE_P:
		dep = prev_IP(sample_num);
		if (dep>=0 && mpeg_lookup_frame(dep)<0)
			return false;
	default:
		break;
	}

	return true;
}

BOOL VideoSourceMPEG::_isKey(LONG lSample) {
	return lSample<0 || lSample>=lSampleLast ? false : parentPtr->video_sample_list[translate_frame(renumber_frame(lSample))].frame_type == MPEG_FRAME_TYPE_I;
}

LONG VideoSourceMPEG::nearestKey(LONG lSample) {
	if (_isKey(lSample))
		return lSample;

	return prevKey(lSample);
}

LONG VideoSourceMPEG::prevKey(LONG lSample) {
	if (lSample < lSampleFirst) return -1;
	if (lSample >= lSampleLast) lSample = lSampleLast-1;

	lSample = translate_frame(renumber_frame(lSample));

	while(--lSample >= lSampleFirst) {
		if (parentPtr->video_sample_list[lSample].frame_type == MPEG_FRAME_TYPE_I)
			return lSample + parentPtr->video_sample_list[lSample].subframe_num;
	}

	return -1;
}

LONG VideoSourceMPEG::nextKey(LONG lSample) {
	if (lSample >= lSampleLast) return -1;
	if (lSample < lSampleFirst) lSample = lSampleFirst;

	lSample = translate_frame(renumber_frame(lSample));

	while(++lSample < lSampleLast) {
		if (parentPtr->video_sample_list[lSample].frame_type == MPEG_FRAME_TYPE_I)
			return lSample + parentPtr->video_sample_list[lSample].subframe_num;
	}

	return -1;
}

void VideoSourceMPEG::streamBegin(bool fRealTime) {
	frame_forw = frame_back = frame_bidir = -1;
}

void VideoSourceMPEG::streamSetDesiredFrame(long frame_num) {
	stream_desired_frame	= translate_frame(renumber_frame(frame_num));

	frame_type = parentPtr->video_sample_list[stream_desired_frame].frame_type;

	stream_current_frame	= stream_desired_frame;

//	_RPT2(0,"Requested frame: %ld (%ld)\n", frame_num, stream_desired_frame);

	switch(frame_type) {
	case MPEG_FRAME_TYPE_P:
		while(frame_forw != stream_current_frame && parentPtr->video_sample_list[stream_current_frame].frame_type != MPEG_FRAME_TYPE_I && stream_current_frame>0)
//			--stream_current_frame;
			stream_current_frame = prev_IP(stream_current_frame);

		// Avoid decoding a I/P frame twice.

		if (frame_forw == stream_current_frame && stream_current_frame != stream_desired_frame)
			++stream_current_frame;

		break;
	case MPEG_FRAME_TYPE_B:
		{
			long f,b;
			long last_IP;

			while(stream_current_frame>0 && parentPtr->video_sample_list[stream_current_frame].frame_type == MPEG_FRAME_TYPE_B)
				--stream_current_frame;

			f = stream_current_frame;	// forward predictive frame

			if (stream_current_frame>0) --stream_current_frame;

			while(stream_current_frame>0 && parentPtr->video_sample_list[stream_current_frame].frame_type == MPEG_FRAME_TYPE_B)
				--stream_current_frame;

			b = stream_current_frame;	// backward predictive frame

//			_RPT4(0,"B-frame requested: desire (%d,%d), have (%d,%d)\n", b, f, frame_back, frame_forw);

			if (frame_forw == f && frame_back == b) {
				stream_current_frame = stream_desired_frame;
				return;	// we got lucky!!!
			}

			// Maybe we only need to read the next I/P frame...

			if (frame_forw == b) {
				stream_current_frame = f;
				return;
			}

			// DAMN.  Back to last I/P; if it's an I, back up to previous I

			if (f==0) return; // No forward predictive frame, use first I

			if (parentPtr->video_sample_list[f].frame_type == MPEG_FRAME_TYPE_I)
				stream_current_frame = f-1;

			stream_current_frame = prev_IP(stream_current_frame);
			while(stream_current_frame>0 && parentPtr->video_sample_list[stream_current_frame].frame_type != MPEG_FRAME_TYPE_I) {
				last_IP = stream_current_frame;

				stream_current_frame = prev_IP(stream_current_frame);

				if (stream_current_frame == frame_back && last_IP == frame_forw) {
					stream_current_frame = frame_forw+1;
					break;
				}
			}

//			_RPT1(0,"Beginning B-frame scan: %ld\n", stream_current_frame);

		}
		break;
	}
}

long VideoSourceMPEG::streamGetNextRequiredFrame(BOOL *is_preroll) {
	if (	frame_forw == stream_desired_frame
		||	frame_back == stream_desired_frame
		||	frame_bidir == stream_desired_frame) {

		*is_preroll = FALSE;

		return -1;
	}

//	_RPT1(0,"current: %ld\n", stream_current_frame);

	switch(frame_type) {

		case MPEG_FRAME_TYPE_P:
		case MPEG_FRAME_TYPE_B:

			while(stream_current_frame != stream_desired_frame
					&& parentPtr->video_sample_list[stream_current_frame].frame_type == MPEG_FRAME_TYPE_B)

					++stream_current_frame;

			break;
	}

	switch(parentPtr->video_sample_list[stream_current_frame].frame_type) {
		case MPEG_FRAME_TYPE_I:
		case MPEG_FRAME_TYPE_P:
			frame_back = frame_forw;
			frame_forw = stream_current_frame;
			break;
		case MPEG_FRAME_TYPE_B:
			frame_bidir = stream_current_frame;
			break;
	}

	*is_preroll = (stream_desired_frame != stream_current_frame);

//	_RPT1(0,"MPEG: You want frame %ld.\n", stream_current_frame);

	return stream_current_frame++;
}

int VideoSourceMPEG::streamGetRequiredCount(long *pSize) {
	int current = stream_current_frame;
	int needed = 1;
	long size = 0;

	if (frame_forw == stream_desired_frame
		|| frame_back == stream_desired_frame
		|| frame_bidir == stream_desired_frame) {
		if (pSize)
			*pSize = 0;
		return 0;
	}


	if (frame_type == MPEG_FRAME_TYPE_I) {
		if (pSize)
			*pSize = parentPtr->video_sample_list[current].size;
		return 1;
	}

	while(current < stream_desired_frame) {

		while(current != stream_desired_frame
				&& parentPtr->video_sample_list[current].frame_type == MPEG_FRAME_TYPE_B)

				++current;

		size += parentPtr->video_sample_list[current].size;

		++needed;
		++current;
	}

	if (pSize)
		*pSize = size;

	return needed;
}

void *VideoSourceMPEG::streamGetFrame(void *inputBuffer, long data_len, BOOL is_key, BOOL is_preroll, long frame_num) {
	int buffer;

//	_RPT2(0,"Attempting to fetch frame %d [%c].\n", frame_num, "0IPBD567"[parentPtr->video_sample_list[frame_num].frame_type]);

	if (is_preroll || (buffer = mpeg_lookup_frame(frame_num))<0) {
		if (!frame_num) mpeg_reset();
//		_RPT2(0,"Decoding frame %d (%d bytes)\n", frame_num, data_len);

		// the "read" function gave us the extra 3 bytes we need

		if (data_len<=3) {
			if (MMX_enabled)
				__asm emms

			return lpvBuffer;	// HACK
		}

		mpeg_decode_frame(inputBuffer, data_len, frame_num);
	}
	
	if (!is_preroll) {
		int nBuffer = mpeg_lookup_frame(frame_num);

		if (bmihDecompressedFormat->biCompression == 'YVYU')
			mpeg_convert_frameUYVY16(lpvBuffer, nBuffer);
		else if (bmihDecompressedFormat->biCompression == '2YUY')
			mpeg_convert_frameYUY216(lpvBuffer, nBuffer);
		else if (bmihDecompressedFormat->biBitCount == 16)
			mpeg_convert_frame16(lpvBuffer, nBuffer);
		else if (bmihDecompressedFormat->biBitCount == 24)
			mpeg_convert_frame24(lpvBuffer, nBuffer);
		else
			mpeg_convert_frame32(lpvBuffer, nBuffer);
		fFBValid = TRUE;
	}

	if (MMX_enabled)
		__asm emms

	return lpvBuffer;
}

void *VideoSourceMPEG::getFrame(LONG frameNum) {
	LONG lCurrent, lKey;
	MPEGSampleInfo *msi;
	int buffer;

	frameNum = translate_frame(renumber_frame(frameNum));

	// Do we have the buffer stored somewhere?

	if ((buffer = mpeg_lookup_frame(frameNum))>=0) {
		if (bmihDecompressedFormat->biCompression == 'YVYU')
			mpeg_convert_frameUYVY16(lpvBuffer, buffer);
		else if (bmihDecompressedFormat->biCompression == '2YUY')
			mpeg_convert_frameYUY216(lpvBuffer, buffer);
		else if (bmihDecompressedFormat->biBitCount == 16)
			mpeg_convert_frame16(lpvBuffer, buffer);
		else if (bmihDecompressedFormat->biBitCount == 24)
			mpeg_convert_frame24(lpvBuffer, buffer);
		else
			mpeg_convert_frame32(lpvBuffer, buffer);

		fFBValid = TRUE;

		return lpvBuffer;
	}

	// I-frames have no prediction, so all we have to do there is decode the I-frame.
	// P-frames decode from the last P or I, so we have to decode all frames from the
	//		last I-frame.
	// B-frames decode from the last two P or I.  If the last I/P-frame is a P-frame,
	//		we can just decode from the last I.  If the last frame is an I-frame, we
	//		need to back up *two* I-frames.

	// possible cases:
	//
	//	I-frame		1)	Decompress the I-frame.
	//	P-frame		1)	No buffer contains the required pre-frame - read up from I-frame.
	//				2)	Earlier I/P-frame in a buffer - predict from that.
	//	B-frame		1)	No buffers with required prediction frames - read up from I-frame.
	//				2)	Forward prediction frame only - read the backward prediction I/P.
	//				3)	Backward prediction frame only - 

	lCurrent = frameNum;

	if (!is_I(frameNum) && -1 == (lCurrent = prev_I(frameNum)))
		throw MyError("Unable to decode: cannot find I-frame");

	switch(parentPtr->video_sample_list[frameNum].frame_type) {

	// if it's a B-frame, back off from the current frame; if the previous I/P frame
	// is an I, we will need to back off again

	case MPEG_FRAME_TYPE_B:
		lKey = frameNum;

		while(lKey > lCurrent && parentPtr->video_sample_list[lKey].frame_type == MPEG_FRAME_TYPE_B) --lKey;

		if (is_I(lKey))
			if (-1 != (lKey = prev_I(lKey)))
				lCurrent = lKey;
		break;

	// P-frame: start backing up from the current frame to the last I-frame.  If we find
	//			a I or P frame in one of our buffers beforehand, then swap it to the FORWARD
	//			buffer and start predicting off of that.

	case MPEG_FRAME_TYPE_P:
		lKey = lCurrent;
		buffer = -1;
		while(lKey < frameNum) {
			if (parentPtr->video_sample_list[lKey].frame_type != MPEG_FRAME_TYPE_B)
				if ((buffer = mpeg_lookup_frame(lKey))>=0)
					break;
			++lKey;
		}

		if (buffer>=0) {
			lCurrent = lKey+1;
			if (buffer != MPEG_BUFFER_FORWARD)
				mpeg_swap_buffers(buffer, MPEG_BUFFER_FORWARD);
		}
		break;
	}

	msi = &parentPtr->video_sample_list[lCurrent];
	do {
		//_RPT4(0,"getFrame: looking for %ld, at %ld (%c-frame, #%d)\n"
		//			,frameNum
		//			,lCurrent
		//			," IPBD567"[msi->frame_type]
		//			,msi->subframe_num);

		if (lCurrent == frameNum || (msi->frame_type == MPEG_FRAME_TYPE_I || msi->frame_type == MPEG_FRAME_TYPE_P)) {

			parentPtr->ReadStream(parentPtr->video_packet_buffer, msi->stream_pos, msi->size, FALSE);

			mpeg_decode_frame(parentPtr->video_packet_buffer, msi->size+4, lCurrent);

		}

		++msi;
	} while(lCurrent++ < frameNum);

	--msi;

	if (bmihDecompressedFormat->biCompression == 'YVYU')
		mpeg_convert_frameUYVY16(lpvBuffer, msi->frame_type>2 ? MPEG_BUFFER_BIDIRECTIONAL : MPEG_BUFFER_FORWARD);
	else if (bmihDecompressedFormat->biCompression == '2YUY')
		mpeg_convert_frameYUY216(lpvBuffer, msi->frame_type>2 ? MPEG_BUFFER_BIDIRECTIONAL : MPEG_BUFFER_FORWARD);
	else if (bmihDecompressedFormat->biBitCount == 16)
		mpeg_convert_frame16(lpvBuffer, msi->frame_type>2 ? MPEG_BUFFER_BIDIRECTIONAL : MPEG_BUFFER_FORWARD);
	else if (bmihDecompressedFormat->biBitCount == 24)
		mpeg_convert_frame24(lpvBuffer, msi->frame_type>2 ? MPEG_BUFFER_BIDIRECTIONAL : MPEG_BUFFER_FORWARD);
	else
		mpeg_convert_frame32(lpvBuffer, msi->frame_type>2 ? MPEG_BUFFER_BIDIRECTIONAL : MPEG_BUFFER_FORWARD);

	if (MMX_enabled)
		__asm emms

	fFBValid = TRUE;

	return getFrameBuffer();
}

int VideoSourceMPEG::_read(LONG lStart, LONG lCount, LPVOID lpBuffer, LONG cbBuffer, LONG *lBytesRead, LONG *lSamplesRead) {
	MPEGSampleInfo *msi = &parentPtr->video_sample_list[lStart];
	long len = msi->size;

	// Check to see if this is a frame type we're omitting

	switch(msi->frame_type) {
		case MPEG_FRAME_TYPE_P:
			if (parentPtr->iDecodeMode & InputFileMPEGOptions::DECODE_NO_P) {
				*lBytesRead = 1;
				*lSamplesRead = 1;
				return AVIERR_OK;
			}
			break;
		case MPEG_FRAME_TYPE_B:
			if (parentPtr->iDecodeMode & InputFileMPEGOptions::DECODE_NO_B) {
				*lBytesRead = 1;
				*lSamplesRead = 1;
				return AVIERR_OK;
			}
			break;
	}

	// We must add 3 bytes to hold the end marker when we decode

	if (!lpBuffer) {
		if (lSamplesRead) *lSamplesRead = 1;
		if (lBytesRead) *lBytesRead = len+4;
		return AVIERR_OK;
	}

	if (len > cbBuffer) {
		if (lSamplesRead) *lSamplesRead = 0;
		if (lBytesRead) *lBytesRead = 0;
		return AVIERR_BUFFERTOOSMALL;
	}

	parentPtr->ReadStream(lpBuffer, msi->stream_pos, len, FALSE);

	if (lSamplesRead) *lSamplesRead = 1;
	if (lBytesRead) *lBytesRead = len+4;

	return AVIERR_OK;
}

///////

long VideoSourceMPEG::renumber_frame(LONG lSample) {
	LONG lKey=lSample, lCurrent;

	if (lSample < lSampleFirst || lSample >= lSampleLast || (!is_I(lSample) && (lKey = prev_I(lSample))==-1))
		throw MyError("Frame not found (looking for %ld)", lSample);

	lCurrent = lKey;
	do {
		if (lSample-lKey == parentPtr->video_sample_list[lCurrent].subframe_num)
			return lCurrent;
	} while(++lCurrent < lSampleLast && !is_I(lCurrent));

	throw MyError("Frame not found (looking for %ld)", lSample);

	return -1;	// throws can't return!
}

bool VideoSourceMPEG::is_I(long lSample) {
	return parentPtr->video_sample_list[lSample].frame_type == MPEG_FRAME_TYPE_I;
}

long VideoSourceMPEG::prev_I(long lSample) {
	if (lSample < lSampleFirst) return -1;
	if (lSample >= lSampleLast) lSample = lSampleLast-1;

	while(--lSample >= lSampleFirst) {
		if (parentPtr->video_sample_list[lSample].frame_type == MPEG_FRAME_TYPE_I)
			return lSample;
	}

	return -1;
}

long VideoSourceMPEG::prev_IP(long f) {
	if (f<=0) return f;

	do
		--f;
	while (f>0 && parentPtr->video_sample_list[f].frame_type == MPEG_FRAME_TYPE_B);

	return f;
}

long VideoSourceMPEG::translate_frame(LONG lSample) {
	MPEGSampleInfo *msi = &parentPtr->video_sample_list[lSample];

	// Check to see if this is a frame type we're omitting; if so,
	// keep backing up until it's one we're not.

	while(lSample > 0) {
		switch(msi->frame_type) {
			case MPEG_FRAME_TYPE_P:
				if (!(parentPtr->iDecodeMode & InputFileMPEGOptions::DECODE_NO_P))
					return lSample;

				break;
			case MPEG_FRAME_TYPE_B:
				if (!(parentPtr->iDecodeMode & InputFileMPEGOptions::DECODE_NO_B))
					return lSample;

				break;
			default:
				return lSample;
		}

		--lSample;
		--msi;
	}

	return lSample;
}

//////////////////////////////////////////////////////////////////////////
//
//
//						AudioSourceMPEG
//
//
//////////////////////////////////////////////////////////////////////////

	// 0000F0FF 12 bits	sync mark
	//
	// 00000800  1 bit	version
	// 00000600  2 bits	layer (3 = layer I, 2 = layer II, 1 = layer III)
	// 00000100  1 bit	error protection (0 = enabled)
	//
	// 00F00000  4 bits	bitrate_index
	// 000C0000  2 bits	sampling_freq
	// 00020000  1 bit	padding
	// 00010000  1 bit	extension
	//
	// C0000000  2 bits	mode (0=stereo, 1=joint stereo, 2=dual channel, 3=mono)
	// 30000000  2 bits	mode_ext
	// 08000000  1 bit	copyright
	// 04000000  1 bit	original
	// 03000000  2 bits	emphasis

#define MPEGAHDR_SYNC_MASK			(0x0000F0FF)
#define MPEGAHDR_VERSION_MASK		(0x00000800)
#define	MPEGAHDR_LAYER_MASK			(0x00000600)
#define MPEGAHDR_CRC_MASK			(0x00000100)
#define MPEGAHDR_BITRATE_MASK		(0x00F00000)
#define MPEGAHDR_SAMPLERATE_MASK	(0x000C0000)
#define MPEGAHDR_PADDING_MASK		(0x00020000)
#define MPEGAHDR_EXT_MASK			(0x00010000)
#define MPEGAHDR_MODE_MASK			(0xC0000000)
#define MPEGAHDR_MODEEXT_MASK		(0x30000000)
#define MPEGAHDR_COPYRIGHT_MASK		(0x08000000)
#define MPEGAHDR_ORIGINAL_MASK		(0x04000000)
#define MPEGAHDR_EMPHASIS_MASK		(0x03000000)

#ifdef ENABLE_AUDIO_SUPPORT

class AudioSourceMPEG : public AudioSource, IAMPBitsource {
private:
	InputFileMPEG *parentPtr;
	IAMPDecoder *iad;
	void *pkt_buffer;
	short sample_buffer[1152*2][2];
	char *pDecoderPoint;
	char *pDecoderLimit;

	long lCurrentPacket;
	long lFrameLen, lFrameDataLen;
	int layer;

	BOOL _isKey(LONG lSample);

public:
	AudioSourceMPEG(InputFileMPEG *);
	~AudioSourceMPEG();

	// AudioSource methods

	BOOL init();
	int _read(LONG lStart, LONG lCount, LPVOID lpBuffer, LONG cbBuffer, LONG *lSamplesRead, LONG *lBytesRead);

	// IAMPBitsource methods

	int read(void *buffer, int bytes);

};

AudioSourceMPEG::AudioSourceMPEG(InputFileMPEG *pp) : AudioSource() {
	parentPtr = pp;

	if (!(pkt_buffer = new char[8192]))
		throw MyMemoryError();

	if (!(iad = CreateAMPDecoder())) {
		delete pkt_buffer;
		throw MyMemoryError();
	}

	lCurrentPacket = -1;
}

AudioSourceMPEG::~AudioSourceMPEG() {
	delete pkt_buffer;
	delete iad;
}

BOOL AudioSourceMPEG::_isKey(LONG lSample) {
	return TRUE;
}

static const int bitrate[3][15] = {
          {0,32,64,96,128,160,192,224,256,288,320,352,384,416,448},
          {0,32,48,56,64,80,96,112,128,160,192,224,256,320,384},
          {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320}
        };

static const long samp_freq[4] = {44100, 48000, 32000, 0};

BOOL AudioSourceMPEG::init() {
	WAVEFORMATEX *wfex;

	layer = 4 - ((parentPtr->audio_first_header>>9)&3);

	lSampleFirst = 0;
	lSampleLast = parentPtr->aframes * (layer==1 ? 384 : 1152);

	if (layer == 3) {
		lFrameLen = MulDiv(
							bitrate	[3-((parentPtr->audio_first_header>>9)&3)][(parentPtr->audio_first_header>>20)&15],
							144000,
							samp_freq[(parentPtr->audio_first_header>>18)&3]);

		lFrameDataLen = lFrameLen - (((parentPtr->audio_first_header & 0xC0000000) == 0xC0000000)
			? 13 : 21);
	}

	if (!(wfex = (WAVEFORMATEX*)allocFormat(sizeof(PCMWAVEFORMAT))))
		return FALSE;

	// [?] * [bits/sec] / [samples/sec] = [bytes/frame]
	// [?] * [bits/sample] = [bytes/frame]
	// [?] * [.125 byte/bit] * [bits/sample] = [bytes/frame]
	// [samples/frame]


	wfex->wFormatTag		= WAVE_FORMAT_PCM;
	wfex->nChannels			= ((parentPtr->audio_first_header & 0xC0000000) == 0xC0000000)
								? 1 : 2;
	wfex->nBlockAlign		= wfex->nChannels*2;
	wfex->nSamplesPerSec	= samp_freq[(parentPtr->audio_first_header>>18)&3];
	wfex->nAvgBytesPerSec	= wfex->nSamplesPerSec * wfex->nBlockAlign;
	wfex->wBitsPerSample	= 16;

	streamInfo.fccType					= streamtypeAUDIO;
	streamInfo.fccHandler				= 0;
	streamInfo.dwFlags					= 0;
	streamInfo.dwScale					= wfex->nBlockAlign;
	streamInfo.dwRate					= samp_freq[(parentPtr->audio_first_header>>18)&3] * wfex->nBlockAlign;
	streamInfo.dwStart					= 0;
	streamInfo.dwLength					= parentPtr->aframes * (layer==1 ? 384 : 1152);
	streamInfo.dwInitialFrames			= 0;
	streamInfo.dwSuggestedBufferSize	= 0;
	streamInfo.dwQuality				= 0xffffffffL;
	streamInfo.dwSampleSize				= wfex->nBlockAlign;
	streamInfo.rcFrame.left				= 0;
	streamInfo.rcFrame.top				= 0;
	streamInfo.rcFrame.right			= 0;
	streamInfo.rcFrame.bottom			= 0;

	try {
		iad->Init();
		iad->setSource(this);
	} catch(int i) {
		throw MyError(iad->getErrorString(i));
	}

	return TRUE;
}

int AudioSourceMPEG::_read(LONG lStart, LONG lCount, LPVOID lpBuffer, LONG cbBuffer, LONG *lBytesRead, LONG *lSamplesRead) {
	long lAudioPacket;
	MPEGSampleInfo *msi;
	long len;
	long samples, ba = getWaveFormat()->nBlockAlign;

	if (layer==1) {
		lAudioPacket = lStart/384;
		samples = 384 - (lStart % 384);
	} else {
		lAudioPacket = lStart/1152;
		samples = 1152 - (lStart % 1152);
	}

	if (lCount != AVISTREAMREAD_CONVENIENT)
		if (samples > lCount) samples = lCount;

#if 0
	if (samples*ba > cbBuffer) {
		if (lSamplesRead) *lSamplesRead = 0;
		if (lBytesRead) *lBytesRead = 0;
		return AVIERR_BUFFERTOOSMALL;
	}
#else
	if (samples*ba > cbBuffer && lpBuffer) {
		samples = cbBuffer / ba;

		if (samples <= 0) {
			if (lSamplesRead) *lSamplesRead = 0;
			if (lBytesRead) *lBytesRead = 0;
			return AVIERR_BUFFERTOOSMALL;
		}
	}
#endif

	if (!lpBuffer) {
		if (lSamplesRead) *lSamplesRead = samples;
		if (lBytesRead) *lBytesRead = samples * ba;
		return AVIERR_OK;
	}

	// Layer 3 audio is a royal pain in the *(@#&$*( because the data can start
	// up to 511 bytes behind the beginning of the current frame.  To get around
	// this problem, we preread up to 511 bytes from the previous frame.


	if (lCurrentPacket != lAudioPacket) {
		try {

			if (layer!=3 || lCurrentPacket<0 || lCurrentPacket+1 != lAudioPacket) {
//				_RPT0(0,"Resetting...\n");
				iad->Reset();

				lCurrentPacket = lAudioPacket;

				if (layer == 3) {
					lCurrentPacket -= (510 + lFrameDataLen) / lFrameDataLen;
				} else
					--lCurrentPacket;

				if (lCurrentPacket < 0)
					lCurrentPacket = 0;
			} else
				lCurrentPacket = lAudioPacket;

			do {
//				_RPT1(0,"Decoding packet: %d\n", lCurrentPacket);

				msi = &parentPtr->audio_sample_list[lCurrentPacket];
				len = msi->size;

				parentPtr->ReadStream(pkt_buffer, msi->stream_pos, len, TRUE);

				pDecoderPoint = (char *)pkt_buffer;
				pDecoderLimit = (char *)pkt_buffer + len;

				if ((unsigned char)pDecoderPoint[0] != 0xff || ((unsigned char)pDecoderPoint[1]&0xf0)!=0xf0)
					throw MyError("Sync error");

				iad->setDestination((short *)sample_buffer);
				iad->ReadHeader();
				if (layer==3 && lCurrentPacket < lAudioPacket)
					iad->PrereadFrame();
				else
					iad->DecodeFrame();

			} while(++lCurrentPacket <= lAudioPacket);
			--lCurrentPacket;
		} catch(int i) {
			throw MyError("MPEG-1 audio decode error: %s", iad->getErrorString(i));
		}
	}

	memcpy(lpBuffer, (short *)sample_buffer + (lStart%(layer==1?384:1152))*(ba/2), samples*ba);

	if (lSamplesRead) *lSamplesRead = samples;
	if (lBytesRead) *lBytesRead = samples*ba;

	return AVIERR_OK;
}

int AudioSourceMPEG::read(void *buffer, int bytes) {
	if (pDecoderPoint+bytes > pDecoderLimit)
		throw MyError("Incomplete audio frame");

	memcpy(buffer, pDecoderPoint, bytes);
	pDecoderPoint += bytes;

	return bytes;
}

#endif

//////////////////////////////////////////////////////////////////////////
//
//
//					MPEGAudioParser
//
//
//////////////////////////////////////////////////////////////////////////

static bool MPEG_check_audio_header_validity(long hdr) {
	// 0000F0FF 12 bits	sync mark
	//
	// 00000800  1 bit	version
	// 00000600  2 bits	layer (3 = layer I, 2 = layer II, 1 = layer III)
	// 00000100  1 bit	error protection (0 = enabled)
	//
	// 00F00000  4 bits	bitrate_index
	// 000C0000  2 bits	sampling_freq
	// 00020000  1 bit	padding
	// 00010000  1 bit	extension
	//
	// C0000000  2 bits	mode (0=stereo, 1=joint stereo, 2=dual channel, 3=mono)
	// 30000000  2 bits	mode_ext
	// 08000000  1 bit	copyright
	// 04000000  1 bit	original
	// 03000000  2 bits	emphasis

	// 00 for layer ("layer 4") is not valid
	if (!(hdr & 0x00000600))
		return false;

	// 1111 for bitrate is not valid
	if ((hdr & 0x00F00000) == 0x00F00000)
		return false;

	// 11 for sampling frequency is not valid
	if ((hdr & 0x000C0000) == 0x000C0000)
		return false;

	// Looks okay to me...
	return true;
}

////////////////////////////////////////////////////////////////////

class MPEGAudioParser {
private:
	unsigned long lFirstHeader;
	int hstate, skip;
	unsigned long header;
	MPEGSampleInfo msi;
	__int64 bytepos;

public:
	MPEGAudioParser();

	void Parse(const void *, int, DataVector *);
	unsigned long getHeader();
};

MPEGAudioParser::MPEGAudioParser() {
	lFirstHeader = 0;
	header = 0;
	hstate = 0;
	skip = 0;
	bytepos = 0;
}

unsigned long MPEGAudioParser::getHeader() {
	return lFirstHeader;
}

void MPEGAudioParser::Parse(const void *pData, int len, DataVector *dst) {
	unsigned char *src = (unsigned char *)pData;

	while(len>0) {
		if (skip) {
			int tc = skip;

			if (tc > len)
				tc = len;

			len -= tc;
			skip -= tc;
			src += tc;

			// Audio frame finished?

			if (!skip) {
				dst->Add(&msi);
			}

			continue;
		}

		// Collect header bytes.

		++hstate;
		header = (header>>8) | ((long)*src++ << 24);
		--len;

		if (hstate>=4 && MPEG_check_audio_header_validity(header)) {

			// Check for header consistency.
			//
			// It conceivably could be possible for the stream to change, but
			// this is VERY unlikely.  Instead, we'll assume the bitrate,
			// sampling frequency, MPEG level/layer, mode, copyright, and
			// original flags do not change.
			//
			// 11/21:	EH_ED.MPG switches between stereo and joint_stereo mode
			//			in the middle of the audio stream.  Mode check disabled.
			//
			// 02/21/00: Hyper Police #3 switches original bits.

			if (lFirstHeader && ((header ^ lFirstHeader) & 0x08FC0E00)) {
				continue;
			}

			// Okay, we like the header.

			hstate = 0;

			// Must be a frame start.

			if (!lFirstHeader)
				lFirstHeader = header;

			long lFrameLen;

			// Layer I works in units of 4 bytes.  Layer II and III have byte granularity.
			// Both may or may not have one extra unit depending on the padding bit.

			int layer			= ((header>>9)&3)^3;
			int bitrateidx		= (header>>20)&15;
			int samprate		= samp_freq[(header>>18)&3];
			bool padding		= !!(header & 0x00020000);

			if (!layer) {	// layer I
				lFrameLen = 4*((bitrate[layer][bitrateidx]*12000) / samprate);
				if (padding) lFrameLen+=4;
			} else {													// layer II, III
				lFrameLen = (bitrate[layer][bitrateidx] * 144000) / samprate;
				if (padding) ++lFrameLen;
			}

			// Setup the sample information.  Don't add the sample, in case it's incomplete.

			msi.stream_pos		= bytepos + (src - (unsigned char *)pData) - 4;
			msi.header			= header;
			msi.size			= lFrameLen;

			// Now skip the remainder of the sample.

			skip = lFrameLen-4;
		}
	}

	bytepos += src - (unsigned char *)pData;
}

//////////////////////////////////////////////////////////////////////////
//
//
//					MPEGVideoParser
//
//
//////////////////////////////////////////////////////////////////////////

class MPEGVideoParser {
private:
	unsigned char buf[72+64];
	char nonintramatrix[64];
	char intramatrix[64];

	int idx, bytes;

	MPEGSampleInfo msi;
	__int64 bytepos;
	long header;

	bool fCustomIntra, fCustomNonintra;
	bool fPicturePending;
	bool fFoundSequenceStart;

public:
	long frame_rate;
	int width, height;

	MPEGVideoParser();

	void setPos(__int64);
	void Parse(const void *, int, DataVector *);
};

MPEGVideoParser::MPEGVideoParser() {
	bytepos = 0;
	header = -1;

	fCustomIntra = false;
	fCustomNonintra = false;
	fPicturePending = false;
	fFoundSequenceStart = false;

	idx = bytes = 0;
}

void MPEGVideoParser::setPos(__int64 pos) {
	bytepos = pos;
}

void MPEGVideoParser::Parse(const void *pData, int len, DataVector *dst) {
	static const long frame_speeds[16]={
		0,			// 0
		23976,		// 1 (23.976 fps) - FILM
		24000,		// 2 (24.000 fps)
		25000,		// 3 (25.000 fps) - PAL
		29970,		// 4 (29.970 fps) - NTSC
		30000,		// 5 (30.000 fps)
		50000,		// 6 (50.000 fps) - PAL noninterlaced
		59940,		// 7 (59.940 fps) - NTSC noninterlaced
		60000,		// 8 (60.000 fps)
		0,			// 9
		0,			// 10
		0,			// 11
		0,			// 12
		0,			// 13
		0,			// 14
		0			// 15
	};

	unsigned char *src = (unsigned char *)pData;

	while(len>0) {
		if (idx<bytes) {
			int tc = bytes - idx;

			if (tc > len)
				tc = len;

			memcpy(buf+idx, src, tc);

			len -= tc;
			idx += tc;
			src += tc;

			// Finished?

			if (idx>=bytes) {
				switch(header) {
					case VIDPKT_TYPE_PICTURE_START:
						msi.frame_type		= (buf[1]>>3)&7;
						msi.subframe_num	= (buf[0]<<2) | (buf[1]>>6);
						fPicturePending		= true;
						header = 0xFFFFFFFF;
						break;

					case VIDPKT_TYPE_SEQUENCE_START:
						//	12 bits: width
						//	12 bits: height
						//	 4 bits: aspect ratio
						//	 4 bits: picture rate
						//	18 bits: bitrate
						//	 1 bit : ?
						//	10 bits: VBV buffer
						//	 1 bit : const_param
						//	 1 bit : intramatrix present
						//[256 bits: intramatrix]
						//	 1 bit : nonintramatrix present
						//[256 bits: nonintramatrix]
						if (bytes == 8) {
							width	= (buf[0]<<4) + (buf[1]>>4);
							height	= ((buf[1]<<8)&0xf00) + buf[2];
							frame_rate = frame_speeds[(unsigned char)buf[3] & 15];

							if (!frame_rate)
								throw MyError("MPEG-1 video stream contains an invalid frame rate (%d).", buf[3] & 15);

							if (buf[7]&2) {		// Intramatrix present
								bytes = 72;	// can't decide yet
								break;
							} else if (buf[7]&1) {	// Nonintramatrix present
								bytes = 72;
								break;
							}
						} else if (bytes == 72) {
							if (buf[7]&2) {
								for(int i=0; i<64; i++)
									intramatrix[i] = ((buf[i+7]<<7)&0x80) | (buf[i+8]>>1);

								fCustomIntra = true;

								if (buf[71]&1) {
									bytes = 72+64;		// both matrices
									break;
								}
							} else {		// Nonintramatrix only
								memcpy(nonintramatrix, buf+8, 64);
								fCustomNonintra = true;
							}
						} else if (bytes == 72+64) {	// Both matrices (intra already loaded)
							memcpy(nonintramatrix, buf+72, 64);

							fCustomIntra = fCustomNonintra = true;
						}

						// Initialize MPEG-1 video decoder.

						mpeg_initialize(width, height, fCustomIntra ? intramatrix : NULL, fCustomNonintra ? nonintramatrix : NULL, FALSE);
						header = 0xFFFFFFFF;
						break;
				}	
			}
			continue;
		}

		// Look for a valid MPEG-1 header

		header = (header<<8) + *src++;
		--len;

		if ((header&0xffffff00) == 0x00000100) {
			header &= 0xff;
			if (fPicturePending && (header<VIDPKT_TYPE_SLICE_START_MIN || header>VIDPKT_TYPE_SLICE_START_MAX) && header != VIDPKT_TYPE_USER_START) {
				msi.size = (int)(bytepos + (src - (unsigned char *)pData) - 4 - msi.stream_pos);
				dst->Add(&msi);
				fPicturePending = false;
			}

			switch(header) {
			case VIDPKT_TYPE_SEQUENCE_START:
				if (fFoundSequenceStart) break;
				fFoundSequenceStart = true;

				bytes = 8;
				idx = 0;
				break;

			case VIDPKT_TYPE_PICTURE_START:
				idx = 0;
				bytes = 2;
				msi.stream_pos = bytepos + (src - (unsigned char *)pData) - 4;
				break;

			case VIDPKT_TYPE_EXT_START:
				throw MyError("VirtualDub cannot decode MPEG-2 video streams.");

			default:
				header = 0xFFFFFFFF;
			}
		}
	}

	bytepos += src - (unsigned char *)pData;
}

//////////////////////////////////////////////////////////////////////////
//
//
//							InputFileMPEG
//
//
//////////////////////////////////////////////////////////////////////////


extern HWND g_hWnd;

const char InputFileMPEG::szME[]="MPEG Import Filter";

#define VIDEO_PACKET_BUFFER_SIZE	(1048576)
#define AUDIO_PACKET_BUFFER_SIZE	(65536)

InputFileMPEG::InputFileMPEG() {
	// clear variables

	file_cpos = 0;
	fh = -1;
	video_packet_buffer = NULL;
	audio_packet_buffer = NULL;
	video_packet_list = NULL;
	video_sample_list = NULL;
	audio_packet_list = NULL;
	audio_sample_list = NULL;
	audio_first_header = 0;

	audioSrc = NULL;
	videoSrc = NULL;

	fInterleaved = fHasAudio = FALSE;

	iDecodeMode = 0;
	fAcceptPartial = false;
	fAbort = false;
	fIsVCD = false;

	pFastRead = NULL;

	last_packet[0] = last_packet[1] = 0;
}

InputFile *CreateInputFileMPEG() {
	return new InputFileMPEG();
}

void InputFileMPEG::Init(char *szFile) {
	BOOL finished = FALSE;
	HWND hWndStatus;

	AddFilename(szFile);

    // allocate packet buffer

	if (!(video_packet_buffer = new char[VIDEO_PACKET_BUFFER_SIZE]))
		throw MyMemoryError();

#ifdef ENABLE_AUDIO_SUPPORT
	if (!(audio_packet_buffer = new char[AUDIO_PACKET_BUFFER_SIZE]))
		throw MyMemoryError();
#endif

	// see if we can open the file

	if (-1 == (fh = _open(szFile, _O_BINARY | _O_RDONLY | _O_SEQUENTIAL)))
		throw MyError("%s: couldn't open \"%s\"", szME, szFile);

	pFastRead = new FastReadStream(fh, 24, 32768);

	// determine the file's size...

	if (-1 == (file_len = _filelengthi64(fh)))
		throw MyError("%s: couldn't determine file size", szME);

	// Begin file parsing!  This is a royal bitch!

	StartScan();

	try {
		DataVector video_stream_blocks(sizeof MPEGPacketInfo);
		DataVector video_stream_samples(sizeof MPEGSampleInfo);
		__int64 video_stream_pos = 0;
		MPEGVideoParser videoParser;


#ifdef ENABLE_AUDIO_SUPPORT
		DataVector audio_stream_blocks(sizeof MPEGPacketInfo);
		DataVector audio_stream_samples(sizeof MPEGSampleInfo);
		MPEGAudioParser audioParser;
		__int64 audio_stream_pos = 0;
#endif

		bool first_packet = true;
		bool end_of_file = false;
		bool fTrimLastOff = false;

		// seek to first pack code

		{
			char ch[3];
			int scan_count = 256;

			_read(fh, ch, 3);

			while(scan_count > 0) {
				if (ch[0]=='R' && ch[1]=='I' && ch[2]=='F') {
					fIsVCD = true;
					fInterleaved = true;

					// The Read() code skips over the last 4 bytes of a sector
					// and the beginning 16 bytes of the next, so we need to
					// back up 4.

					i64ScanCpos = 40 + 256 - scan_count;
					_lseeki64(fh, i64ScanCpos, SEEK_SET);

					break;
				} else if (ch[0]==0 && ch[1]==0 && ch[2]==1) {
					fIsVCD = false;

					// We want reads to be aligned.

					i64ScanCpos = 0;
					_lseeki64(fh, 0, SEEK_SET);
					Skip(256 + 3 - scan_count);

					break;
				}

				ch[0] = ch[1];
				ch[1] = ch[2];
				_read(fh, ch+2, 1);

				--scan_count;

				if (!scan_count)
					throw MyError("%s: Invalid MPEG file", szME);
			}
		}

		try {
			do {
				int c;
				int stream_id, pack_length;

				file_cpos = Tell();
				guiDlgMessageLoop(hWndStatus);

				if (fAbort)
					throw MyUserAbortError();

				if (first_packet) {
					if (!fIsVCD) {
						c = Read();

						fInterleaved = (c==0xBA);

						if (!fInterleaved) {
							videoParser.setPos(Tell()-4);

							unsigned char buf[4];

							buf[0] = buf[1] = 0;
							buf[2] = 1;
							buf[3] = c;

							videoParser.Parse(buf, 4, &video_stream_samples);
						}
					}

					// pop up the dialog...

					if (!(hWndStatus = CreateDialogParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_PROGRESS), g_hWnd, ParseDialogProc, (LPARAM)this)))
						throw MyMemoryError();

					first_packet = false;
				} else if (fInterleaved)
					c=Read();
				else
					c = 0xe0;

				switch(c) {

//					One for audio and for video?

					case VIDPKT_TYPE_SEQUENCE_END:
					case 0xb9:		// ISO 11172 end code
						break;

					case 0xba:		// new pack
						if ((Read() & 0xf0) != 0x20) throw MyError("%s: pack synchronization error", szME);
						Skip(7);
						break;

					case 0xbb:		// system header
						Skip(8);
						while((c=Read()) & 0x80)
							Skip(2);

						UnRead();
						break;

					default:
						if (c < 0xc0 || c>=0xf0)
							break;

						if (fInterleaved) {
							__int64 tagpos = Tell();
							stream_id = c;
							pack_length = Read()<<8;
							pack_length += Read();

//							_RPT3(0,"Encountered packet: stream %02x, pack length %ld, position %08lx\n", stream_id, pack_length, file_cpos);

							if (stream_id != PRIVATE_STREAM2) {
								--pack_length;

								while((c=Read()) == 0xff) {
									--pack_length;
								}

								if ((c>>6) == 1) {	// 01
									pack_length-=2;
									Read();			// skip one byte
									c=Read();
								}
								if ((c>>4) == 2) {	// 0010
									pack_length -= 4;
									Skip(4);
								} else if ((c>>4) == 3) {	// 0011
									pack_length -= 9;
									Skip(9);
								} else if (c != 0x0f)
									throw MyError("%s: packet sync error on packet stream (%I64x)", szME, tagpos);
							}
						} else {
							stream_id = 0xe0;
							pack_length = 65536; //VIDEO_PACKET_BUFFER_SIZE;
						}

						// check packet type

						if ((0xe0 & stream_id) == 0xc0) {			// audio packet

#ifdef ENABLE_AUDIO_SUPPORT
							fHasAudio = TRUE;

							MPEGPacketInfo mpi;

							mpi.file_pos		= Tell();
							mpi.stream_pos		= audio_stream_pos;
							audio_stream_blocks.Add(&mpi);
							audio_stream_pos += pack_length;

							Read(audio_packet_buffer, pack_length, false);
							audioParser.Parse(audio_packet_buffer, pack_length, &audio_stream_samples);
							pack_length = 0;
#endif	// ENABLE_AUDIO_SUPPORT

						} else if ((0xf0 & stream_id) == 0xe0) {	// video packet

							if (fInterleaved) {
								MPEGPacketInfo mpi;

								mpi.file_pos		= Tell();
								mpi.stream_pos		= video_stream_pos;
								video_stream_blocks.Add(&mpi);
								video_stream_pos += pack_length;
							}

							int actual = Read(video_packet_buffer, pack_length, !fInterleaved);

							if (!fInterleaved && actual < pack_length)
								end_of_file = true;

							videoParser.Parse(video_packet_buffer, actual, &video_stream_samples);
							pack_length = 0;
						}

						if (pack_length)
								Skip(pack_length);
						break;
				}
			} while(!finished && (fInterleaved ? NextStartCode() : !end_of_file));
		} catch(const MyError&) {
			if (!fAcceptPartial)
				throw;

			fTrimLastOff = true;
		}

		// We're done scanning the file.  Finish off any ending packets we may have.

		static const unsigned char finish_tag[]={ 0, 0, 1, 0xff };

		videoParser.Parse(finish_tag, 4, &video_stream_samples);

		this->width = videoParser.width;
		this->height = videoParser.height;
		this->frame_rate = videoParser.frame_rate;

		// Construct stream and packet lookup tables.

		if (fInterleaved) {
			MPEGPacketInfo mpi;

			mpi.file_pos		= 0;
			mpi.stream_pos		= video_stream_pos;
			video_stream_blocks.Add(&mpi);

			video_packet_list = (MPEGPacketInfo *)video_stream_blocks.MakeArray();
			packets = video_stream_blocks.Length() - 1;

#ifdef ENABLE_AUDIO_SUPPORT
			mpi.file_pos		= 0;
			mpi.stream_pos		= audio_stream_pos;
			audio_stream_blocks.Add(&mpi);

			audio_packet_list = (MPEGPacketInfo *)audio_stream_blocks.MakeArray();
			apackets = audio_stream_blocks.Length() - 1;

			audio_sample_list = (MPEGSampleInfo *)audio_stream_samples.MakeArray();
			aframes = audio_stream_samples.Length();
			audio_first_header = audioParser.getHeader();
#endif // ENABLE_AUDIO_SUPPORT
		}

		video_sample_list = (MPEGSampleInfo *)video_stream_samples.MakeArray();
		frames = video_stream_samples.Length();

		// Begin renumbering the frames.  Some MPEG files have incorrectly numbered
		// subframes within each group.  So we do them from scratch.

		{
			int i;
			int sf = 0;		// subframe #
			MPEGSampleInfo *cached_IP = NULL;

			for(i=0; i<frames; i++) {

//				_RPT3(0,"Frame #%d: %c-frame (subframe: %d)\n", i, video_sample_list[i].frame_type[" IPBD567"], video_sample_list[i].subframe_num);

				if (video_sample_list[i].frame_type != MPEG_FRAME_TYPE_B) {
					if (cached_IP) cached_IP->subframe_num = sf++;
					cached_IP = &video_sample_list[i];

					if (video_sample_list[i].frame_type == MPEG_FRAME_TYPE_I)
						sf = 0;
				} else
					video_sample_list[i].subframe_num = sf++;

//				_RPT3(0,"Frame #%d: %c-frame (subframe: %d)\n", i, video_sample_list[i].frame_type[" IPBD567"], video_sample_list[i].subframe_num);
			}

			if (cached_IP)
				cached_IP->subframe_num = sf;
		}

		// If we are accepting partial streams, then cut off the last video frame, as it may be incomplete.
		// The audio parser checks for the entire frame to arrive, so we don't need to trim the audio.

		if (fTrimLastOff) {
			if (frames) --frames;
		}

	} catch(const MyError&) {
		DestroyWindow(hWndStatus);
		hWndStatus = NULL;
		throw;
	}

	EndScan();

	DestroyWindow(hWndStatus);
	hWndStatus = NULL;

	// initialize DubSource pointers

#ifdef ENABLE_AUDIO_SUPPORT
	audioSrc = fHasAudio ? new AudioSourceMPEG(this) : NULL;
	if (audioSrc)
		audioSrc->init();
#endif

	videoSrc = new VideoSourceMPEG(this);
	videoSrc->init();
}

InputFileMPEG::~InputFileMPEG() {
	_RPT0(0,"Destructor called\n");

	EndScan();

	delete videoSrc;
	delete audioSrc;

	mpeg_deinitialize();
	delete video_packet_buffer;
	delete audio_packet_buffer;
	delete video_packet_list;
	delete video_sample_list;
	delete audio_packet_list;
	delete audio_sample_list;

	if (pFastRead)
		delete pFastRead;

	if (fh >= 0)
		_close(fh);
}

void InputFileMPEG::StartScan() {
	if (!(pScanBuffer = new char[SCAN_BUFFER_SIZE]))
		throw MyMemoryError();

	pScan = pScanLimit = pScanBuffer;
	i64ScanCpos = 0;
}

void InputFileMPEG::EndScan() {
	delete pScanBuffer;
	pScanBuffer = NULL;
}

bool InputFileMPEG::NextStartCode() {
	int c;

	while(EOF!=(c=Read())) {
		if (!c) {	// 00
			if (EOF==(c=Read())) return false;

			if (!c) {	// 00 00
				do {
					if (EOF==(c=Read())) return false;
				} while(!c);

				if (c==1)	// (00 00 ...) 00 00 01 xx
					return true;
			}
		}
	}

	return false;
}

void InputFileMPEG::Skip(int bytes) {
	int tc;

	while(bytes>0) {
		tc = pScanLimit - pScan;

		if (!tc) {
			if (EOF == _Read())
				throw MyError("%s: unexpected end of file", szME);

			--bytes;
			continue;
		}
			

		if (tc >= bytes) {
			pScan += bytes;
			break;
		}

		bytes -= tc;
		i64ScanCpos += (pScanLimit - pScanBuffer);
		pScan = pScanLimit = pScanBuffer;
	}
}

int InputFileMPEG::_Read() {
	char c;

	if (!pScan)
		return EOF;

	if (!Read(&c, 1, true))
		return EOF;

	return (unsigned char)c;
}

int InputFileMPEG::Read(void *buffer, int bytes, bool fShortOkay) {
	int total = 0;
	int tc;

	if (!pScan)
		if (fShortOkay)
			return 0;
		else
			throw MyError("%s: unexpected end of file", szME);

	if (pScan < pScanLimit) {
		tc = pScanLimit - pScan;

		if (tc > bytes)
			tc = bytes;

		memcpy(buffer, pScan, tc);

		pScan += tc;
		buffer = (char *)buffer + tc;
		total = tc;
		bytes -= tc;
	}

	if (bytes) {
		int actual;

		// Split the read into SCAN_BUFFER_SIZE chunks so we can
		// keep reading big chunks throughout the file.

		tc = bytes - bytes % SCAN_BUFFER_SIZE;

		// With a VideoCD, read the header and then the pack.

		_RPT1(0,"Starting at %I64lx\n", _telli64(fh));

		if (tc) do {
			if (fIsVCD) {
				char hdr[20];

				actual = _read(fh, hdr, 20);

				if (actual < 0)
					throw MyError("%s: read error", szME);
				else if (actual != 20)
					if (fShortOkay)
						return total;

				i64ScanCpos += 20;

				tc = 2332;
			}

			if (tc > 0) {
				actual = _read(fh, buffer, tc);

				if (actual < 0)
					throw MyError("%s: read error", szME);
				else if (actual != tc)
					if (fShortOkay)
						return total + actual;
					else
						throw MyError("%s: unexpected end of file", szME);

				total += actual;
				i64ScanCpos += actual;
				buffer = (char *)buffer + actual;
			}

			bytes -= tc;
		} while(fIsVCD && bytes >= 2332);

		tc = bytes;

		if (tc) {
			if (fIsVCD) {
				char hdr[20];

				actual = _read(fh, hdr, 20);

				if (actual < 0)
					throw MyError("%s: read error", szME);
				else if (actual != 20)
					if (fShortOkay)
						return total;

				i64ScanCpos += 20;

				actual = _read(fh, pScanBuffer, 2332);
			} else
				actual = _read(fh, pScanBuffer, SCAN_BUFFER_SIZE);

			if (actual < 0)
				throw MyError("%s: read error", szME);
			else if (!fShortOkay && actual < tc)
				throw MyError("%s: unexpected end of file", szME);

			i64ScanCpos += (pScan - pScanBuffer);

			if (actual < tc)
				tc = actual;

			memcpy(buffer, pScanBuffer, tc);

			total += tc;

			pScanLimit	= pScanBuffer + actual;
			pScan		= pScanBuffer + tc;
		}
	}

	return total;
}

__int64 InputFileMPEG::Tell() {
	return i64ScanCpos + (pScan - pScanBuffer);
}

//////////////////////

void InputFileMPEG::ReadStream(void *buffer, __int64 pos, long len, bool fAudio) {
	int pkt = 0;
	long delta;
	char *ptr = (char *)buffer;
	MPEGPacketInfo *packet_list = fAudio ? audio_packet_list : video_packet_list;
	int pkts = fAudio ? apackets : packets;

//	_RPT2(0,"Stream read request: pos %ld, len %ld\n", pos, len);

	if (!fInterleaved) {

		if (pFastRead)
			pFastRead->Read(0, pos, buffer, len);
		else {
			if (_lseeki64(fh, pos, SEEK_SET)<0) throw MyError("seek error");
			if (len != _read(fh, buffer, len)) throw MyError("read error");
		}
		return;
	}

	// find the packet containing the data start using a binary search

	do {
		int l = 0;
		int r = pkts-1;

		// Check current packet

		pkt = last_packet[fAudio?1:0];

		if (pkt>=0 && pkt<pkts) {
			if (pos < packet_list[pkt].stream_pos)
				r = pkt-1;
			else if (pos < packet_list[pkt+1].stream_pos)
				break;
			else if (pkt+1 < pkts && pos < packet_list[pkt+2].stream_pos) {
				++pkt;
				break;
			} else
				l = pkt+2;
		}

		for(;;) {
			if (l > r) {
				pkt = l;
				break;
			}

			pkt = (l+r)/2;

			if (pos < packet_list[pkt].stream_pos)
				r = pkt-1;
			else if (pos >= packet_list[pkt+1].stream_pos)
				l = pkt+1;
			else
				break;
		}

		if (pkt<0 || pkt >= pkts) throw MyError("MPEG Internal error: Invalid stream read position (%ld)", pos);

	} while(false);

	delta = (long)(pos - packet_list[pkt].stream_pos);

	// read data from packets

	while(len) {
		if (pkt >= pkts) throw MyError("Attempt to read past end of stream (pos %ld)", pos);

		long tc = (long)((packet_list[pkt+1].stream_pos - packet_list[pkt].stream_pos) - delta);

		if (tc>len) tc=len;

//		_RPT3(0,"Reading %ld bytes at %08lx+%ld\n", tc, video_packet_list[pkt].file_pos,delta);

		if (pFastRead) {
			pFastRead->Read(fAudio ? 1 : 0, packet_list[pkt].file_pos + delta, ptr, tc);
		} else {
			if (_lseeki64(fh, packet_list[pkt].file_pos + delta, SEEK_SET)<0)
				throw MyError("seek error");

			if (tc!=_read(fh, ptr, tc)) throw MyError("read error");
		}

		len -= tc;
		ptr += tc;
		++pkt;
		delta = 0;
	}

	last_packet[fAudio?1:0] = pkt-1;
}

BOOL CALLBACK InputFileMPEG::ParseDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	InputFileMPEG *thisPtr = (InputFileMPEG *)GetWindowLong(hDlg, DWL_USER);

	switch(uMsg) {
	case WM_INITDIALOG:
		SetWindowLong(hDlg, DWL_USER, lParam);
		thisPtr = (InputFileMPEG *)lParam;

		SendMessage(hDlg, WM_SETTEXT, 0, (LPARAM)"MPEG Import Filter");
		SetDlgItemText(hDlg, IDC_STATIC_MESSAGE,
			thisPtr->fIsVCD
				? "Parsing VideoCD stream"
				: thisPtr->fInterleaved ? "Parsing interleaved MPEG file" : "Parsing MPEG video file");
		SendMessage(GetDlgItem(hDlg, IDC_PROGRESS), PBM_SETRANGE, 0, MAKELPARAM(0, 16384));
		SetTimer(hDlg, 1, 250, (TIMERPROC)NULL);
		return TRUE;

	case WM_TIMER:
		{
			char buf[128];
		
			SendMessage(GetDlgItem(hDlg, IDC_PROGRESS), PBM_SETPOS, (WPARAM)((thisPtr->file_cpos*16384) / thisPtr->file_len), 0);

			if (thisPtr->fIsVCD)
				wsprintf(buf, "%ld of %ld sectors", (long)(thisPtr->file_cpos/2352), (long)(thisPtr->file_len/2352));
			else
				wsprintf(buf, "%ldK of %ldK", (long)(thisPtr->file_cpos>>10), (long)((thisPtr->file_len+1023)>>10));

			SendDlgItemMessage(hDlg, IDC_CURRENT_VALUE, WM_SETTEXT, 0, (LPARAM)buf);
		}
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDCANCEL)
			thisPtr->fAbort = true;
		return TRUE;
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////

void InputFileMPEG::setOptions(InputFileOptions *_ifo) {
	InputFileMPEGOptions *ifo = (InputFileMPEGOptions *)_ifo;

	iDecodeMode = ifo->opts.iDecodeMode;
	fAcceptPartial = ifo->opts.fAcceptPartial;

	if (iDecodeMode & InputFileMPEGOptions::DECODE_NO_P)
		iDecodeMode |= InputFileMPEGOptions::DECODE_NO_B;
}

InputFileOptions *InputFileMPEG::createOptions(const char *buf) {
	InputFileMPEGOptions *ifo = new InputFileMPEGOptions();

	if (!ifo) throw MyMemoryError();

	if (!ifo->read(buf)) {
		delete ifo;
		return NULL;
	}

	return ifo;
}

InputFileOptions *InputFileMPEG::promptForOptions(HWND hwnd) {
	InputFileMPEGOptions *ifo = new InputFileMPEGOptions();

	if (!ifo) throw MyMemoryError();

	DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_EXTOPENOPTS_MPEG), hwnd, InputFileMPEGOptions::SetupDlgProc, (LPARAM)ifo);

	return ifo;
}

///////////////////////////////////////////////////////////////////////////

typedef struct MyFileInfo {
	InputFileMPEG *thisPtr;

	volatile HWND hWndAbort;
	UINT statTimer;

	long lFrames;
	long lTotalSize;
	long lFrameCnt[3];
	long lFrameMinSize[3];
	long lFrameMaxSize[3];
	long lFrameTotalSize[3];
	long lAudioSize;
	long lAudioAvgBitrate;
	const char *lpszAudioMode;

} MyFileInfo;

void InputFileMPEG::_InfoDlgThread(void *pvInfo) {
	MyFileInfo *pInfo = (MyFileInfo *)pvInfo;
	InputFileMPEG *thisPtr = pInfo->thisPtr;
	long i;
	VideoSourceMPEG *vSrc = (VideoSourceMPEG *)pInfo->thisPtr->videoSrc;
	AudioSourceMPEG *aSrc = (AudioSourceMPEG *)pInfo->thisPtr->audioSrc;
	MPEGSampleInfo *msi;

	for(i=0; i<3; i++)
		pInfo->lFrameMinSize[i] = 0x7FFFFFFF;

	msi = thisPtr->video_sample_list;
	for(i = vSrc->lSampleFirst; i < vSrc->lSampleLast; ++i) {
		int iFrameType = msi->frame_type;

		if (iFrameType) {
			long lSize = msi->size;
			--iFrameType;

			++pInfo->lFrameCnt[iFrameType];
			pInfo->lFrameTotalSize[iFrameType] += lSize;

			if (lSize < pInfo->lFrameMinSize[iFrameType])
				pInfo->lFrameMinSize[iFrameType] = lSize;

			if (lSize > pInfo->lFrameMaxSize[iFrameType])
				pInfo->lFrameMaxSize[iFrameType] = lSize;

			pInfo->lTotalSize += lSize;

		}
		++pInfo->lFrames;

		++msi;
		if (pInfo->hWndAbort) {
			SendMessage(pInfo->hWndAbort, WM_USER+256, 0, 0);
			return;
		}
	}

	///////////////////////////////////////////////////////////////////////

	if (aSrc) {
		static const char *szModes[4]={ "stereo", "joint stereo", "dual channel", "mono" };
		bool fAudioMixedMode = false;
		bool fAudioMono = false;
		long lTotalBitrate = 0;

		msi = thisPtr->audio_sample_list;

		for(i = 0; i < thisPtr->aframes; ++i) {
			long fAudioHeader = msi->header;

			if ((thisPtr->audio_first_header ^ fAudioHeader) & MPEGAHDR_MODE_MASK)
				fAudioMixedMode = true;

			// mode==3 is mono, all others are stereo

			if (!(~thisPtr->audio_first_header & MPEGAHDR_MODE_MASK))
				fAudioMono = true;

			lTotalBitrate += bitrate[3-((fAudioHeader>>9)&3)][(fAudioHeader>>20)&15];

			pInfo->lAudioSize += msi->size;

			++msi;
			if (pInfo->hWndAbort) {
				SendMessage(pInfo->hWndAbort, WM_USER+256, 0, 0);
				return;
			}
		}

		pInfo->lAudioAvgBitrate = lTotalBitrate / thisPtr->aframes;

		if (fAudioMixedMode) {
			if (fAudioMono)
				pInfo->lpszAudioMode = "mixed mode";
			else
				pInfo->lpszAudioMode = "mixed stereo";
		} else
			pInfo->lpszAudioMode = szModes[(thisPtr->audio_first_header>>30) & 3];
	}

	pInfo->hWndAbort = (HWND)1;
}

BOOL APIENTRY InputFileMPEG::_InfoDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam) {
	MyFileInfo *pInfo = (MyFileInfo *)GetWindowLong(hDlg, DWL_USER);
	InputFileMPEG *thisPtr;

	if (pInfo)
		thisPtr = pInfo->thisPtr;

    switch (message)
    {
        case WM_INITDIALOG:
			SetWindowLong(hDlg, DWL_USER, lParam);
			pInfo = (MyFileInfo *)lParam;
			thisPtr = pInfo->thisPtr;

			if (thisPtr->videoSrc) {
				char *s;

				sprintf(g_msgBuf, "%dx%d, %.3f fps (%ld µs)",
							thisPtr->width,
							thisPtr->height,
							(float)thisPtr->videoSrc->streamInfo.dwRate / thisPtr->videoSrc->streamInfo.dwScale,
							MulDiv(thisPtr->videoSrc->streamInfo.dwScale, 1000000L, thisPtr->videoSrc->streamInfo.dwRate));
				SetDlgItemText(hDlg, IDC_VIDEO_FORMAT, g_msgBuf);

				s = g_msgBuf + sprintf(g_msgBuf, "%ld (", thisPtr->videoSrc->streamInfo.dwLength);
				ticks_to_str(s, MulDiv(1000L*thisPtr->videoSrc->streamInfo.dwLength, thisPtr->videoSrc->streamInfo.dwScale, thisPtr->videoSrc->streamInfo.dwRate));
				strcat(s,")");
				SetDlgItemText(hDlg, IDC_VIDEO_NUMFRAMES, g_msgBuf);
			}
			if (thisPtr->audioSrc) {
				WAVEFORMATEX *fmt = thisPtr->audioSrc->getWaveFormat();

				sprintf(g_msgBuf, "%ldHz, %s", fmt->nSamplesPerSec, fmt->nChannels>1 ? "Stereo" : "Mono");
				SetDlgItemText(hDlg, IDC_AUDIO_FORMAT, g_msgBuf);

				sprintf(g_msgBuf, "%ld", thisPtr->aframes);
				SetDlgItemText(hDlg, IDC_AUDIO_NUMFRAMES, g_msgBuf);
			}

			_beginthread(_InfoDlgThread, 10000, pInfo);

			pInfo->statTimer = SetTimer(hDlg, 1, 250, NULL);

            return (TRUE);

        case WM_COMMAND:                      
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
            {
				if (pInfo->hWndAbort == (HWND)1)
					EndDialog(hDlg, TRUE);  
                return TRUE;
            }
            break;

		case WM_DESTROY:
			if (pInfo->statTimer) KillTimer(hDlg, pInfo->statTimer);
			break;

		case WM_TIMER:
			_RPT0(0,"timer hit\n");
			sprintf(g_msgBuf, "%ld", pInfo->lFrames);
			SetDlgItemText(hDlg, IDC_VIDEO_NUMKEYFRAMES, g_msgBuf);

			sprintf(g_msgBuf, "%ld / %ld / %ld", pInfo->lFrameCnt[0], pInfo->lFrameCnt[1], pInfo->lFrameCnt[2]);
			SetDlgItemText(hDlg, IDC_VIDEO_FRAMETYPECNT, g_msgBuf);

			{
				int i;

				UINT uiCtlIds[]={ IDC_VIDEO_IFRAMES, IDC_VIDEO_PFRAMES, IDC_VIDEO_BFRAMES };

				for(i=0; i<3; i++) {

					if (pInfo->lFrameCnt[i])
						sprintf(g_msgBuf, "%ld / %ld / %ld (%ldK)"
									,pInfo->lFrameMinSize[i]
									,pInfo->lFrameTotalSize[i]/pInfo->lFrameCnt[i]
									,pInfo->lFrameMaxSize[i]
									,(pInfo->lFrameTotalSize[i]+1023)>>10);
					else
						sprintf(g_msgBuf,"(no %c-frames)", "IPB"[i]);

					SetDlgItemText(hDlg, uiCtlIds[i], g_msgBuf);
				}

			}

			if (pInfo->lTotalSize) {
				long lBytesPerSec;

				// bits * (frames/sec) / frames = bits/sec

				lBytesPerSec = (long)((pInfo->lTotalSize * ((double)thisPtr->videoSrc->streamInfo.dwRate / thisPtr->videoSrc->streamInfo.dwScale)) / pInfo->lFrames + 0.5);

				sprintf(g_msgBuf, "%ld Kbps (%ldKB/s)", (lBytesPerSec+124)/125, (lBytesPerSec+1023)/1024);
				SetDlgItemText(hDlg, IDC_VIDEO_AVGBITRATE, g_msgBuf);
			}

			if (pInfo->lpszAudioMode && thisPtr->audioSrc) {
				static const char *szLayers[]={"I","II","III"};
				WAVEFORMATEX *fmt = thisPtr->audioSrc->getWaveFormat();

				sprintf(g_msgBuf, "%ldKHz %s, %ldKbps layer %s", fmt->nSamplesPerSec/1000, pInfo->lpszAudioMode, pInfo->lAudioAvgBitrate, szLayers[3-((thisPtr->audio_first_header>>9)&3)]);
				SetDlgItemText(hDlg, IDC_AUDIO_FORMAT, g_msgBuf);

				sprintf(g_msgBuf, "%ldK", (pInfo->lAudioSize + 1023) / 1024);
				SetDlgItemText(hDlg, IDC_AUDIO_SIZE, g_msgBuf);
			}

			/////////

			if (pInfo->hWndAbort) {
				KillTimer(hDlg, pInfo->statTimer);
				return TRUE;
			}

			break;

		case WM_USER+256:
			EndDialog(hDlg, TRUE);  
			break;
    }
    return FALSE;
}

void InputFileMPEG::InfoDialog(HWND hwndParent) {
	MyFileInfo mai;

	memset(&mai, 0, sizeof mai);
	mai.thisPtr = this;

	DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_MPEG_INFO), hwndParent, _InfoDlgProc, (LPARAM)&mai);
}

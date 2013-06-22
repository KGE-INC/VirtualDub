#include <stdio.h>

#include <vd2/system/cpuaccel.h>
#include <vd2/Priss/decoder.h>
#include "AVIAudioOutput.h"

class FileInputStream : public IAMPBitsource {
private:
	FILE *f;

public:
	FileInputStream(FILE *f) {
		this->f = f;
	}

	int read(void *buffer, int bytes) {
		return fread(buffer, 1, bytes, f);
	}
};

short buffer[16384];

int main(int argc, char **argv) {
	IAMPDecoder *iad;
	FILE *f;
	AVIAudioOutput aao(32768, 4);

//	if (!(f=fopen(argc>1 ? argv[1] : "f:/inuyasha.mp3","rb"))) {
//	if (!(f=fopen(argc>1 ? argv[1] : "f:/tokimeki-js.mpa","rb"))) {
//	if (!(f=fopen(argc>1 ? argv[1] : "f:/tokimek2.mp2","rb"))) {
	if (!(f=fopen(argc>1 ? argv[1] : "f:/sine.mp3","rb"))) {
//	if (!(f=fopen(argc>1 ? argv[1] : "f:/mp3/l3test/layer2/layer2/fl16.mp2","rb"))) {
//	if (!(f=fopen(argc>1 ? argv[1] : "f:/mp3/l3test/layer3/he_48khz.bit","rb"))) {
//	if (!(f=fopen(argc>1 ? argv[1] : "s:\\nstore\\mp3_2\\daijo-bu.mp3","rb"))) {
//	if (!(f=fopen(argc>1 ? argv[1] : "s:\\trust.mp3","rb"))) {
//	if (!(f=fopen(argc>1 ? argv[1] : "s:\\trust-mpeg2.mp3","rb"))) {
//	if (!(f=fopen(argc>1 ? argv[1] : "s:\\trust-mpeg25.mp3","rb"))) {
//	if (!(f=fopen(argc>1 ? argv[1] : "s:\\trust-11.mp2","rb"))) {
//	if (!(f=fopen(argc>1 ? argv[1] : "s:\\trust-12.mp2","rb"))) {
//	if (!(f=fopen(argc>1 ? argv[1] : "s:\\nstore\\mp3\\amgova01.mp3","rb"))) {
//	if (!(f=fopen(argc>1 ? argv[1] : "s:\\nstore\\mp3\\hold_up.mp3","rb"))) {
//	if (!(f=fopen(argc>1 ? argv[1] : "s:\\nstore\\mp3\\fy-04-15.mp3","rb"))) {
//	if (!(f=fopen(argc>1 ? argv[1] : "s:\\nstore\\mp3\\(nuku)-Watasi_Ni_Happy_Birthday.mp3","rb"))) {
//	if (!(f=fopen(argc>1 ? argv[1] : "s:\\nstore\\mp3\\yururenai_negai.mp3","rb"))) {
//	if (!(f=fopen(argc>1 ? argv[1] : "s:/inuyasha.wav","rb"))) {
		puts("Can't open mp3 file");
		return 5;
	}

	FILE *fo = NULL;//fopen("s:/inuyasha.pcm", "wb");

	FileInputStream fis(f);

	CPUEnableExtensions(CPUCheckForExtensions());

	if (!(iad = CreateAMPDecoder())) {
		puts("Can't create Amp decoder");
		return 5;
	}

	printf("decoder: %s\n", iad->GetAmpVersionString());

	try {
		AMPStreamInfo asi;
		int frames=0;
		WAVEFORMATEX wfex;

		iad->Init();
		iad->Reset();
		iad->setSource(&fis);


		iad->ReadHeader();

		iad->getStreamInfo(&asi);

		printf("MPEG-%d layer %.*s stream, %ldKbits/sec, %ldKHz, %s\n",
				asi.nMPEGVer,
				asi.nLayer,	"III",
				asi.lBitrate,
				(asi.lSamplingFreq+500)/1000,
				asi.fStereo ? "stereo" : "mono");

		wfex.wFormatTag		= WAVE_FORMAT_PCM;
		wfex.nChannels		= asi.fStereo ? 2 : 1;
		wfex.nSamplesPerSec	= asi.lSamplingFreq;
		wfex.wBitsPerSample	= 16;
		wfex.nBlockAlign	= wfex.nChannels*2;
		wfex.nAvgBytesPerSec= wfex.nSamplesPerSec * wfex.nBlockAlign;
		wfex.cbSize			= 0;

		aao.init(&wfex);
		aao.start();

		while(!feof(f)) {
			long lSamples;
			__int64 time_st, time_ed;
			static __int64 totaltime = 0;

			iad->setDestination(buffer);

			__asm rdtsc
			__asm mov dword ptr time_st+0, eax
			__asm mov dword ptr time_st+4, edx
			iad->DecodeFrame();
			__asm rdtsc
			__asm mov dword ptr time_ed+0, eax
			__asm mov dword ptr time_ed+4, edx

			totaltime += (time_ed - time_st);

			if (lSamples = iad->getSampleCount()) {
				aao.write(buffer, lSamples * sizeof(short), INFINITE);

				if (fo)
					fwrite(buffer, lSamples*sizeof(short), 1, fo);

				if (!frames) {
					aao.start();
					aao.flush();
				}
			}

			if (!(++frames & 15)) {
				printf("\rDecoded %ld frames (%ld samples, %ld us per frame)", frames, lSamples, (long)(totaltime/(frames * 450i64)));fflush(stdout);
			}

			iad->ReadHeader();
		}
	} catch(int i) {
		printf("\nCaught AMPDecoder exception: %s.\n", iad->getErrorString(i));
	}

	aao.finalize(INFINITE);

	fclose(f);

	printf("All done.\n");
	return 0;
}

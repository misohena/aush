#ifndef SMBPITCHSHIFT_H
#define SMBPITCHSHIFT_H

// http://www.dspdimension.com/admin/pitch-shifting-using-the-ft/

/****************************************************************************
*
* NAME: smbPitchShift.cpp
* VERSION: 1.2
* HOME URL: http://www.dspdimension.com
* KNOWN BUGS: none
*
* SYNOPSIS: Routine for doing pitch shifting while maintaining
* duration using the Short Time Fourier Transform.
*
* DESCRIPTION: The routine takes a pitchShift factor value which is between 0.5
* (one octave down) and 2. (one octave up). A value of exactly 1 does not change
* the pitch. numSampsToProcess tells the routine how many samples in indata[0...
* numSampsToProcess-1] should be pitch shifted and moved to outdata[0 ...
* numSampsToProcess-1]. The two buffers can be identical (ie. it can process the
* data in-place). fftFrameSize defines the FFT frame size used for the
* processing. Typical values are 1024, 2048 and 4096. It may be any value <=
* MAX_FRAME_LENGTH but it MUST be a power of 2. osamp is the STFT
* oversampling factor which also determines the overlap between adjacent STFT
* frames. It should at least be 4 for moderate scaling ratios. A value of 32 is
* recommended for best quality. sampleRate takes the sample rate for the signal 
* in unit Hz, ie. 44100 for 44.1 kHz audio. The data passed to the routine in 
* indata[] should be in the range [-1.0, 1.0), which is also the output range 
* for the data, make sure you scale the data accordingly (for 16bit signed integers
* you would have to divide (and multiply) by 32768). 
*
* COPYRIGHT 1999-2009 Stephan M. Bernsee <smb [AT] dspdimension [DOT] com>
*
* 						The Wide Open License (WOL)
*
* Permission to use, copy, modify, distribute and sell this software and its
* documentation for any purpose is hereby granted without fee, provided that
* the above copyright notice and this license appear in all source copies. 
* THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY OF
* ANY KIND. See http://www.dspguru.com/wol.htm for more information.
*
*****************************************************************************/ 

#include <cstring>
#include <cmath>
#include <vector>

#define SMB_PITCH_SHIFT_USE_OOURA_RDFT 1 // use rdft() direct.
#define SMB_PITCH_SHIFT_USE_OOURA_CDFT 0 // use cdft as smbFft()
#define SMB_PITCH_SHIFT_USE_OOURA_FFT (SMB_PITCH_SHIFT_USE_OOURA_RDFT || SMB_PITCH_SHIFT_USE_OOURA_CDFT)

#if SMB_PITCH_SHIFT_USE_OOURA_FFT
#include "fftsg.h"
#endif

namespace aush{

struct SmbPitchShift
{
	unsigned int fftFrameSize;
	std::vector<float> gInFIFO;//[fftFrameSize];
	std::vector<float> gOutFIFO;//[fftFrameSize];
	std::vector<float> gFFTworksp;//[2*fftFrameSize] or [(fftFrameSize/2+1)*2];
	std::vector<float> gLastPhase;//[fftFrameSize/2+1];
	std::vector<float> gSumPhase;//[fftFrameSize/2+1];
	std::vector<float> gOutputAccum;//[2*fftFrameSize];
	std::vector<float> gAnaFreq;//[fftFrameSize];
	std::vector<float> gAnaMagn;//[fftFrameSize];
	std::vector<float> gSynFreq;//[fftFrameSize];
	std::vector<float> gSynMagn;//[fftFrameSize];
	unsigned int gRover;
#if SMB_PITCH_SHIFT_USE_OOURA_FFT
	std::vector<int> fftTemp;
	std::vector<float> fftSinCos;
#endif
	std::vector<float> gWindow;//[fftFrameLength]

	explicit SmbPitchShift(unsigned int fftFrameSize)
			: fftFrameSize(fftFrameSize)
			, gInFIFO(fftFrameSize)
			, gOutFIFO(fftFrameSize)
#if SMB_PITCH_SHIFT_USE_OOURA_RDFT
			, gFFTworksp((fftFrameSize/2+1)*2)
#else
			, gFFTworksp(2*fftFrameSize)
#endif
			, gLastPhase(fftFrameSize/2+1)
			, gSumPhase(fftFrameSize/2+1)
			, gOutputAccum(2*fftFrameSize)
			, gAnaFreq(fftFrameSize)
			, gAnaMagn(fftFrameSize)
			, gSynFreq(fftFrameSize)
			, gSynMagn(fftFrameSize)
			, gRover(0)
#if SMB_PITCH_SHIFT_USE_OOURA_FFT
			, fftTemp(std::size_t(2+std::sqrt(double(fftFrameSize))+1))
			, fftSinCos(fftFrameSize/2)
#endif
	{
#if SMB_PITCH_SHIFT_USE_OOURA_FFT
		fftTemp[0] = 0;
#endif
		gWindow.reserve(fftFrameSize);
		static const double M_PI = 3.14159265358979323846;
		for (unsigned int k = 0; k < fftFrameSize; k++) {
			//gWindow[k] =
			gWindow.push_back(float( -.5*cos(2.*M_PI*(double)k/(double)fftFrameSize)+.5 ));
		}
	}

	/*
	Routine smbPitchShift(). See top of file for explanation
	Purpose: doing pitch shifting while maintaining duration using the Short
	Time Fourier Transform.
	Author: (c)1999-2009 Stephan M. Bernsee <smb [AT] dspdimension [DOT] com>
	*/
	void smbPitchShift(float pitchShift,
					   unsigned int numSampsToProcess,
					   //long fftFrameSize,
					   unsigned int osamp,
					   float sampleRate,
					   float *indata,
					   float *outdata)
	{
		static const double M_PI = 3.14159265358979323846;
		double magn, phase, tmp, window, real, imag;

		/* set up some handy variables */
		const unsigned int fftFrameSize2 = fftFrameSize/2;
		const unsigned int stepSize = fftFrameSize/osamp;
		const double freqPerBin = sampleRate/(double)fftFrameSize;
		const double expct = 2.*M_PI*(double)stepSize/(double)fftFrameSize;
		const unsigned int inFifoLatency = fftFrameSize-stepSize;

		if (gRover == 0) gRover = inFifoLatency;

		/* main processing loop */
		for (unsigned int i = 0; i < numSampsToProcess; i++){

			/* As long as we have not yet collected enough data just read in */
			gInFIFO[gRover] = indata[i];
			outdata[i] = gOutFIFO[gRover-inFifoLatency];
			gRover++;

			/* now we have enough data for processing */
			if (gRover >= fftFrameSize) {
				gRover = inFifoLatency;

				/* do windowing and re,im interleave */
				for (unsigned int k = 0; k < fftFrameSize;k++) {
					//window = -.5*cos(2.*M_PI*(double)k/(double)fftFrameSize)+.5;
					window = gWindow[k];
#if SMB_PITCH_SHIFT_USE_OOURA_RDFT
					gFFTworksp[k] = static_cast<float>(gInFIFO[k] * window);
#else
					gFFTworksp[2*k] = static_cast<float>(gInFIFO[k] * window);
					gFFTworksp[2*k+1] = 0.;
#endif
				}


				/* ***************** ANALYSIS ******************* */
				/* do transform */
#if SMB_PITCH_SHIFT_USE_OOURA_RDFT
				fftsg::rdft(fftFrameSize, 1, &gFFTworksp[0], &fftTemp[0], &fftSinCos[0]);
				gFFTworksp[2*fftFrameSize2] = gFFTworksp[1]; //Real[fftFrameSize/2]
				gFFTworksp[2*fftFrameSize2+1] = 0; //Imag[fftFrameSize/2] is always 0.
				gFFTworksp[1] = 0; //Imag[0] is always 0.

#else
				smbFft(&gFFTworksp[0], fftFrameSize, -1);
#endif

				/* this is the analysis step */
				for (unsigned int k = 0; k <= fftFrameSize2; k++) {

					/* de-interlace FFT buffer */
#if SMB_PITCH_SHIFT_USE_OOURA_RDFT
					real = gFFTworksp[2*k];
					imag = -gFFTworksp[2*k+1];
#else
					real = gFFTworksp[2*k];
					imag = gFFTworksp[2*k+1];
#endif

					/* compute magnitude and phase */
					magn = 2.*sqrt(real*real + imag*imag);
					phase = atan2(imag,real);

					/* compute phase difference */
					tmp = phase - gLastPhase[k];
					gLastPhase[k] = static_cast<float>(phase);

					/* subtract expected phase difference */
					tmp -= (double)k*expct;

					/* map delta phase into +/- Pi interval */
					long qpd = static_cast<long>(tmp/M_PI);
					if (qpd >= 0) qpd += qpd&1;
					else qpd -= qpd&1;
					tmp -= M_PI*(double)qpd;

					/* get deviation from bin frequency from the +/- Pi interval */
					tmp = osamp*tmp/(2.*M_PI);

					/* compute the k-th partials' true frequency */
					tmp = (double)k*freqPerBin + tmp*freqPerBin;

					/* store magnitude and true frequency in analysis arrays */
					gAnaMagn[k] = static_cast<float>(magn);
					gAnaFreq[k] = static_cast<float>(tmp);

				}

				/* ***************** PROCESSING ******************* */
				/* this does the actual pitch shifting */
				memset(&gSynMagn[0], 0, fftFrameSize*sizeof(float));
				memset(&gSynFreq[0], 0, fftFrameSize*sizeof(float));
				for (unsigned int k = 0; k <= fftFrameSize2; k++) { 
					const unsigned int index = static_cast<unsigned int>(k * pitchShift); //float to long
					if (index <= fftFrameSize2) { 
						gSynMagn[index] += gAnaMagn[k]; 
						gSynFreq[index] = gAnaFreq[k] * pitchShift; 
					} 
				}
			
				/* ***************** SYNTHESIS ******************* */
				/* this is the synthesis step */
				for (unsigned int k = 0; k <= fftFrameSize2; k++) {

					/* get magnitude and true frequency from synthesis arrays */
					magn = gSynMagn[k];
					tmp = gSynFreq[k];

					/* subtract bin mid frequency */
					tmp -= (double)k*freqPerBin;

					/* get bin deviation from freq deviation */
					tmp /= freqPerBin;

					/* take osamp into account */
					tmp = 2.*M_PI*tmp/osamp;

					/* add the overlap phase advance back in */
					tmp += (double)k*expct;

					/* accumulate delta phase to get bin phase */
					gSumPhase[k] += static_cast<float>(tmp);
					phase = gSumPhase[k];

					/* get real and imag part and re-interleave */
#if SMB_PITCH_SHIFT_USE_OOURA_RDFT
					gFFTworksp[2*k] = static_cast<float>(magn * cos(phase));
					gFFTworksp[2*k+1] = -static_cast<float>(magn * sin(phase));
#else
					gFFTworksp[2*k] = static_cast<float>(magn * cos(phase));
					gFFTworksp[2*k+1] = static_cast<float>(magn * sin(phase));
#endif
				} 

#if SMB_PITCH_SHIFT_USE_OOURA_RDFT
				gFFTworksp[1] = gFFTworksp[fftFrameSize2]; //R[fftFrameSize/2]
				/* do inverse transform */
				fftsg::rdft(fftFrameSize, -1, &gFFTworksp[0], &fftTemp[0], &fftSinCos[0]);
#else
				/* zero negative frequencies */
				for (unsigned int k = fftFrameSize+2; k < 2*fftFrameSize; k++) gFFTworksp[k] = 0.;
				/* do inverse transform */
				smbFft(&gFFTworksp[0], fftFrameSize, 1);
#endif

				/* do windowing and add to output accumulator */ 
				for(unsigned int k=0; k < fftFrameSize; k++) {
					//window = -.5*cos(2.*M_PI*(double)k/(double)fftFrameSize)+.5;
					window = gWindow[k];
#if SMB_PITCH_SHIFT_USE_OOURA_RDFT
					const float rm = gFFTworksp[k];
#else
					const float rm = gFFTworksp[2*k];
#endif
					gOutputAccum[k] += static_cast<float>( 2.*window*rm/(fftFrameSize2*osamp) );
				}
				for (unsigned int k = 0; k < stepSize; k++) gOutFIFO[k] = gOutputAccum[k];

				/* shift accumulator */
				std::memmove(&gOutputAccum[0], &gOutputAccum[0]+stepSize, fftFrameSize*sizeof(float));

				/* move input FIFO */
				for (unsigned int k = 0; k < inFifoLatency; k++) gInFIFO[k] = gInFIFO[k+stepSize];
			}
		}
	}

	/*
	12/12/02, smb

	PLEASE NOTE:

	There have been some reports on domain errors when the atan2() function was used
	as in the above code. Usually, a domain error should not interrupt the program flow
	(maybe except in Debug mode) but rather be handled "silently" and a global variable
	should be set according to this error. However, on some occasions people ran into
	this kind of scenario, so a replacement atan2() function is provided here.

	If you are experiencing domain errors and your program stops, simply replace all
	instances of atan2() with calls to the smbAtan2() function below.
	*/
	double smbAtan2(double x, double y)
	{
		static const double M_PI = 3.14159265358979323846;
		double signx;
		if (x > 0.) signx = 1.;  
		else signx = -1.;

		if (x == 0.) return 0.;
		if (y == 0.) return signx * M_PI / 2.;

		return atan2(x, y);
	}

#if SMB_PITCH_SHIFT_USE_OOURA_CDFT

	void smbFft(float *fftBuffer, long fftFrameSize, long sign)
	{
		fftsg::cdft<float>(2*fftFrameSize, sign, fftBuffer, &fftTemp[0], &fftSinCos[0]);
	}
#endif

#if !SMB_PITCH_SHIFT_USE_OOURA_FFT
	/* 
	FFT routine, (C)1996 S.M.Bernsee. Sign = -1 is FFT, 1 is iFFT (inverse)
	Fills fftBuffer[0...2*fftFrameSize-1] with the Fourier transform of the
	time domain data in fftBuffer[0...2*fftFrameSize-1]. The FFT array takes
	and returns the cosine and sine parts in an interleaved manner, ie.
	fftBuffer[0] = cosPart[0], fftBuffer[1] = sinPart[0], asf. fftFrameSize
	must be a power of 2. It expects a complex input signal (see footnote 2),
	ie. when working with 'common' audio signals our input signal has to be
	passed as {in[0],0.,in[1],0.,in[2],0.,...} asf. In that case, the transform
	of the frequencies of interest is in fftBuffer[0...fftFrameSize].
	*/
	void smbFft(float *fftBuffer, long fftFrameSize, long sign)
	{
		static const double M_PI = 3.14159265358979323846;
		float wr, wi, arg, *p1, *p2, temp;
		float tr, ti, ur, ui, *p1r, *p1i, *p2r, *p2i;
		long i, bitm, j, le, le2, k;

		for (i = 2; i < 2*fftFrameSize-2; i += 2) {
			for (bitm = 2, j = 0; bitm < 2*fftFrameSize; bitm <<= 1) {
				if (i & bitm) j++;
				j <<= 1;
			}
			if (i < j) {
				p1 = fftBuffer+i; p2 = fftBuffer+j;
				temp = *p1; *(p1++) = *p2;
				*(p2++) = temp; temp = *p1;
				*p1 = *p2; *p2 = temp;
			}
		}
		const long N = (long)(log(double(fftFrameSize))/log(2.)+.5);
		for (k = 0, le = 2; k < N; k++) {
			le <<= 1;
			le2 = le>>1;
			ur = 1.0;
			ui = 0.0;
			arg = M_PI / (le2>>1);
			wr = cos(arg);
			wi = sign*sin(arg);
			for (j = 0; j < le2; j += 2) {
				p1r = fftBuffer+j; p1i = p1r+1;
				p2r = p1r+le2; p2i = p2r+1;
				for (i = j; i < 2*fftFrameSize; i += le) {
					tr = *p2r * ur - *p2i * ui;
					ti = *p2r * ui + *p2i * ur;
					*p2r = *p1r - tr; *p2i = *p1i - ti;
					*p1r += tr; *p1i += ti;
					p1r += le; p1i += le;
					p2r += le; p2i += le;
				}
				tr = ur*wr - ui*wi;
				ui = ur*wi + ui*wr;
				ur = tr;
			}
		}
	}
#endif
};

} //namespace aush
#endif

/**
 * @file
 * @brief SoundBuffer�C���^�t�F�[�X���`���邽�߂̃t�@�C���ł��B
 * @author AKIYAMA Kouhei
 * @since 2002-10-02
 */
#ifndef AUSH_DSOUND_SOUNDBUFFER_H_INCLUDED_20021002
#define AUSH_DSOUND_SOUNDBUFFER_H_INCLUDED_20021002

#include "WaveReader.h"

namespace aush{namespace dsound{
	class SoundBuffer
	{
	public:
		static const WaveSize UNKNOWN_SAMPLES = (WaveSize)-1;

		virtual ~SoundBuffer(){}
		virtual bool play(void) = 0;
		virtual bool playFromBeginning(void) = 0;
		virtual bool stop(void) = 0;
		virtual bool isPlaying(void) = 0;
		virtual bool setPositionSamples(WaveSize pos) = 0;
		virtual WaveSize getPositionSamples(void) = 0; //return position or UNKNOWN_SAMPLES
		virtual bool setVolume(int vol) = 0;
		virtual bool getVolume(int *vol) = 0;

		virtual bool setPan(int pan) = 0;
		virtual bool getPan(int *pan) = 0;
		virtual bool setPitch(int semitone1000) = 0; // �����g���ɑ΂���2^(semitone1000/12000)�{�B
		virtual bool getPitch(int *semitone1000) = 0;
	};

}}//namespace aush::dsound
#endif

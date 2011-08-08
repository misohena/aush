/**
 * @file
 * @brief PlainWavFileReader�N���X�̃C���^�t�F�[�X���L�q���邽�߂̃t�@�C���ł��B
 * @author AKIYAMA Kouhei
 * @since 2002-09-25
 */
#ifndef AUSH_DSOUND_PLAINWAVFILEREADER_H_INCLUDED_20020925
#define AUSH_DSOUND_PLAINWAVFILEREADER_H_INCLUDED_20020925

#include "../util/PathString.h"
#include "WaveReader.h"

namespace aush{namespace dsound{
	/**
	 * WAV�t�@�C����ǂݍ��ނ��߂̃N���X�ł��B
	 * ���k����(WAVE_FORMAT_PCM)�̃t�@�C�������ǂ߂܂���B
	 */
	class PlainWavFileReader
		: public WaveReader
	{
	private:
		HANDLE hFile_;
	
		WaveSize topPtr_;
		WaveSize totalSize_;
		WaveSize remainSize_;

		WaveFormat format_;
	
	public:
		PlainWavFileReader();
		virtual ~PlainWavFileReader();
		bool open(const PathChar *filename);
		virtual void close(void);
		virtual bool isOpen(void) const;
		virtual bool read(void *dst, std::size_t size, std::size_t *actualSize);
		virtual bool seek(WaveSize sample);
		virtual WaveSize tell(void) const;
		virtual bool isTerminated(void) const;
		virtual WaveFormat getFormat(void) const;
		virtual WaveSize getTotal(void) const;
	};

}}//namespace aush::dsound
#endif

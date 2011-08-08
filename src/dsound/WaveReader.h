/**
 * @file
 * @brief WaveReader�N���X���`���邽�߂̃t�@�C���ł��B
 * @author AKIYAMA Kouhei
 * @since 2002-09-25
 */
#ifndef AUSH_DSOUND_WAVEREADER_H_INCLUDED_20020925
#define AUSH_DSOUND_WAVEREADER_H_INCLUDED_20020925

#include <boost/cstdint.hpp>

namespace aush{namespace dsound{
	typedef boost::uint64_t WaveSize;

	struct WaveFormat
	{
		unsigned int samplesPerSec; // samples per second
		unsigned int channels; // 1 or 2
		unsigned int bytesPerSample; //1=8bit or 2=16bit
		unsigned int blockAlign; // channels * bytesPerSample
		WaveFormat()
				: samplesPerSec(0)
				, channels(0)
				, bytesPerSample(0)
				, blockAlign(0)
		{}
		WaveFormat(unsigned int samplesPerSec,
				   unsigned int channels,
				   unsigned int bytesPerSample,
				   unsigned int blockAlign)
				: samplesPerSec(samplesPerSec)
				, channels(channels)
				, bytesPerSample(bytesPerSample)
				, blockAlign(blockAlign)
		{}
	};

	/**
	 * WAVE�f�[�^��ǂݍ��ނ��߂̃C���^�t�F�[�X���K�肷��N���X�ł��B
	 */
	class WaveReader
	{
	public:
		/**
		 * �I�u�W�F�N�g����̂��܂��B�K�v�Ȃ�Close���Ăяo���܂��B
		 */
		virtual ~WaveReader(){}

		/**
		 * �S�Ẵ��\�[�X��������A���ɂ܂��I�[�v���ł����Ԃɂ��܂��B
		 */
		virtual void close(void) = 0;

		/**
		 * �I�[�v����Ԃ��ǂ�����Ԃ��܂��B
		 */
		virtual bool isOpen(void) const = 0;

		/**
		 * �w�肵���o�C�g������pcm�f�[�^��ǂݍ��݂܂��B
		 * �G���[���������邩�A�I�[�ɒB����܂�size�Ŏw�肵���o�C�g�������ǂݍ��߂܂��B
		 */
		virtual bool read(void *dst, std::size_t size, std::size_t *actualSize) = 0;

		/**
		 * ����Read���Ă񂾂Ƃ��Ɏ��o����pcm�f�[�^�̈ʒu���T���v�����Ŏw�肵�܂��B
		 */
		virtual bool seek(WaveSize sample) = 0;
	
		/**
		 * ����Read���Ă񂾂Ƃ��Ɏ��o����pcm�f�[�^�̈ʒu���T���v�����ŕԂ��܂��B
		 */
		virtual WaveSize tell(void) const = 0;
	
		/**
		 * ���[�_�[���I�[�ɒB���Ă��邩�ǂ�����Ԃ��܂��B
		 * �܂�Seek���Ȃ����肱��ȏ�Read���Ă�ł��f�[�^���o�Ă��Ȃ��Ƃ�true��Ԃ��܂��B
		 */
		virtual bool isTerminated(void) const = 0;
	
	
		//���擾����
	

		/**
		 * Read�œǂݍ��߂�f�[�^�̌`����Ԃ��܂��B
		 */
		virtual WaveFormat getFormat(void) const = 0;
		
		/**
		 * �S�̂̃T���v������Ԃ��܂��B
		 */
		virtual WaveSize getTotal(void) const = 0;
	};

}}//namespace aush::dsound
#endif

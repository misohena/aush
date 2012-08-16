/**
 * @file
 * @brief StreamSoundBuffer�N���X�̃C���^�t�F�[�X���L�q���邽�߂̃t�@�C���ł��B
 * @author AKIYAMA Kouhei
 * @since 2002-09-26
 */
#ifndef AUSH_DSOUND_STREAMSOUNDBUFFER_H_INCLUDED_20020926
#define AUSH_DSOUND_STREAMSOUNDBUFFER_H_INCLUDED_20020926

#include "DirectSound.h"

#include <boost/shared_ptr.hpp>
#include "SoundBuffer.h"

#include "CriticalSection.h"

namespace aush{namespace dsound{class WaveReader;}}
namespace aush{namespace dsound{class ThreadPool;}}

namespace aush{namespace dsound{
	
	/**
	 * �X�g���[���Đ������̃T�E���h�o�b�t�@�N���X�ł��B
	 * ���̃N���X�͊��蓖�Ă�ꂽWaveReader�C���^�t�F�[�X�����I�u�W�F�N�g����
	 * PCM�f�[�^��������肾���Ȃ��牉�t���܂��B
	 */
	class StreamSoundBuffer
		: public SoundBuffer
	{
		//�萔
		static const int BUFFER_LENGTH_SECONDS = 3; //�X�g���[���o�b�t�@�S�̂̕b��
		static const int NUM_BLOCKS = 8; //�u���b�N��(�o�b�t�@�����������邩�̐�)

		//�����A�֘A
		CriticalSection cs_;

		//��create���\�b�h�ŃZ�b�g�����ϐ�
		boost::shared_ptr<WaveReader> source_;	// �f�[�^�\�[�X�B
		WaveFormat sourceFormat_; // ���t����wave�̃t�H�[�}�b�g�Bsource_->getFormat()�̒l�B
		WaveSize sourceSize_; // �S�f�[�^�o�C�g��(byte)
		CComPtr<IDirectSoundBuffer> streamBuffer_; // DirectSound�X�g���[���o�b�t�@
		DWORD bufferSize_;	// �o�b�t�@�̃o�C�g��
		DWORD blockSize_;	// �u���b�N�̃o�C�g��(�ʒm����Ԋu)
		int threadHandle_;	//���t�Ď��p�X���b�h�̃n���h��
		enum {
			EV_CLOSE,
			EV_PASS,
			EV_STOPPED, //��~���̎��V�O�i����ԁB�Đ����̎��A��V�O�i����ԁB
			EV_COUNT,
		};
		HANDLE threadControlEvents_[EV_COUNT];	//�ʒm�p�C�x���g�I�u�W�F�N�g�̃n���h��

		//�K�X�ω�����ϐ�
		int pitchBend_;  //���g�����炵

		//��play���\�b�h�ŃZ�b�g�����ϐ�
		WaveSize sourcePos_;	//�ǂݍ��݃I�t�Z�b�g(byte)�B��{�I��source_->tell()*bytesPerSample�Ɠ����͂��B

		struct LoopState
		{
			WaveSize loopIn_;     //���[�v�C���I�t�Z�b�g(byte)
			WaveSize loopOut_;    //���[�v�A�E�g�I�t�Z�b�g(byte)
			WaveSize loopCount_;  //�c�胋�[�v��(-1�̂Ƃ��͖������Ӗ�����)

			LoopState() : loopIn_(0), loopOut_(0), loopCount_(0) {}
			LoopState(WaveSize loopIn, WaveSize loopOut, WaveSize loopCount)
					: loopIn_(loopIn), loopOut_(loopOut), loopCount_(loopCount)
			{}
		};
		LoopState loopState_; //���[�v�ݒ�B

		unsigned int blockFrontIndex_; //�擪�u���b�N�̃C���f�b�N�X�B[0, NUM_BLOCKS)
		unsigned int blockCount_; //�f�[�^�������Ă���u���b�N�̐��B[0, NUM_BLOCKS]
		unsigned int numBlocksValid_; //�L���ȃu���b�N�̌��B[0, NUM_BLOCKS]

		struct BlockInfo
		{
			static const WaveSize INVALID_POS = (WaveSize)-1;
			WaveSize sourcePos_; //�u���b�N�擪�ɑΉ�����\�[�X�ʒu(�T���v����)�B�����ȏꍇsourceSize_�ȏ�̒l�B
			LoopState loopState_; //�u���b�N�f�[�^�𖄂߂��Ƃ��̃��[�v�ݒ�B�u���b�N���I�t�Z�b�g����\�[�X�ʒu�����߂邽�߂ɕK�v�B
			DWORD validBytes_; //�u���b�N�擪����̗L���ȃf�[�^�̃o�C�g���B

			BlockInfo()
				: sourcePos_(INVALID_POS), loopState_(), validBytes_(0) {}
			BlockInfo(WaveSize sourcePos, const LoopState &loopState, DWORD validBytes)
				: sourcePos_(sourcePos), loopState_(loopState), validBytes_(validBytes) {}

			bool isValidBlock() const { return validBytes_ > 0;}
			bool hasSourcePos() const { return sourcePos_ != INVALID_POS;}
		};
		BlockInfo blockInfo_[NUM_BLOCKS];



	public:
		StreamSoundBuffer();
		virtual ~StreamSoundBuffer();
		bool create(const CComPtr<IDirectSound> &pds, const boost::shared_ptr<WaveReader> &source);
		//SoundBuffer�C���^�t�F�[�X
		virtual bool play(void);
		virtual bool playFromBeginning(void);
		virtual bool stop(void);
		virtual bool isPlaying(void);
		virtual bool setPositionSamples(WaveSize pos);
		virtual WaveSize getPositionSamples(void); //UNKNOWN_SAMPLES�̂Ƃ��s��
		virtual bool setVolume(int vol);
		virtual bool getVolume(int *vol);
		virtual bool setPan(int pan);
		virtual bool getPan(int *pan);
		virtual bool setPitch(int semitone1000); // �����g���ɑ΂���2^(semitone1000/12000)�{�B
		virtual bool getPitch(int *semitone1000);

		void updateSourceSize(void);

		void waitForStop(void);
	private:
		WaveSize getDefaultLoopIn() const { return 0;}
		WaveSize getDefaultLoopOut() const { return source_ ? source_->getTotal() : 0;}
		int getDefaultLoopCount() const { return 0;}
		void setLoopDefault(void);

		bool seekPlayingPosition(WaveSize posSamples);
		bool clearBuffer(void);
		bool seekSource(WaveSize posSamples);

		static unsigned __stdcall handleNotificationsStatic(void *pThis);
		void handleNotifications(void);
		bool fillBuffer(void);
		void clearBlocks(void);
		unsigned int getNewBlockIndex(void);
		void pushBackBlock(const BlockInfo &info);
		void popBlocksBeforePlayCursor(DWORD currentPlayPos);
		void popFrontBlock(void);
		BlockInfo fillBlock(unsigned int blockIndex);
		DWORD readWave(BYTE *bufferPtr, DWORD bufferSize);

		static ThreadPool *getThreadPool(void);
	};

}}//namespace aush::dsound
#endif

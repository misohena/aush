/**
 * @file
 * @brief StreamSoundBuffer�N���X�̎������L�q���邽�߂̃t�@�C���ł��B
 * @author AKIYAMA Kouhei
 * @since 2002-09-26
 */

#include <iostream>
#include <windows.h>
#include <tchar.h>
#include <process.h> 
#include <cmath>
// aush
#include "WaveReader.h"
#include "ThreadPool.h"

#include "StreamSoundBuffer.h"

namespace aush{namespace dsound{


/**
 * �I�u�W�F�N�g���\�z���܂��B
 */
StreamSoundBuffer::StreamSoundBuffer()
	: source_()
	, sourceFormat_()
	, sourceSize_(0)

	, streamBuffer_()
	, bufferSize_(0)
	, blockSize_(0)
	//, threadControlEvents_({})
	, threadHandle_(ThreadPool::INVALID_THREAD_HANDLE)

	, pitchBend_(0)

	, sourcePos_(0)
	, loopState_()
	, blockFrontIndex_(0)
	, blockCount_(0)
	, numBlocksValid_(0)
	//, blockInfo_({})
{
	for(int i = 0; i < EV_COUNT; ++i){
		threadControlEvents_[i] = NULL;
	}
}

/**
 * �I�u�W�F�N�g����̂��܂��B
 */
StreamSoundBuffer::~StreamSoundBuffer()
{
	//���t���~�߁A�X���b�h���~�߂܂��B
	{
		CriticalSectionLock lock(&cs_);

		if(streamBuffer_){
			streamBuffer_->Stop();
		}
		if(threadControlEvents_[EV_CLOSE]){
			SetEvent(threadControlEvents_[EV_CLOSE]);
		}
	}

	//�X���b�h�����ʂ܂ő҂��A�X���b�h�n���h����������܂��B
	if(threadHandle_ != ThreadPool::INVALID_THREAD_HANDLE){
		getThreadPool()->waitAndClose(threadHandle_);
	}

	//�X���b�h�����񂾂̂ŁA���X�ƐF�X������܂��B
	streamBuffer_.Release();
	for(int i = 0; i < EV_COUNT; ++i){
		if(threadControlEvents_[i]){
			CloseHandle(threadControlEvents_[i]);
		}
	}
}


/**
 * �T�E���h�o�b�t�@���������܂��B
 */
bool StreamSoundBuffer::create(const CComPtr<IDirectSound> &pds, const boost::shared_ptr<WaveReader> &source)
{
	struct Local
	{
		static CComPtr<IDirectSoundBuffer> createDSB(IDirectSound *pds, DWORD bufferSize, WAVEFORMATEX &sourceFormat)
		{
			CComPtr<IDirectSoundBuffer> buffer;
			DSBUFFERDESC dsbd = {};
			dsbd.dwSize = sizeof(dsbd);
			dsbd.dwFlags
				= DSBCAPS_CTRLPOSITIONNOTIFY	//�Đ��ʒu�ʒm
				| DSBCAPS_CTRLVOLUME			//�t�F�[�h�����Ɏg�p
				| DSBCAPS_CTRLPAN
				| DSBCAPS_CTRLFREQUENCY
				| DSBCAPS_GLOBALFOCUS | DSBCAPS_LOCSOFTWARE;
			dsbd.dwBufferBytes = bufferSize;
			dsbd.lpwfxFormat = &sourceFormat;
		
			HRESULT hr = pds->CreateSoundBuffer(&dsbd, &buffer, NULL);
			if(FAILED(hr)){
				//Create()�� �X�g���[���p�̃T�E���h�o�b�t�@�̍쐬�Ɏ��s���܂����B
				return CComPtr<IDirectSoundBuffer>();
			}
			return buffer;
		}

		static bool setNotifyPositions(IDirectSoundBuffer *buffer, HANDLE ev, DWORD blockSize)
		{
			// IDirectSoundNotify�C���^�t�F�[�X���擾����B
			CComPtr<IDirectSoundNotify> notify;
			HRESULT hr = buffer->QueryInterface(IID_IDirectSoundNotify, (LPVOID*)&notify);
			if(FAILED(hr)){
				return false; //IDirectSoundNotify�C���^�t�F�[�X�̎擾�Ɏ��s���܂����B
			}

			// �Đ����C�x���g�����ʒu���Z�b�g����
			DSBPOSITIONNOTIFY notifyPos[NUM_BLOCKS];
			for(int i = 0; i < NUM_BLOCKS; i++){
				notifyPos[i].dwOffset = blockSize * (i+1);
				notifyPos[i].hEventNotify = ev;
			}
			notifyPos[NUM_BLOCKS-1].dwOffset--;

			// �o�b�t�@�ɒʒm�ʒu��ݒ肷��
			hr = notify->SetNotificationPositions(NUM_BLOCKS, notifyPos);
			if(FAILED(hr)){
				return false; //�ʒm�ʒu�̐ݒ�Ɏ��s���܂����B
			}
			return true;
		}
	};//struct Local

	if(!pds || !source){
		return false;
	}
	const WaveFormat sourceFormat = source->getFormat();
	
	// �o�b�t�@�T�C�Y���v�Z����B�u���b�N�T�C�Y(�����T�C�Y)���v�Z����B
	// �u���b�N�T�C�Y��1�T���v���̃o�C�g���̔{���łȂ���΂Ȃ�Ȃ��B
	const DWORD blockSize
		= (sourceFormat.samplesPerSec * BUFFER_LENGTH_SECONDS + NUM_BLOCKS - 1) / NUM_BLOCKS * sourceFormat.blockAlign;
	const DWORD bufferSize = blockSize * NUM_BLOCKS;

	// �o�b�t�@���쐬����B
	WAVEFORMATEX wavefmtex = {};
	wavefmtex.wFormatTag      = WAVE_FORMAT_PCM;
	wavefmtex.nChannels       = sourceFormat.channels;
	wavefmtex.nSamplesPerSec  = sourceFormat.samplesPerSec;
	wavefmtex.nAvgBytesPerSec = sourceFormat.channels * sourceFormat.bytesPerSample * sourceFormat.samplesPerSec;
	wavefmtex.nBlockAlign     = sourceFormat.channels * sourceFormat.bytesPerSample;
	wavefmtex.wBitsPerSample  = sourceFormat.bytesPerSample * 8;
	wavefmtex.cbSize          = 0;
	const CComPtr<IDirectSoundBuffer> buffer = Local::createDSB(pds, bufferSize, wavefmtex);

	// �C�x���g�����B
	const HANDLE evPass = CreateEvent(NULL, FALSE, FALSE, NULL);//���������Z�b�g, �������FALSE
	const HANDLE evClose = CreateEvent(NULL, TRUE, FALSE, NULL);//���}�j���A�����Z�b�g, �������FALSE
	if(!evPass || !evClose){
		return false; //�C�x���g�I�u�W�F�N�g�����Ȃ������I
	}
	
	// �o�b�t�@�ɒʒm�|�C���g��ݒ肷��B
	if(!Local::setNotifyPositions(buffer, evPass, blockSize)){
		CloseHandle(evPass);
		CloseHandle(evClose);
		return false;
	}
	
	// �K�v�ȏ��������o�ϐ��ɃZ�b�g����
	source_ = source;
	sourceFormat_ = sourceFormat;
	sourceSize_ = source->getTotal() * sourceFormat.blockAlign; //�S�f�[�^�o�C�g��(byte)
	streamBuffer_ = buffer;
	bufferSize_ = bufferSize;
	blockSize_ = blockSize;
	threadControlEvents_[EV_PASS] = evPass;
	threadControlEvents_[EV_CLOSE] = evClose;
	
	//�Đ��X���b�h���쐬����(���}���`�X���b�h���C�u�������w�肵�Ă��������B)
	threadHandle_ = getThreadPool()->run(
		&handleNotificationsStatic, (void*)this);
	if(threadHandle_ == ThreadPool::INVALID_THREAD_HANDLE){
		//Create()�� �X�g���[���Đ��p�X���b�h�̍쐬�Ɏ��s���܂����B
		return false;
	}

	return true;
}

/**
 * �\�[�X�T�C�Y���ω������Ƃ��ɌĂяo����܂��B
 */
void StreamSoundBuffer::updateSourceSize(void)
{
	sourceSize_ = source_->getTotal() * sourceFormat_.blockAlign;
}


/**
 * ���[�v�����f�t�H���g�ɐݒ肵�܂��B
 */
void StreamSoundBuffer::setLoopDefault(void)
{
	// �N���e�B�J���Z�N�V����
	CriticalSectionLock lock(&cs_);

	// ���[�v���ݒ�
	loopState_ = LoopState(
		getDefaultLoopIn() * sourceFormat_.blockAlign, //���[�v�C���I�t�Z�b�g(byte)
		getDefaultLoopOut() * sourceFormat_.blockAlign, //���[�v�A�E�g�I�t�Z�b�g(byte)
		getDefaultLoopCount() );
}


/**
 * ���݂̍Đ��J�[�\���̈ʒu���牉�t���J�n���܂��B
 */
bool StreamSoundBuffer::play(void)
{
	// �N���e�B�J���Z�N�V����
	CriticalSectionLock lock(&cs_);

	// �Đ����Ȃ�Ή������Ȃ��ėǂ��B
	DWORD status = 0;
	if(FAILED(streamBuffer_->GetStatus(&status))){
		return false; //�\���ł��Ȃ��G���[�BDSERR_INVALIDPARAM�����N����Ȃ��͂��B
	}
	if(status & DSBSTATUS_PLAYING){
		return true; //���łɍĐ����B
	}

	// sourcePos_�ƃX�g���[���̓ǂݍ��݈ʒu�������ł��邱�Ƃ�ۏ؂���B
	assert(sourcePos_ == source_->tell() * sourceFormat_.blockAlign); // sourcePos_�͊�{�I��blockSize_�ő������AblockSize_��sourceFormat_.blockAlign�̐����{�̂͂��Ȃ̂ő��v�Ȃ͂��B
	if(sourcePos_ != source_->tell() * sourceFormat_.blockAlign){
		source_->seek(sourcePos_ / sourceFormat_.blockAlign);
	}
	// �o�b�t�@�̓N���A���Ȃ��B
	// clearBuffer();

	// ���[�v����ݒ肷��B(�o�b�t�@�Ƀf�[�^�𖄂߂�O��)
	setLoopDefault();
	//�o�b�t�@���f�[�^�Ŗ��߂�B
	fillBuffer();
	//�Đ����J�n����
	if(FAILED(streamBuffer_->Play(0, 0, DSBPLAY_LOOPING))){
		return false;
	}

	return true;
}

/**
 * �֘A�Â����Ă��鉹���\�[�X�̐擪���牉�t���J�n���܂��B
 */
bool StreamSoundBuffer::playFromBeginning(void)
{
	// �N���e�B�J���Z�N�V����
	CriticalSectionLock lock(&cs_);

	// ���ɍĐ����Ȃ��~������
	streamBuffer_->Stop();

	// ���o������B�\�[�X�ʒu��ύX���ăo�b�t�@���N���A����B
	seekPlayingPosition(0); //�V�[�N�ł��Ȃ������ꍇ�͎d���Ȃ��̂ł��̂܂܌p���ōĐ������݂�B

	// ���[�v����ݒ肷��B(�o�b�t�@�Ƀf�[�^�𖄂߂�O��)
	setLoopDefault();
	//�o�b�t�@���f�[�^�Ŗ��߂�B
	fillBuffer();
	//�Đ����J�n����
	if(FAILED(streamBuffer_->Play(0, 0, DSBPLAY_LOOPING))){
		return false;
	}
	
	return true;
}

/**
 * ���t���~���܂��B
 */
bool StreamSoundBuffer::stop(void)
{
	// �N���e�B�J���Z�N�V����
	CriticalSectionLock lock(&cs_);

	// �Đ����Ȃ��~������
	const HRESULT hr = streamBuffer_->Stop();

	return (SUCCEEDED(hr)) ? true : false;
}

/**
 * ���݉��t���ł��邩�ǂ�����Ԃ��܂��B
 */
bool StreamSoundBuffer::isPlaying(void)
{
	//�N���e�B�J���Z�N�V����
	CriticalSectionLock lock(&cs_);

	DWORD status = 0;
	const HRESULT hr = streamBuffer_->GetStatus(&status);

	return SUCCEEDED(hr)
		? (status & DSBSTATUS_PLAYING) ? true : false
		: false;
}

/**
 * �Đ��J�[�\���̈ʒu��ݒ肵�܂��B
 * @param pos �֘A�Â����Ă��鉹���\�[�X(WaveReader)�擪����̃T���v�����ł��B0�̂Ƃ������\�[�X�擪��\���܂��B
 */
bool StreamSoundBuffer::setPositionSamples(WaveSize posSamples)
{
	//�N���e�B�J���Z�N�V����
	CriticalSectionLock lock(&cs_);

	// ���t���Ȃ�Ή��t���~����B
	// ��~�����ɃV�[�N���邱�Ƃ��ł��邪�A�u���b�N�P�ʂŏ��������Ă���̂łǂ����Ă��x�����o�Ă��܂��B
	// 1�u���b�NBUFFER_LENGTH_SECONDS(3)/NUM_BLOCKS(8)=0.375�b���炢�B
	// �������݃J�[�\���̌�Ɉ��S�}�[�W����1�u���b�N���Ƃ��Ă���̂ŁA�V�[�N��̉��t���n�܂�܂�400ms�ȏォ�����Ă��܂��B
	// ����Ȃ�~�߂Ă�蒼���������������B
	const bool playing = isPlaying();
	if(playing){
		streamBuffer_->Stop();
	}

	// �V�[�N���ăo�b�t�@���N���A����B
	const bool succeeded = seekPlayingPosition(posSamples);

	// �K�v�Ȃ牉�t���ĊJ����B
	if(playing){
		fillBuffer();
		streamBuffer_->Play(0, 0, DSBPLAY_LOOPING);
	}

	return succeeded;
}


/**
 * �Đ��J�[�\���̈ʒu��Ԃ��܂��B
 * @return �֘A�Â����Ă��鉹���\�[�X(WaveReader)�擪����̃T���v�����ł��B0�̂Ƃ������\�[�X�擪��\���܂��B
 *         �Ȃ�炩�̎���ňʒu���擾�ł��Ȃ��ꍇ�� UNKNOWN_SAMPLES ��Ԃ��܂��B
 */
WaveSize StreamSoundBuffer::getPositionSamples(void)
{
	//�N���e�B�J���Z�N�V����
	CriticalSectionLock lock(&cs_);

	// �u���b�N��񂪈�������ꍇ�͎���̓ǂݎ��ʒu�ł���sourcePos_���g���B
	if(blockCount_ == 0){
		return sourcePos_ / sourceFormat_.blockAlign;
	}

	// �Đ��J�[�\�����擾����B
	DWORD currentPlayPos = 0;
	const HRESULT hr = streamBuffer_->GetCurrentPosition(&currentPlayPos, NULL);
	if(!SUCCEEDED(hr)){
		return UNKNOWN_SAMPLES;
	}

	// �Đ��J�[�\�����w���u���b�N���擾����B
	const DWORD currentPlayIndex = currentPlayPos / blockSize_;
	const DWORD currentPlayOffset = currentPlayPos % blockSize_;

	const BlockInfo &binfo = blockInfo_[currentPlayIndex];
	if(!binfo.hasSourcePos()){
		return sourcePos_ / sourceFormat_.blockAlign; //�ǂ��z���Ă��܂����󋵂��Ǝv����̂ŁAsourcePos_��Ԃ��B
	}

	// ���[�v���l�����ău���b�N���ł̐��m�ȃ\�[�X�ʒu������o���B
	///@todo isTerminated�܂ł͍l�����Ă��Ȃ��B���̑���sourceSize_���l�����Ă��邯�ǂ���ő��v�H
	WaveSize remainingBytes = currentPlayOffset;
	WaveSize pos = binfo.sourcePos_;
	LoopState ls = binfo.loopState_;
	while(ls.loopCount_ != 0){
		if(pos <= ls.loopOut_ && pos+remainingBytes > ls.loopOut_){
			remainingBytes -= (ls.loopOut_ - pos);
		}
		else if(pos <= sourceSize_ && pos+remainingBytes > sourceSize_){
			remainingBytes -= (sourceSize_ - pos);
		}
		else{
			break;
		}

		if(ls.loopIn_ >= ls.loopOut_){ //���[�v�C���ʒu�����[�v�A�E�g�ʒu�Ɠ���������̏ꍇ�A���[�v�񐔂͖��Ӗ��B
			if(ls.loopCount_ > 0){
				ls.loopCount_ = 0;
			}
			else{
				break;//���̂܂܂��ᖳ�����[�v����I�I
			}
		}

		pos = ls.loopIn_;
		if(ls.loopCount_ > 0){
			--ls.loopCount_;
		}
	}
	pos += remainingBytes;

	return (pos <= sourceSize_ ? pos : sourceSize_) / sourceFormat_.blockAlign;
}


/**
 * �{�����[����ύX���܂��B
 * @param vol 0�`-10000�̊Ԃ̒l�ŁA1/100db�P�ʂŎw�肵�܂��B
 */
bool StreamSoundBuffer::setVolume(int vol)
{
	//�N���e�B�J���Z�N�V����
	CriticalSectionLock lock(&cs_);

	const HRESULT hr = streamBuffer_->SetVolume(vol);

	return (SUCCEEDED(hr)) ? true : false;
}

/**
 * �{�����[����Ԃ��܂��B
 * @return �{�����[����0�`-10000�̊Ԃ̒l�ŁA1/100db�P�ʂŕԂ��܂��B
 */
bool StreamSoundBuffer::getVolume(int *vol)
{
	//�N���e�B�J���Z�N�V����
	CriticalSectionLock lock(&cs_);

	LONG lVol;
	const HRESULT hr = streamBuffer_->GetVolume(&lVol);
	if(vol){
		*vol = lVol;
	}

	return (SUCCEEDED(hr)) ? true : false;
}

/**
 * �p���ʒu��ݒ肵�܂��B
 */
bool StreamSoundBuffer::setPan(int pan)
{
	//�N���e�B�J���Z�N�V����
	CriticalSectionLock lock(&cs_);

	const HRESULT hr = streamBuffer_->SetPan(pan);

	return (SUCCEEDED(hr)) ? true : false;
}


/**
 * �p���ʒu��Ԃ��܂��B
 */
bool StreamSoundBuffer::getPan(int *pan)
{
	//�N���e�B�J���Z�N�V����
	CriticalSectionLock lock(&cs_);

	LONG lPan;
	const HRESULT hr = streamBuffer_->GetPan(&lPan);
	if(pan){
		*pan = lPan;
	}

	return (SUCCEEDED(hr)) ? true : false;
}

/**
 * �Đ��s�b�`��ݒ肵�܂��B
 * @param semitone1000 �s�b�`�x���h�l�𔼉�=1000�Ŏw�肵�܂��B
 */
bool StreamSoundBuffer::setPitch(int semitone1000) // �����g���ɑ΂���2^(semitone1000/12000)�{�B
{
	//�N���e�B�J���Z�N�V����
	CriticalSectionLock lock(&cs_);

	pitchBend_ = semitone1000;

	const double fd = sourceFormat_.samplesPerSec * std::pow(2.0, pitchBend_ / 12000.0);
	const DWORD f = static_cast<DWORD>(
		(fd < DSBFREQUENCY_MIN) ? DSBFREQUENCY_MIN :
		(fd > DSBFREQUENCY_MAX) ? DSBFREQUENCY_MAX :
		fd);

	const HRESULT hr = streamBuffer_->SetFrequency(f);

	return (SUCCEEDED(hr)) ? true : false;
}

/**
 * �Đ��s�b�`���擾���܂��B
 */
bool StreamSoundBuffer::getPitch(int *semitone1000)
{
	//�N���e�B�J���Z�N�V����
	CriticalSectionLock lock(&cs_);

	*semitone1000 = pitchBend_;

	return true;
}



/**
 * �Đ��p�X���b�h�ł��B
 */
unsigned __stdcall StreamSoundBuffer::handleNotificationsStatic(void *pThis)
{
	((StreamSoundBuffer*)pThis)->handleNotifications();

	return 0;
}

/**
 * �Đ��ʉ߃C�x���g���Ď����A�K�؂ȏ������s�����߂̊֐��ł��B
 */
void StreamSoundBuffer::handleNotifications(void)
{
	DWORD event = 0;
	
	while((event = WaitForMultipleObjects(EV_COUNT, threadControlEvents_, FALSE, INFINITE))!= WAIT_FAILED){
		// �I���C�x���g
		if(event - WAIT_OBJECT_0 == EV_CLOSE){
			//�蓮���Z�b�g�C�x���g�I�u�W�F�N�g���V�O�i����Ԃɂ͂��܂���B
			break;
		}
		// �Đ��ʉ߃C�x���g
		else if(event - WAIT_OBJECT_0 == EV_PASS){
			//�������Z�b�g�C�x���g�I�u�W�F�N�g�Ȃ̂ŁA���Z�b�g���܂���B
			{
				CriticalSectionLock lock(&cs_);

				if(!streamBuffer_){
					break;//�X�g���[���o�b�t�@���L������Ȃ��I�I
				}
				if(!isPlaying()){
					continue;//���t���~�܂��Ă���I�I
				}

				//�܂��u���b�N�𖞂����]�n������Ȃ�u���b�N�𖞂����B
				fillBuffer();

				//�L���ȃu���b�N�������A�I�[�ɒB���Ă���Ȃ�Đ���~�B
				if(numBlocksValid_ == 0
					&& loopState_.loopCount_ == 0
					&& sourcePos_ == sourceSize_){

					streamBuffer_->Stop();
					std::cout << "stopped." << std::endl;
					continue;
				}
			}
		}
	}
}

/**
 * streamBuffer_���f�[�^�Ŗ��߂܂��B
 *
 * �Đ��ς݂̃u���b�N��j�����āA�ł��邾�������̃u���b�N�փf�[�^���l�߂܂��B
 */
bool StreamSoundBuffer::fillBuffer(void) //lock cs_
{
	// �������݋֎~�͈�(�Đ��J�[�\���Ə������݃J�[�\���Ƃ̊�)�����߂�B
	DWORD currentPlayPos = 0;
	DWORD currentWritePos = 0;
	const HRESULT hr = streamBuffer_->GetCurrentPosition(&currentPlayPos, &currentWritePos);
	if(FAILED(hr)){
		//fillBuffer()�� GetCurrentPosition�Ɏ��s���܂����B
		return false;
	}

	DWORD status = 0;
	streamBuffer_->GetStatus(&status);
	if(status & DSBSTATUS_PLAYING){
		// �Đ����Ȃ��currentWritePos�̌�ɏ������S�}�[�W�������B
		const DWORD MARGIN = blockSize_;
		const DWORD readOnlyLength
			= currentWritePos >= currentPlayPos ? currentWritePos - currentPlayPos : currentWritePos + (bufferSize_ - currentPlayPos);
		if(readOnlyLength >= bufferSize_ - MARGIN){
			return false; // �������݋֎~�͈͂��L������B
		}
		currentWritePos += MARGIN;
		if(currentWritePos >= bufferSize_){
			currentWritePos -= bufferSize_;
		}
		assert(currentPlayPos != currentWritePos); ///@todo �v�m�F
	}
	else{
		assert(currentPlayPos == currentWritePos); ///@todo �v�m�F
	}

	// �������݋֎~�u���b�N�������߂�B
	const unsigned int currentPlayIndex = currentPlayPos / blockSize_; //floor
	const unsigned int currentWriteIndex = (currentWritePos + blockSize_ - 1)/blockSize_; //ceil
	const unsigned int readOnlyBlockCount //����:�l���[0, NUM_BLOCKS+1]
		= (currentPlayPos < currentWritePos) ? currentWriteIndex - currentPlayIndex
		: (currentPlayPos > currentWritePos) ? currentWriteIndex + NUM_BLOCKS - currentPlayIndex
		: 0;
	if(readOnlyBlockCount >= NUM_BLOCKS){
		return false; //�S�Ẵu���b�N���������݋֎~���Ɖ����ł��Ȃ��B
	} //����ɂ���āAreadOnlyBlockCount>=NUM_BLOCKS�̂Ƃ���currentPlayPos == currentWritePos��currentPlayPos+1 == currentWritePos�ł���󋵂��l�����Ȃ��ėǂ��Ȃ�B

	///@todo �Đ��J�[�\�����f�[�^���Ȃ��u���b�N���w���Ă���ꍇ�ɂǂ����邩�B�Đ����ɂ͋N���肦�Ȃ��H�@��~���Ȃ�Έ�ԍŏ��̏󋵂����������ǁB

	// �Đ��J�[�\���̑O�ɂ���Đ��ς݂̃f�[�^��j������B
	popBlocksBeforePlayCursor(currentPlayPos);

	// �󂢂Ă���u���b�N�������f�[�^���l�߂�B
	while(blockCount_ < NUM_BLOCKS){
		const unsigned int newBlockIndex = getNewBlockIndex();

		const bool intersects
			= (readOnlyBlockCount <= 0) ? false
			: (currentPlayIndex < currentWriteIndex) 
				? (newBlockIndex >= currentPlayIndex && newBlockIndex < currentWriteIndex)
				: (newBlockIndex >= currentPlayIndex || newBlockIndex < currentWriteIndex);

		// �Ώۃu���b�N(newBlockIndex)���������݋֎~�ȏꍇ(�J�[�\���ɒǂ����ꂽ�ꍇ)�̏����́A���̂����ꂩ�̕��@�����蓾��B
		// - ������ł��؂��ď������݋֎~�͈͂��ʂ�߂���̂�҂B�o�b�t�@��T���������Ԃ̒x���e�F����B
		// - �Ώۃu���b�N�Ƀf�[�^���������܂��ɗL���ȃu���b�N�Ƃ��Ė�����o�^���A�������݋֎~�͈͂��o���Ƃ��납�瑱�����������ށB���Ԃ̒x��𑽏��͗e�F����B
		// - �Ώۃu���b�N�Ƀf�[�^���������܂��ɗL���ȃu���b�N�Ƃ��Ė�����o�^���A���̕������\�[�X���X�L�b�v���A�������݋֎~�͈͂��o���Ƃ��납�瑱�����������ށB�x�ꂽ���Ԃ����߂����Ƃ���B���Y���͈ێ�����₷���B
		// �ǂ݂̂����������Ȃ�͖̂h���Ȃ��̂ŁA�ł��邾���ȒP�ȕ��@���̗p���Ă����B
		if(intersects){
			break;
		}

		const BlockInfo newBlockInfo = fillBlock(newBlockIndex);
		pushBackBlock(newBlockInfo);
	}
	return true;
}


/**
 * �Đ��ʒu��ύX���܂��B
 *
 * �ł��邾����~���ɌĂяo���ĉ������B��~���łȂ��Ƃ��̌��fillBuffer�Ăяo�������΂炭�̊ԋ@�\���Ȃ����Ƃ����肦�܂��B
 *
 * �V�[�N���ăo�b�t�@���N���A���܂��B
 * �V�[�N�ł��Ȃ������ꍇ�͂����炭�������Ȃ��������ƂɂȂ�܂��B
 * �V�[�N�ł��ăo�b�t�@���N���A�ł��Ȃ������ꍇ�́A�o�b�t�@�ɒ��܂��Ă����f�[�^�̌�ɃV�[�N��̃f�[�^���ǂݍ��܂�邱�ƂɂȂ�܂��B
 */
bool StreamSoundBuffer::seekPlayingPosition(WaveSize posSamples) //cs_
{
	if(!seekSource(posSamples)){
		return false;
	}
	if(!clearBuffer()){
		//���s���Ă��V�[�N�����b���x��邾���Ȃ̂Ŗ�������B
	}
	return true;
}

/**
 * �\�[�X�̓ǂݎ��ʒu��ύX���܂��B
 */
bool StreamSoundBuffer::seekSource(WaveSize posSamples) //cs_
{
	if(posSamples > sourceSize_ / sourceFormat_.blockAlign){
		posSamples = sourceSize_ / sourceFormat_.blockAlign; // sourceSize_(�o�C�g��)���z���Ȃ��悤�ɂ���B
	}

	if(!source_->seek(posSamples)){
		return false;
	}
	sourcePos_ = source_->tell() * sourceFormat_.blockAlign; //�ǂݍ��݈ʒu(�o�C�g��)�ϐ����X�V����B
	return true;
}

/**
 * �o�b�t�@���N���A���A�Đ��J�[�\�����o�b�t�@�̐擪�ֈړ����܂��B
 *
 * �ł��邾����~���ɌĂяo���ĉ������B��~���łȂ��Ƃ��̌��fillBuffer�Ăяo�������΂炭�̊ԋ@�\���Ȃ����Ƃ����肦�܂��B
 */
bool StreamSoundBuffer::clearBuffer(void) //cs_
{
	//streamBuffer_->Stop(); �O�����

	if(FAILED(streamBuffer_->SetCurrentPosition(0))){
		return false;
	}
	clearBlocks();
	return true;
}

/**
 * �S�u���b�N��j�����܂��B
 */
void StreamSoundBuffer::clearBlocks(void) //lock cs_
{
	blockFrontIndex_ = 0;
	blockCount_ = 0;
	numBlocksValid_ = 0;
}

/**
 * �V�����u���b�N�̃C���f�b�N�X�ԍ���Ԃ��܂��B
 */
unsigned int StreamSoundBuffer::getNewBlockIndex(void) //lock cs_
{
	assert(blockCount_ < NUM_BLOCKS);
	const unsigned int index = blockFrontIndex_ + blockCount_;
	return index < NUM_BLOCKS ? index : index - NUM_BLOCKS;
}

/**
 * �V�����u���b�N��ǉ����܂��B
 * �f�[�^�͂��ł�getNewBlockIndex()���Ԃ����u���b�N�֊i�[����Ă�����̂Ƃ��܂��B
 */
void StreamSoundBuffer::pushBackBlock(const BlockInfo &info) //lock cs_
{
	assert(blockCount_ < NUM_BLOCKS);
	if(blockCount_ >= NUM_BLOCKS){
		return;
	}
	const unsigned int newBlockIndex = getNewBlockIndex();
	blockInfo_[newBlockIndex] = info;
	++blockCount_;

	if(info.isValidBlock()){
		++numBlocksValid_;
	}
}

/**
 * ���t���I������u���b�N��j�����܂��B
 * �w�肳�ꂽ�Đ��J�[�\���ʒu���O�̃u���b�N��S�Ĕj�����܂��B
 */
void StreamSoundBuffer::popBlocksBeforePlayCursor(DWORD currentPlayPos) //lock cs_
{
	assert(currentPlayPos <= bufferSize_); //���͏���

	const DWORD currentPlayIndex = (currentPlayPos / blockSize_) % NUM_BLOCKS;

	// currentPlayIndex���͈�[blockFrontIndex_, blockFrontIndex_+blockCount)�̒��ɂ���ꍇ�AcurrentPlayIndex�̎�O�܂Ŕj������B
	// currentPlayIndex���͈�[blockFrontIndex_, blockFrontIndex_+blockCount)�̊O�ɂ���ꍇ�A�S�Ĕj������B�f�[�^�������ǂ������A�Đ��J�[�\���������u���b�N��ǂ��z���Ă��܂����Ɖ��߂���B
	while(blockCount_ > 0 && blockFrontIndex_ != currentPlayIndex){
		popFrontBlock();
	}
}

/**
 * �ł��Â��u���b�N����j�����܂��B
 */
void StreamSoundBuffer::popFrontBlock(void) //lock cs_
{
	if(blockCount_ <= 0){
		return;
	}

	if(blockInfo_[blockFrontIndex_].isValidBlock()){
		--numBlocksValid_;
	}
	blockInfo_[blockFrontIndex_] = BlockInfo();

	--blockCount_;
	if(++blockFrontIndex_ >= NUM_BLOCKS){
		blockFrontIndex_ = 0;
	}
}

/**
 * �w�肳�ꂽ�u���b�N���f�[�^�Ŗ��߂܂��B
 * ���̊֐��̓N���e�B�J���Z�N�V����cs_�����b�N���Ă���ԂɌĂяo���ĉ������B
 *
 * @return �������񂾌��ʂ̃u���b�N����Ԃ��܂��B
 */
StreamSoundBuffer::BlockInfo StreamSoundBuffer::fillBlock(unsigned int blockIndex) //lock cs_
{
	assert(streamBuffer_ && blockSize_ > 0); // ���O����
	assert(blockIndex < NUM_BLOCKS); // ���͏���

	const DWORD bufferPos = blockIndex * blockSize_;

	//�o�b�t�@�����b�N���܂��B
	BYTE *writePtr1 = NULL;
	BYTE *writePtr2 = NULL;
	DWORD len1;
	DWORD len2;
	HRESULT hr = streamBuffer_->Lock(bufferPos, blockSize_
		, (LPVOID *)&writePtr1, &len1
		, (LPVOID *)&writePtr2, &len2, 0);
	if(hr == DSERR_BUFFERLOST){
		//FillBlock()�� Restore�����݂܂��B
		streamBuffer_->Restore();
		hr = streamBuffer_->Lock(bufferPos, blockSize_
			, (LPVOID *)&writePtr1, &len1
			, (LPVOID *)&writePtr2, &len2, 0);
	}
	if(FAILED(hr)){
		//FillBlock()�� ���b�N�Ɏ��s���܂����B
		return BlockInfo();
	}
	//writePtr2�͕K��NULL�̂͂��B�łȂ���΃A�����b�N���Ĉُ�I���B
	if(writePtr2){
		streamBuffer_->Unlock(writePtr1, len1, writePtr2, len2);
		//FillBlock()�� writePtr2!=NULL
		return BlockInfo();
	}
	//len1��blockSize_��������͂��B�łȂ���΃A�����b�N���Ĉُ�I���B
	if(len1 < blockSize_){
		streamBuffer_->Unlock(writePtr1, len1, writePtr2, len2);
		//FillBlock()�� len1 < blockSize_
		return BlockInfo();
	}

	//���b�N�����o�b�t�@�͈̔͂ɗL���ȃf�[�^��ǂݍ��݂܂��B
	const WaveSize blockHeadSourcePos = sourcePos_;
	const LoopState blockHeadLoopState = loopState_;
	const DWORD writtenSize = readWave(writePtr1, blockSize_);

	//�̂����0�Ŗ��߂�
	if(writtenSize < blockSize_){
		FillMemory(writePtr1 + writtenSize, blockSize_ - writtenSize
			, (BYTE)(sourceFormat_.bytesPerSample == 1 ? 128 : 0));
	}
	
	//�A�����b�N
	streamBuffer_->Unlock(writePtr1, len1, NULL, 0);

	return BlockInfo(blockHeadSourcePos, blockHeadLoopState, writtenSize);
}

/**
 * wave���[�_�[����Đ��p�̃f�[�^��ǂݍ��݁A�w�肵���o�b�t�@�ɏ������݂܂��B
 *
 * ���t�f�[�^����������͏o���邾�� bufferSize ���������������Ƃ��܂��B���[�v���l�����܂��B
 * ���t�f�[�^�I�[�ȍ~�ɖ����f�[�^�͏������݂܂���B
 *
 * ���̊֐��� wave���[�_�[�A�c�胋�[�v���A���ǂݍ��݈ʒu��K�؂ɏ��������܂��B
 *
 * @param bufferPtr �������݈ʒu�擪�B
 * @param bufferSize �ő发�����݃o�C�g���B
 * @return �������񂾃o�C�g���B
 *         ���t�f�[�^�̏I�[�ɒB������A�\�����Ȃ���肪���������ꍇ��bufferSize��菬�����l��Ԃ��܂��B
 */
DWORD StreamSoundBuffer::readWave(BYTE *bufferPtr, DWORD bufferSize) //lock cs_
{
	DWORD writtenSize = 0;
	while(writtenSize < bufferSize){
		// ���[�v�ݒ肪�L���Ȃ�΃��[�v���������܂��B
		// ���[�v�A�E�g�ʒu���z���Ă����烋�[�v�C���ʒu�փV�[�N���A���[�v�J�E���^�����炵�܂��B
		if(loopState_.loopCount_ != 0 && (sourcePos_ == loopState_.loopOut_ || source_->isTerminated())){
			if(loopState_.loopIn_ >= loopState_.loopOut_){ //���[�v�C���ʒu�����[�v�A�E�g�ʒu�Ɠ���������̏ꍇ�A���[�v�񐔂͖��Ӗ��B
				if(loopState_.loopCount_ > 0){
					loopState_.loopCount_ = 0;
				}
				else{
					break;//���̂܂܂��ᖳ�����[�v����I�I
				}
			}
			if(!source_->seek(loopState_.loopIn_ / sourceFormat_.blockAlign)){
				break;//�V�[�N�ł��Ȃ��I�I
			}
			sourcePos_ = loopState_.loopIn_;
			if(loopState_.loopCount_ > 0){
				loopState_.loopCount_--;
			}
		}
		//��x�ɓǂ߂邾���ǂݍ��݂܂��B
		const WaveSize limit = (loopState_.loopCount_ != 0) ? loopState_.loopOut_ : sourceSize_;
		const WaveSize limitSize = limit - sourcePos_; //���݂̒n�_����(���[�v�A�E�g�n�_�܂��̓f�[�^�I�[)�܂ł̃T�C�Y
		const DWORD remainSize = bufferSize - writtenSize;
		const DWORD readSize = static_cast<DWORD>((limitSize < remainSize) ? limitSize : remainSize);
		if(readSize <= 0){
			break;//�I�[�ɒB�����͗l
		}
		if(loopState_.loopCount_ == 0 && source_->isTerminated()){
			break;//�\�������I�[�ɒB�����͗l(�����炭loopState_.loopOut_��sourceSize_�����ۂ̏I�[�����ɂ��邩�A�J�n���ɐ擪�ɃV�[�N�ł��Ă��Ȃ�������)
		}
		std::size_t actualReadSize;
		if(!source_->read(bufferPtr + writtenSize, readSize, &actualReadSize)){
			break;//���s�I�H
		}
		//
		writtenSize += actualReadSize;
		sourcePos_ += actualReadSize;
	}
	return writtenSize;
}

/**
 * �X���b�h�v�[�����擾���܂��B
 */
ThreadPool *StreamSoundBuffer::getThreadPool(void)
{
	static ThreadPool threadPool;
	return &threadPool;
}






}}//namespace aush::dsound

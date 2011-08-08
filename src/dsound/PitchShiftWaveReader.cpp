/**
 * @file
 * @author AKIYAMA Kouhei
 * @since 2011-08-05
 */
#include <algorithm>
#include <boost/cstdint.hpp>
#include "../audioproc/SmbPitchShift.h"
#include "../audioproc/Multiplex.h"
#include "../util/HeapArray.h"
#include "PitchShiftWaveReader.h"

using boost::uint8_t;
using boost::uint16_t;
using boost::uint32_t;

namespace aush{ namespace dsound{

PitchShiftWaveReader::PitchShiftWaveReader(
		const boost::shared_ptr<WaveReader> &source,
		int fftFrameSize,
		int fftOverlapFactor)
	: source_(source)
	, format_(source->getFormat())
	, fftFrameSize_(fftFrameSize)
	, fftOverlapFactor_(fftOverlapFactor)
	, shiftRatio_(1.0f)
	, resampleRatio_(1.0f)
	, sourceNumSamples_(source->getTotal())
	, outputNumSamples_(sourceNumSamples_) //������������
	, outputPos_(0)
{
	initShifter();
}

void PitchShiftWaveReader::initShifter(void)
{
	for(unsigned int ch = 0; ch < format_.channels; ++ch){
		const boost::shared_ptr<SmbPitchShift> shifter(new SmbPitchShift(fftFrameSize_));
		shifters_.push_back(shifter);
	}
}

void PitchShiftWaveReader::setShiftRatio(float shiftRatio)
{
	shiftRatio_ = shiftRatio < 0.5f ? 0.5f : shiftRatio > 2.0f ? 2.0f : shiftRatio;
}

void PitchShiftWaveReader::setResampleRatio(float resampleRatio)
{
	if(resampleRatio < 0){
		resampleRatio = 0;
	}
	else if(resampleRatio > 8.0f){
		resampleRatio = 8.0f;
	}

	const double num = std::floor(sourceNumSamples_ * resampleRatio);
	if(!(num >= 0 && num <= UINT_MAX/3U)){
		return;
	}

	resampleRatio_ = resampleRatio;
	outputNumSamples_ = static_cast<unsigned int>(num);
	if(outputPos_ > outputNumSamples_){
		outputPos_ = outputNumSamples_;
	}

	if(resampleRatio_ == 1.0f){
		resampler_.reset();
	}
	else{
		resampler_.reset(new NearestResampler<unsigned int>(sourceNumSamples_, outputNumSamples_));
		resampler_->incrementOutput(outputPos_);
	}
}



/**
 * �S�Ẵ��\�[�X��������A���ɂ܂��I�[�v���ł����Ԃɂ��܂��B
 */
void PitchShiftWaveReader::close(void)
{}

/**
 * �I�[�v����Ԃ��ǂ�����Ԃ��܂��B
 */
bool PitchShiftWaveReader::isOpen(void) const
{
	return source_->isOpen();
}

/**
 * �w�肵���o�C�g������pcm�f�[�^��ǂݍ��݂܂��B
 * �G���[���������邩�A�I�[�ɒB����܂�size�Ŏw�肵���o�C�g�������ǂݍ��߂܂��B
 */
bool PitchShiftWaveReader::read(void *dst, std::size_t size, std::size_t *actualSize)
{
	if(!isOpen()){
		return false;
	}
	
	if(size == 0){
		if(actualSize){
			*actualSize = 0;
		}
		return true;
	}

	// �o�͂ɕK�v�ȓ��̓T���v���������߂�B
	const std::size_t reqOutSamples = size / format_.blockAlign; //floor
	const WaveSize reqSrcSamples0 = resampler_ ? resampler_->calcRequiredInputSize(reqOutSamples) : reqOutSamples;
	if(reqSrcSamples0 > std::numeric_limits<std::size_t>::max() / format_.blockAlign){
		return false;
	}
	const std::size_t reqSrcSamples = static_cast<std::size_t>(reqSrcSamples0);
	const std::size_t reqSrcBytes = reqSrcSamples * format_.blockAlign;

	// read source bytes.
	std::size_t actualSizeSrc = 0;
	HeapArray<uint8_t> byteBuffer(reqSrcBytes);
	if(!source_->read(byteBuffer.get(), reqSrcBytes, &actualSizeSrc)){
		return false;
	}

	// pitch shift.
	const unsigned int actualSrcSamples = actualSizeSrc / format_.blockAlign; //floor

	if(shiftRatio_ != 1.0f){
		HeapArray<float> floatBuffer(actualSrcSamples);
		for(unsigned int ch = 0; ch < format_.channels; ++ch){
			demultiplex(floatBuffer.get(), byteBuffer.get(), ch, actualSrcSamples, format_.blockAlign, format_.bytesPerSample);

			shifters_[ch]->smbPitchShift(
				shiftRatio_,
				actualSrcSamples,
				fftFrameSize_,
				fftOverlapFactor_,
				static_cast<float>(format_.samplesPerSec),
				floatBuffer.get(),
				floatBuffer.get());

			multiplex(byteBuffer.get(), floatBuffer.get(), ch, actualSrcSamples, format_.blockAlign, format_.bytesPerSample);
		}
	}

	// resample.
	if(resampler_){
		assert(actualSrcSamples == reqSrcSamples);
		unsigned int actualOutSamples = reqOutSamples;
		if(actualSrcSamples != reqSrcSamples){
			const std::size_t maxOutSamples = static_cast<std::size_t>(std::min(resampler_->calcMaxOutputSize(actualSrcSamples), static_cast<WaveSize>(reqOutSamples)));
			if(actualOutSamples > maxOutSamples){
				actualOutSamples = maxOutSamples;
			}
		}

		switch(format_.blockAlign){
		case 1:
			resampler_->resample(
				reinterpret_cast<uint8_t *>(dst),
				reinterpret_cast<uint8_t *>(dst) + actualOutSamples,
				byteBuffer.get(),
				byteBuffer.get() + actualSrcSamples);
			break;
		case 2:
			resampler_->resample(
				reinterpret_cast<uint16_t *>(dst),
				reinterpret_cast<uint16_t *>(dst) + actualOutSamples,
				reinterpret_cast<const uint16_t *>(byteBuffer.get()),
				reinterpret_cast<const uint16_t *>(byteBuffer.get()) + actualSrcSamples);
			break;
		case 4:
			resampler_->resample(
				reinterpret_cast<uint32_t *>(dst),
				reinterpret_cast<uint32_t *>(dst) + actualOutSamples,
				reinterpret_cast<const uint32_t *>(byteBuffer.get()),
				reinterpret_cast<const uint32_t *>(byteBuffer.get()) + actualSrcSamples);
			break;
		}
		if(actualSize){
			*actualSize = actualOutSamples * format_.blockAlign;
		}
		outputPos_ += actualOutSamples;
	}
	else{
		std::memcpy(dst, byteBuffer.get(), actualSrcSamples * format_.blockAlign);
		if(actualSize){
			*actualSize = actualSrcSamples * format_.blockAlign;
		}
		outputPos_ += actualSrcSamples;
	}

	return true;
}

/**
 * ����Read���Ă񂾂Ƃ��Ɏ��o����pcm�f�[�^�̈ʒu���T���v�����Ŏw�肵�܂��B
 */
bool PitchShiftWaveReader::seek(WaveSize sample)
{
	if(sample > outputNumSamples_){
		return false;
	}

	if(resampler_){
		///@todo �����ɂ�resampler_->inputLastData_��seekPos��O�̒l���i�[���������ǂ��B���Ԃ�C�����Ȃ����炢�̍������o�Ȃ����ǁB
		const boost::shared_ptr<NearestResampler<unsigned int> >
			newResampler(new NearestResampler<unsigned int>(
						sourceNumSamples_, outputNumSamples_));
		const WaveSize seekPos = newResampler->incrementOutput(sample);
		if(!source_->seek(seekPos)){
			return false;
		}
		resampler_ = newResampler;
	}
	else{
		if(!source_->seek(sample)){
			return false;
		}
	}
	outputPos_ = sample;
	initShifter();
	return true;
}
	
/**
 * ����Read���Ă񂾂Ƃ��Ɏ��o����pcm�f�[�^�̈ʒu���T���v�����ŕԂ��܂��B
 */
WaveSize PitchShiftWaveReader::tell(void) const
{
	return outputPos_;
}
	
/**
 * ���[�_�[���I�[�ɒB���Ă��邩�ǂ�����Ԃ��܂��B
 * �܂�Seek���Ȃ����肱��ȏ�Read���Ă�ł��f�[�^���o�Ă��Ȃ��Ƃ�true��Ԃ��܂��B
 */
bool PitchShiftWaveReader::isTerminated(void) const
{
	return outputPos_ >= outputNumSamples_ || source_->isTerminated();
}

	
//���擾����
	

/**
 * Read�œǂݍ��߂�f�[�^�̌`����Ԃ��܂��B
 */
WaveFormat PitchShiftWaveReader::getFormat(void) const
{
	return source_->getFormat();
}

/**
 * �S�̂̃T���v������Ԃ��܂��B
 */
WaveSize PitchShiftWaveReader::getTotal(void) const
{
	return outputNumSamples_;
}


}}//namespace aush::dsound

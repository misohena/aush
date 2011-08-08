/**
 * @file
 * @author AKIYAMA Kouhei
 * @since 2011-08-03
 */
#include "ByteWaveReader.h"

namespace aush{ namespace dsound{

ByteWaveReader::ByteWaveReader(const unsigned char *begin, const unsigned char *end, const WaveFormat &format,
							   const boost::shared_ptr<void> &buffer)
	: bufferBegin_(begin)
	, bufferEnd_(end)
	, format_(format)
	, total_(begin && end && end >= begin ? (end - begin) / format.blockAlign : 0)
	, rptr_(begin)
	, buffer_(buffer)
{}

void ByteWaveReader::close(void)
{}

bool ByteWaveReader::isOpen(void) const
{
	return bufferBegin_ && bufferEnd_ && bufferEnd_ >= bufferBegin_;
}

bool ByteWaveReader::read(void *dst, std::size_t size, std::size_t *actualSize)
{
	if(!isOpen()){
		return false;
	}
	
	if(size > static_cast<std::size_t>(bufferEnd_ - rptr_)){
		size = bufferEnd_ - rptr_;
	}

	std::memcpy(dst, rptr_, size);
	rptr_ += size;

	if(actualSize){
		*actualSize = size;
	}
	return true;
}

bool ByteWaveReader::seek(WaveSize sample)
{
	if(sample > total_){
		return false;
	}
	rptr_ = bufferBegin_ + sample * format_.blockAlign;
	return true;
}
	
WaveSize ByteWaveReader::tell(void) const
{
	return (rptr_ - bufferBegin_) / format_.blockAlign;

}
	
bool ByteWaveReader::isTerminated(void) const
{
	return rptr_ == bufferEnd_;
}

	
WaveFormat ByteWaveReader::getFormat(void) const
{
	return format_;
}

WaveSize ByteWaveReader::getTotal(void) const
{
	return total_;
}


}}//namespace aush::dsound

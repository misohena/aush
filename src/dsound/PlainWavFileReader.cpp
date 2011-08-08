/**
 * @file
 * @brief PlainWavFileReaderクラスの実装を記述するためのファイルです。
 * @author AKIYAMA Kouhei
 * @since 2002-09-25
 */

#include <windows.h>
#include "PlainWavFileReader.h"

namespace {
	using namespace aush;

	static boost::uint64_t INVALID_SEEK64 = 0xffffffffffffffff;
	boost::uint64_t seek64(HANDLE handle, boost::int64_t offset, DWORD method)
	{
		LARGE_INTEGER li;
		li.QuadPart = offset;
		li.LowPart = ::SetFilePointer(handle, li.LowPart, &li.HighPart, method);
		if(li.LowPart == INVALID_SET_FILE_POINTER && ::GetLastError() != NO_ERROR){
			return INVALID_SEEK64;
		}
		return li.QuadPart;
	}

	class Win32File
	{
		HANDLE handle_;
		bool error_;
	public:
		explicit Win32File(const PathChar *filename)
		{
			handle_ = CreateFile(
					filename, GENERIC_READ, FILE_SHARE_READ, NULL,
					OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			error_ = (handle_ == INVALID_HANDLE_VALUE);
		}

		~Win32File()
		{
			if(handle_ != INVALID_HANDLE_VALUE){
				CloseHandle(handle_);
			}
		}

		HANDLE release()
		{
			const HANDLE h = handle_;
			handle_ = INVALID_HANDLE_VALUE;
			return h;
		}

		bool hasError() const { return error_;}
		void setError() { error_ = true;}
		void readFull(void *dst, unsigned int size)
		{
			DWORD actualSize = 0;
			if(!ReadFile(handle_, dst, size, &actualSize, NULL)){
				setError();
			}
			if(actualSize != size){
				setError();
			}
		}

		boost::uint32_t readUInt32()
		{
			unsigned char data[4];
			readFull(data, 4);
			return hasError() ? 0 : (data[0] | (data[1]<<8) | (data[2]<<16) | (data[3]<<24));
		}

		boost::uint64_t seek(boost::int64_t offset, DWORD method)
		{
			const boost::uint64_t pos = seek64(handle_, offset, method);
			if(pos == INVALID_SEEK64){
				setError();
			}
			return pos;
		}

		void skip(boost::uint32_t size)
		{
			seek(size, FILE_CURRENT);
		}
		boost::uint64_t getFilePointer()
		{
			return seek(0, FILE_CURRENT);
		}
	};
	
}//namespace

namespace aush{namespace dsound{

/**
 * オブジェクトを構築します。
 */
PlainWavFileReader::PlainWavFileReader()
	: hFile_(INVALID_HANDLE_VALUE)
{
}

/**
 * オブジェクトを解体します。
 */
PlainWavFileReader::~PlainWavFileReader()
{
	close();
}

/**
 * wavファイルをオープンします。WAVE_FORMAT_PCM形式のみ開けます。
 * @param filename ファイル名
 * @return オープンに成功したらtrueを返します。
 */
bool PlainWavFileReader::open(const PathChar *filename)
{
	if(hFile_ != INVALID_HANDLE_VALUE){
		return false;
	}
	// open
	Win32File file(filename);
	if(file.hasError()){
		return false;
	}

	// RIFF
	if(file.readUInt32() != ('R'+('I'<<8)+('F'<<16)+('F'<<24))){
		return false;
	}
	const boost::uint32_t riffSize = file.readUInt32();
	if(file.readUInt32() != ('W'+('A'<<8)+('V'<<16)+('E'<<24))){
		return false;
	}

	// chunks(fmt, data)
	PCMWAVEFORMAT pcmwf = {};
	WaveSize dataPos = 0;
	WaveSize dataSize = 0;
	
	while(!file.hasError()){
		boost::uint32_t id = file.readUInt32();
		boost::uint32_t size = file.readUInt32();
		if(file.hasError()){
			break;
		}

		switch(id){
		case ('f'+('m'<<8)+('t'<<16)+(' '<<24)):
			if(size < sizeof(pcmwf)){
				return false;
			}
			file.readFull(&pcmwf, sizeof(pcmwf));
			file.skip(size - sizeof(pcmwf));
			break;
		case ('d'+('a'<<8)+('t'<<16)+('a'<<24)):
			dataPos = file.getFilePointer();
			dataSize = size;
			file.skip(size);
			break;
		default:
			file.skip(size);
			break;
		}

		if(file.hasError()){
			return false;
		}
	}

	// check format.
	if(pcmwf.wBitsPerSample != 8 && pcmwf.wBitsPerSample != 16){
		return false;
	}
	if(pcmwf.wf.wFormatTag != WAVE_FORMAT_PCM){
		return false;
	}
	if(!(pcmwf.wf.nChannels >= 0 && pcmwf.wf.nChannels < 16)){
		return false;
	}
	if(pcmwf.wf.nBlockAlign != pcmwf.wf.nChannels * (pcmwf.wBitsPerSample / 8)){
		return false;
	}
	if(dataPos == 0){
		return false;
	}

	// seek to data
	file.seek(dataPos, FILE_BEGIN);

	// save variable.
	hFile_ = file.release();
	topPtr_ = dataPos;
	remainSize_ = dataSize;
	totalSize_ = dataSize;
	format_ = WaveFormat(
		pcmwf.wf.nSamplesPerSec,
		pcmwf.wf.nChannels,
		pcmwf.wBitsPerSample / 8,
		pcmwf.wf.nBlockAlign);
	
	return true;
}

void PlainWavFileReader::close(void)
{
	if(hFile_ != INVALID_HANDLE_VALUE){
		CloseHandle(hFile_);
		hFile_ = INVALID_HANDLE_VALUE;
	}
}

bool PlainWavFileReader::isOpen(void) const
{
	return (hFile_ != INVALID_HANDLE_VALUE);
}

bool PlainWavFileReader::read(void *dst, std::size_t size, std::size_t *actualSize)
{
	if(!isOpen()){
		return false;//開いてない
	}
	
	if(size > remainSize_){
		size = static_cast<std::size_t>(remainSize_);
	}
	DWORD actualRead = 0;
	ReadFile(hFile_, dst, size, &actualRead, NULL);
	remainSize_ -= actualRead;
	
	if(actualSize){
		*actualSize = actualRead;
	}
	return true;
}

bool PlainWavFileReader::seek(WaveSize sample)
{
	if(!isOpen()){
		return false;//開いていない
	}
	
	//シーク先を計算(topPtr_からの相対値)
	const WaveSize moveTo = sample * format_.blockAlign;
	if(moveTo > totalSize_){
		return false;//sampleが大きすぎ
	}
	//シーク
	const WaveSize p = seek64(hFile_, moveTo, FILE_BEGIN);
	if(p == INVALID_SEEK64){
		return false;//シークできなかった
	}
	//残りサイズを調節
	remainSize_ = totalSize_ - moveTo;
	return true;
}

WaveSize PlainWavFileReader::tell(void) const
{
	return (totalSize_ - remainSize_) / format_.blockAlign;
}

bool PlainWavFileReader::isTerminated(void) const
{
	if(isOpen()){
		if(remainSize_ == 0){
			return true;
		}
	}
	return false;
}

WaveFormat PlainWavFileReader::getFormat(void) const
{
	return format_;
}

WaveSize PlainWavFileReader::getTotal(void) const
{
	return totalSize_ / format_.blockAlign;
}

}}//namespace aush::dsound

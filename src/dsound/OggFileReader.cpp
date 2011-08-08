/**
 * @file
 * @brief OggFileReaderクラスの実装を記述するためのファイルです。
 * @author AKIYAMA Kouhei
 * @since 2002-09-29
 */
/*
#ifdef _DEBUG
# pragma comment(lib, "libogg_static_d.lib")
# pragma comment(lib, "libvorbis_static_d.lib")
# pragma comment(lib, "libvorbisfile_static_d.lib")
#else
# pragma comment(lib, "libogg_static.lib")
# pragma comment(lib, "libvorbis_static.lib")
# pragma comment(lib, "libvorbisfile_static.lib")
#endif
*/

#include <windows.h>
#include "OggFileReader.h"

namespace aush{namespace dsound{

/**
 * 指定したファイル全体にアクセスするファイル識別子をオープンします。
 */
bool OggWin32File::open(const PathChar *name)
{
	if(isOpen()){
		return false; //すでに開いている。
	}
	HANDLE handle = CreateFile(name, GENERIC_READ, FILE_SHARE_READ, NULL
		, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(handle == INVALID_HANDLE_VALUE){
		return false;
	}

	fileHandle_ = handle;
	offset_ = 0;
	size_ = GetFileSize(handle, NULL);
	curr_ = 0;

	return true;
}

/**
 * Vorbisfileからのコールバックを受けてファイルから指定されたサイズを読みとります。
 */
size_t OggWin32File::callbackRead(void *ptr, size_t size, size_t nmemb, void *datasource)
{
	OggWin32File *file = (OggWin32File *)datasource;

	const FileSizeType requestedSize = FileSizeType(size) * FileSizeType(nmemb);
	const FileSizeType clippedSize = (file->curr_ + requestedSize > file->size_)
		? file->size_ - file->curr_   // ブロック末尾を超える場合、ブロック末尾までのサイズだけ読み込む。
		: requestedSize; // ブロック末尾を超えない場合、要求されたサイズだけ読み込む。

	DWORD actualSize = 0;
	ReadFile(file->getHandle(), ptr,
		static_cast<DWORD>(clippedSize), ///@todo キャストしちゃって良いの？　とはいっても戻り値が32bitだしなぁ。
		&actualSize, NULL);

	file->curr_ += actualSize;

	return actualSize;
}

/**
 * Vorbisfileからのコールバックを受けてファイルポインタを移動します。
 */
int OggWin32File::callbackSeek(void *datasource, ogg_int64_t offset, int whence)
{
	OggWin32File *file = (OggWin32File *)datasource;

	// 基準位置を求める。
	LONGLONG base;
	if(whence == SEEK_CUR){
		base = file->getCurrentInFile();
	}
	else if(whence == SEEK_END){
		base = file->getBlockEnd();
	}
	else if(whence == SEEK_SET){
		base = file->getBlockBegin();
	}
	else{
		return -1;
	}

	// ファイル先頭からシーク先へのバイト数を求める。
	LONGLONG to = base + offset;
	if(to < file->getBlockBegin()){
		return -1; //範囲外。
	}
	if(to > file->getBlockEnd()){ //終端を含まない。
		return -1; //範囲外。
	}


	// ファイル先頭からシークする。
	LARGE_INTEGER li;
	li.QuadPart = to;
	li.LowPart = ::SetFilePointer(file->getHandle(), li.LowPart, &li.HighPart, FILE_BEGIN);
	if(li.LowPart == INVALID_SET_FILE_POINTER && ::GetLastError() != NO_ERROR){
		return -1;
	}

	// 現在の位置を記録する。
	file->curr_ = to - file->getBlockBegin();

	return 0;
}

/**
 * Vorbisfileからのコールバックを受けてファイルをクローズします。ov_clear時に呼ばれるようです。
 */
int OggWin32File::callbackClose(void *datasource)
{
	OggWin32File *file = (OggWin32File *)datasource;
	BOOL result = CloseHandle(file->getHandle());
	*file = OggWin32File();
	return result ? 0 : EOF;
}

/**
 * Vorbisfileからのコールバックを受けて現在の位置を返します。
 */
long OggWin32File::callbackTell(void *datasource)
{
	OggWin32File *file = (OggWin32File *)datasource;

	return static_cast<long>(file->curr_); ///@todo キャストしちゃって良いの？
}

/**
 * ov_open_callbacksに引数として渡す構造体です。
 */
ov_callbacks OggWin32File::oggCallbacks =
{
	OggWin32File::callbackRead,
	OggWin32File::callbackSeek,
	OggWin32File::callbackClose,
	OggWin32File::callbackTell
};




/**
 * オブジェクトを構築します。
 */
OggFileReader::OggFileReader()
	: file_()
	, format_()
	, totalSamples_(0)
	, terminated_(false)
{
}

/**
 * オブジェクトを解体します。
 */
OggFileReader::~OggFileReader()
{
	close();
}

/**
 * oggファイルをファイル名からオープンします。
 */
bool OggFileReader::open(const PathChar *filename)
{
	if(isOpen()){
		return false;
	}
	//ファイルを開く
	OggWin32File file;
	if(!file.open(filename)){
		return false;//開けなかった
	}
	return open(file);
}

/**
 * ファイル識別子からオープンします。
 * @param file 既に開いているファイル識別子。
 *        ここで指定したファイル識別子の所有権は呼び出した瞬間にこのオブジェクトに移ります。
 *        例えばこの関数がfalseを返した場合、ファイルは自動的にクローズされています。
 */
bool OggFileReader::open(const OggWin32File &file)
{
	file_ = file;
	//Oggを開く
	OggVorbis_File vf;
	if(ov_open_callbacks(&file_, &vf, NULL, 0, OggWin32File::oggCallbacks) < 0){
		file_.close();
		return false;//開けなかった
	}

	//シーク可能か調べる
	if(!ov_seekable(&vf)){
		ov_clear(&vf);
		return false;//seekableではない
	}

	//これ以降失敗することは無い
	vf_ = vf;

	//情報収集
	const vorbis_info *info = ov_info(&vf, -1);
	const int sampleBits = 16;	//デコードする量子化ビット数は16で固定
	const int blockAlign = info->channels * (sampleBits / 8);	//1ブロックのバイト数
	format_ = WaveFormat(
		info->rate, //samplesPerSec
		info->channels, //channels
		(sampleBits / 8), //bytesPerSample
		blockAlign); //blockAlign
	//format_.nAvgBytesPerSec = info->rate * blockAlign;

	totalSamples_ = ov_pcm_total(&vf, -1);

	terminated_ = false;

	return true;
}

/**
 * クローズします。
 */
void OggFileReader::close(void)
{
	if(file_.isOpen()){
		ov_clear(&vf_);//コールバック経由でファイルハンドルもクローズされるはず。
	}
}

/**
 * オープンされているかどうかを返します。
 */
bool OggFileReader::isOpen(void) const
{
	return file_.isOpen();
}

bool OggFileReader::read(void *dst, std::size_t size, std::size_t *actualSize)
{
	if(!isOpen()){
		return false;//開いていない
	}

	char *dstPtr = (char *)dst;
	std::size_t remain = size;
	while(remain > 0){
		const long actualRead
			= ov_read(&vf_, dstPtr, remain, 0, 2, 1, NULL);
		//終端に達したか何らかのエラーが発生したならおしまい
		if(actualRead <= 0){
			terminated_ = true;
			break;
		}
		remain -= actualRead;
		dstPtr += actualRead;
	}
	if(actualSize){
		*actualSize = size - remain;
	}
	return true;
}

bool OggFileReader::seek(WaveSize sample)
{
	if(!isOpen()){
		return false;//開いていない
	}

	if(ov_pcm_seek(&vf_, sample)){
		return false;
	}

	terminated_ = false;

	return true;
}

WaveSize OggFileReader::tell(void) const
{
	if(!isOpen()){
		return 0;//開いていない
	}
	return ov_pcm_tell(const_cast<OggVorbis_File *>(&vf_));
}

bool OggFileReader::isTerminated(void) const
{
	return terminated_;
}

WaveFormat OggFileReader::getFormat(void) const
{
	return format_;
}

WaveSize OggFileReader::getTotal(void) const
{
	return totalSamples_;
}

}}//namespace aush::dsound

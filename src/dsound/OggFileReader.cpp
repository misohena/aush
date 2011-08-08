/**
 * @file
 * @brief OggFileReader�N���X�̎������L�q���邽�߂̃t�@�C���ł��B
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
 * �w�肵���t�@�C���S�̂ɃA�N�Z�X����t�@�C�����ʎq���I�[�v�����܂��B
 */
bool OggWin32File::open(const PathChar *name)
{
	if(isOpen()){
		return false; //���łɊJ���Ă���B
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
 * Vorbisfile����̃R�[���o�b�N���󂯂ăt�@�C������w�肳�ꂽ�T�C�Y��ǂ݂Ƃ�܂��B
 */
size_t OggWin32File::callbackRead(void *ptr, size_t size, size_t nmemb, void *datasource)
{
	OggWin32File *file = (OggWin32File *)datasource;

	const FileSizeType requestedSize = FileSizeType(size) * FileSizeType(nmemb);
	const FileSizeType clippedSize = (file->curr_ + requestedSize > file->size_)
		? file->size_ - file->curr_   // �u���b�N�����𒴂���ꍇ�A�u���b�N�����܂ł̃T�C�Y�����ǂݍ��ށB
		: requestedSize; // �u���b�N�����𒴂��Ȃ��ꍇ�A�v�����ꂽ�T�C�Y�����ǂݍ��ށB

	DWORD actualSize = 0;
	ReadFile(file->getHandle(), ptr,
		static_cast<DWORD>(clippedSize), ///@todo �L���X�g��������ėǂ��́H�@�Ƃ͂����Ă��߂�l��32bit�����Ȃ��B
		&actualSize, NULL);

	file->curr_ += actualSize;

	return actualSize;
}

/**
 * Vorbisfile����̃R�[���o�b�N���󂯂ăt�@�C���|�C���^���ړ����܂��B
 */
int OggWin32File::callbackSeek(void *datasource, ogg_int64_t offset, int whence)
{
	OggWin32File *file = (OggWin32File *)datasource;

	// ��ʒu�����߂�B
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

	// �t�@�C���擪����V�[�N��ւ̃o�C�g�������߂�B
	LONGLONG to = base + offset;
	if(to < file->getBlockBegin()){
		return -1; //�͈͊O�B
	}
	if(to > file->getBlockEnd()){ //�I�[���܂܂Ȃ��B
		return -1; //�͈͊O�B
	}


	// �t�@�C���擪����V�[�N����B
	LARGE_INTEGER li;
	li.QuadPart = to;
	li.LowPart = ::SetFilePointer(file->getHandle(), li.LowPart, &li.HighPart, FILE_BEGIN);
	if(li.LowPart == INVALID_SET_FILE_POINTER && ::GetLastError() != NO_ERROR){
		return -1;
	}

	// ���݂̈ʒu���L�^����B
	file->curr_ = to - file->getBlockBegin();

	return 0;
}

/**
 * Vorbisfile����̃R�[���o�b�N���󂯂ăt�@�C�����N���[�Y���܂��Bov_clear���ɌĂ΂��悤�ł��B
 */
int OggWin32File::callbackClose(void *datasource)
{
	OggWin32File *file = (OggWin32File *)datasource;
	BOOL result = CloseHandle(file->getHandle());
	*file = OggWin32File();
	return result ? 0 : EOF;
}

/**
 * Vorbisfile����̃R�[���o�b�N���󂯂Č��݂̈ʒu��Ԃ��܂��B
 */
long OggWin32File::callbackTell(void *datasource)
{
	OggWin32File *file = (OggWin32File *)datasource;

	return static_cast<long>(file->curr_); ///@todo �L���X�g��������ėǂ��́H
}

/**
 * ov_open_callbacks�Ɉ����Ƃ��ēn���\���̂ł��B
 */
ov_callbacks OggWin32File::oggCallbacks =
{
	OggWin32File::callbackRead,
	OggWin32File::callbackSeek,
	OggWin32File::callbackClose,
	OggWin32File::callbackTell
};




/**
 * �I�u�W�F�N�g���\�z���܂��B
 */
OggFileReader::OggFileReader()
	: file_()
	, format_()
	, totalSamples_(0)
	, terminated_(false)
{
}

/**
 * �I�u�W�F�N�g����̂��܂��B
 */
OggFileReader::~OggFileReader()
{
	close();
}

/**
 * ogg�t�@�C�����t�@�C��������I�[�v�����܂��B
 */
bool OggFileReader::open(const PathChar *filename)
{
	if(isOpen()){
		return false;
	}
	//�t�@�C�����J��
	OggWin32File file;
	if(!file.open(filename)){
		return false;//�J���Ȃ�����
	}
	return open(file);
}

/**
 * �t�@�C�����ʎq����I�[�v�����܂��B
 * @param file ���ɊJ���Ă���t�@�C�����ʎq�B
 *        �����Ŏw�肵���t�@�C�����ʎq�̏��L���͌Ăяo�����u�Ԃɂ��̃I�u�W�F�N�g�Ɉڂ�܂��B
 *        �Ⴆ�΂��̊֐���false��Ԃ����ꍇ�A�t�@�C���͎����I�ɃN���[�Y����Ă��܂��B
 */
bool OggFileReader::open(const OggWin32File &file)
{
	file_ = file;
	//Ogg���J��
	OggVorbis_File vf;
	if(ov_open_callbacks(&file_, &vf, NULL, 0, OggWin32File::oggCallbacks) < 0){
		file_.close();
		return false;//�J���Ȃ�����
	}

	//�V�[�N�\�����ׂ�
	if(!ov_seekable(&vf)){
		ov_clear(&vf);
		return false;//seekable�ł͂Ȃ�
	}

	//����ȍ~���s���邱�Ƃ͖���
	vf_ = vf;

	//�����W
	const vorbis_info *info = ov_info(&vf, -1);
	const int sampleBits = 16;	//�f�R�[�h����ʎq���r�b�g����16�ŌŒ�
	const int blockAlign = info->channels * (sampleBits / 8);	//1�u���b�N�̃o�C�g��
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
 * �N���[�Y���܂��B
 */
void OggFileReader::close(void)
{
	if(file_.isOpen()){
		ov_clear(&vf_);//�R�[���o�b�N�o�R�Ńt�@�C���n���h�����N���[�Y�����͂��B
	}
}

/**
 * �I�[�v������Ă��邩�ǂ�����Ԃ��܂��B
 */
bool OggFileReader::isOpen(void) const
{
	return file_.isOpen();
}

bool OggFileReader::read(void *dst, std::size_t size, std::size_t *actualSize)
{
	if(!isOpen()){
		return false;//�J���Ă��Ȃ�
	}

	char *dstPtr = (char *)dst;
	std::size_t remain = size;
	while(remain > 0){
		const long actualRead
			= ov_read(&vf_, dstPtr, remain, 0, 2, 1, NULL);
		//�I�[�ɒB���������炩�̃G���[�����������Ȃ炨���܂�
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
		return false;//�J���Ă��Ȃ�
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
		return 0;//�J���Ă��Ȃ�
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

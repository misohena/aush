/**
 * @file
 * @brief OggFileReader�N���X�̃C���^�t�F�[�X���L�q���邽�߂̃t�@�C���ł��B
 * @author AKIYAMA Kouhei
 * @since 2002-09-28
 */
#ifndef AUSH_DSOUND_OGGFILEREADER_H_INCLUDED_20020928
#define AUSH_DSOUND_OGGFILEREADER_H_INCLUDED_20020928

#include <vorbis/vorbisfile.h>
#include "../util/PathString.h"
#include "WaveReader.h"

namespace aush{namespace dsound{

	/**
	 * ov_open_callbacks�p�̃t�@�C�����ʎq�N���X�ł��B
	 * ov_open_callbacks�ɂ�FILE*�̑���ɂ��̃I�u�W�F�N�g�ւ̃|�C���^�������n���܂��B
	 */
	class OggWin32File
	{
	private:
		typedef LONGLONG FileSizeType;
		HANDLE fileHandle_;
		FileSizeType offset_; ///< �u���b�N�擪�̃t�@�C���擪����̃I�t�Z�b�g�B
		FileSizeType size_; ///< �u���b�N�̃o�C�g���B

		FileSizeType curr_; ///< ���݂̓ǂݍ��݈ʒu�B�u���b�N�擪����̑��΃o�C�g���B


	public:
		/**
		 * �J���Ă��Ȃ��t�@�C�����ʎq���\�z���܂��B
		 */
		OggWin32File()
			: fileHandle_(INVALID_HANDLE_VALUE)
			, offset_(0)
			, size_(0)
			, curr_(0)
		{}

		/**
		 * fileHandle����offset��size�ŕ\�����̈���A�N�Z�X����t�@�C�����ʎq���\�z���܂��B
		 */
		OggWin32File(HANDLE fileHandle, FileSizeType offset, FileSizeType size)
			: fileHandle_(fileHandle)
			, offset_(offset)
			, size_(size)
			, curr_(0)
		{}

		inline HANDLE getHandle(void) const { return fileHandle_;}
		//inline FileSizeType getOffset(void) const { return offset_;}
		//inline FileSizeType getSize(void) const { return size_;}

		inline FileSizeType getBlockBegin(void) const { return offset_;}
		inline FileSizeType getBlockEnd(void) const { return offset_ + size_;}
		inline FileSizeType getCurrentInFile(void) const { return offset_ + curr_;}

		bool isOpen(void) const { return (fileHandle_ != INVALID_HANDLE_VALUE);}
		bool open(const PathChar *name);
		void close(void) { callbackClose(this);}

	public:
		static ov_callbacks oggCallbacks;
	private:
		static size_t callbackRead  (void *ptr, size_t size, size_t nmemb, void *datasource);
		static int    callbackSeek  (void *datasource, ogg_int64_t offset, int whence);
		static int    callbackClose (void *datasource);
		static long   callbackTell  (void *datasource);
	};


	/**
	 * ogg�t�@�C������wave�f�[�^�����o�����߂̃N���X�ł��B
	 */
	class OggFileReader
		: public WaveReader
	{
	private:
		OggWin32File file_;
		OggVorbis_File vf_;
		WaveFormat format_;
		WaveSize totalSamples_;

		bool terminated_;
	public:
		OggFileReader();
		virtual ~OggFileReader();
		bool open(const PathChar *filename);
		virtual void close(void);
		virtual bool isOpen(void) const;
		virtual bool read(void *dst, std::size_t size, std::size_t *actualSize);
		virtual bool seek(WaveSize sample);
		virtual WaveSize tell(void) const;
		virtual bool isTerminated(void) const;
		virtual WaveFormat getFormat(void) const;
		virtual WaveSize getTotal(void) const;
	
		bool open(const OggWin32File &file);
	};

}}//namespace aush::dsound
#endif

/**
 * @file
 * @brief DirectSound Object Wrapper.
 * @author AKIYAMA Kouhei
 * @since 2002-10-02
 */
#ifndef AUSH_DSOUND_DIRECTSOUND_H_INCLUDED_20021002
#define AUSH_DSOUND_DIRECTSOUND_H_INCLUDED_20021002

#ifndef DIRECTSOUND_VERSION
# define DIRECTSOUND_VERSION  0x0300 //←変更したいならコンパイルオプション/Dで指定すること
#endif
#include <dsound.h>

#include <atlbase.h>

namespace aush{namespace dsound{
	/**
	 * CLSID_DirectSoundを薄く包み込み、使いやすくするためのクラスです。
	 */
	class DirectSound  
	{
		CComPtr<IDirectSound> ds_;
		CComPtr<IDirectSoundBuffer> primaryBuffer_;

	public:
		DirectSound();
		virtual ~DirectSound();
		bool create(HWND hWnd);
		bool createPrimaryBuffer(void);
		bool setPrimaryBufferFormat(int channels, int samplePerSec, int bitPerSample);

		inline const CComPtr<IDirectSound> &getDirectSound(void) const { return ds_;}
		inline const CComPtr<IDirectSoundBuffer> &getPrimaryBuffer(void) const { return primaryBuffer_;}

		inline operator const CComPtr<IDirectSound>& () const { return ds_;}
	};

}}//namespace aush::dsound
#endif

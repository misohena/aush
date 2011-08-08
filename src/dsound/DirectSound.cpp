/**
 * @file
 * @brief DirectSound Object Wrapper.
 * @author AKIYAMA Kouhei
 * @since 2002-10-02
 */

#include <windows.h>
#include <tchar.h>

#include "DirectSound.h"


namespace aush{namespace dsound{

DirectSound::DirectSound()
{
}

DirectSound::~DirectSound()
{
}

/**
 * DirectSoundオブジェクトを作成します。
 * この関数が成功すると getDirectSound() は有効なポインタを返すようになります。
 */
bool DirectSound::create(HWND hWnd)
{
	// create CLSID_DirectSound
	CComPtr<IDirectSound> pds;
	HRESULT hr = CoCreateInstance(CLSID_DirectSound,
		NULL, 
		CLSCTX_INPROC_SERVER,
		IID_IDirectSound,
		(VOID**)&pds);
	if(FAILED(hr)){
		MessageBox(NULL, _T("Failed to create DirectSound object."), _T("error"), MB_OK);
		return false;
	}

	// initialize
	hr = pds->Initialize(NULL);
	if(FAILED(hr)){
		MessageBox(NULL, _T("Failed to initialize DirectSound object."), _T("error"), MB_OK);
		return false;
	}

	// set cooperative level.
	hr = pds->SetCooperativeLevel(hWnd, DSSCL_PRIORITY);
	if(FAILED(hr)){
		MessageBox(NULL, _T("Failed to SetCooperativeLevel."), _T("error"), MB_OK);
		return false;
	}

	// update variable.
	ds_ = pds;

	return true;
}


/**
 * プライマリバッファを作成します。
 * この関数が成功すると getPrimaryBuffer() は有効なポインタを返すようになります。
 * 既にプライマリバッファが作られているときは何もせずtrueを返します。
 */
bool DirectSound::createPrimaryBuffer(void)
{
	if(!getDirectSound()){
		return false;// no DirectSound object found.
	}
	if(primaryBuffer_){
		return true;// already created.
	}

	CComPtr<IDirectSoundBuffer> pPrimaryBuffer;
	DSBUFFERDESC dsbd = {};
	dsbd.dwSize        = sizeof(DSBUFFERDESC); 
	dsbd.dwFlags       = DSBCAPS_PRIMARYBUFFER; 
	dsbd.dwBufferBytes = 0; 
	dsbd.lpwfxFormat   = NULL; // primary buffer
	HRESULT hr = getDirectSound()->CreateSoundBuffer(&dsbd, &pPrimaryBuffer, NULL); 
	if(FAILED(hr)){
		MessageBox(NULL, _T("Failed to create a primary buffer."), _T("error"), MB_OK);
		return false;
	}

	//保存
	primaryBuffer_ = pPrimaryBuffer;
	return true;
}


/**
 * プライマリバッファのフォーマットを変更します。
 * DXヘルプのIDirectSound::SetFormatの注意事項を見てください。
 * @param channels チャネル数。通常1か2を指定する。
 * @param samplePerSec サンプルレート。
 * @param bitPerSample 量子化ビット数。通常8か16を指定する。
 */
bool DirectSound::setPrimaryBufferFormat(int channels, int samplePerSec, int bitPerSample)
{
	if(!createPrimaryBuffer()){
		return false;
	}

	WAVEFORMATEX wfx = {};
	wfx.wFormatTag = WAVE_FORMAT_PCM; 
	wfx.nChannels = channels;
	wfx.nSamplesPerSec = samplePerSec;
	wfx.wBitsPerSample = bitPerSample;
	wfx.nBlockAlign = wfx.nChannels * (wfx.wBitsPerSample / 8);
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
	HRESULT hr = getPrimaryBuffer()->SetFormat(&wfx);
	return SUCCEEDED(hr) ? true : false;
}

}}//namespace aush::dsound

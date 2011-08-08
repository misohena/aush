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
 * DirectSound�I�u�W�F�N�g���쐬���܂��B
 * ���̊֐������������ getDirectSound() �͗L���ȃ|�C���^��Ԃ��悤�ɂȂ�܂��B
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
 * �v���C�}���o�b�t�@���쐬���܂��B
 * ���̊֐������������ getPrimaryBuffer() �͗L���ȃ|�C���^��Ԃ��悤�ɂȂ�܂��B
 * ���Ƀv���C�}���o�b�t�@������Ă���Ƃ��͉�������true��Ԃ��܂��B
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

	//�ۑ�
	primaryBuffer_ = pPrimaryBuffer;
	return true;
}


/**
 * �v���C�}���o�b�t�@�̃t�H�[�}�b�g��ύX���܂��B
 * DX�w���v��IDirectSound::SetFormat�̒��ӎ��������Ă��������B
 * @param channels �`���l�����B�ʏ�1��2���w�肷��B
 * @param samplePerSec �T���v�����[�g�B
 * @param bitPerSample �ʎq���r�b�g���B�ʏ�8��16���w�肷��B
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

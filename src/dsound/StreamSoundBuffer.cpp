/**
 * @file
 * @brief StreamSoundBufferクラスの実装を記述するためのファイルです。
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
 * オブジェクトを構築します。
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
 * オブジェクトを解体します。
 */
StreamSoundBuffer::~StreamSoundBuffer()
{
	//演奏を止め、スレッドを止めます。
	{
		CriticalSectionLock lock(&cs_);

		if(streamBuffer_){
			streamBuffer_->Stop();
		}
		if(threadControlEvents_[EV_CLOSE]){
			SetEvent(threadControlEvents_[EV_CLOSE]);
		}
	}

	//スレッドが死ぬまで待ち、スレッドハンドルを解放します。
	if(threadHandle_ != ThreadPool::INVALID_THREAD_HANDLE){
		getThreadPool()->waitAndClose(threadHandle_);
	}

	//スレッドが死んだので、堂々と色々解放します。
	streamBuffer_.Release();
	for(int i = 0; i < EV_COUNT; ++i){
		if(threadControlEvents_[i]){
			CloseHandle(threadControlEvents_[i]);
		}
	}
}


/**
 * サウンドバッファを準備します。
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
				= DSBCAPS_CTRLPOSITIONNOTIFY	//再生位置通知
				| DSBCAPS_CTRLVOLUME			//フェード処理に使用
				| DSBCAPS_CTRLPAN
				| DSBCAPS_CTRLFREQUENCY
				| DSBCAPS_GLOBALFOCUS | DSBCAPS_LOCSOFTWARE;
			dsbd.dwBufferBytes = bufferSize;
			dsbd.lpwfxFormat = &sourceFormat;
		
			HRESULT hr = pds->CreateSoundBuffer(&dsbd, &buffer, NULL);
			if(FAILED(hr)){
				//Create()内 ストリーム用のサウンドバッファの作成に失敗しました。
				return CComPtr<IDirectSoundBuffer>();
			}
			return buffer;
		}

		static bool setNotifyPositions(IDirectSoundBuffer *buffer, HANDLE ev, DWORD blockSize)
		{
			// IDirectSoundNotifyインタフェースを取得する。
			CComPtr<IDirectSoundNotify> notify;
			HRESULT hr = buffer->QueryInterface(IID_IDirectSoundNotify, (LPVOID*)&notify);
			if(FAILED(hr)){
				return false; //IDirectSoundNotifyインタフェースの取得に失敗しました。
			}

			// 再生中イベント発生位置をセットする
			DSBPOSITIONNOTIFY notifyPos[NUM_BLOCKS];
			for(int i = 0; i < NUM_BLOCKS; i++){
				notifyPos[i].dwOffset = blockSize * (i+1);
				notifyPos[i].hEventNotify = ev;
			}
			notifyPos[NUM_BLOCKS-1].dwOffset--;

			// バッファに通知位置を設定する
			hr = notify->SetNotificationPositions(NUM_BLOCKS, notifyPos);
			if(FAILED(hr)){
				return false; //通知位置の設定に失敗しました。
			}
			return true;
		}
	};//struct Local

	if(!pds || !source){
		return false;
	}
	const WaveFormat sourceFormat = source->getFormat();
	
	// バッファサイズを計算する。ブロックサイズ(分割サイズ)も計算する。
	// ブロックサイズは1サンプルのバイト数の倍数でなければならない。
	const DWORD blockSize
		= (sourceFormat.samplesPerSec * BUFFER_LENGTH_SECONDS + NUM_BLOCKS - 1) / NUM_BLOCKS * sourceFormat.blockAlign;
	const DWORD bufferSize = blockSize * NUM_BLOCKS;

	// バッファを作成する。
	WAVEFORMATEX wavefmtex = {};
	wavefmtex.wFormatTag      = WAVE_FORMAT_PCM;
	wavefmtex.nChannels       = sourceFormat.channels;
	wavefmtex.nSamplesPerSec  = sourceFormat.samplesPerSec;
	wavefmtex.nAvgBytesPerSec = sourceFormat.channels * sourceFormat.bytesPerSample * sourceFormat.samplesPerSec;
	wavefmtex.nBlockAlign     = sourceFormat.channels * sourceFormat.bytesPerSample;
	wavefmtex.wBitsPerSample  = sourceFormat.bytesPerSample * 8;
	wavefmtex.cbSize          = 0;
	const CComPtr<IDirectSoundBuffer> buffer = Local::createDSB(pds, bufferSize, wavefmtex);

	// イベントを作る。
	const HANDLE evPass = CreateEvent(NULL, FALSE, FALSE, NULL);//←自動リセット, 初期状態FALSE
	const HANDLE evClose = CreateEvent(NULL, TRUE, FALSE, NULL);//←マニュアルリセット, 初期状態FALSE
	if(!evPass || !evClose){
		return false; //イベントオブジェクトが作れなかった！
	}
	
	// バッファに通知ポイントを設定する。
	if(!Local::setNotifyPositions(buffer, evPass, blockSize)){
		CloseHandle(evPass);
		CloseHandle(evClose);
		return false;
	}
	
	// 必要な情報をメンバ変数にセットする
	source_ = source;
	sourceFormat_ = sourceFormat;
	sourceSize_ = source->getTotal() * sourceFormat.blockAlign; //全データバイト数(byte)
	streamBuffer_ = buffer;
	bufferSize_ = bufferSize;
	blockSize_ = blockSize;
	threadControlEvents_[EV_PASS] = evPass;
	threadControlEvents_[EV_CLOSE] = evClose;
	
	//再生スレッドを作成する(↓マルチスレッドライブラリを指定してください。)
	threadHandle_ = getThreadPool()->run(
		&handleNotificationsStatic, (void*)this);
	if(threadHandle_ == ThreadPool::INVALID_THREAD_HANDLE){
		//Create()内 ストリーム再生用スレッドの作成に失敗しました。
		return false;
	}

	return true;
}

/**
 * ソースサイズが変化したときに呼び出されます。
 */
void StreamSoundBuffer::updateSourceSize(void)
{
	sourceSize_ = source_->getTotal() * sourceFormat_.blockAlign;
}


/**
 * ループ情報をデフォルトに設定します。
 */
void StreamSoundBuffer::setLoopDefault(void)
{
	// クリティカルセクション
	CriticalSectionLock lock(&cs_);

	// ループ情報設定
	loopState_ = LoopState(
		getDefaultLoopIn() * sourceFormat_.blockAlign, //ループインオフセット(byte)
		getDefaultLoopOut() * sourceFormat_.blockAlign, //ループアウトオフセット(byte)
		getDefaultLoopCount() );
}


/**
 * 現在の再生カーソルの位置から演奏を開始します。
 */
bool StreamSoundBuffer::play(void)
{
	// クリティカルセクション
	CriticalSectionLock lock(&cs_);

	// 再生中ならば何もしなくて良い。
	DWORD status = 0;
	if(FAILED(streamBuffer_->GetStatus(&status))){
		return false; //予測できないエラー。DSERR_INVALIDPARAMしか起こらないはず。
	}
	if(status & DSBSTATUS_PLAYING){
		return true; //すでに再生中。
	}

	// sourcePos_とストリームの読み込み位置が同じであることを保証する。
	assert(sourcePos_ == source_->tell() * sourceFormat_.blockAlign); // sourcePos_は基本的にblockSize_で増加し、blockSize_はsourceFormat_.blockAlignの整数倍のはずなので大丈夫なはず。
	if(sourcePos_ != source_->tell() * sourceFormat_.blockAlign){
		source_->seek(sourcePos_ / sourceFormat_.blockAlign);
	}
	// バッファはクリアしない。
	// clearBuffer();

	// ループ情報を設定する。(バッファにデータを埋める前に)
	setLoopDefault();
	//バッファをデータで埋める。
	fillBuffer();
	//再生を開始する
	if(FAILED(streamBuffer_->Play(0, 0, DSBPLAY_LOOPING))){
		return false;
	}

	return true;
}

/**
 * 関連づけられている音源ソースの先頭から演奏を開始します。
 */
bool StreamSoundBuffer::playFromBeginning(void)
{
	// クリティカルセクション
	CriticalSectionLock lock(&cs_);

	// 既に再生中なら停止させる
	streamBuffer_->Stop();

	// 頭出しする。ソース位置を変更してバッファをクリアする。
	seekPlayingPosition(0); //シークできなかった場合は仕方ないのでそのまま継続で再生を試みる。

	// ループ情報を設定する。(バッファにデータを埋める前に)
	setLoopDefault();
	//バッファをデータで埋める。
	fillBuffer();
	//再生を開始する
	if(FAILED(streamBuffer_->Play(0, 0, DSBPLAY_LOOPING))){
		return false;
	}
	
	return true;
}

/**
 * 演奏を停止します。
 */
bool StreamSoundBuffer::stop(void)
{
	// クリティカルセクション
	CriticalSectionLock lock(&cs_);

	// 再生中なら停止させる
	const HRESULT hr = streamBuffer_->Stop();

	return (SUCCEEDED(hr)) ? true : false;
}

/**
 * 現在演奏中であるかどうかを返します。
 */
bool StreamSoundBuffer::isPlaying(void)
{
	//クリティカルセクション
	CriticalSectionLock lock(&cs_);

	DWORD status = 0;
	const HRESULT hr = streamBuffer_->GetStatus(&status);

	return SUCCEEDED(hr)
		? (status & DSBSTATUS_PLAYING) ? true : false
		: false;
}

/**
 * 再生カーソルの位置を設定します。
 * @param pos 関連づけられている音源ソース(WaveReader)先頭からのサンプル数です。0のとき音源ソース先頭を表します。
 */
bool StreamSoundBuffer::setPositionSamples(WaveSize posSamples)
{
	//クリティカルセクション
	CriticalSectionLock lock(&cs_);

	// 演奏中ならば演奏を停止する。
	// 停止せずにシークすることもできるが、ブロック単位で書き換えているのでどうしても遅延が出てしまう。
	// 1ブロックBUFFER_LENGTH_SECONDS(3)/NUM_BLOCKS(8)=0.375秒くらい。
	// 書き込みカーソルの後に安全マージンを1ブロック分とっているので、シーク先の演奏が始まるまで400ms以上かかってしまう。
	// これなら止めてやり直した方が速そう。
	const bool playing = isPlaying();
	if(playing){
		streamBuffer_->Stop();
	}

	// シークしてバッファをクリアする。
	const bool succeeded = seekPlayingPosition(posSamples);

	// 必要なら演奏を再開する。
	if(playing){
		fillBuffer();
		streamBuffer_->Play(0, 0, DSBPLAY_LOOPING);
	}

	return succeeded;
}


/**
 * 再生カーソルの位置を返します。
 * @return 関連づけられている音源ソース(WaveReader)先頭からのサンプル数です。0のとき音源ソース先頭を表します。
 *         なんらかの事情で位置が取得できない場合は UNKNOWN_SAMPLES を返します。
 */
WaveSize StreamSoundBuffer::getPositionSamples(void)
{
	//クリティカルセクション
	CriticalSectionLock lock(&cs_);

	// ブロック情報が一つも無い場合は次回の読み取り位置であるsourcePos_を使う。
	if(blockCount_ == 0){
		return sourcePos_ / sourceFormat_.blockAlign;
	}

	// 再生カーソルを取得する。
	DWORD currentPlayPos = 0;
	const HRESULT hr = streamBuffer_->GetCurrentPosition(&currentPlayPos, NULL);
	if(!SUCCEEDED(hr)){
		return UNKNOWN_SAMPLES;
	}

	// 再生カーソルが指すブロックを取得する。
	const DWORD currentPlayIndex = currentPlayPos / blockSize_;
	const DWORD currentPlayOffset = currentPlayPos % blockSize_;

	const BlockInfo &binfo = blockInfo_[currentPlayIndex];
	if(!binfo.hasSourcePos()){
		return sourcePos_ / sourceFormat_.blockAlign; //追い越してしまった状況だと思われるので、sourcePos_を返す。
	}

	// ループを考慮してブロック内での正確なソース位置を割り出す。
	///@todo isTerminatedまでは考慮していない。その代わりsourceSize_を考慮しているけどそれで大丈夫？
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

		if(ls.loopIn_ >= ls.loopOut_){ //ループイン位置がループアウト位置と同じか後方の場合、ループ回数は無意味。
			if(ls.loopCount_ > 0){
				ls.loopCount_ = 0;
			}
			else{
				break;//このままじゃ無限ループだよ！！
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
 * ボリュームを変更します。
 * @param vol 0～-10000の間の値で、1/100db単位で指定します。
 */
bool StreamSoundBuffer::setVolume(int vol)
{
	//クリティカルセクション
	CriticalSectionLock lock(&cs_);

	const HRESULT hr = streamBuffer_->SetVolume(vol);

	return (SUCCEEDED(hr)) ? true : false;
}

/**
 * ボリュームを返します。
 * @return ボリュームを0～-10000の間の値で、1/100db単位で返します。
 */
bool StreamSoundBuffer::getVolume(int *vol)
{
	//クリティカルセクション
	CriticalSectionLock lock(&cs_);

	LONG lVol;
	const HRESULT hr = streamBuffer_->GetVolume(&lVol);
	if(vol){
		*vol = lVol;
	}

	return (SUCCEEDED(hr)) ? true : false;
}

/**
 * パン位置を設定します。
 */
bool StreamSoundBuffer::setPan(int pan)
{
	//クリティカルセクション
	CriticalSectionLock lock(&cs_);

	const HRESULT hr = streamBuffer_->SetPan(pan);

	return (SUCCEEDED(hr)) ? true : false;
}


/**
 * パン位置を返します。
 */
bool StreamSoundBuffer::getPan(int *pan)
{
	//クリティカルセクション
	CriticalSectionLock lock(&cs_);

	LONG lPan;
	const HRESULT hr = streamBuffer_->GetPan(&lPan);
	if(pan){
		*pan = lPan;
	}

	return (SUCCEEDED(hr)) ? true : false;
}

/**
 * 再生ピッチを設定します。
 * @param semitone1000 ピッチベンド値を半音=1000で指定します。
 */
bool StreamSoundBuffer::setPitch(int semitone1000) // 元周波数に対して2^(semitone1000/12000)倍。
{
	//クリティカルセクション
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
 * 再生ピッチを取得します。
 */
bool StreamSoundBuffer::getPitch(int *semitone1000)
{
	//クリティカルセクション
	CriticalSectionLock lock(&cs_);

	*semitone1000 = pitchBend_;

	return true;
}



/**
 * 再生用スレッドです。
 */
unsigned __stdcall StreamSoundBuffer::handleNotificationsStatic(void *pThis)
{
	((StreamSoundBuffer*)pThis)->handleNotifications();

	return 0;
}

/**
 * 再生通過イベントを監視し、適切な処理を行うための関数です。
 */
void StreamSoundBuffer::handleNotifications(void)
{
	DWORD event = 0;
	
	while((event = WaitForMultipleObjects(EV_COUNT, threadControlEvents_, FALSE, INFINITE))!= WAIT_FAILED){
		// 終了イベント
		if(event - WAIT_OBJECT_0 == EV_CLOSE){
			//手動リセットイベントオブジェクトを非シグナル状態にはしません。
			break;
		}
		// 再生通過イベント
		else if(event - WAIT_OBJECT_0 == EV_PASS){
			//自動リセットイベントオブジェクトなので、リセットしません。
			{
				CriticalSectionLock lock(&cs_);

				if(!streamBuffer_){
					break;//ストリームバッファが有効じゃない！！
				}
				if(!isPlaying()){
					continue;//演奏が止まっている！！
				}

				//まだブロックを満たす余地があるならブロックを満たす。
				fillBuffer();

				//有効なブロックが無く、終端に達しているなら再生停止。
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
 * streamBuffer_をデータで埋めます。
 *
 * 再生済みのブロックを破棄して、できるだけ多くのブロックへデータを詰めます。
 */
bool StreamSoundBuffer::fillBuffer(void) //lock cs_
{
	// 書き込み禁止範囲(再生カーソルと書き込みカーソルとの間)を求める。
	DWORD currentPlayPos = 0;
	DWORD currentWritePos = 0;
	const HRESULT hr = streamBuffer_->GetCurrentPosition(&currentPlayPos, &currentWritePos);
	if(FAILED(hr)){
		//fillBuffer()内 GetCurrentPositionに失敗しました。
		return false;
	}

	DWORD status = 0;
	streamBuffer_->GetStatus(&status);
	if(status & DSBSTATUS_PLAYING){
		// 再生中ならばcurrentWritePosの後に少し安全マージンを作る。
		const DWORD MARGIN = blockSize_;
		const DWORD readOnlyLength
			= currentWritePos >= currentPlayPos ? currentWritePos - currentPlayPos : currentWritePos + (bufferSize_ - currentPlayPos);
		if(readOnlyLength >= bufferSize_ - MARGIN){
			return false; // 書き込み禁止範囲が広すぎる。
		}
		currentWritePos += MARGIN;
		if(currentWritePos >= bufferSize_){
			currentWritePos -= bufferSize_;
		}
		assert(currentPlayPos != currentWritePos); ///@todo 要確認
	}
	else{
		assert(currentPlayPos == currentWritePos); ///@todo 要確認
	}

	// 書き込み禁止ブロック数を求める。
	const unsigned int currentPlayIndex = currentPlayPos / blockSize_; //floor
	const unsigned int currentWriteIndex = (currentWritePos + blockSize_ - 1)/blockSize_; //ceil
	const unsigned int readOnlyBlockCount //注意:値域は[0, NUM_BLOCKS+1]
		= (currentPlayPos < currentWritePos) ? currentWriteIndex - currentPlayIndex
		: (currentPlayPos > currentWritePos) ? currentWriteIndex + NUM_BLOCKS - currentPlayIndex
		: 0;
	if(readOnlyBlockCount >= NUM_BLOCKS){
		return false; //全てのブロックが書き込み禁止だと何もできない。
	} //これによって、readOnlyBlockCount>=NUM_BLOCKSのときのcurrentPlayPos == currentWritePosやcurrentPlayPos+1 == currentWritePosである状況を考慮しなくて良くなる。

	///@todo 再生カーソルがデータがないブロックを指している場合にどうするか。再生中には起こりえない？　停止中ならば一番最初の状況がそうだけど。

	// 再生カーソルの前にある再生済みのデータを破棄する。
	popBlocksBeforePlayCursor(currentPlayPos);

	// 空いているブロック数だけデータを詰める。
	while(blockCount_ < NUM_BLOCKS){
		const unsigned int newBlockIndex = getNewBlockIndex();

		const bool intersects
			= (readOnlyBlockCount <= 0) ? false
			: (currentPlayIndex < currentWriteIndex) 
				? (newBlockIndex >= currentPlayIndex && newBlockIndex < currentWriteIndex)
				: (newBlockIndex >= currentPlayIndex || newBlockIndex < currentWriteIndex);

		// 対象ブロック(newBlockIndex)が書き込み禁止な場合(カーソルに追いつかれた場合)の処理は、次のいずれかの方法があり得る。
		// - 処理を打ち切って書き込み禁止範囲が通り過ぎるのを待つ。バッファ一週分だけ時間の遅れを容認する。
		// - 対象ブロックにデータを書き込まずに有効なブロックとして無理矢理登録し、書き込み禁止範囲を出たところから続きを書き込む。時間の遅れを多少は容認する。
		// - 対象ブロックにデータを書き込まずに有効なブロックとして無理矢理登録し、その分だけソースもスキップし、書き込み禁止範囲を出たところから続きを書き込む。遅れた時間を取り戻そうとする。リズムは維持されやすい。
		// どのみちおかしくなるのは防げないので、できるだけ簡単な方法を採用しておく。
		if(intersects){
			break;
		}

		const BlockInfo newBlockInfo = fillBlock(newBlockIndex);
		pushBackBlock(newBlockInfo);
	}
	return true;
}


/**
 * 再生位置を変更します。
 *
 * できるだけ停止中に呼び出して下さい。停止中でないとその後のfillBuffer呼び出しがしばらくの間機能しないことがありえます。
 *
 * シークしてバッファをクリアします。
 * シークできなかった場合はおそらく何もしなかったことになります。
 * シークできてバッファをクリアできなかった場合は、バッファに貯まっていたデータの後にシーク後のデータが読み込まれることになります。
 */
bool StreamSoundBuffer::seekPlayingPosition(WaveSize posSamples) //cs_
{
	if(!seekSource(posSamples)){
		return false;
	}
	if(!clearBuffer()){
		//失敗してもシークが何秒か遅れるだけなので無視する。
	}
	return true;
}

/**
 * ソースの読み取り位置を変更します。
 */
bool StreamSoundBuffer::seekSource(WaveSize posSamples) //cs_
{
	if(posSamples > sourceSize_ / sourceFormat_.blockAlign){
		posSamples = sourceSize_ / sourceFormat_.blockAlign; // sourceSize_(バイト数)を越えないようにする。
	}

	if(!source_->seek(posSamples)){
		return false;
	}
	sourcePos_ = source_->tell() * sourceFormat_.blockAlign; //読み込み位置(バイト数)変数を更新する。
	return true;
}

/**
 * バッファをクリアし、再生カーソルをバッファの先頭へ移動します。
 *
 * できるだけ停止中に呼び出して下さい。停止中でないとその後のfillBuffer呼び出しがしばらくの間機能しないことがありえます。
 */
bool StreamSoundBuffer::clearBuffer(void) //cs_
{
	//streamBuffer_->Stop(); 前提条件

	if(FAILED(streamBuffer_->SetCurrentPosition(0))){
		return false;
	}
	clearBlocks();
	return true;
}

/**
 * 全ブロックを破棄します。
 */
void StreamSoundBuffer::clearBlocks(void) //lock cs_
{
	blockFrontIndex_ = 0;
	blockCount_ = 0;
	numBlocksValid_ = 0;
}

/**
 * 新しいブロックのインデックス番号を返します。
 */
unsigned int StreamSoundBuffer::getNewBlockIndex(void) //lock cs_
{
	assert(blockCount_ < NUM_BLOCKS);
	const unsigned int index = blockFrontIndex_ + blockCount_;
	return index < NUM_BLOCKS ? index : index - NUM_BLOCKS;
}

/**
 * 新しいブロックを追加します。
 * データはすでにgetNewBlockIndex()が返したブロックへ格納されているものとします。
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
 * 演奏し終わったブロックを破棄します。
 * 指定された再生カーソル位置より前のブロックを全て破棄します。
 */
void StreamSoundBuffer::popBlocksBeforePlayCursor(DWORD currentPlayPos) //lock cs_
{
	assert(currentPlayPos <= bufferSize_); //入力条件

	const DWORD currentPlayIndex = (currentPlayPos / blockSize_) % NUM_BLOCKS;

	// currentPlayIndexが範囲[blockFrontIndex_, blockFrontIndex_+blockCount)の中にある場合、currentPlayIndexの手前まで破棄する。
	// currentPlayIndexが範囲[blockFrontIndex_, blockFrontIndex_+blockCount)の外にある場合、全て破棄する。データ供給が追いつかず、再生カーソルが末尾ブロックを追い越してしまったと解釈する。
	while(blockCount_ > 0 && blockFrontIndex_ != currentPlayIndex){
		popFrontBlock();
	}
}

/**
 * 最も古いブロックを一つ破棄します。
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
 * 指定されたブロックをデータで埋めます。
 * この関数はクリティカルセクションcs_をロックしている間に呼び出して下さい。
 *
 * @return 書き込んだ結果のブロック情報を返します。
 */
StreamSoundBuffer::BlockInfo StreamSoundBuffer::fillBlock(unsigned int blockIndex) //lock cs_
{
	assert(streamBuffer_ && blockSize_ > 0); // 事前条件
	assert(blockIndex < NUM_BLOCKS); // 入力条件

	const DWORD bufferPos = blockIndex * blockSize_;

	//バッファをロックします。
	BYTE *writePtr1 = NULL;
	BYTE *writePtr2 = NULL;
	DWORD len1;
	DWORD len2;
	HRESULT hr = streamBuffer_->Lock(bufferPos, blockSize_
		, (LPVOID *)&writePtr1, &len1
		, (LPVOID *)&writePtr2, &len2, 0);
	if(hr == DSERR_BUFFERLOST){
		//FillBlock()内 Restoreを試みます。
		streamBuffer_->Restore();
		hr = streamBuffer_->Lock(bufferPos, blockSize_
			, (LPVOID *)&writePtr1, &len1
			, (LPVOID *)&writePtr2, &len2, 0);
	}
	if(FAILED(hr)){
		//FillBlock()内 ロックに失敗しました。
		return BlockInfo();
	}
	//writePtr2は必ずNULLのはず。でなければアンロックして異常終了。
	if(writePtr2){
		streamBuffer_->Unlock(writePtr1, len1, writePtr2, len2);
		//FillBlock()内 writePtr2!=NULL
		return BlockInfo();
	}
	//len1はblockSize_だけあるはず。でなければアンロックして異常終了。
	if(len1 < blockSize_){
		streamBuffer_->Unlock(writePtr1, len1, writePtr2, len2);
		//FillBlock()内 len1 < blockSize_
		return BlockInfo();
	}

	//ロックしたバッファの範囲に有効なデータを読み込みます。
	const WaveSize blockHeadSourcePos = sourcePos_;
	const LoopState blockHeadLoopState = loopState_;
	const DWORD writtenSize = readWave(writePtr1, blockSize_);

	//のこりを0で埋める
	if(writtenSize < blockSize_){
		FillMemory(writePtr1 + writtenSize, blockSize_ - writtenSize
			, (BYTE)(sourceFormat_.bytesPerSample == 1 ? 128 : 0));
	}
	
	//アンロック
	streamBuffer_->Unlock(writePtr1, len1, NULL, 0);

	return BlockInfo(blockHeadSourcePos, blockHeadLoopState, writtenSize);
}

/**
 * waveリーダーから再生用のデータを読み込み、指定したバッファに書き込みます。
 *
 * 演奏データが続く限りは出来るだけ bufferSize だけ書き込もうとします。ループも考慮します。
 * 演奏データ終端以降に無音データは書き込みません。
 *
 * この関数は waveリーダー、残りループ数、次読み込み位置を適切に書き換えます。
 *
 * @param bufferPtr 書き込み位置先頭。
 * @param bufferSize 最大書き込みバイト数。
 * @return 書き込んだバイト数。
 *         演奏データの終端に達したり、予期しない問題が発生した場合にbufferSizeより小さい値を返します。
 */
DWORD StreamSoundBuffer::readWave(BYTE *bufferPtr, DWORD bufferSize) //lock cs_
{
	DWORD writtenSize = 0;
	while(writtenSize < bufferSize){
		// ループ設定が有効ならばループ処理をします。
		// ループアウト位置を越えていたらループイン位置へシークし、ループカウンタを減らします。
		if(loopState_.loopCount_ != 0 && (sourcePos_ == loopState_.loopOut_ || source_->isTerminated())){
			if(loopState_.loopIn_ >= loopState_.loopOut_){ //ループイン位置がループアウト位置と同じか後方の場合、ループ回数は無意味。
				if(loopState_.loopCount_ > 0){
					loopState_.loopCount_ = 0;
				}
				else{
					break;//このままじゃ無限ループだよ！！
				}
			}
			if(!source_->seek(loopState_.loopIn_ / sourceFormat_.blockAlign)){
				break;//シークできない！！
			}
			sourcePos_ = loopState_.loopIn_;
			if(loopState_.loopCount_ > 0){
				loopState_.loopCount_--;
			}
		}
		//一度に読めるだけ読み込みます。
		const WaveSize limit = (loopState_.loopCount_ != 0) ? loopState_.loopOut_ : sourceSize_;
		const WaveSize limitSize = limit - sourcePos_; //現在の地点から(ループアウト地点またはデータ終端)までのサイズ
		const DWORD remainSize = bufferSize - writtenSize;
		const DWORD readSize = static_cast<DWORD>((limitSize < remainSize) ? limitSize : remainSize);
		if(readSize <= 0){
			break;//終端に達した模様
		}
		if(loopState_.loopCount_ == 0 && source_->isTerminated()){
			break;//予期せず終端に達した模様(おそらくloopState_.loopOut_やsourceSize_が実際の終端より後ろにあるか、開始時に先頭にシークできていなかったか)
		}
		std::size_t actualReadSize;
		if(!source_->read(bufferPtr + writtenSize, readSize, &actualReadSize)){
			break;//失敗！？
		}
		//
		writtenSize += actualReadSize;
		sourcePos_ += actualReadSize;
	}
	return writtenSize;
}

/**
 * スレッドプールを取得します。
 */
ThreadPool *StreamSoundBuffer::getThreadPool(void)
{
	static ThreadPool threadPool;
	return &threadPool;
}






}}//namespace aush::dsound

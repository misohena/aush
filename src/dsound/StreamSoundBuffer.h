/**
 * @file
 * @brief StreamSoundBufferクラスのインタフェースを記述するためのファイルです。
 * @author AKIYAMA Kouhei
 * @since 2002-09-26
 */
#ifndef AUSH_DSOUND_STREAMSOUNDBUFFER_H_INCLUDED_20020926
#define AUSH_DSOUND_STREAMSOUNDBUFFER_H_INCLUDED_20020926

#include "DirectSound.h"

#include <boost/shared_ptr.hpp>
#include "SoundBuffer.h"

#include "CriticalSection.h"

namespace aush{namespace dsound{class WaveReader;}}
namespace aush{namespace dsound{class ThreadPool;}}

namespace aush{namespace dsound{
	
	/**
	 * ストリーム再生方式のサウンドバッファクラスです。
	 * このクラスは割り当てられたWaveReaderインタフェースを持つオブジェクトから
	 * PCMデータを順次取りだしながら演奏します。
	 */
	class StreamSoundBuffer
		: public SoundBuffer
	{
		//定数
		static const int BUFFER_LENGTH_SECONDS = 3; //ストリームバッファ全体の秒数
		static const int NUM_BLOCKS = 8; //ブロック数(バッファを何分割するかの数)

		//属性、関連
		CriticalSection cs_;

		//↓createメソッドでセットされる変数
		boost::shared_ptr<WaveReader> source_;	// データソース。
		WaveFormat sourceFormat_; // 演奏するwaveのフォーマット。source_->getFormat()の値。
		WaveSize sourceSize_; // 全データバイト数(byte)
		CComPtr<IDirectSoundBuffer> streamBuffer_; // DirectSoundストリームバッファ
		DWORD bufferSize_;	// バッファのバイト数
		DWORD blockSize_;	// ブロックのバイト数(通知する間隔)
		int threadHandle_;	//演奏監視用スレッドのハンドル
		enum {
			EV_CLOSE,
			EV_PASS,
			EV_STOPPED, //停止中の時シグナル状態。再生中の時、非シグナル状態。
			EV_COUNT,
		};
		HANDLE threadControlEvents_[EV_COUNT];	//通知用イベントオブジェクトのハンドル

		//適宜変化する変数
		int pitchBend_;  //周波数ずらし

		//↓playメソッドでセットされる変数
		WaveSize sourcePos_;	//読み込みオフセット(byte)。基本的にsource_->tell()*bytesPerSampleと同じはず。

		struct LoopState
		{
			WaveSize loopIn_;     //ループインオフセット(byte)
			WaveSize loopOut_;    //ループアウトオフセット(byte)
			WaveSize loopCount_;  //残りループ数(-1のときは無限を意味する)

			LoopState() : loopIn_(0), loopOut_(0), loopCount_(0) {}
			LoopState(WaveSize loopIn, WaveSize loopOut, WaveSize loopCount)
					: loopIn_(loopIn), loopOut_(loopOut), loopCount_(loopCount)
			{}
		};
		LoopState loopState_; //ループ設定。

		unsigned int blockFrontIndex_; //先頭ブロックのインデックス。[0, NUM_BLOCKS)
		unsigned int blockCount_; //データが入っているブロックの数。[0, NUM_BLOCKS]
		unsigned int numBlocksValid_; //有効なブロックの個数。[0, NUM_BLOCKS]

		struct BlockInfo
		{
			static const WaveSize INVALID_POS = (WaveSize)-1;
			WaveSize sourcePos_; //ブロック先頭に対応するソース位置(サンプル数)。無効な場合sourceSize_以上の値。
			LoopState loopState_; //ブロックデータを埋めたときのループ設定。ブロック内オフセットからソース位置を求めるために必要。
			DWORD validBytes_; //ブロック先頭からの有効なデータのバイト数。

			BlockInfo()
				: sourcePos_(INVALID_POS), loopState_(), validBytes_(0) {}
			BlockInfo(WaveSize sourcePos, const LoopState &loopState, DWORD validBytes)
				: sourcePos_(sourcePos), loopState_(loopState), validBytes_(validBytes) {}

			bool isValidBlock() const { return validBytes_ > 0;}
			bool hasSourcePos() const { return sourcePos_ != INVALID_POS;}
		};
		BlockInfo blockInfo_[NUM_BLOCKS];



	public:
		StreamSoundBuffer();
		virtual ~StreamSoundBuffer();
		bool create(const CComPtr<IDirectSound> &pds, const boost::shared_ptr<WaveReader> &source);
		//SoundBufferインタフェース
		virtual bool play(void);
		virtual bool playFromBeginning(void);
		virtual bool stop(void);
		virtual bool isPlaying(void);
		virtual bool setPositionSamples(WaveSize pos);
		virtual WaveSize getPositionSamples(void); //UNKNOWN_SAMPLESのとき不明
		virtual bool setVolume(int vol);
		virtual bool getVolume(int *vol);
		virtual bool setPan(int pan);
		virtual bool getPan(int *pan);
		virtual bool setPitch(int semitone1000); // 元周波数に対して2^(semitone1000/12000)倍。
		virtual bool getPitch(int *semitone1000);

		void updateSourceSize(void);

		void waitForStop(void);
	private:
		WaveSize getDefaultLoopIn() const { return 0;}
		WaveSize getDefaultLoopOut() const { return source_ ? source_->getTotal() : 0;}
		int getDefaultLoopCount() const { return 0;}
		void setLoopDefault(void);

		bool seekPlayingPosition(WaveSize posSamples);
		bool clearBuffer(void);
		bool seekSource(WaveSize posSamples);

		static unsigned __stdcall handleNotificationsStatic(void *pThis);
		void handleNotifications(void);
		bool fillBuffer(void);
		void clearBlocks(void);
		unsigned int getNewBlockIndex(void);
		void pushBackBlock(const BlockInfo &info);
		void popBlocksBeforePlayCursor(DWORD currentPlayPos);
		void popFrontBlock(void);
		BlockInfo fillBlock(unsigned int blockIndex);
		DWORD readWave(BYTE *bufferPtr, DWORD bufferSize);

		static ThreadPool *getThreadPool(void);
	};

}}//namespace aush::dsound
#endif

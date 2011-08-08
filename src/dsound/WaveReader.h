/**
 * @file
 * @brief WaveReaderクラスを定義するためのファイルです。
 * @author AKIYAMA Kouhei
 * @since 2002-09-25
 */
#ifndef AUSH_DSOUND_WAVEREADER_H_INCLUDED_20020925
#define AUSH_DSOUND_WAVEREADER_H_INCLUDED_20020925

#include <boost/cstdint.hpp>

namespace aush{namespace dsound{
	typedef boost::uint64_t WaveSize;

	struct WaveFormat
	{
		unsigned int samplesPerSec; // samples per second
		unsigned int channels; // 1 or 2
		unsigned int bytesPerSample; //1=8bit or 2=16bit
		unsigned int blockAlign; // channels * bytesPerSample
		WaveFormat()
				: samplesPerSec(0)
				, channels(0)
				, bytesPerSample(0)
				, blockAlign(0)
		{}
		WaveFormat(unsigned int samplesPerSec,
				   unsigned int channels,
				   unsigned int bytesPerSample,
				   unsigned int blockAlign)
				: samplesPerSec(samplesPerSec)
				, channels(channels)
				, bytesPerSample(bytesPerSample)
				, blockAlign(blockAlign)
		{}
	};

	/**
	 * WAVEデータを読み込むためのインタフェースを規定するクラスです。
	 */
	class WaveReader
	{
	public:
		/**
		 * オブジェクトを解体します。必要ならCloseを呼び出します。
		 */
		virtual ~WaveReader(){}

		/**
		 * 全てのリソースを解放し、次にまたオープンできる状態にします。
		 */
		virtual void close(void) = 0;

		/**
		 * オープン状態かどうかを返します。
		 */
		virtual bool isOpen(void) const = 0;

		/**
		 * 指定したバイト数だけpcmデータを読み込みます。
		 * エラーが発生するか、終端に達するまでsizeで指定したバイト数だけ読み込めます。
		 */
		virtual bool read(void *dst, std::size_t size, std::size_t *actualSize) = 0;

		/**
		 * 次にReadを呼んだときに取り出せるpcmデータの位置をサンプル数で指定します。
		 */
		virtual bool seek(WaveSize sample) = 0;
	
		/**
		 * 次にReadを呼んだときに取り出せるpcmデータの位置をサンプル数で返します。
		 */
		virtual WaveSize tell(void) const = 0;
	
		/**
		 * リーダーが終端に達しているかどうかを返します。
		 * つまりSeekしない限りこれ以上Readを呼んでもデータが出てこないときtrueを返します。
		 */
		virtual bool isTerminated(void) const = 0;
	
	
		//情報取得操作
	

		/**
		 * Readで読み込めるデータの形式を返します。
		 */
		virtual WaveFormat getFormat(void) const = 0;
		
		/**
		 * 全体のサンプル数を返します。
		 */
		virtual WaveSize getTotal(void) const = 0;
	};

}}//namespace aush::dsound
#endif

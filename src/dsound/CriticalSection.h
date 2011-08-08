/**
 * @file
 * @brief クリティカル関連クラスのインタフェースを記述するためのファイルです。
 * @author AKIYAMA Kouhei
 * @since 2002-11-11
 */
#ifndef AUSH_DSOUND_CRITICALSECTION_H_INCLUDED_20051207
#define AUSH_DSOUND_CRITICALSECTION_H_INCLUDED_20051207

namespace aush{namespace dsound{

	/**
	 * クリティカルセクションクラス
	 */
	class CriticalSection
	{
		CRITICAL_SECTION cs_;
	public:
		CriticalSection()
		{
			::InitializeCriticalSection(&cs_);
		}
		~CriticalSection()
		{
			::DeleteCriticalSection(&cs_);
		}
		void enter(void)
		{
			::EnterCriticalSection(&cs_);
		}
		void leave(void)
		{
			::LeaveCriticalSection(&cs_);
		}
	};

	/**
	 * クリティカルセクションをロックするためのクラスです。
	 * このオブジェクトの生存期間中、
	 * 指定されたクリティカルセクションの所有権を握ります。
	 */
	class CriticalSectionLock
	{
		CriticalSection *cs_;
	public:
		/**
		 * クリティカルセクションをロックします。
		 * ロックはデストラクタで解放されます。
		 */
		CriticalSectionLock(CriticalSection *cs)
			: cs_(cs)
		{
			cs_->enter();
		}
		
		/**
		 * クリティカルセクションの所有権を解放します。
		 */
		~CriticalSectionLock()
		{
			cs_->leave();
		}
	};

}}//namespace aush::dsound
#endif

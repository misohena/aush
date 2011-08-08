/**
 * @file
 * @brief �N���e�B�J���֘A�N���X�̃C���^�t�F�[�X���L�q���邽�߂̃t�@�C���ł��B
 * @author AKIYAMA Kouhei
 * @since 2002-11-11
 */
#ifndef AUSH_DSOUND_CRITICALSECTION_H_INCLUDED_20051207
#define AUSH_DSOUND_CRITICALSECTION_H_INCLUDED_20051207

namespace aush{namespace dsound{

	/**
	 * �N���e�B�J���Z�N�V�����N���X
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
	 * �N���e�B�J���Z�N�V���������b�N���邽�߂̃N���X�ł��B
	 * ���̃I�u�W�F�N�g�̐������Ԓ��A
	 * �w�肳�ꂽ�N���e�B�J���Z�N�V�����̏��L��������܂��B
	 */
	class CriticalSectionLock
	{
		CriticalSection *cs_;
	public:
		/**
		 * �N���e�B�J���Z�N�V���������b�N���܂��B
		 * ���b�N�̓f�X�g���N�^�ŉ������܂��B
		 */
		CriticalSectionLock(CriticalSection *cs)
			: cs_(cs)
		{
			cs_->enter();
		}
		
		/**
		 * �N���e�B�J���Z�N�V�����̏��L����������܂��B
		 */
		~CriticalSectionLock()
		{
			cs_->leave();
		}
	};

}}//namespace aush::dsound
#endif

/**
 * @file
 * @brief �X���b�h�v�[���֘A�̃N���X���L�q��������t�@�C���ł��B
 * @author AKIYAMA Kouhei
 * @since 2004-06-16
 */

#include <process.h>
#include <windows.h>
#include <tchar.h>

#include "ThreadPool.h"

namespace aush{namespace dsound{

// ---------------------------------------------------------------------------
// PooledThread
// ---------------------------------------------------------------------------


/**
 * �X���b�h�v�[���p�̃X���b�h����쐬���܂��B
 */
PooledThread::PooledThread()
	: func_(NULL), arg_(NULL), threadHandle_(NULL)
{
	events_[EV_EXIT]    = CreateEvent(NULL, TRUE, FALSE, NULL);
	events_[EV_RUNNING] = CreateEvent(NULL, TRUE, FALSE, NULL);
	events_[EV_FREE]    = CreateEvent(NULL, TRUE, TRUE, NULL);

	unsigned threadId = -1;
	threadHandle_ = (HANDLE)_beginthreadex(
			NULL, 0, &staticThreadProc, (void*)this, 0, &threadId);
}

/**
 * �X���b�h�v�[���p�̃X���b�h�����̂��܂��B
 */
PooledThread::~PooledThread()
{
	SetEvent(events_[EV_EXIT]);
	if(threadHandle_){
		WaitForSingleObject(threadHandle_, 60 * 1000);
	}
	if(events_[EV_EXIT]){
		CloseHandle(events_[EV_EXIT]);
	}
	if(events_[EV_RUNNING]){
		CloseHandle(events_[EV_RUNNING]);
	}
	if(events_[EV_FREE]){
		CloseHandle(events_[EV_FREE]);
	}
	if(threadHandle_){
		CloseHandle(threadHandle_);
	}
}

/**
 * �X���b�h�Ɏw�肵���֐������s�����܂��B
 */
void PooledThread::run(FuncPtr func, void *arg)
{
	if(isRunning()){
		return;
	}
	func_ = func;
	arg_ = arg;
	ResetEvent(events_[EV_FREE]);
	SetEvent(events_[EV_RUNNING]);
}

/**
 * �X���b�h�����s���I�����A�ɂɂȂ�̂�҂��܂��B
 */
void PooledThread::waitFree()
{
	WaitForSingleObject(events_[EV_FREE], INFINITE);
}

/**
 * �d�������ǂ�����Ԃ��܂��B
 */
bool PooledThread::isRunning()
{
	return (WAIT_TIMEOUT != WaitForSingleObject(events_[EV_RUNNING], 0));
}

/**
 * ����\�ȗL���ȃX���b�h�ł��邩�ǂ�����Ԃ��܂��B
 * �X���b�h�̍쐬�Ɏ��s�����ꍇ��A�X���b�h�쐬��ɉ��炩�̗��R�ŃX���b�h���I�������ꍇ
 * false��Ԃ��܂��B
 */
bool PooledThread::isValid(void)
{
	return (threadHandle_ != NULL);
}

/**
 * �ÓI�ȃX���b�h�֐��ł��Bthis�|�C���^���������邽�߂����Ɏg���܂��B
 */
unsigned __stdcall PooledThread::staticThreadProc(void *arg)
{
	PooledThread *pThis = reinterpret_cast<PooledThread *>(arg);
	pThis->threadProc(arg);
	return 0;
}

/**
 * ���s�J�n�C�x���g���󂯂� func_ ���w���֐����Ăяo���X���b�h�֐��ł��B
 */
void PooledThread::threadProc(void *arg)
{
	for(;;){
		DWORD event = WaitForMultipleObjects(2, events_, FALSE, INFINITE);
		if(event == WAIT_OBJECT_0 + EV_EXIT){
			break;
		}
		else if(event == WAIT_OBJECT_0 + EV_RUNNING){
			(*func_)(arg_);

			func_ = NULL;
			arg_ = NULL;
			SetEvent(events_[EV_FREE]);
			ResetEvent(events_[EV_RUNNING]);
		}
	}
}



// ---------------------------------------------------------------------------
// ThreadPool
// ---------------------------------------------------------------------------

/**
 * �X���b�h�v�[�����\�z���܂��B
 */
ThreadPool::ThreadPool()
{
}

/**
 * �X���b�h�v�[������̂��܂��B
 */
ThreadPool::~ThreadPool()
{
}

/**
 * �ʃX���b�h�Ŏw�肵���֐������s���܂��B
 */
ThreadPool::ThreadHandle
ThreadPool::run(FuncPtr func, void *arg)
{
	ThreadHandle index = getFreeThread();
	if(index == INVALID_THREAD_HANDLE){
		return INVALID_THREAD_HANDLE;
	}

	threads_[index]->run(func, arg);
	return index;
}

/**
 * �w�肵���X���b�h�̎��s�I����҂��܂��B
 * �X���b�h�̓X���b�h�v�[���ɕԂ�A���̎d�������蓖�Ă���܂ŉɂɂȂ�܂��B
 */
void ThreadPool::waitAndClose(ThreadHandle index)
{
	if(index < 0 || static_cast<std::size_t>(index) >= threads_.size()){
		return;
	}
	threads_[index]->waitFree();
}

/**
 * �ɂȃX���b�h���擾���܂��B
 * �v�[�����Ă���X���b�h�̒��ɉɂȃX���b�h���Ȃ���΁A�V���ɃX���b�h�����܂��B
 */
ThreadPool::ThreadHandle ThreadPool::getFreeThread(void)
{
	//�ɂȃX���b�h���ė��p����
	for(int index = 0; index < (int)threads_.size(); index++){
		if( ! threads_[index]->isRunning()){
#ifdef _DEBUG
			TCHAR str[256];
			wsprintf(str, _T("�X���b�h %d ���g���񂵂܂����B"), index);
			OutputDebugString(str);
#endif
			return index;
		}
	}

	//�ɂȃX���b�h��������ΐV�������
	PooledThreadPtr t(new PooledThread());
	if( ! t->isValid()){
		return INVALID_THREAD_HANDLE;//�X���b�h�����Ȃ������c�c
	}
	threads_.push_back(t);

#ifdef _DEBUG
	{
		TCHAR str[256];
		wsprintf(str, _T("�V�����X���b�h %d �����܂����B"), (int)threads_.size()-1);
		OutputDebugString(str);
	}
#endif

	return static_cast<int>(threads_.size()) - 1;
}

}}//namespace aush::dsound

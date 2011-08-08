/**
 * @file
 * @brief スレッドプール関連のクラスを記述する実装ファイルです。
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
 * スレッドプール用のスレッドを一つ作成します。
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
 * スレッドプール用のスレッドを一つ解体します。
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
 * スレッドに指定した関数を実行させます。
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
 * スレッドが実行を終了し、暇になるのを待ちます。
 */
void PooledThread::waitFree()
{
	WaitForSingleObject(events_[EV_FREE], INFINITE);
}

/**
 * 仕事中かどうかを返します。
 */
bool PooledThread::isRunning()
{
	return (WAIT_TIMEOUT != WaitForSingleObject(events_[EV_RUNNING], 0));
}

/**
 * 動作可能な有効なスレッドであるかどうかを返します。
 * スレッドの作成に失敗した場合や、スレッド作成後に何らかの理由でスレッドが終了した場合
 * falseを返します。
 */
bool PooledThread::isValid(void)
{
	return (threadHandle_ != NULL);
}

/**
 * 静的なスレッド関数です。thisポインタを解決するためだけに使います。
 */
unsigned __stdcall PooledThread::staticThreadProc(void *arg)
{
	PooledThread *pThis = reinterpret_cast<PooledThread *>(arg);
	pThis->threadProc(arg);
	return 0;
}

/**
 * 実行開始イベントを受けて func_ が指す関数を呼び出すスレッド関数です。
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
 * スレッドプールを構築します。
 */
ThreadPool::ThreadPool()
{
}

/**
 * スレッドプールを解体します。
 */
ThreadPool::~ThreadPool()
{
}

/**
 * 別スレッドで指定した関数を実行します。
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
 * 指定したスレッドの実行終了を待ちます。
 * スレッドはスレッドプールに返り、次の仕事が割り当てられるまで暇になります。
 */
void ThreadPool::waitAndClose(ThreadHandle index)
{
	if(index < 0 || static_cast<std::size_t>(index) >= threads_.size()){
		return;
	}
	threads_[index]->waitFree();
}

/**
 * 暇なスレッドを取得します。
 * プールしているスレッドの中に暇なスレッドがなければ、新たにスレッドを作ります。
 */
ThreadPool::ThreadHandle ThreadPool::getFreeThread(void)
{
	//暇なスレッドを再利用する
	for(int index = 0; index < (int)threads_.size(); index++){
		if( ! threads_[index]->isRunning()){
#ifdef _DEBUG
			TCHAR str[256];
			wsprintf(str, _T("スレッド %d を使い回しました。"), index);
			OutputDebugString(str);
#endif
			return index;
		}
	}

	//暇なスレッドが無ければ新しく作る
	PooledThreadPtr t(new PooledThread());
	if( ! t->isValid()){
		return INVALID_THREAD_HANDLE;//スレッドが作れなかった……
	}
	threads_.push_back(t);

#ifdef _DEBUG
	{
		TCHAR str[256];
		wsprintf(str, _T("新しくスレッド %d を作りました。"), (int)threads_.size()-1);
		OutputDebugString(str);
	}
#endif

	return static_cast<int>(threads_.size()) - 1;
}

}}//namespace aush::dsound

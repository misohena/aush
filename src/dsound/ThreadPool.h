/**
 * @file
 * @brief スレッドプール関連のクラスを記述するヘッダーファイルです。
 * @author AKIYAMA Kouhei
 * @since 2004-06-16
 */
#ifndef AUSH_FRAMEWORK_THREADPOOL_H_INCLUDED_20040616
#define AUSH_FRAMEWORK_THREADPOOL_H_INCLUDED_20040616

#include <boost/shared_ptr.hpp>
#include <vector>

namespace aush{namespace dsound{

/**
 * ThreadPool クラスによってプールされているスレッドを表すクラスです。
 */
class PooledThread
{
public:
	typedef unsigned (__stdcall *FuncPtr)(void *);
private:
	enum{
		EV_EXIT = 0,
		EV_RUNNING = 1,
		EV_FREE = 2
	};
	FuncPtr func_;
	void *arg_;
	HANDLE threadHandle_;
	HANDLE events_[3];
public:
	PooledThread();
	~PooledThread();
	void run(FuncPtr func, void *arg);
	void waitFree(void);
	bool isRunning();
	bool isValid(void);
private:
	static unsigned __stdcall staticThreadProc(void *arg);
	void threadProc(void *arg);
};

/**
 * スレッドを再利用するためのクラスです。
 */
class ThreadPool
{
public://---型、定数---
	typedef unsigned (__stdcall *FuncPtr)(void *);
	typedef	int ThreadHandle;
	enum{ INVALID_THREAD_HANDLE = -1};

private://---属性、関連---
	typedef boost::shared_ptr<PooledThread> PooledThreadPtr;
	typedef std::vector<PooledThreadPtr> PooledThreadContainer;
	PooledThreadContainer threads_;

public://---公開操作---
	ThreadPool();
	~ThreadPool();
	ThreadHandle run(FuncPtr func, void *arg);
	void waitAndClose(ThreadHandle index);

private://---非公開操作---
	ThreadHandle getFreeThread(void);
};

}}//namespace aush::dsound
#endif

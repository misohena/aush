/**
 * @file
 * @brief �X���b�h�v�[���֘A�̃N���X���L�q����w�b�_�[�t�@�C���ł��B
 * @author AKIYAMA Kouhei
 * @since 2004-06-16
 */
#ifndef AUSH_FRAMEWORK_THREADPOOL_H_INCLUDED_20040616
#define AUSH_FRAMEWORK_THREADPOOL_H_INCLUDED_20040616

#include <boost/shared_ptr.hpp>
#include <vector>

namespace aush{namespace dsound{

/**
 * ThreadPool �N���X�ɂ���ăv�[������Ă���X���b�h��\���N���X�ł��B
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
 * �X���b�h���ė��p���邽�߂̃N���X�ł��B
 */
class ThreadPool
{
public://---�^�A�萔---
	typedef unsigned (__stdcall *FuncPtr)(void *);
	typedef	int ThreadHandle;
	enum{ INVALID_THREAD_HANDLE = -1};

private://---�����A�֘A---
	typedef boost::shared_ptr<PooledThread> PooledThreadPtr;
	typedef std::vector<PooledThreadPtr> PooledThreadContainer;
	PooledThreadContainer threads_;

public://---���J����---
	ThreadPool();
	~ThreadPool();
	ThreadHandle run(FuncPtr func, void *arg);
	void waitAndClose(ThreadHandle index);

private://---����J����---
	ThreadHandle getFreeThread(void);
};

}}//namespace aush::dsound
#endif

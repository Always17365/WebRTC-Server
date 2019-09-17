/*
 * MainLoop.cpp
 *
 *  Created on: 2019/07/30
 *      Author: max
 *		Email: Kingsleyyau@gmail.com
 */

#include "MainLoop.h"

#include <common/LogManager.h>

namespace mediaserver {

/***************************** 状态监视处理 **************************************/
class MainLoop;
class MainLoopRunnable : public KRunnable {
public:
	MainLoopRunnable(MainLoop *container) {
		mContainer = container;
	}
	virtual ~MainLoopRunnable() {
		mContainer = NULL;
	}
protected:
	void onRun() {
		mContainer->MainLoopHandle();
	}
private:
	MainLoop *mContainer;
};
/***************************** 状态监视处理 **************************************/

static MainLoop *gMainLoop = NULL;
MainLoop *MainLoop::GetMainLoop() {
	if( gMainLoop == NULL ) {
		gMainLoop = new MainLoop();
	}
	return gMainLoop;
}

MainLoop::MainLoop()
:mRunningMutex(KMutex::MutexType_Recursive) {
	// TODO Auto-generated constructor stub

	// 超时处理线程
	mpMainLoopRunnable = new MainLoopRunnable(this);

	mRunning = false;
}

MainLoop::~MainLoop() {
	// TODO Auto-generated destructor stub
	if ( mpMainLoopRunnable ) {
		delete mpMainLoopRunnable;
		mpMainLoopRunnable = NULL;
	}
}

bool MainLoop::Start() {
	bool bFlag = true;

	mRunningMutex.lock();
	if( mRunning ) {
		Stop();
	}
	mRunning = true;

	bFlag = (mMainLoopThread.Start(mpMainLoopRunnable, "MainLoop") != 0);
	if( bFlag ) {
		// 服务启动成功
		LogAync(
				LOG_WARNING,
				"MainLoop::Start( "
				"[OK] "
				")"
				);

	} else {
		// 服务启动失败
		LogAync(
				LOG_ERR_SYS,
				"MainLoop::Start( "
				"[Fail] "
				")"
				);
		Stop();
	}
	mRunningMutex.unlock();

	return bFlag;
}

void MainLoop::Stop() {
	LogAync(
			LOG_WARNING,
			"MainLoop::Stop("
			")"
			);

	mRunningMutex.lock();

	if( mRunning ) {
		mRunning = false;

		mMainLoopThread.Stop();

		mCallbackMap.Lock();
		for (MainLoopCallbackMap::iterator itr = mCallbackMap.Begin(); itr != mCallbackMap.End();) {
			MainLoopObj *obj = itr->second;
			delete obj;
			mCallbackMap.Erase(itr++);
		}
		mCallbackMap.Unlock();
	}

	mRunningMutex.unlock();

	LogAync(
			LOG_WARNING,
			"MainLoop::Stop( "
			"[OK] "
			")"
			);
}

void MainLoop::Call(int pid) {
	mCallbackMap.Lock();
	MainLoopCallbackMap::iterator itr = mCallbackMap.Find(pid);
	if ( itr != mCallbackMap.End() ) {
		MainLoopObj *obj = itr->second;
		obj->isExit = true;
//		MainLoopCallback *cb = obj->cb;
//		cb->OnChildExit(pid);
//		mCallbackMap.Erase(itr);
	}
	mCallbackMap.Unlock();
}

void MainLoop::StartWatchChild(int pid, MainLoopCallback *cb) {
	mCallbackMap.Lock();
	MainLoopObj *obj = new MainLoopObj(pid, cb);
	mCallbackMap.Insert(pid, obj);
    mCallbackMap.Unlock();
}

void MainLoop::StopWatchChild(int pid) {
	mCallbackMap.Lock();
	MainLoopCallbackMap::iterator itr = mCallbackMap.Find(pid);
	if ( itr != mCallbackMap.End() ) {
		MainLoopObj *obj = itr->second;
		mCallbackMap.Erase(itr);
		delete obj;
	}
    mCallbackMap.Unlock();
}

void MainLoop::MainLoopHandle() {
	LogAync(
			LOG_MSG,
			"MainLoop::MainLoopHandle( [Start] )"
			);

	while( mRunning ) {
		mCallbackMap.Lock();
		for (MainLoopCallbackMap::iterator itr = mCallbackMap.Begin(); itr != mCallbackMap.End();) {
			MainLoopObj *obj = itr->second;
			if ( obj->isExit ) {
				mCallbackMap.Erase(itr++);
				obj->cb->OnChildExit(obj->pid);
				delete obj;
			} else {
				itr++;
			}
		}
		mCallbackMap.Unlock();

		sleep(1);
	}

	LogAync(
			LOG_MSG,
			"MainLoop::MainLoopHandle( [Exit] )"
			);
}

} /* namespace mediaserver */
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <thread>
#include "Public/Semaphore.h"
#include "Public/OpenGLProgram.h"
#include "Public/CommonBase.h"
#include "Public/RHICommandList.h"
#include "Public/RHI.h"
#include <assert.h>
#include <queue>

GLFWwindow* GWindow = nullptr;

Semaphore mGameSemaphere(2);
Semaphore mRHISemaphere;
Semaphore mRHIInitSemaphere;

template<typename T>
class LockFreeQueue
{
public:
	LockFreeQueue()
		: mIn(0), mOut(0) {}

	void Push(T value)
	{
		int next = (mIn + 1) % MAX_SIZE;
		while (next == mOut)
		{
			mWriteSem.wait();
		}

		mContent[mIn] = value;
		mIn = next;

		mReadSem.notify();
	}

	T Pop()
	{
		while (mOut == mIn)
		{
			mReadSem.wait();
		}

		int temp = mOut;

		mOut = (mOut + 1) % MAX_SIZE;
		mWriteSem.notify();

		return mContent[temp];;
	}

private:
	const static int MAX_SIZE = 1024;
	T mContent[MAX_SIZE];

	int mIn;
	int mOut;
	Semaphore mReadSem;
	Semaphore mWriteSem;
};

class TaskBase
{
public:
	virtual void DoTask() = 0;
};

class RHIExecuteCommandList : public TaskBase
{
public:
	RHIExecuteCommandList(RHICommandList* list)
		: mCmdList(list) { assert(mCmdList != nullptr); }

	virtual void DoTask()
	{
		mCmdList->ExcuteInnter();

		delete mCmdList;
		mCmdList = nullptr;
	}

private:
	RHICommandList* mCmdList;
};

LockFreeQueue<TaskBase*> GRHITasks;

void InitWindow()
{
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	GWindow = glfwCreateWindow(1344, 768, "MultiThreadRender", NULL, NULL);
}

void InitDevice()
{
	glfwMakeContextCurrent(GWindow);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		glfwTerminate();
		return;
	}
}

void GameThread()
{
	// Wait RHIThread init.
	if (GbUseRHI)
	{
		mRHIInitSemaphere.wait();
	}
	else
	{
		InitDevice();
	}

	while (!glfwWindowShouldClose(GWindow))
	{
		if (GbUseRHI)
			mGameSemaphere.wait();

		// Render ...
		GetCommandList().RHIBeginDrawViewport();
		GetCommandList().RHIEndDrawViewport(GWindow);

		// ExcuteCommandlist.
		RHICommandList* swapCmdLists = new RHICommandList();
		GetCommandList().ExchangeCmdList(*swapCmdLists);

		GRHITasks.Push(new RHIExecuteCommandList(swapCmdLists));

		if (GbUseRHI)
			mRHISemaphere.notify();
	}
}

void RHIThread()
{
	InitWindow();
	if (GWindow == nullptr)
		glfwTerminate();

	InitDevice();
	mRHIInitSemaphere.notify();

	while (!glfwWindowShouldClose(GWindow))
	{
		mRHISemaphere.wait();

		// Really ExecuteCommands.
		TaskBase* task = GRHITasks.Pop();
		task->DoTask();
		delete task;

		mGameSemaphere.notify();
	}

	glfwTerminate();
}

int main()
{
	std::thread gameThread = std::thread(GameThread);
	std::thread rhiThread = std::thread(RHIThread);
	if ( GbUseRHI )
		rhiThread.join();

	gameThread.join();

	return 0;
}
#include <iostream>
#include <thread>
#include "Public/Semaphore.h"
#include "Public/CommonBase.h"
#include <assert.h>
#include <queue>
#include <functional>
#include "Public/RHIIncludes.h"
#include <atomic>

GLFWwindow* GWindow = nullptr;

Event* gRHIInitEvent;

VertexBuffer* gVertexBuffer;
IndexBuffer* gIndexBuffer;
float* gVertices = nullptr;
unsigned* gIndices = nullptr;
ELockMode gVertexLockMode = ELM_WRITE;

void InitDevice()
{
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	GWindow = glfwCreateWindow(1344, 768, "MultiThreadRender", NULL, NULL);

	glfwMakeContextCurrent(GWindow);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		glfwTerminate();
		return;
	}

	float vertices[] = {
	 0.5f,  0.5f, 0.0f,  // top right
	 0.5f, -0.5f, 0.0f,  // bottom right
	-0.5f, -0.5f, 0.0f,  // bottom left
	-0.5f,  0.5f, 0.0f   // top left 
	};

	gVertices = new float[sizeof(vertices) / sizeof(float)];
	memcpy(gVertices, vertices, sizeof(vertices));

	unsigned int indices[] = {  // note that we start from 0!
		0, 1, 3,  // first Triangle
		1, 2, 3   // second Triangle
	};

	gIndices = new unsigned int[sizeof(indices) / sizeof(unsigned int)];
	memcpy(gIndices, indices, sizeof(indices));

	gVertexBuffer = GetCommandList().CreateVertexBuffer(sizeof(vertices), ERU_Dynamic, gVertices);
	gIndexBuffer = GetCommandList().CreateIndexBuffer(sizeof(unsigned int), sizeof(indices), ERU_Static, gIndices);
}

void GameThread()
{
	if (GbUseRHI)
	{
		gRHIInitEvent->Wait();
		delete gRHIInitEvent;
		gRHIInitEvent = nullptr;
	}
	else
	{
		InitDevice();
	}

	int frameCount = 0;
	int changeFrame = 10;
	float offset = 0.0f;
	bool added = true;

	while (!glfwWindowShouldClose(GWindow))
	{
		//static int count = 1;
		//std::cout << "game thread count !!!!!!!!!!!!!!!!!" << count++ << std::endl;

		frameCount++;
		if (frameCount > 10)
		{
			frameCount = 0;

			if (added)
				offset += 0.05f;
			else
				offset -= 0.05f;

			if (offset > 0.5f)
				added = false;
			else if (offset < -0.5f)
				added = true;

			float* vbuffer = (float*)GetCommandList().LockVertexBuffer(gVertexBuffer, gVertexLockMode);

			if (gVertexLockMode != ELM_WRITE)
			{
				for (int i = 0; i < 12; i++)
					*vbuffer++ += added ? 0.05f : -0.05f;
			}
			else
			{
				float* buffer = gVertices;
				for (int i = 0; i < 12; i++)
					*buffer++ += added ? 0.05f : -0.05f;

				memcpy(vbuffer, gVertices, 48);
			}

			GetCommandList().UnlockVertexBuffer(gVertexBuffer);
		}

		// Render ...
		GetCommandList().RHIBeginDrawViewport();
		GetCommandList().SetStreamSource(gVertexBuffer);
		GetCommandList().DrawIndexedPrimitive(gIndexBuffer);
		GetCommandList().RHIEndDrawViewport(GWindow);

		GetCommandList().Flush();
	}
}

void RHIThread()
{
	InitDevice();
	gRHIInitEvent->Notify();

	GetRHICommandFence(0).Notify();
	GetRHICommandFence(1).Notify();

	while (!glfwWindowShouldClose(GWindow))
	{
		static int count = 1;

		// Really ExecuteCommands.
		TaskBase* task = GRHITasks.Pop();
		task->DoTask();
		delete task;
	}

	glfwTerminate();
}

void OnClose()
{
	delete[] gVertices;
	delete[] gIndices;
	// InRHI
	// delete gVertexBuffer;
	// InRHI.
	//delete gIndexBuffer;
}

const int MAX = 50000;

enum EThreadType
{
	THREAD_A = 1,
	THREAD_B = 2,
};

Event ThreadAStallEvent;
Event ThreadBStallEvent;
Event ThreadCStallEvent;

class TaskGraphBase
{
public:
	virtual void DoTask() = 0;
};

LockFreeQueue<TaskGraphBase*> gThreadATaskQueue;
LockFreeQueue<TaskGraphBase*> gThreadBTaskQueue;
LockFreeQueue<TaskGraphBase*> gThreadCTaskQueue;

int gTaskASum;
class TaskA : public TaskGraphBase
{
public:
	virtual void DoTask()
	{
		gTaskASum = 0;
		for (int i = 0; i < MAX; i++)
		{
			if (i % 2)
				gTaskASum += i;
		}
	}
};

int gTaskBSum;
class TaskB : public TaskGraphBase
{
public:
	virtual void DoTask()
	{
		gTaskBSum = 0;
		for (int i = 0; i < MAX; i++)
		{
			if ((i % 2) == 0)
				gTaskBSum += i;
		}
	}
};

int gTaskCSum;
class TaskC : public TaskGraphBase
{
public:
	virtual void DoTask()
	{
		gTaskCSum = gTaskASum + gTaskBSum;
	}
};

class TaskThreadANotify : public TaskGraphBase
{
public:
	virtual void DoTask()
	{
		ThreadAStallEvent.Notify();
	}
};

class TaskThreadBNotify : public TaskGraphBase
{
public:
	virtual void DoTask()
	{
		ThreadBStallEvent.Notify();
	}
};

class TaskThreadCNotify : public TaskGraphBase
{
public:
	virtual void DoTask()
	{
		ThreadCStallEvent.Notify();
	}
};

class TaskThreadAWait : public TaskGraphBase
{
public:
	virtual void DoTask()
	{
		ThreadAStallEvent.Wait();
	}
};

class TaskThreadBWait : public TaskGraphBase
{
public:
	virtual void DoTask()
	{
		ThreadBStallEvent.Wait();
	}
};

class TaskThreadCWait : public TaskGraphBase
{
public:
	virtual void DoTask()
	{
		ThreadCStallEvent.Wait();
	}
};

void CreateTaskToThreadA(TaskGraphBase* task)
{
	if (task == nullptr)
		return;

	gThreadATaskQueue.Push(task);
}

void CreateTaskToThreadB(TaskGraphBase* task)
{
	if (task == nullptr)
		return;

	gThreadBTaskQueue.Push(task);
}

void CreateTaskToThreadC(TaskGraphBase* task)
{
	if (task == nullptr)
		return;

	gThreadCTaskQueue.Push(task);
}

class BaseGraphTask
{

};

class GraphEvent
{
private:
	LockFreeQueue<BaseGraphTask*> mSubSequentList;
};

typedef std::vector<GraphEvent*> GraphEventArray;

//struct ThreadTaskQueue
//{
//	std::queue<BaseGraphTask*> mTaskQueue;
//};

class TaskThreadBase
{
public:
	void Run()
	{

	}

	void EnqueueTask(BaseGraphTask* task)
	{
		mTaskQueue.push(task);
	}

private:
	std::queue<BaseGraphTask*> mTaskQueue;
};

template<typename TTask>
class TGraphTask : public BaseGraphTask
{
public:
	class Constructor
	{

	};

	template<typename... T>
	static TGraphTask* CreateTask( const GraphEventArray* prerequisities, T&&... Args)
	{
		TGraphTask task* = new TGraphTask();

		// Setup prequisities.

		// Afterprequisities. 
	}

	void ExecuteTask()
	{
		// Dotask and ~TTtask();

		// Dispatchsubsequence.
	}

private:
	const GraphEventArray mPrerequisities;
};

void ThreadProc1()
{
	// Run.
	while (1)
	{
		CreateTaskToThreadB(new TaskA());
		CreateTaskToThreadB(new TaskB());

		CreateTaskToThreadB(new TaskThreadCNotify());

		// Wait A and B.
		CreateTaskToThreadC(new TaskThreadCWait());
		CreateTaskToThreadC(new TaskC());
		CreateTaskToThreadC(new TaskThreadANotify());

		ThreadAStallEvent.Wait();
		std::cout << "task a b c " << gTaskASum << "  " << gTaskBSum << "  " << gTaskCSum << std::endl;
	}
}

void ThreadProc2()
{
	while (1)
	{
		TaskGraphBase* task  = gThreadBTaskQueue.Pop();
		task->DoTask();
	}
}

void ThreadProc3()
{
	while (1)
	{
		TaskGraphBase* task = gThreadCTaskQueue.Pop();
		task->DoTask();
	}
}


int Share_Flag = 0;

void TestLock1()
{
	Share_Flag |= 0x1;
	//std::InterlockedCompareExchange(&Share_Flag, 0, 0);
}

void TestLock2()
{
	Share_Flag |= 0x10;
}

void TestLock3()
{
	Share_Flag = 0;
}

void TestLock4()
{
	std::cout << "test lock4 : " << Share_Flag << std::endl;
}

int main()
{
	//gRHIInitEvent = new Event();

	//std::thread gameThread = std::thread(GameThread);
	//if (GbUseRHI)
	//{
	//	std::thread rhiThread = std::thread(RHIThread);
	//	rhiThread.join();
	//}

	//gameThread.join();

	//OnClose();

	//std::thread thread1 = std::thread(ThreadProc1);
	//std::thread thread2 = std::thread(ThreadProc2);
	//std::thread thread3 = std::thread(ThreadProc3);
	//thread1.join();
	//thread2.join();
	//thread3.join();

	std::thread thread1 = std::thread(TestLock1);
	std::thread thread2 = std::thread(TestLock2);
	std::thread thread3 = std::thread(TestLock3);
	std::thread thread4 = std::thread(TestLock4);
	thread1.join();
	thread2.join();
	thread3.join();
	thread4.join();




	return 0;
} 
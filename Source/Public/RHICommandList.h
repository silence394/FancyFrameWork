#pragma once

#include <list>
#include "../Public/OpenGLRHI.h"
#include "RHI.h"
#include <functional>
#include "Semaphore.h"
#include <assert.h>
#include <iostream>

//#define ENABLE_LOG

#ifdef ENABLE_LOG
#define LOG_OUTPUT() \
std::cout << "FUNCTION :" << __FUNCTION__ << " line " << __LINE__ << " CUR THREAD : " << GetCurrentThreadId() << std::endl;
#else
#define LOG_OUTPUT()
#endif

struct RHICommandBase
{
	virtual void Execute() = 0;
};

enum EResouceUsage
{
	ERU_Static = 1,
	ERU_Dynamic = 2,
};

enum ELockMode
{
	ELM_READ = 1,
	ELM_WRITE = 2,
	ELM_READ_WRITE = 3,
};

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

#define CHECK_GL_ERROR() \
{\
	int error = glGetError(); \
	if (error!=0) \
		std::cout << error << "in line " << __LINE__ << std::endl; \
}

class TaskBase
{
public:
	virtual void DoTask() = 0;
};

class IndexBuffer;
class VertexBuffer;

struct DynamicRHIState
{
	VertexBuffer* mVertexBuffer;
	IndexBuffer* mIndexBuffer;
};

extern DynamicRHIState GDynamicRHIState;

class RHICommandList
{
public:
	void ExchangeCmdList(RHICommandList& cmdList);
	void Execute();
	void ExcuteInnter();

	void RHIBeginDrawViewport();
	void RHIEndDrawViewport(void* window);
	void RHIBeginRenderPass() {}
	void RHIEndRenderPass() {}

	VertexBuffer* CreateVertexBuffer(unsigned int size, EResouceUsage usage, void* data);
	void* LockVertexBuffer(VertexBuffer* vb, ELockMode lockMode);
	void UnlockVertexBuffer(VertexBuffer* vb);
	void SetStreamSource(VertexBuffer* vb);

	IndexBuffer* CreateIndexBuffer(unsigned int stride, unsigned int size, EResouceUsage usage, void* data);
	void* LockIndexBuffer(IndexBuffer* vb, ELockMode lockMode);
	void UnlockIndexBuffer(IndexBuffer* ib);
	void DrawIndexedPrimitive(IndexBuffer* ib);

	void AllocCommand(RHICommandBase* cmd)
	{
		if (cmd)
			mCommandLists.push_back(cmd);
	}

private:
	std::list<RHICommandBase*>	mCommandLists;
};

__forceinline RHICommandList& GetCommandList()
{
	static RHICommandList sRHICommandList;
	return sRHICommandList;
}

// TODO.
__forceinline HANDLE GetRHIEvent()
{
	static HANDLE sRHIEvent = nullptr;
	if (sRHIEvent == nullptr)
	{
		sRHIEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	}

	return sRHIEvent;
}

__forceinline HANDLE GetRHICommandFence(int index)
{
	static HANDLE sFence0 = nullptr;
	if (sFence0 == nullptr)
	{
		sFence0 = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		SetEvent(sFence0);
	}

	static HANDLE sFence1 = nullptr;
	if (sFence1 == nullptr)
	{
		sFence1 = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		SetEvent(sFence1);
	}

	if (index != 0 && index != 1)
		return nullptr;

	return index == 0 ? sFence0 : sFence1;
}

extern int GRHIFenceIndex;
struct RHICommandFence : public RHICommandBase
{
	int mFenceIndex;
	RHICommandFence(int index)
		: mFenceIndex(index)
	{
	}

	void Execute()
	{
		LOG_OUTPUT()
		SetEvent(GetRHICommandFence(mFenceIndex));
	}
};

class RHIExecuteCommandListTask : public TaskBase
{
public:
	RHIExecuteCommandListTask(RHICommandList* list)
		: mCmdList(list) {
		assert(mCmdList != nullptr);
	}

	virtual void DoTask()
	{
		mCmdList->ExcuteInnter();

		delete mCmdList;
		mCmdList = nullptr;
	}

private:
	RHICommandList* mCmdList;
};

extern LockFreeQueue<TaskBase*> GRHITasks;

__forceinline void AddTask(TaskBase* task)
{
	GRHITasks.Push(task);
}

struct RHIOpenGLCommand : public RHICommandBase
{
	RHIOpenGLCommand(std::function<void()> func)
		: mFunc(func) {}

	void Execute()
	{
		mFunc();
	}

	std::function<void()> mFunc;
};

class VertexBuffer
{
public:
	//friend class RHICommandCreateVertexBuffer;
	//struct RHICommandCreateVertexBuffer : public RHICommandBase
	//{
	//	RHICommandCreateVertexBuffer(void* data, VertexBuffer* vb)
	//		: mBuffer(vb)
	//	{
	//		mData = new char[mBuffer->mSize];
	//		// UE4����ط��ǳ������CommandList�õ���ͬһ���ڴ棬commandlist�ύ��reset��ʱ��һ��黹�ڴ�.
	//		memcpy(mData, data, mBuffer->mSize);
	//	}

	//	~RHICommandCreateVertexBuffer()
	//	{
	//		delete[] mData;
	//	}

	//	void Execute()
	//	{
	//		glGenBuffers(1, &mBuffer->mVBO);
	//		mBuffer->Bind();

	//		unsigned int usage = GL_STATIC_DRAW;
	//		if (mBuffer->mUsage == ERU_Dynamic)
	//			usage = GL_DYNAMIC_DRAW;

	//		glBufferData(GL_ARRAY_BUFFER, mBuffer->mSize, mData, mBuffer->mUsage);
	//	}

	//	void* mData;
	//	VertexBuffer* mBuffer;
	//};

	VertexBuffer(unsigned int size, EResouceUsage usage, void* data)
		: mSize(size), mVAO(0), mUsage(usage), mIsLocked(false), mLockedBuffer(nullptr)
	{
		CreateGLBuffer(data);
	}

	~VertexBuffer()
	{
		glDeleteBuffers(1, &mVBO);
	}

	void* Lock(ELockMode lockMode)
	{
		void* buffer = nullptr;

		mIsLocked = true;
		mLockMode = lockMode;

		unsigned int mode = GL_READ_WRITE;
		if (lockMode == ELM_READ)
			mode = GL_READ_ONLY;
		else if (lockMode == ELM_WRITE)
			mode = GL_WRITE_ONLY;

		if (!GbUseRHI)
		{
			Bind();

			buffer = glMapBuffer(GL_ARRAY_BUFFER, mode);
			CHECK_GL_ERROR();
		}
		else
		{
			if (mode != GL_WRITE_ONLY)
			{
				auto cmd = [&]()
				{
					Bind();

					buffer = glMapBuffer(GL_ARRAY_BUFFER, mode);
					CHECK_GL_ERROR();
					LOG_OUTPUT()
				};

				GetCommandList().AllocCommand(new RHIOpenGLCommand(cmd));

				// SetEvent.
				auto waitCmd = [=]()
				{
					LOG_OUTPUT()
					SetEvent(GetRHIEvent());
				};

				GetCommandList().AllocCommand(new RHIOpenGLCommand(waitCmd));

				RHICommandList* swapCmdLists = new RHICommandList();
				LOG_OUTPUT()
				GetCommandList().ExchangeCmdList(*swapCmdLists);
				// Add Task.
				AddTask(new RHIExecuteCommandListTask(swapCmdLists));
				LOG_OUTPUT()

				WaitForSingleObject(GetRHIEvent(), INFINITE);
			}
			else
			{
				buffer = new char[mSize];
				mLockedBuffer = buffer;

				return buffer;
			}
		}

		return buffer;
	}

	void Unlock()
	{
		assert(mIsLocked);
		if (!GbUseRHI)
		{
			Bind();
			glUnmapBuffer(GL_ARRAY_BUFFER);
		}
		else
		{
			if (mLockMode != ELM_WRITE)
			{
				auto cmd = [=]()
				{
					Bind();
					CHECK_GL_ERROR();
					glUnmapBuffer(GL_ARRAY_BUFFER);
					CHECK_GL_ERROR();
				};

				GetCommandList().AllocCommand(new RHIOpenGLCommand(cmd));
			}
			else
			{
				auto cmd = [=]()
				{
					unsigned int usage = GL_STATIC_DRAW;
					if (mUsage == ERU_Dynamic)
						usage = GL_DYNAMIC_DRAW;

					CHECK_GL_ERROR()
					glBufferData(GL_ARRAY_BUFFER, mSize, mLockedBuffer, usage);
					CHECK_GL_ERROR()

					delete[] mLockedBuffer;
					mLockedBuffer = nullptr;
				};

				GetCommandList().AllocCommand(new RHIOpenGLCommand(cmd));
			}
		}

		mIsLocked = false;
	}

	void Bind()
	{
		glBindBuffer(GL_ARRAY_BUFFER, mVBO);
	}

	unsigned int GetSize() const
	{
		return mSize;
	}

	void SetupVertexArray(IndexBuffer* ib);

private:
	void CreateGLBuffer(void* data)
	{
		if (!GbUseRHI)
		{
			glGenBuffers(1, &mVBO);
			Bind();

			unsigned int usage = GL_STATIC_DRAW;
			if (mUsage == ERU_Dynamic)
				usage = GL_DYNAMIC_DRAW;

			glBufferData(GL_ARRAY_BUFFER, mSize, data, usage);
		}
		else
		{
			char* buffer = new char[mSize];
			memcpy(buffer, data, mSize);
			auto cmd = [=]()
			{
				glGenBuffers(1, &mVBO);
				Bind();

				unsigned int usage = GL_STATIC_DRAW;
				if (mUsage == ERU_Dynamic)
					usage = GL_DYNAMIC_DRAW;

				glBufferData(GL_ARRAY_BUFFER, mSize, buffer, usage);
				CHECK_GL_ERROR()
				delete buffer;
			};

			GetCommandList().AllocCommand(new RHIOpenGLCommand(cmd));
		}
	}

private:
	unsigned int mSize;
	unsigned int mVBO;
	unsigned int mVAO;
	EResouceUsage mUsage;
	bool mIsLocked;
	ELockMode mLockMode;
	void* mLockedBuffer;
};

class IndexBuffer
{
public:
	IndexBuffer(unsigned int stride, unsigned int size, EResouceUsage usage, void* data)
		: mStride(stride), mSize(size), mUsage(usage)
	{
		CreateGLBuffer(data);
	}

	~IndexBuffer()
	{
		glDeleteBuffers(1, &mEBO);
	}

	void* Lock(ELockMode lockMode)
	{
		Bind();

		unsigned int mode = GL_READ_WRITE;
		if (lockMode == ELM_READ)
			mode = GL_READ_ONLY;
		else if (lockMode == ELM_WRITE)
			mode = GL_WRITE_ONLY;

		void* buffer = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, mode);

		return buffer;
	}

	void Unlock()
	{
		Bind();
		glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
	}

	void Bind()
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBO);
	}

	unsigned int GetStride()
	{
		return mStride;
	}

	unsigned int GetSize()
	{
		return mSize;
	}

private:
	void CreateGLBuffer(void* data)
	{
		if (!GbUseRHI)
		{
			glGenBuffers(1, &mEBO);
			Bind();

			unsigned int usage = GL_STATIC_DRAW;
			if (mUsage == ERU_Dynamic)
				usage = GL_DYNAMIC_DRAW;

			glBufferData(GL_ELEMENT_ARRAY_BUFFER, mSize, data, usage);
		}
		else
		{
			char* buffer = new char[mSize];
			memcpy(buffer, data, mSize);
			auto cmd = [=]()
			{
				glGenBuffers(1, &mEBO);
				Bind();

				unsigned int usage = GL_STATIC_DRAW;
				if (mUsage == ERU_Dynamic)
					usage = GL_DYNAMIC_DRAW;

				glBufferData(GL_ELEMENT_ARRAY_BUFFER, mSize, buffer, usage);
				CHECK_GL_ERROR()
				delete buffer;
			};

			GetCommandList().AllocCommand(new RHIOpenGLCommand(cmd));
		}
	}

private:
	unsigned int mEBO;
	unsigned int mStride;
	unsigned int mSize;
	EResouceUsage mUsage;
};
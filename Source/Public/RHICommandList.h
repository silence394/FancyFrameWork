#pragma once

#include <list>
#include "../Public/OpenGLRHI.h"
#include "RHI.h"
#include <functional>
#include "Semaphore.h"
#include <assert.h>
#include <iostream>

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
if (error) \
std::cout << error << "in line " << __LINE__ << std::endl; \
}

class TaskBase
{
public:
	virtual void DoTask() = 0;
};

class IndexBuffer;
class VertexBuffer;

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

	// Reset() 	MemManager.Flush();
	void AllocCommand(RHICommandBase* cmd)
	{
		if (cmd)
			mCommandLists.push_back(cmd);
	}

	void Flush();

private:
	std::list<RHICommandBase*>	mCommandLists;

	VertexBuffer* mCurVertexBuffer;
	IndexBuffer* mCurIndexBuffer;
};

__forceinline RHICommandList& GetCommandList()
{
	static RHICommandList sRHICommandList;
	return sRHICommandList;
}

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
	//		// UE4这个地方非常巧妙，跟CommandList用的是同一块内存，commandlist提交完reset的时候一起归还内存.
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
		: mSize(size), mVAO(0), mUsage(usage)
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

		if (!GbUseRHI)
		{
			Bind();

			unsigned int mode = GL_READ_WRITE;
			if (lockMode == ELM_READ)
				mode = GL_READ_ONLY;
			else if (lockMode == ELM_WRITE)
				mode = GL_WRITE_ONLY;

			buffer = glMapBuffer(GL_ARRAY_BUFFER, mode);
		}
		else
		{
			// readonly.
			auto cmd = [&]()
			{
				Bind();

				unsigned int mode = GL_READ_WRITE;
				if (lockMode == ELM_READ)
					mode = GL_READ_ONLY;
				else if (lockMode == ELM_WRITE)
					mode = GL_WRITE_ONLY;

				buffer = glMapBuffer(GL_ARRAY_BUFFER, mode);
			};

			GetCommandList().AllocCommand(new RHIOpenGLCommand(cmd));

			auto waitCmd = [=]()
			{
				mSem.notify();
			};

			GetCommandList().AllocCommand(new RHIOpenGLCommand(waitCmd));

			// 分发给RHI线程执行。
			RHICommandList* swapCmdLists = new RHICommandList();
			GetCommandList().ExchangeCmdList(*swapCmdLists);
			// Add Task.

			AddTask(new RHIExecuteCommandListTask(swapCmdLists));

			// Wait flush.
			mSem.wait();
		}


		return buffer;
	}

	void Unlock()
	{
		if (!GbUseRHI)
		{
			Bind();
			glUnmapBuffer(GL_ARRAY_BUFFER);
		}
		else
		{
			auto cmd = [=]()
			{
				Bind();
				glUnmapBuffer(GL_ARRAY_BUFFER);
			};

			GetCommandList().AllocCommand(new RHIOpenGLCommand(cmd));
		}
	}

	void Bind()
	{
		glBindBuffer(GL_ARRAY_BUFFER, mVBO);
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
				//delete buffer;
			};

			GetCommandList().AllocCommand(new RHIOpenGLCommand(cmd));
		}
	}

private:
	unsigned int mSize;
	unsigned int mVBO;
	unsigned int mVAO;
	EResouceUsage mUsage;

	Semaphore mSem;
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
				//delete buffer;
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

#pragma once

#include "RHI.h"
#include "OpenGLRHI.h"
#include "RHIResouce.h"
#include "Task.h"
#include "Event.h"
#include <list>
#include <functional>
#include <assert.h>


struct RHICommandBase
{
	virtual void Execute() = 0;
};

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

class IndexBuffer;
class VertexBuffer;

class RHICommandList
{
public:
	void ExchangeCmdList(RHICommandList& cmdList);
	void Execute();
	void ExecuteInner();

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

extern int GRHIFenceIndex;
struct RHICommandRHIThreadFence : public RHICommandBase
{
	int mFenceIndex;
	RHICommandRHIThreadFence(int index)
		: mFenceIndex(index)
	{
	}

	void Execute()
	{
		LOG_OUTPUT()
		GetRHICommandFence(mFenceIndex).Notify();
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
		mCmdList->ExecuteInner();

		delete mCmdList;
		mCmdList = nullptr;
	}

private:
	RHICommandList* mCmdList;
};
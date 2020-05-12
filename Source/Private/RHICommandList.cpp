#include "../Public/RHICommandList.h"
#include "../Public/RHI.h"
#include "../Public/CommonBase.h"

LockFreeQueue<TaskBase*> GRHITasks;
int GRHIFenceIndex = 0;
DynamicRHIState GDynamicRHIState;

void RHICommandList::ExchangeCmdList(RHICommandList& cmdList)
{
	std::swap(*this, cmdList);

	LOG_OUTPUT()
}

void RHICommandList::Execute()
{
	if (GbUseRHI)
	{
		std::list<RHICommandBase*> swaplists;

		std::swap(swaplists, mCommandLists);
	}
	else
	{
		ExcuteInnter();
	}
}

struct RHICommandBeginDrawViewport : public RHICommandBase
{
	Color clearColor;
	RHICommandBeginDrawViewport(const Color& color)
		: clearColor(color) {}

	virtual void Execute()
	{
		LOG_OUTPUT()
		glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);

		glClear(GL_COLOR_BUFFER_BIT);
	}
};

void RHICommandList::RHIBeginDrawViewport()
{
	Color clearColor(0.2f, 0.3f, 0.4f, 1.0f);
	if (!GbUseRHI)
	{
		glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	else
	{
		mCommandLists.push_back(new RHICommandBeginDrawViewport(clearColor));
	}
}

struct RHICommandEndDrawViewport : public RHICommandBase
{
	GLFWwindow* mWindow;
	RHICommandEndDrawViewport(GLFWwindow* window)
		: mWindow(window) {}

	virtual void Execute()
	{
		LOG_OUTPUT()
		glfwSwapBuffers(mWindow);
		CHECK_GL_ERROR()
		glfwPollEvents();
		CHECK_GL_ERROR()
	}
};

void RHICommandList::RHIEndDrawViewport(void* window)
{
	GLFWwindow* win = static_cast<GLFWwindow*> (window);
	if (!GbUseRHI)
	{
		glfwSwapBuffers(win);
		glfwPollEvents();
	}
	else
	{
		mCommandLists.push_back(new RHICommandEndDrawViewport(win));
		mCommandLists.push_back(new RHICommandFence(GRHIFenceIndex));
	}

	if (GbUseRHI)
	{
		int preIndex = 1 - GRHIFenceIndex;
		HANDLE fence = GetRHICommandFence(preIndex);
		LOG_OUTPUT()
		WaitForSingleObject(fence, INFINITE);
		LOG_OUTPUT()
		GRHIFenceIndex = preIndex;
	}
}

void RHICommandList::ExcuteInnter()
{
	for (std::list<RHICommandBase*>::iterator it = mCommandLists.begin(); it != mCommandLists.end(); it++)
		(*it)->Execute();

	for (std::list<RHICommandBase*>::iterator it = mCommandLists.begin(); it != mCommandLists.end(); it++)
		delete* it;

	mCommandLists.clear();
}

VertexBuffer* RHICommandList::CreateVertexBuffer(unsigned int size, EResouceUsage usage, void* data)
{
	return new VertexBuffer(size, usage, data);
}

void* RHICommandList::LockVertexBuffer(VertexBuffer* vb, ELockMode lockMode)
{
	return vb->Lock(lockMode);
}

void RHICommandList::UnlockVertexBuffer(VertexBuffer* vb)
{
	vb->Unlock();
}

//struct RHICommandSetStreamSource : public RHICommandBase
//{
//	RHICommandSetStreamSource(VertexBuffer* vb)
//		: mVertexBuffer(vb) { }
//
//	void Execute()
//	{
//		GDynamicRHIState.mVertexBuffer = mVertexBuffer;
//	}
//
//	VertexBuffer* mVertexBuffer;
//};

void RHICommandList::SetStreamSource(VertexBuffer* vb)
{
	// 它用的都是发起命令时候的环境中变量。
	auto Cmd = [=]()
	{
		LOG_OUTPUT()
		GDynamicRHIState.mVertexBuffer = vb;
	};
	if (!GbUseRHI)
		Cmd();
	else
		GetCommandList().AllocCommand(new RHIOpenGLCommand(Cmd));
}

IndexBuffer* RHICommandList::CreateIndexBuffer(unsigned int stride, unsigned int size, EResouceUsage usage, void* data)
{
	IndexBuffer* ib = new IndexBuffer(stride, size, usage, data);

	return ib;
}

void* RHICommandList::LockIndexBuffer(IndexBuffer* ib, ELockMode lockMode)
{
	return ib->Lock(lockMode);
}

void RHICommandList::UnlockIndexBuffer(IndexBuffer* ib)
{
	ib->Unlock();
}

void VertexBuffer::SetupVertexArray(IndexBuffer* ib)
{
	if (mVAO == 0)
	{
		glGenVertexArrays(1, &mVAO);
		glBindVertexArray(mVAO);

		Bind();
		ib->Bind();
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
	}
	else
	{
		glBindVertexArray(mVAO);
	}
}

void RHICommandList::DrawIndexedPrimitive(IndexBuffer* ib)
{
	auto Cmd = [=]()
	{
		GDynamicRHIState.mIndexBuffer = ib;
		LOG_OUTPUT()
		GDynamicRHIState.mVertexBuffer->SetupVertexArray(GDynamicRHIState.mIndexBuffer);
		CHECK_GL_ERROR()
		glDrawElements(GL_TRIANGLES, ib->GetSize() / ib->GetStride(), ib->GetStride() == sizeof(unsigned int) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, 0);
		CHECK_GL_ERROR()
	};

	if (!GbUseRHI)
		Cmd();
	else
		GetCommandList().AllocCommand(new RHIOpenGLCommand(Cmd));
}

void RHICommandList::Flush()
{

}
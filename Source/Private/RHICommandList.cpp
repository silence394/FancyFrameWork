#include "../Public/RHICommandList.h"
#include "../Public/RHI.h"
#include "../Public/CommonBase.h"
#include "../Public/VertexBuffer.h"
#include "../Public/IndexBuffer.h"

int GRHIFenceIndex = 0;

void RHICommandList::ExchangeCmdList(RHICommandList& cmdList)
{
	std::swap(*this, cmdList);
}

void RHICommandList::Flush()
{
	if (GbUseRHI)
	{
			// ExcuteCommandlist.
		RHICommandList* swapCmdLists = new RHICommandList();
		GetCommandList().ExchangeCmdList(*swapCmdLists);
		AddTask(new RHIExecuteCommandListTask(swapCmdLists));
	}
	else
	{
		ExecuteInner();
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
		mCommandLists.push_back(new RHICommandRHIThreadFence(GRHIFenceIndex));
	}

	if (GbUseRHI)
	{
		int preIndex = 1 - GRHIFenceIndex;
		LOG_OUTPUT()
		GetRHICommandFence(preIndex).Wait();
		LOG_OUTPUT()
		GRHIFenceIndex = preIndex;
	}
}

void RHICommandList::ExecuteInner()
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
	// Dynamic的buffer Map会不会影响速度，
	//if (FOpenGL::SupportsBufferStorage() && OpenGLConsoleVariables::bUseStagingBuffer)
	//{
	//	if (PoolVB == 0)
	//	{
	//		FOpenGL::GenBuffers(1, &PoolVB);
	//		glBindBuffer(GL_COPY_READ_BUFFER, PoolVB);
	//		FOpenGL::BufferStorage(GL_COPY_READ_BUFFER, PerFrameMax * 4, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
	//		PoolPointer = (uint8*)FOpenGL::MapBufferRange(GL_COPY_READ_BUFFER, 0, PerFrameMax * 4, FOpenGL::EResourceLockMode::RLM_WriteOnlyPersistent);

	//		FreeSpace = PerFrameMax * 4;
	//	}
	//}
	// 如果是READONLY，肯定要调用Map.
	return vb->Lock(lockMode);
}

void RHICommandList::UnlockVertexBuffer(VertexBuffer* vb)
{
	vb->Unlock();
}

void RHICommandList::SetStreamSource(VertexBuffer* vb)
{
	// 它用的都是发起命令时候的环境中变量, a exchageComandList(b)里交换了cmdlist，但是RHI执行lamda的时候用的还是 a环境里的东西。
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
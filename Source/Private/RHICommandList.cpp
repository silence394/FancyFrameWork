#include "../Public/RHICommandList.h"
#include "../Public/RHI.h"
#include "../Public/CommonBase.h"

void RHICommandList::ExchangeCmdList(RHICommandList& cmdList)
{
	std::swap(*this, cmdList);
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
		glfwSwapBuffers(mWindow);
		glfwPollEvents();
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

void RHICommandList::SetStreamSource(VertexBuffer* vb)
{
	mCurVertexBuffer = vb;
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
	mCurIndexBuffer = ib;
	mCurVertexBuffer->SetupVertexArray(mCurIndexBuffer);

	glDrawElements(GL_TRIANGLES, ib->GetSize() / ib->GetStride(), ib->GetStride() == sizeof(unsigned int) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, 0);
}
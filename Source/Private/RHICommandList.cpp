#include "../Public/RHICommandList.h"
#include "../Public/RHI.h"
#include "../Public/OpenGLRHI.h"
#include "../Public/CommonBase.h"

RHICommandList& GetCommandList()
{
static RHICommandList sRHICommandList;
return sRHICommandList;
}

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
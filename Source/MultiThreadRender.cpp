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
#include <functional>

GLFWwindow* GWindow = nullptr;

Semaphore mGameSemaphere(2);
Semaphore mRHISemaphere;
Semaphore mRHIInitSemaphere;

void InitWindow()
{
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	GWindow = glfwCreateWindow(1344, 768, "MultiThreadRender", NULL, NULL);
}

VertexBuffer* gVertexBuffer;
IndexBuffer* gIndexBuffer;


unsigned int VBO, VAO, EBO;

void InitDevice()
{
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
	unsigned int indices[] = {  // note that we start from 0!
		0, 1, 3,  // first Triangle
		1, 2, 3   // second Triangle
	};

	gVertexBuffer = GetCommandList().CreateVertexBuffer(sizeof(vertices), ERU_Dynamic, vertices);
	gIndexBuffer = GetCommandList().CreateIndexBuffer(sizeof(unsigned int), sizeof(indices), ERU_Static, indices);
}

void GameThread()
{
	if (GbUseRHI)
	{
		mRHIInitSemaphere.wait();
	}
	else
	{
		InitWindow();
		if (GWindow == nullptr)
			glfwTerminate();

		InitDevice();
	}

	int frameCount = 0;
	int changeFrame = 10;
	float offset = 0.0f;
	bool added = true;

	while (!glfwWindowShouldClose(GWindow))
	{

		static int count = 1;
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

			float* vbuffer = (float*)GetCommandList().LockVertexBuffer(gVertexBuffer, ELM_WRITE);

			for (int i = 0; i < 12; i++)
				*vbuffer++ += added ? 0.05f : -0.05f;

			GetCommandList().UnlockVertexBuffer(gVertexBuffer);
		}

		// Render ...
		GetCommandList().RHIBeginDrawViewport();
		GetCommandList().SetStreamSource(gVertexBuffer);
		GetCommandList().DrawIndexedPrimitive(gIndexBuffer);
		GetCommandList().RHIEndDrawViewport(GWindow);

		if (GbUseRHI)
		{
			// ExcuteCommandlist.
			RHICommandList* swapCmdLists = new RHICommandList();
			LOG_OUTPUT()
			GetCommandList().ExchangeCmdList(*swapCmdLists);
			AddTask(new RHIExecuteCommandListTask(swapCmdLists));
		}
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
		static int count = 1;
		//std::cout << "rhi count ~~~~~~~~~$$$$$$$$$$$$$$$$$$$" << count++ << std::endl;

		// Really ExecuteCommands.
		TaskBase* task = GRHITasks.Pop();
		task->DoTask();
		delete task;
	}

	glfwTerminate();
}

int main()
{
	std::thread gameThread = std::thread(GameThread);
	if (GbUseRHI)
	{
		std::thread rhiThread = std::thread(RHIThread);
		rhiThread.join();
	}

	gameThread.join();

	return 0;
}
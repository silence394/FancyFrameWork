#pragma once

#include "OpenGLRHI.h"
#include "RHI.h"
#include <assert.h>
#include "RHICommandList.h"
#include <iostream>
#include "RHIResouce.h"

class IndexBuffer;

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
					GetRHIEvent().Notify();
				};

				GetCommandList().AllocCommand(new RHIOpenGLCommand(waitCmd));

				RHICommandList* swapCmdLists = new RHICommandList();
				LOG_OUTPUT()
				GetCommandList().ExchangeCmdList(*swapCmdLists);
				// Add Task.
				AddTask(new RHIExecuteCommandListTask(swapCmdLists));
				LOG_OUTPUT()

				GetRHIEvent().Wait();
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

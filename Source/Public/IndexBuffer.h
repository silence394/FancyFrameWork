#pragma once

#include "OpenGLRHI.h"
#include "RHI.h"
#include <assert.h>
#include "RHICommandList.h"
#include <iostream>
#include "RHIResouce.h"

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
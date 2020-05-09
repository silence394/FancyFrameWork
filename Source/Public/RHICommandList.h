#pragma once

#include <list>
#include "../Public/OpenGLRHI.h"

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

class IndexBuffer;
class VertexBuffer
{
public:
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
		Bind();

		unsigned int mode = GL_READ_WRITE;
		if (lockMode == ELM_READ)
			mode = GL_READ_ONLY;
		else if (lockMode == ELM_WRITE)
			mode = GL_WRITE_ONLY;

		void* buffer = glMapBuffer(GL_ARRAY_BUFFER, mode);

		return buffer;
	}

	void Unlock()
	{
		Bind();
		glUnmapBuffer(GL_ARRAY_BUFFER);
	}

	void Bind()
	{
		glBindBuffer(GL_ARRAY_BUFFER, mVBO);
	}

	void SetupVertexArray(IndexBuffer* ib);

private:
	void CreateGLBuffer(void* data)
	{
		glGenBuffers(1, &mVBO);
		Bind();

		unsigned int usage = GL_STATIC_DRAW;
		if (mUsage == ERU_Dynamic)
			usage = GL_DYNAMIC_DRAW;

		glBufferData(GL_ARRAY_BUFFER, mSize, data, usage);
	}

private:
	unsigned int mSize;
	unsigned int mVBO;
	unsigned int mVAO;
	EResouceUsage mUsage;
};


class IndexBuffer
{
public:
	IndexBuffer(unsigned int stride, unsigned int size,  EResouceUsage usage, void* data) 
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
		glGenBuffers(1, &mEBO);
		Bind();

		unsigned int usage = GL_STATIC_DRAW;
		if (mUsage == ERU_Dynamic)
			usage = GL_DYNAMIC_DRAW;

		glBufferData(GL_ELEMENT_ARRAY_BUFFER, mSize, data, usage);
	}

private:
	unsigned int mEBO;
	unsigned int mStride;
	unsigned int mSize;
	EResouceUsage mUsage;
};

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
#pragma once

extern bool GbUseRHI;

//#define ENABLE_LOG
#ifdef ENABLE_LOG
#define LOG_OUTPUT() \
std::cout << "FUNCTION :" << __FUNCTION__ << " line " << __LINE__ << " CUR THREAD : " << GetCurrentThreadId() << std::endl;
#else
#define LOG_OUTPUT() ;
#endif

struct DynamicRHIState
{
	class VertexBuffer* mVertexBuffer;
	class IndexBuffer* mIndexBuffer;
};

extern DynamicRHIState GDynamicRHIState;

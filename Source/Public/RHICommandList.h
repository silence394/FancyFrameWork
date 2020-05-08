#pragma once

#include <list>

struct RHICommandBase
{
	virtual void Execute() = 0;
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

private:
	std::list<RHICommandBase*>	mCommandLists;
};

RHICommandList& GetCommandList();
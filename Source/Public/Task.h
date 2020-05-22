#pragma once

#include "LockFreeQueue.h"

class TaskBase
{
public:
	virtual void DoTask() = 0;
};

extern LockFreeQueue<TaskBase*> GRHITasks;

__forceinline void AddTask(TaskBase* task)
{
	GRHITasks.Push(task);
}
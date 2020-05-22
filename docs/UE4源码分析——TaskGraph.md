
```
FScopedEvent::FScopedEvent()
	: Event(TLazySingleton<FEventPool<EEventPoolTypes::AutoReset>>::Get().GetEventFromPool())
{ }


FEvent* FGenericPlatformProcess::GetSynchEventFromPool(bool bIsManualReset)

class FEventPool	FEvent* GetEventFromPool()
```

主要是想解决，任务之间同步的问题。

首先是lockBuffer(READONLY)的同步问题。
```
case EImmediateFlushType::FlushRHIThread:
    {
        CSV_SCOPED_TIMING_STAT(RHITFlushes, FlushRHIThreadTotal);
        if (HasCommands())
        {
            GRHICommandList.ExecuteList(*this);
        }
        WaitForDispatch();
        if (IsRunningRHIInSeparateThread())
        {
            WaitForRHIThreadTasks();
        }
        WaitForTasks(true); // these are already done, but this resets the outstanding array
    }
```

交给CPU执行了命令。
```
if (RHIThreadTask.GetReference())
{
    Prereq.Add(RHIThreadTask);
}
PrevRHIThreadTask = RHIThreadTask;
RHIThreadTask = TGraphTask<FExecuteRHIThreadTask>::CreateTask(&Prereq, ENamedThreads::GetRenderThread()).ConstructAndDispatchWhenReady(SwapCmdList);
```

之后调用WaitForRHIThreadTasks()进行wait.
```
FTaskGraphInterface::Get().WaitUntilTaskCompletes(RHIThreadTask, RenderThread_Local); 
```


WaitUntilTaskCompletes 创建了 FReturnGraphTask
```
class FReturnGraphTask
{
public:

	/** 
	 *	Constructor
	 *	@param InThreadToReturnFrom; named thread to cause to return
	**/
	FReturnGraphTask(ENamedThreads::Type InThreadToReturnFrom)
		: ThreadToReturnFrom(InThreadToReturnFrom)
	{
		checkThreadGraph(ThreadToReturnFrom != ENamedThreads::AnyThread); // doesn't make any sense to return from any thread
	}

	FORCEINLINE TStatId GetStatId() const
	{
		return GET_STATID(STAT_FReturnGraphTask);
	}

	/** 
	 *	Retrieve the thread that this task wants to run on.
	 *	@return the thread that this task should run on.
	 **/
	ENamedThreads::Type GetDesiredThread()
	{
		return ThreadToReturnFrom;
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	/** 
	 *	Actually execute the task.
	 *	@param	CurrentThread; the thread we are running on
	 *	@param	MyCompletionGraphEvent; my completion event. Not always useful since at the end of DoWork, you can assume you are done and hence further tasks do not need you as a prerequisite. 
	 *	However, MyCompletionGraphEvent can be useful for passing to other routines or when it is handy to set up subsequents before you actually do work.
	 **/
	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		checkThreadGraph(ENamedThreads::GetThreadIndex(ThreadToReturnFrom) == ENamedThreads::GetThreadIndex(CurrentThread)); // we somehow are executing on the wrong thread.
		FTaskGraphInterface::Get().RequestReturn(ThreadToReturnFrom);
	}

private:
	/** Named thread that we want to cause to return to the caller of ProcessThreadUntilRequestReturn. **/
	ENamedThreads::Type ThreadToReturnFrom;
};

	virtual void RequestQuit(int32 QueueIndex) override
	{
		// this will not work under arbitrary circumstances. For example you should not attempt to stop threads unless they are known to be idle.
		if (!Queue(0).StallRestartEvent)
		{
			return;
		}
		if (QueueIndex == -1)
		{
			// we are shutting down
			checkThreadGraph(Queue(0).StallRestartEvent); // make sure we are started up
			checkThreadGraph(Queue(1).StallRestartEvent); // make sure we are started up
			Queue(0).QuitForShutdown = true;
			Queue(1).QuitForShutdown = true;
			Queue(0).StallRestartEvent->Trigger();
			Queue(1).StallRestartEvent->Trigger();
		}
		else
		{
			checkThreadGraph(Queue(QueueIndex).StallRestartEvent); // make sure we are started up
			Queue(QueueIndex).QuitForReturn = true;
		}
	}

	virtual void RequestQuit(int32 QueueIndex) override
	{
		check(QueueIndex < 1);

		// this will not work under arbitrary circumstances. For example you should not attempt to stop threads unless they are known to be idle.
		checkThreadGraph(Queue.StallRestartEvent); // make sure we are started up
		Queue.QuitForShutdown = true;
		Queue.StallRestartEvent->Trigger();
	}
```


WaitFor (Task) Complete.

```
if (Tasks[Index]->IsComplete())
	return;
else
{
	// 讲之前依赖的task加到列表里，比如之前的RHIThreadTask.
	TGraphTask<FReturnGraphTask>::CreateTask(&Tasks, CurrentThread).ConstructAndDispatchWhenReady(CurrentThread);
	ProcessThreadUntilRequestReturn(CurrentThread);
}
```
ReturnGraphTask执行完，之后会调用
```
FTaskGraphInterface::Get().RequestReturn(ThreadToReturnFrom);
```

而ProcessThreadUntilRequestReturn会调用ProcessTasksUntilQuit，
```
virtual void ProcessTasksUntilQuit(int32 QueueIndex) override
{
	check(Queue(QueueIndex).StallRestartEvent); // make sure we are started up

	Queue(QueueIndex).QuitForReturn = false;
	verify(++Queue(QueueIndex).RecursionGuard == 1);
	do
	{
		ProcessTasksNamedThread(QueueIndex, FPlatformProcess::SupportsMultithreading());
	} while (!Queue(QueueIndex).QuitForReturn && !Queue(QueueIndex).QuitForShutdown && FPlatformProcess::SupportsMultithreading()); // @Hack - quit now when running with only one thread.
	verify(!--Queue(QueueIndex).RecursionGuard);
}
```
在 	void ProcessTasksNamedThread(int32 QueueIndex, bool bAllowStall)中
```
while (!Queue(QueueIndex).QuitForReturn)
{
	FBaseGraphTask* Task = Queue(QueueIndex).StallQueue.Pop(0, bAllowStall);
	if (!Task)
	{
		if (bAllowStall)
		{
			{
				FScopeCycleCounter Scope(StallStatId);
				Queue(QueueIndex).StallRestartEvent->Wait(MAX_uint32, bCountAsStall);
				if (Queue(QueueIndex).QuitForShutdown)
				{
					return;
				}
				TestRandomizedThreads();
			}
			continue;
		}
		else
		{
			break; // we were asked to quit
		}
	}
	else
	{
		Task->Execute(NewTasks, ENamedThreads::Type(ThreadId | (QueueIndex << ENamedThreads::QueueIndexShift)));
		TestRandomizedThreads();
	}
}
```
队列里的任务执行完的时候，就会Queue(QueueIndex).StallRestartEvent->Wait(MAX_uint32, bCountAsStall); 使线程中的Event进行wait()。

当接收到quit的时候, 这个循环就退出了，可以回到原来的地方了。
```
FTaskGraphInterface::Get().WaitUntilTaskCompletes(RHIThreadTask, RenderThread_Local);  
```

那么跟这个Event相关的任务时如何执行完毕并Trigger的呢？

RHI线程在执行Task完Task之后，会根据条件，DispatchSubsequents.
```
virtual void ExecuteTask(TArray<FBaseGraphTask*>& NewTasks, ENamedThreads::Type CurrentThread) final override
{
	TTask& Task = *(TTask*)&TaskStorage;
	{
		FScopeCycleCounter Scope(Task.GetStatId(), true); 
		Task.DoTask(CurrentThread, Subsequents);
		Task.~TTask();
		checkThreadGraph(ENamedThreads::GetThreadIndex(CurrentThread) <= ENamedThreads::GetRenderThread() || FMemStack::Get().IsEmpty()); // you must mark and pop memstacks if you use them in tasks! Named threads are excepted.
	}
	
	TaskConstructed = false;
	if (TTask::GetSubsequentsMode() == ESubsequentsMode::TrackSubsequents)
	{
		FPlatformMisc::MemoryBarrier();
		Subsequents->DispatchSubsequents(NewTasks, CurrentThread);
	}

	if (sizeof(TGraphTask) <= FBaseGraphTask::SMALL_TASK_SIZE)
	{
		this->TGraphTask::~TGraphTask();
		FBaseGraphTask::GetSmallTaskAllocator().Free(this);
	}
	else
	{
		delete this;
	}
}
```

这个 TTask::GetSubsequentsMode() == ESubsequentsMode::TrackSubsequents来自于用户的定义，标志会有其他的任务依赖于这个Task。
```
class FExecuteRHIThreadTask
{
	FRHICommandListBase* RHICmdList;

public:

	FExecuteRHIThreadTask(FRHICommandListBase* InRHICmdList)
		: RHICmdList(InRHICmdList)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FExecuteRHIThreadTask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		check(IsRunningRHIInSeparateThread()); // this should never be used on a platform that doesn't support the RHI thread
		return IsRunningRHIInDedicatedThread() ? ENamedThreads::RHIThread : CPrio_RHIThreadOnTaskThreads.Get();
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		SCOPE_CYCLE_COUNTER(STAT_RHIThreadExecute);
		if (IsRunningRHIInTaskThread())
		{
			GRHIThreadId = FPlatformTLS::GetCurrentThreadId();
		}
		{
			FScopeLock Lock(&GRHIThreadOnTasksCritical);
			GWorkingRHIThreadStartCycles = FPlatformTime::Cycles();

			FRHICommandListExecutor::ExecuteInner_DoExecute(*RHICmdList);
			delete RHICmdList;

			GWorkingRHIThreadTime += (FPlatformTime::Cycles() - GWorkingRHIThreadStartCycles); // this subtraction often wraps and the math stuff works
		}
		if (IsRunningRHIInTaskThread())
		{
			GRHIThreadId = 0;
		}
	}
};
```

然后会对每个任务执行QueueTask
```
for (int32 Index = NewTasks.Num() - 1; Index >= 0 ; Index--) // reverse the order since PopAll is implicitly backwards
{
	FBaseGraphTask* NewTask = NewTasks[Index];
	checkThreadGraph(NewTask);
	NewTask->ConditionalQueueTask(CurrentThreadIfKnown);
}
NewTasks.Reset();
```
会根据线程的条件执行
```
if (ThreadToExecuteOn == ENamedThreads::GetThreadIndex(CurrentThreadIfKnown))
{
	Target->EnqueueFromThisThread(QueueToExecuteOn, Task);
}
else
{
	Target->EnqueueFromOtherThread(QueueToExecuteOn, Task);
}
```
RHIThread的执行到了这里。
```
virtual bool EnqueueFromOtherThread(int32 QueueIndex, FBaseGraphTask* Task) override
{
	TestRandomizedThreads();
	checkThreadGraph(Task && Queue(QueueIndex).StallRestartEvent); // make sure we are started up

	uint32 PriIndex = ENamedThreads::GetTaskPriority(Task->ThreadToExecuteOn) ? 0 : 1;
	int32 ThreadToStart = Queue(QueueIndex).StallQueue.Push(Task, PriIndex);

	if (ThreadToStart >= 0)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_TaskGraph_EnqueueFromOtherThread_Trigger);
		checkThreadGraph(ThreadToStart == 0);
		TASKGRAPH_SCOPE_CYCLE_COUNTER(1, STAT_TaskGraph_EnqueueFromOtherThread_Trigger);
		Queue(QueueIndex).StallRestartEvent->Trigger();
		return true;
	}
	return false;
}
```

在Queue任务的时候，发现有线程需要唤醒，那么久TriggerEvent.

有个疑问，newTasks是什么？newTask是依赖TaskEvent的任务，
```
void SetupPrereqs(const FGraphEventArray* Prerequisites, ENamedThreads::Type CurrentThreadIfKnown, bool bUnlock)
{
	if (Prerequisites)
	{
		for (int32 Index = 0; Index < Prerequisites->Num(); Index++)
		{
			check((*Prerequisites)[Index]);
			if (!(*Prerequisites)[Index]->AddSubsequent(this))
			{
				AlreadyCompletedPrerequisites++;
			}
		}
	}
}
```
如果当前Task任务已经完成，那么会把依赖于当前任务的所有Task拿出来，再加到任务队列里。
由于有依赖关系，所有这个依赖关系是通过TaskEvent管理的。

### 写个测试例子模拟TaskGraph
![](https://pic3.zhimg.com/80/v2-87e8e0a3904861827271a7736d5df306_720w.jpg)

### 首先是单个线程里执行的任务顺序依赖
1. 主线程发布任务后，由子线程按照任务依赖顺序执行任务。
2. 子线程等待结果。

```
A Task：1~50000 单数累加
B Task ： 1 ~ 50000 双数累加
C Task: 求 A和BTask结果的和，然后结束
```
A、B、C 由同一个线程执行。

A、B、C分别由不同的线程执行。

主线程 WaitC执行完毕。
完成这个例子，TaskGraph应该就有个初步的样子了。

### 进阶
- 子线程中，再发布任务，然后继续等待其他线程的任务完成。

### 引擎里对这个的需求是什么？
1. Game线程等待RHI线程中的命令执行完毕
2. RHI线程等待之前的任务执行完毕
3. Game线程等待上一帧的RHI命令执行完毕。

对线程来说 它没有这帧还是上帧执行的问题，只关心任务是否执行完毕。

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

push有个返回值，是否需要Trigger线程。

```
int32 ThreadToStart = Queue(QueueIndex).StallQueue.Push(Task, PriIndex);

if (ThreadToStart >= 0)
{
QUICK_SCOPE_CYCLE_COUNTER(STAT_TaskGraph_EnqueueFromOtherThread_Trigger);
checkThreadGraph(ThreadToStart == 0);
TASKGRAPH_SCOPE_CYCLE_COUNTER(1, STAT_TaskGraph_EnqueueFromOtherThread_Trigger);
Queue(QueueIndex).StallRestartEvent->Trigger();
return true;
}

FStallingTaskQueue<FBaseGraphTask, PLATFORM_CACHE_LINE_SIZE, 2> StallQueue;
```

如果是空的，那么阻塞线程。
push的时候，看是否为空的，如果为空就trigger？


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

```
createTask的执行逻辑
ConstructAndDispatchWhenReady 调用Setup，然后调用void SetupPrereqs(const FGraphEventArray* Prerequisites, ENamedThreads::Type CurrentThreadIfKnown, bool bUnlock)

1. 设置BaseGraphTask的执行线程
2. 向其依赖的Prequisites中，把task加进去。
3. 如果加成功了，说明依赖还没执行完毕
4. 如果没有加成功，说明依赖已经执行完毕

然后执行 PrerequisitesComplete(CurrentThreadIfKnown, AlreadyCompletedPrerequisites, bUnlock);

如果所有依赖都执行完毕了，那么将其加入到对应的线程task中。

int32 NumToSub = NumAlreadyFinishedPrequistes + (bUnlock ? 1 : 0); // the +1 is for the "lock" we set up in the constructor
if (NumberOfPrerequistitesOutstanding.Subtract(NumToSub) == NumToSub) 
{
	QueueTask(CurrentThread);
}

根据线程是不是自己，那么决定是否要TriggerEvent
if (ThreadToExecuteOn == ENamedThreads::GetThreadIndex(CurrentThreadIfKnown))
{
	Target->EnqueueFromThisThread(QueueToExecuteOn, Task);
}
else
{
	Target->EnqueueFromOtherThread(QueueToExecuteOn, Task);
}
```

如果创建Task时，其依赖没有执行完毕，那么等依赖项执行完毕的时候，是怎么加入到ThreadTask列表中的呢？注意，这个task可能会有多个依赖任务。
```
执行完task
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
```

开始执行DispatchSubsequents。
```
SubsequentList.PopAllAndClose(NewTasks);
for (int32 Index = NewTasks.Num() - 1; Index >= 0 ; Index--) // reverse the order since PopAll is implicitly backwards
{
	FBaseGraphTask* NewTask = NewTasks[Index];
	checkThreadGraph(NewTask);
	NewTask->ConditionalQueueTask(CurrentThreadIfKnown);
}
NewTasks.Reset();
```
GraphEvent会把依赖其的所有task都pop出来，并把这个GraphEvent给关闭掉。分别对每个任务执行NewTask->ConditionalQueueTask(CurrentThreadIfKnown);
```
if (NumberOfPrerequistitesOutstanding.Decrement()==0)
{
	QueueTask(CurrentThread);
}
```
NumberOfPrerequistitesOutstanding是GraphTask的成员变量，每次执行到这里都会将其减一。当减到0的时候，说明这个Task依赖的所有任务都执行完毕，那么可以将其加入到对应的线程中去执行 QueueTask。
这个部分就跟之前是一样的了。

那么Trigger逻辑是如何触发的呢？说不太通。

显式的wait WaitUntilTaskCompletes
```
void WaitUntilTaskCompletes(const FGraphEventRef& Task, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)
{
	FGraphEventArray Prerequistes;
	Prerequistes.Add(Task);
	WaitUntilTasksComplete(Prerequistes, CurrentThreadIfKnown);
}

virtual void WaitUntilTasksComplete
{
	if (Tasks.Num() < 8) // don't bother to check for completion if there are lots of prereqs...too expensive to check
	{
		bool bAnyPending = false;
		for (int32 Index = 0; Index < Tasks.Num(); Index++)
		{
			if (!Tasks[Index]->IsComplete())
			{
				bAnyPending = true;
				break;
			}
		}
		if (!bAnyPending)
		{
			return;
		}
	}
	// named thread process tasks while we wait
	TGraphTask<FReturnGraphTask>::CreateTask(&Tasks, CurrentThread).ConstructAndDispatchWhenReady(CurrentThread);
	ProcessThreadUntilRequestReturn(CurrentThread);
}
```

可以看到，如果Task已经完成，那么就不用等了。如果Task还没有完成。会创建一个FReturnGraphTask的Task（由于依赖的没有执行完毕，所以这个Task 不会被加到queue里吗？ 那么它使如何被执行的呢？）。然后会处理这个线程的任务，直到ProcessThreadUntilRequestReturn，FReturnGraphTask被执行完毕。

如果queue里的任务没有执行完毕，那么就一直执行，如果queue里的任务执行完毕了，那么就这个线程就需要等待。

如果依赖的任务没有执行完毕，那么不会将其加到queue中，那么ProcessTasksNamedThread中，当命令队列执行完毕后仍然没有加入到queue中，会阻塞当前线程wait.

阻塞之后，把FReturnGraphTask任务加到queue里了，那么wait到之后，下个任务就会执行FReturnGraphTask，然后quit。

如果依赖的任务执行完毕，并且在queue执行完毕前 将FReturnGraphTask加到到queue中，那么这个任务会将队列quit.

那么另外一个线程怎么trigger的条件是什么呢？


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

while (!Queue(QueueIndex).QuitForReturn)
		{
			FBaseGraphTask* Task = Queue(QueueIndex).StallQueue.Pop(0, bAllowStall);
			TestRandomizedThreads();
			if (!Task)
			{
#if STATS
				if (bTasksOpen)
				{
					ProcessingTasks.Stop();
					bTasksOpen = false;
				}
#endif
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
#if STATS
					if (!bTasksOpen && FThreadStats::IsCollectingData(StatName))
					{
						bTasksOpen = true;
						ProcessingTasks.Start(StatName);
					}
#endif
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

```

#### Render线程的 commandlist的flush的调用栈
UE中的GameThread就是主线程

```
 	UE4Editor-RHI.dll!FRHICommandListExecutor::ExecuteInner(FRHICommandListBase & CmdList) 行 466	C++
 	UE4Editor-RHI.dll!FRHICommandListExecutor::ExecuteList(FRHICommandListImmediate & CmdList) 行 672	C++
 	[内联框架] UE4Editor-RHI.dll!FRHICommandListImmediate::ImmediateFlush(EImmediateFlushType::Type) 行 80	C++
 	UE4Editor-RHI.dll!FRHICommandList::EndFrame() 行 1645	C++
 	[内联框架] UE4Editor.exe!EndFrameRenderThread(FRHICommandListImmediate &) 行 4210	C++
 	[内联框架] UE4Editor.exe!FEngineLoop::Tick::__l38::<lambda_9dcd0e3c332efa1dae8605253371d1fc>::operator()(FRHICommandListImmediate &) 行 4712	C++
>	UE4Editor.exe!TEnqueueUniqueRenderCommandType<`FEngineLoop::Tick'::`38'::EndFrameName,<lambda_9dcd0e3c332efa1dae8605253371d1fc> >::DoTask(ENamedThreads::Type CurrentThread, const TRefCountPtr<FGraphEvent> & MyCompletionGraphEvent) 行 198	C++
 	UE4Editor.exe!TGraphTask<TEnqueueUniqueRenderCommandType<`FEngineLoop::Tick'::`38'::EndFrameName,<lambda_9dcd0e3c332efa1dae8605253371d1fc> > >::ExecuteTask(TArray<FBaseGraphTask *,TSizedDefaultAllocator<32> > & NewTasks, ENamedThreads::Type CurrentThread) 行 847	C++
 	[内联框架] UE4Editor-Core.dll!FBaseGraphTask::Execute(TArray<FBaseGraphTask *,TSizedDefaultAllocator<32> > & CurrentThread, ENamedThreads::Type) 行 514	C++
 	UE4Editor-Core.dll!FNamedTaskThread::ProcessTasksNamedThread(int QueueIndex, bool bAllowStall) 行 686	C++
 	UE4Editor-Core.dll!FNamedTaskThread::ProcessTasksUntilQuit(int QueueIndex) 行 583	C++
 	UE4Editor-RenderCore.dll!RenderingThreadMain(FEvent * TaskGraphBoundSyncEvent) 行 340	C++
 	UE4Editor-RenderCore.dll!FRenderingThread::Run() 行 490	C++
 	UE4Editor-Core.dll!FRunnableThreadWin::Run() 行 96	C++
 	UE4Editor-Core.dll!FRunnableThreadWin::GuardedRun() 行 45	C++

```
FEngineLoop::Tick()里会将这个EndFrame的任务加到Task中。

```
BeginRenderingViewFamily里会给渲染线程发布SceneRender的命令。
ENQUEUE_RENDER_COMMAND(FDrawSceneCommand)(
	[SceneRenderer](FRHICommandListImmediate& RHICmdList)
	{
		RenderViewFamily_RenderThread(RHICmdList, SceneRenderer);
		FlushPendingDeleteRHIResources_RenderThread();
	});

 	UE4Editor-Renderer.dll!FRendererModule::BeginRenderingViewFamily(FCanvas * Canvas, FSceneViewFamily * ViewFamily) 行 3609	C++
 	UE4Editor-UnrealEd.dll!FEditorViewportClient::Draw(FViewport * InViewport, FCanvas * Canvas) 行 3761	C++
 	UE4Editor-Engine.dll!FViewport::Draw(bool bShouldPresent) 行 1570	C++
 	UE4Editor-UnrealEd.dll!UEditorEngine::UpdateSingleViewportClient(FEditorViewportClient * InViewportClient, const bool bInAllowNonRealtimeViewportToDraw, bool bLinkedOrthoMovement) 行 2093	C++
 	UE4Editor-UnrealEd.dll!UEditorEngine::Tick(float DeltaSeconds, bool bIdleMode) 行 1816	C++
 	UE4Editor-UnrealEd.dll!UUnrealEdEngine::Tick(float DeltaSeconds, bool bIdleMode) 行 410	C++
>	UE4Editor.exe!FEngineLoop::Tick() 行 4485	C++

```


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

### 线程间的等待
前提：有任务依赖的任务，是由其所依赖的任务完成后发布到对应的线程的。

- 线程1：发布任务A, 执行代码，然后wait A的执行完毕。
- 线程2：执行任务A，执行完毕后，告诉线程已经执行完毕。

wait A的时候，可以分为以下几种情况：
1. A已经执行完毕，线程1继续向下执行。
2. A还没有执行完毕，那么把 waitA 的task （将线程1的任务队列标记为quit ） 加到A任务的subsequence 列表中。（这个列表要能处理互斥。）
   1. 线程1需要想办法阻塞
   2. 线程2需要想办法signal
   
1. 线程1 向线程2发布了wait A的task依赖之后，自己一直执行队列里的任务
2. 如果标记为Quit，那表明线程2已经执行完任务，并将 标记线程1位quit的任务放到了线程1的列表中，并由线程1执行完毕。那么退出处理任务的循环，继续执行线程1后面的代码。
3. 如果在任务队列为空之后，那么就需要等待了。StallEvent->Wait()。线程2将任务A执行完毕后，把quit线程1的任务发布到线程1的任务队列中，如果此时任务队列是空的，应该如何处理呢？
   1. 线程1先访问，然后阻塞，线程2往里面push任务，并唤醒线程1
   2. 线程2先访问，然后唤醒，线程1 访问时不为空，这时就出了问题。 所以唤醒的条件是什么？
      1. POP的时候如果为空，那么就把这个线程标记为阻塞，然后后面将其阻塞，然后push的时候查询状态，触发唤醒。

### 进阶
- 子线程中，再发布任务，然后继续等待其他线程的任务完成。

### 引擎里对这个的需求是什么？
1. Game线程等待RHI线程中的命令执行完毕
2. RHI线程等待之前的任务执行完毕
3. Game线程等待上一帧的RHI命令执行完毕。

对线程来说 它没有这帧还是上帧执行的问题，只关心任务是否执行完毕。

### 有两处难点
- wait、trigger 这个可以通过 lockfreelist的 push和pop的原理来暂时解决
- 还有个依赖的问题，isCompleted() PopAndClose()

状态交换。
```
- https://gcc.gnu.org/onlinedocs/gcc-4.1.1/gcc/Atomic-Builtins.html
- https://docs.microsoft.com/en-us/cpp/intrinsics/interlockedcompareexchange-intrinsic-functions?view=vs-2019
```
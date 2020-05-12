### 运行逻辑
#### Render线程flush命令的逻辑
1. 在SceneRender的initView的时候先Dispatch一遍命令
```
>	UE4Editor-RHI.dll!FRHICommandListExecutor::ExecuteInner(FRHICommandListBase & CmdList) 行 466	C++
 	UE4Editor-RHI.dll!FRHICommandListExecutor::ExecuteList(FRHICommandListImmediate & CmdList) 行 672	C++
 	[内联框架] UE4Editor-Renderer.dll!FRHICommandListImmediate::ImmediateFlush(EImmediateFlushType::Type) 行 80	C++
 	UE4Editor-Renderer.dll!FDeferredShadingSceneRenderer::InitViews(FRHICommandListImmediate & RHICmdList, FExclusiveDepthStencil::Type BasePassDepthStencilAccess, FILCUpdatePrimTaskData & ILCTaskData, TArray<TRefCountPtr<FGraphEvent>,TInlineAllocator<4,TSizedDefaultAllocator<32> > > & UpdateViewCustomDataEvents) 行 4229	C++
 	UE4Editor-Renderer.dll!FDeferredShadingSceneRenderer::Render(FRHICommandListImmediate & RHICmdList) 行 1094	C++
 	UE4Editor-Renderer.dll!RenderViewFamily_RenderThread(FRHICommandListImmediate & RHICmdList, FSceneRenderer * SceneRenderer) 行 3477	C++
 	[内联框架] UE4Editor-Renderer.dll!FRendererModule::BeginRenderingViewFamily::__l35::<lambda_7df85a902bb55f107fd3c0b688678f3c>::operator()(FRHICommandListImmediate &) 行 3702	C++
 	[内联框架] UE4Editor-Renderer.dll!TEnqueueUniqueRenderCommandType<`FRendererModule::BeginRenderingViewFamily'::`35'::FDrawSceneCommandName,<lambda_7df85a902bb55f107fd3c0b688678f3c> >::DoTask(ENamedThreads::Type) 行 198	C++
 	UE4Editor-Renderer.dll!TGraphTask<TEnqueueUniqueRenderCommandType<`FRendererModule::BeginRenderingViewFamily'::`35'::FDrawSceneCommandName,<lambda_7df85a902bb55f107fd3c0b688678f3c> > >::ExecuteTask(TArray<FBaseGraphTask *,TSizedDefaultAllocator<32> > & NewTasks, ENamedThreads::Type CurrentThread) 行 847	C++
 	[内联框架] UE4Editor-Core.dll!FBaseGraphTask::Execute(TArray<FBaseGraphTask *,TSizedDefaultAllocator<32> > & CurrentThread, ENamedThreads::Type) 行 514	C++
 	UE4Editor-Core.dll!FNamedTaskThread::ProcessTasksNamedThread(int QueueIndex, bool bAllowStall) 行 686	C++
 	UE4Editor-Core.dll!FNamedTaskThread::ProcessTasksUntilQuit(int QueueIndex) 行 583	C++
 	UE4Editor-RenderCore.dll!RenderingThreadMain(FEvent * TaskGraphBoundSyncEvent) 行 340	C++
 	UE4Editor-RenderCore.dll!FRenderingThread::Run() 行 471	C++
 	UE4Editor-Core.dll!FRunnableThreadWin::Run() 行 96	C++
 	UE4Editor-Core.dll!FRunnableThreadWin::GuardedRun() 行 53	C++
```
2. SceneRender的时候 flush资源
```
>	UE4Editor-RHI.dll!FRHICommandListExecutor::ExecuteInner(FRHICommandListBase & CmdList) 行 466	C++
 	UE4Editor-RHI.dll!FRHICommandListExecutor::ExecuteList(FRHICommandListImmediate & CmdList) 行 672	C++
 	[内联框架] UE4Editor-Renderer.dll!FRHICommandListImmediate::ImmediateFlush(EImmediateFlushType::Type) 行 80	C++
 	UE4Editor-Renderer.dll!FDeferredShadingSceneRenderer::InitViews(FRHICommandListImmediate & RHICmdList, FExclusiveDepthStencil::Type BasePassDepthStencilAccess, FILCUpdatePrimTaskData & ILCTaskData, TArray<TRefCountPtr<FGraphEvent>,TInlineAllocator<4,TSizedDefaultAllocator<32> > > & UpdateViewCustomDataEvents) 行 4229	C++
 	UE4Editor-Renderer.dll!FDeferredShadingSceneRenderer::Render(FRHICommandListImmediate & RHICmdList) 行 1094	C++
 	UE4Editor-Renderer.dll!RenderViewFamily_RenderThread(FRHICommandListImmediate & RHICmdList, FSceneRenderer * SceneRenderer) 行 3477	C++
 	[内联框架] UE4Editor-Renderer.dll!FRendererModule::BeginRenderingViewFamily::__l35::<lambda_7df85a902bb55f107fd3c0b688678f3c>::operator()(FRHICommandListImmediate &) 行 3702	C++
 	[内联框架] UE4Editor-Renderer.dll!TEnqueueUniqueRenderCommandType<`FRendererModule::BeginRenderingViewFamily'::`35'::FDrawSceneCommandName,<lambda_7df85a902bb55f107fd3c0b688678f3c> >::DoTask(ENamedThreads::Type) 行 198	C++
 	UE4Editor-Renderer.dll!TGraphTask<TEnqueueUniqueRenderCommandType<`FRendererModule::BeginRenderingViewFamily'::`35'::FDrawSceneCommandName,<lambda_7df85a902bb55f107fd3c0b688678f3c> > >::ExecuteTask(TArray<FBaseGraphTask *,TSizedDefaultAllocator<32> > & NewTasks, ENamedThreads::Type CurrentThread) 行 847	C++
 	[内联框架] UE4Editor-Core.dll!FBaseGraphTask::Execute(TArray<FBaseGraphTask *,TSizedDefaultAllocator<32> > & CurrentThread, ENamedThreads::Type) 行 514	C++
 	UE4Editor-Core.dll!FNamedTaskThread::ProcessTasksNamedThread(int QueueIndex, bool bAllowStall) 行 686	C++
 	UE4Editor-Core.dll!FNamedTaskThread::ProcessTasksUntilQuit(int QueueIndex) 行 583	C++
 	UE4Editor-RenderCore.dll!RenderingThreadMain(FEvent * TaskGraphBoundSyncEvent) 行 340	C++
 	UE4Editor-RenderCore.dll!FRenderingThread::Run() 行 471	C++
 	UE4Editor-Core.dll!FRunnableThreadWin::Run() 行 96	C++
 	UE4Editor-Core.dll!FRunnableThreadWin::GuardedRun() 行 53	C++

```
3. RenderPrePass的时候Dispatch
```
>	UE4Editor-RHI.dll!FRHICommandListExecutor::ExecuteInner(FRHICommandListBase & CmdList) 行 466	C++
 	UE4Editor-RHI.dll!FRHICommandListExecutor::ExecuteList(FRHICommandListImmediate & CmdList) 行 672	C++
 	[内联框架] UE4Editor-Renderer.dll!FRHICommandListImmediate::ImmediateFlush(EImmediateFlushType::Type) 行 80	C++
 	UE4Editor-Renderer.dll!FDeferredShadingSceneRenderer::InitViews(FRHICommandListImmediate & RHICmdList, FExclusiveDepthStencil::Type BasePassDepthStencilAccess, FILCUpdatePrimTaskData & ILCTaskData, TArray<TRefCountPtr<FGraphEvent>,TInlineAllocator<4,TSizedDefaultAllocator<32> > > & UpdateViewCustomDataEvents) 行 4229	C++
 	UE4Editor-Renderer.dll!FDeferredShadingSceneRenderer::Render(FRHICommandListImmediate & RHICmdList) 行 1094	C++
 	UE4Editor-Renderer.dll!RenderViewFamily_RenderThread(FRHICommandListImmediate & RHICmdList, FSceneRenderer * SceneRenderer) 行 3477	C++
 	[内联框架] UE4Editor-Renderer.dll!FRendererModule::BeginRenderingViewFamily::__l35::<lambda_7df85a902bb55f107fd3c0b688678f3c>::operator()(FRHICommandListImmediate &) 行 3702	C++
 	[内联框架] UE4Editor-Renderer.dll!TEnqueueUniqueRenderCommandType<`FRendererModule::BeginRenderingViewFamily'::`35'::FDrawSceneCommandName,<lambda_7df85a902bb55f107fd3c0b688678f3c> >::DoTask(ENamedThreads::Type) 行 198	C++
 	UE4Editor-Renderer.dll!TGraphTask<TEnqueueUniqueRenderCommandType<`FRendererModule::BeginRenderingViewFamily'::`35'::FDrawSceneCommandName,<lambda_7df85a902bb55f107fd3c0b688678f3c> > >::ExecuteTask(TArray<FBaseGraphTask *,TSizedDefaultAllocator<32> > & NewTasks, ENamedThreads::Type CurrentThread) 行 847	C++
 	[内联框架] UE4Editor-Core.dll!FBaseGraphTask::Execute(TArray<FBaseGraphTask *,TSizedDefaultAllocator<32> > & CurrentThread, ENamedThreads::Type) 行 514	C++
 	UE4Editor-Core.dll!FNamedTaskThread::ProcessTasksNamedThread(int QueueIndex, bool bAllowStall) 行 686	C++
 	UE4Editor-Core.dll!FNamedTaskThread::ProcessTasksUntilQuit(int QueueIndex) 行 583	C++
 	UE4Editor-RenderCore.dll!RenderingThreadMain(FEvent * TaskGraphBoundSyncEvent) 行 340	C++
 	UE4Editor-RenderCore.dll!FRenderingThread::Run() 行 471	C++
 	UE4Editor-Core.dll!FRunnableThreadWin::Run() 行 96	C++
 	UE4Editor-Core.dll!FRunnableThreadWin::GuardedRun() 行 53	C++
```
4. RHI线程执行, 走到 void FRHICommandListExecutor::ExecuteInner_DoExecute(FRHICommandListBase& CmdList)中
```
 	UE4Editor-RHI.dll!FRHICommandListExecutor::ExecuteInner(FRHICommandListBase & CmdList) 行 466	C++
 	UE4Editor-RHI.dll!FRHICommandListExecutor::ExecuteList(FRHICommandListBase & CmdList) 行 645	C++
 	[内联框架] UE4Editor-RHI.dll!FRHICommandListBase::Flush() 行 21	C++
 	[内联框架] UE4Editor-RHI.dll!FRHICommandListBase::{dtor}() 行 878	C++
 	UE4Editor-RHI.dll!FRHICommandWaitForAndSubmitSubList::Execute(FRHICommandListBase & CmdList) 行 1078	C++
 	UE4Editor-RHI.dll!FRHICommand<FRHICommandWaitForAndSubmitSubList,FRHICommandWaitForAndSubmitSubListString1044>::ExecuteAndDestruct(FRHICommandListBase & CmdList, FRHICommandListDebugContext & Context) 行 726	C++
>	UE4Editor-RHI.dll!FRHICommandListExecutor::ExecuteInner_DoExecute(FRHICommandListBase & CmdList) 行 351	C++
 	UE4Editor-RHI.dll!FExecuteRHIThreadTask::DoTask(ENamedThreads::Type CurrentThread, const TRefCountPtr<FGraphEvent> & MyCompletionGraphEvent) 行 410	C++
 	UE4Editor-RHI.dll!TGraphTask<FExecuteRHIThreadTask>::ExecuteTask(TArray<FBaseGraphTask *,TSizedDefaultAllocator<32> > & NewTasks, ENamedThreads::Type CurrentThread) 行 847	C++
 	[内联框架] UE4Editor-Core.dll!FBaseGraphTask::Execute(TArray<FBaseGraphTask *,TSizedDefaultAllocator<32> > & CurrentThread, ENamedThreads::Type) 行 514	C++
 	UE4Editor-Core.dll!FNamedTaskThread::ProcessTasksNamedThread(int QueueIndex, bool bAllowStall) 行 686	C++
 	UE4Editor-Core.dll!FNamedTaskThread::ProcessTasksUntilQuit(int QueueIndex) 行 583	C++
 	UE4Editor-RenderCore.dll!FRHIThread::Run() 行 289	C++
 	UE4Editor-Core.dll!FRunnableThreadWin::Run() 行 96	C++
 	UE4Editor-Core.dll!FRunnableThreadWin::GuardedRun() 行 53	C++
```
5. a
```
 	UE4Editor-RHI.dll!FRHICommandListExecutor::ExecuteList(FRHICommandListImmediate & CmdList) 行 650	C++
 	[内联框架] UE4Editor-RHI.dll!FRHICommandListImmediate::ImmediateFlush(EImmediateFlushType::Type) 行 80	C++
>	UE4Editor-RHI.dll!FRHICommandList::EndDrawingViewport(FRHIViewport * Viewport, bool bPresent, bool bLockToVsync) 行 1585	C++
 	UE4Editor-SlateRHIRenderer.dll!FSlateRHIRenderer::DrawWindow_RenderThread(FRHICommandListImmediate & RHICmdList, FViewportInfo & ViewportInfo, FSlateWindowElementList & WindowElementList, const FSlateDrawWindowCommandParams & DrawCommandParams) 行 1111	C++
 	[内联框架] UE4Editor-SlateRHIRenderer.dll!FSlateRHIRenderer::DrawWindows_Private::__l26::<lambda_2a9c0a9fed59e8507bcd2f8c06e1febe>::operator()(FRHICommandListImmediate &) 行 1277	C++
 	UE4Editor-SlateRHIRenderer.dll!TEnqueueUniqueRenderCommandType<`FSlateRHIRenderer::DrawWindows_Private'::`26'::SlateDrawWindowsCommandName,<lambda_2a9c0a9fed59e8507bcd2f8c06e1febe> >::DoTask(ENamedThreads::Type CurrentThread, const TRefCountPtr<FGraphEvent> & MyCompletionGraphEvent) 行 198	C++
 	UE4Editor-SlateRHIRenderer.dll!TGraphTask<TEnqueueUniqueRenderCommandType<`FSlateRHIRenderer::DrawWindows_Private'::`26'::SlateDrawWindowsCommandName,<lambda_2a9c0a9fed59e8507bcd2f8c06e1febe> > >::ExecuteTask(TArray<FBaseGraphTask *,TSizedDefaultAllocator<32> > & NewTasks, ENamedThreads::Type CurrentThread) 行 847	C++
 	[内联框架] UE4Editor-Core.dll!FBaseGraphTask::Execute(TArray<FBaseGraphTask *,TSizedDefaultAllocator<32> > & CurrentThread, ENamedThreads::Type) 行 514	C++
 	UE4Editor-Core.dll!FNamedTaskThread::ProcessTasksNamedThread(int QueueIndex, bool bAllowStall) 行 686	C++
 	UE4Editor-Core.dll!FNamedTaskThread::ProcessTasksUntilQuit(int QueueIndex) 行 583	C++

void FRHICommandList::EndDrawingViewport(FRHIViewport* Viewport, bool bPresent, bool bLockToVsync)
{
	if (IsRunningRHIInSeparateThread())
	{
		// Wait on the previous frame's RHI thread fence (we never want the rendering thread to get more than a frame ahead)
		uint32 PreviousFrameFenceIndex = 1 - GRHIThreadEndDrawingViewportFenceIndex;
		FGraphEventRef& LastFrameFence = GRHIThreadEndDrawingViewportFences[PreviousFrameFenceIndex];
		FRHICommandListExecutor::WaitOnRHIThreadFence(LastFrameFence);
		GRHIThreadEndDrawingViewportFences[PreviousFrameFenceIndex] = nullptr;
		GRHIThreadEndDrawingViewportFenceIndex = PreviousFrameFenceIndex;
	}
}
```
用Fence实现的，这时候要看一下Fence是如何搞的。

Fence的Wait调用栈是这样的。
```
>	UE4Editor-Core.dll!FNamedTaskThread::ProcessTasksNamedThread(int QueueIndex, bool bAllowStall) 行 663	C++
 	UE4Editor-Core.dll!FNamedTaskThread::ProcessTasksUntilQuit(int QueueIndex) 行 583	C++
 	[内联框架] UE4Editor-Core.dll!FTaskGraphImplementation::ProcessThreadUntilRequestReturn(ENamedThreads::Type) 行 1414	C++
 	UE4Editor-Core.dll!FTaskGraphImplementation::WaitUntilTasksComplete(const TArray<TRefCountPtr<FGraphEvent>,TInlineAllocator<4,TSizedDefaultAllocator<32> > > & Tasks, ENamedThreads::Type CurrentThreadIfKnown) 行 1465	C++
 	UE4Editor-RHI.dll!FTaskGraphInterface::WaitUntilTaskCompletes(const TRefCountPtr<FGraphEvent> & Task, ENamedThreads::Type CurrentThreadIfKnown) 行 380	C++
 	UE4Editor-RHI.dll!FRHICommandListExecutor::WaitOnRHIThreadFence(TRefCountPtr<FGraphEvent> & Fence) 行 854	C++
 	UE4Editor-RHI.dll!FRHICommandList::EndDrawingViewport(FRHIViewport * Viewport, bool bPresent, bool bLockToVsync) 行 1595	C++
 	UE4Editor-SlateRHIRenderer.dll!FSlateRHIRenderer::DrawWindow_RenderThread(FRHICommandListImmediate & RHICmdList, FViewportInfo & ViewportInfo, FSlateWindowElementList & WindowElementList, const FSlateDrawWindowCommandParams & DrawCommandParams) 行 1111	C++
 	[内联框架] UE4Editor-SlateRHIRenderer.dll!FSlateRHIRenderer::DrawWindows_Private::__l26::<lambda_2a9c0a9fed59e8507bcd2f8c06e1febe>::operator()(FRHICommandListImmediate &) 行 1277	C++
 	UE4Editor-SlateRHIRenderer.dll!TEnqueueUniqueRenderCommandType<`FSlateRHIRenderer::DrawWindows_Private'::`26'::SlateDrawWindowsCommandName,<lambda_2a9c0a9fed59e8507bcd2f8c06e1febe> >::DoTask(ENamedThreads::Type CurrentThread, const TRefCountPtr<FGraphEvent> & MyCompletionGraphEvent) 行 198	C++
 	UE4Editor-SlateRHIRenderer.dll!TGraphTask<TEnqueueUniqueRenderCommandType<`FSlateRHIRenderer::DrawWindows_Private'::`26'::SlateDrawWindowsCommandName,<lambda_2a9c0a9fed59e8507bcd2f8c06e1febe> > >::ExecuteTask(TArray<FBaseGraphTask *,TSizedDefaultAllocator<32> > & NewTasks, ENamedThreads::Type CurrentThread) 行 847	C++
 	[内联框架] UE4Editor-Core.dll!FBaseGraphTask::Execute(TArray<FBaseGraphTask *,TSizedDefaultAllocator<32> > & CurrentThread, ENamedThreads::Type) 行 514	C++
 	UE4Editor-Core.dll!FNamedTaskThread::ProcessTasksNamedThread(int QueueIndex, bool bAllowStall) 行 686	C++
 	UE4Editor-Core.dll!FNamedTaskThread::ProcessTasksUntilQuit(int QueueIndex) 行 583	C++
 	UE4Editor-RenderCore.dll!RenderingThreadMain(FEvent * TaskGraphBoundSyncEvent) 行 340	C++
 	UE4Editor-RenderCore.dll!FRenderingThread::Run() 行 490	C++
 	UE4Editor-Core.dll!FRunnableThreadWin::Run() 行 96	C++
 	UE4Editor-Core.dll!FRunnableThreadWin::GuardedRun() 行 45	C++
```

### Render阻塞等待RHI线程的命令执行完毕
一般发生在LockBuffer(READ_ONLY)的时候。

给线程创建Event.
```
struct FThreadTaskQueue
{
    /** Event that this thread blocks on when it runs out of work. **/
    FEvent* StallRestartEvent;
    /** We need to disallow reentry of the processing loop **/
    uint32 RecursionGuard;
    /** Indicates we executed a return task, so break out of the processing loop. **/
    bool QuitForShutdown;
    /** Should we stall for tuning? **/
    bool bStallForTuning;
    FCriticalSection StallForTuning;

    FThreadTaskQueue()
        : StallRestartEvent(FPlatformProcess::GetSynchEventFromPool(false))
        , RecursionGuard(0)
        , QuitForShutdown(false)
        , bStallForTuning(false)
    {

    }
    ~FThreadTaskQueue()
    {
        FPlatformProcess::ReturnSynchEventToPool(StallRestartEvent);
        StallRestartEvent = nullptr;
    }
};

class FTaskThreadAnyThread : public FTaskThreadBase
{
	FBaseGraphTask* FindWork();

	/** Array of queues, only the first one is used for unnamed threads. **/
	FThreadTaskQueue Queue;

	int32 PriorityIndex;
};
```

通过StallRestartEvent完成，Render线程等待RHI线程的命令执行完毕。

RHI线程的命令执行完之后，会执行StallRestartEvent->Trigger(), 从而SetEvent.
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

而Render线程在waitRHIComplete中这样执行，在这个里面WaitForSingleObject.
```
FTaskGraphInterface::Get().WaitUntilTaskCompletes(RHIThreadTask, RenderThread_Local);
```

Render线程等待RHI线程的调用栈是这样的
```
>	UE4Editor-RHI.dll!FRHICommandListBase::WaitForRHIThreadTasks() 行 1879	C++
 	UE4Editor-Renderer.dll!FDeferredShadingSceneRenderer::Render(FRHICommandListImmediate & RHICmdList) 行 1147	C++
 	UE4Editor-Renderer.dll!RenderViewFamily_RenderThread(FRHICommandListImmediate & RHICmdList, FSceneRenderer * SceneRenderer) 行 3477	C++
 	[内联框架] UE4Editor-Renderer.dll!FRendererModule::BeginRenderingViewFamily::__l35::<lambda_7df85a902bb55f107fd3c0b688678f3c>::operator()(FRHICommandListImmediate &) 行 3702	C++
 	[内联框架] UE4Editor-Renderer.dll!TEnqueueUniqueRenderCommandType<`FRendererModule::BeginRenderingViewFamily'::`35'::FDrawSceneCommandName,<lambda_7df85a902bb55f107fd3c0b688678f3c> >::DoTask(ENamedThreads::Type) 行 198	C++
 	UE4Editor-Renderer.dll!TGraphTask<TEnqueueUniqueRenderCommandType<`FRendererModule::BeginRenderingViewFamily'::`35'::FDrawSceneCommandName,<lambda_7df85a902bb55f107fd3c0b688678f3c> > >::ExecuteTask(TArray<FBaseGraphTask *,TSizedDefaultAllocator<32> > & NewTasks, ENamedThreads::Type CurrentThread) 行 847	C++
 	[内联框架] UE4Editor-Core.dll!FBaseGraphTask::Execute(TArray<FBaseGraphTask *,TSizedDefaultAllocator<32> > & CurrentThread, ENamedThreads::Type) 行 514	C++
 	UE4Editor-Core.dll!FNamedTaskThread::ProcessTasksNamedThread(int QueueIndex, bool bAllowStall) 行 686	C++
 	UE4Editor-Core.dll!FNamedTaskThread::ProcessTasksUntilQuit(int QueueIndex) 行 583	C++

```
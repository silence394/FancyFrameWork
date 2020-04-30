
RHI结构的文章中里讲到了，RHICommandList通过执行GetContext()->SetTexture()或GDynamicRHI->CreateVertexBuffer()来调用RHI的API。

下面我们详细看一下CommandList的结构是怎么样的。

感觉有点晕，有的是直接调用的，有的是封装到命令队列里的。

### FRHICommandListBase
主要做的是同步对命令的分配等相关的工作。
```
void QueueAsyncCommandListSubmit(FGraphEventRef& AnyThreadCompletionEvent, class FRHICommandList* CmdList);
void QueueParallelAsyncCommandListSubmit(FGraphEventRef* AnyThreadCompletionEvents, bool bIsPrepass, class FRHICommandList** CmdLists, int32* NumDrawsIfKnown, int32 Num, int32 MinDrawsPerTranslate, bool bSpewMerge);
void QueueRenderThreadCommandListSubmit(FGraphEventRef& RenderThreadCompletionEvent, class FRHICommandList* CmdList);
void QueueCommandListSubmit(class FRHICommandList* CmdList);
void AddDispatchPrerequisite(const FGraphEventRef& Prereq);
void WaitForTasks(bool bKnownToBeComplete = false);
void WaitForDispatch();
void WaitForRHIThreadTasks();
void HandleRTThreadTaskCompletion(const FGraphEventRef& MyCompletionGraphEvent);


FORCEINLINE_DEBUGGABLE void* AllocCommand(int32 AllocSize, int32 Alignment)
{
    checkSlow(!IsExecuting());
    FRHICommandBase* Result = (FRHICommandBase*) MemManager.Alloc(AllocSize, Alignment);
    ++NumCommands;
    *CommandLink = Result;
    CommandLink = &Result->Next;
    return Result;
}

FORCEINLINE void ExchangeCmdList(FRHICommandListBase& Other)
{
    check(!RTTasks.Num() && !Other.RTTasks.Num());
    FMemory::Memswap(this, &Other, sizeof(FRHICommandListBase));
    if (CommandLink == &Other.Root)
    {
        CommandLink = &Root;
    }
    if (Other.CommandLink == &Root)
    {
        Other.CommandLink = &Other.Root;
    }
}

private:
	FRHICommandBase* Root;
	FRHICommandBase** CommandLink;
	bool bExecuting;
	uint32 NumCommands;
	uint32 UID;
	IRHICommandContext* Context;
	IRHIComputeContext* ComputeContext;
	FMemStackBase MemManager; 
	FGraphEventArray RTTasks;

	friend class FRHICommandListExecutor;
	friend class FRHICommandListIterator;
	friend class FRHICommandListScopedFlushAndExecute;
```

### FRHICommandListImmediate
对应DX12中的D3D12_COMMAND_LIST_TYPE_DIRECT，跟渲染相关的命令。
``` c++
public:
void ImmediateFlush(EImmediateFlushType::Type FlushType);

FORCEINLINE FSamplerStateRHIRef CreateSamplerState(const FSamplerStateInitializerRHI& Initializer)
{
    LLM_SCOPE(ELLMTag::RHIMisc);
    return RHICreateSamplerState(Initializer);
}

	
FORCEINLINE FPixelShaderRHIRef CreatePixelShader(FRHIShaderLibrary* Library, FSHAHash Hash)
{
    LLM_SCOPE(ELLMTag::Shaders);
    return GDynamicRHI->CreatePixelShader_RenderThread(*this, Library, Hash);
}

FORCEINLINE void* LockVertexBuffer(FRHIVertexBuffer* VertexBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
{
    return GDynamicRHI->RHILockVertexBuffer(*this, VertexBuffer, Offset, SizeRHI, LockMode);
}
	

FORCEINLINE FGraphicsPipelineStateRHIRef CreateGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer)
{
    LLM_SCOPE(ELLMTag::Shaders);
    return RHICreateGraphicsPipelineState(Initializer);

    // 	virtual FGraphicsPipelineStateRHIRef RHICreateGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer)
	// {
	// 	return new FRHIGraphicsPipelineStateFallBack(Initializer);
	// }
}
// 有个规律，使用GDynamicRHI的方法中，lock之类的走的是->RHILockVertexBuffer,而create相关的走的是createXXX_RenderThread. 这块要弄清楚。

FORCEINLINE void ExecuteCommandList(FRHICommandList* CmdList)
{
    FScopedRHIThreadStaller StallRHIThread(*this);
    GDynamicRHI->RHIExecuteCommandList(CmdList);
}

对调用多次的函数进行了封装，减少写的次数
如 GetContext()
{
    GDynamicRHI->GetContext();
}

```

### FRHIAsyncComputeCommandList
对应DX12中的D3D12_COMMAND_LIST_TYPE_COMPUTE
```

FORCEINLINE_DEBUGGABLE void SetShaderUniformBuffer(FRHIComputeShader* Shader, uint32 BaseIndex, FRHIUniformBuffer* UniformBuffer)
{
    if (Bypass())
    {
        GetComputeContext().RHISetShaderUniformBuffer(Shader, BaseIndex, UniformBuffer);
        return;
    }
    ALLOC_COMMAND(FRHICommandSetShaderUniformBuffer<FRHIComputeShader, ECmdList::ECompute>)(Shader, BaseIndex, UniformBuffer);
}

FORCEINLINE_DEBUGGABLE void SetShaderTexture(FRHIComputeShader* Shader, uint32 TextureIndex, FRHITexture* Texture)
{
    if (Bypass())
    {
        GetComputeContext().RHISetShaderTexture(Shader, TextureIndex, Texture);
        return;
    }
    ALLOC_COMMAND(FRHICommandSetShaderTexture<FRHIComputeShader, ECmdList::ECompute>)(Shader, TextureIndex, Texture);
}


FORCEINLINE_DEBUGGABLE void SetComputePipelineState(FComputePipelineState* ComputePipelineState)
{
    if (Bypass())
    {
        extern FRHIComputePipelineState* ExecuteSetComputePipelineState(FComputePipelineState* ComputePipelineState);
        FRHIComputePipelineState* RHIComputePipelineState = ExecuteSetComputePipelineState(ComputePipelineState);
        GetComputeContext().RHISetComputePipelineState(RHIComputePipelineState);
        return;
    }
    ALLOC_COMMAND(FRHICommandSetComputePipelineState<ECmdList::ECompute>)(ComputePipelineState);
}

FORCEINLINE_DEBUGGABLE void WaitComputeFence(FRHIComputeFence* WaitFence)
{
    if (Bypass())
    {
        GetComputeContext().RHIWaitComputeFence(WaitFence);
        return;
    }
    ALLOC_COMMAND(FRHICommandWaitComputeFence<ECmdList::ECompute>)(WaitFence);
}

FORCEINLINE_DEBUGGABLE void WriteGPUFence(FRHIGPUFence* Fence)
{
    if (Bypass())
    {
        GetComputeContext().RHIWriteGPUFence(Fence);
        return;
    }
    ALLOC_COMMAND(FRHICommandWriteGPUFence<ECmdList::ECompute>)(Fence);
}
```

### Command
对DX11，修改 GRHISupportsRHIThread = !!EXPERIMENTAL_D3D11_RHITHREA; 为 GRHISupportsRHIThread = true;
修改之后，可以看UE的Command封装了。首先看CommandList如何操作
```
/*FORCEINLINE_DEBUGGABLE */void DrawPrimitive(uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances)
{
    //check(IsOutsideRenderPass());
    if (Bypass())
    {
        GetContext().RHIDrawPrimitive(BaseVertexIndex, NumPrimitives, NumInstances);
        return;
    }
    ALLOC_COMMAND(FRHICommandDrawPrimitive)(BaseVertexIndex, NumPrimitives, NumInstances);
}

#define ALLOC_COMMAND(...) new ( AllocCommand(sizeof(__VA_ARGS__), alignof(__VA_ARGS__)) ) __VA_ARGS__

FORCEINLINE_DEBUGGABLE void* AllocCommand(int32 AllocSize, int32 Alignment)
{
    checkSlow(!IsExecuting());
    FRHICommandBase* Result = (FRHICommandBase*) MemManager.Alloc(AllocSize, Alignment);
    ++NumCommands;
    *CommandLink = Result;
    CommandLink = &Result->Next;
    return Result;
}
```

我们看一下 FRHICommandDrawPrimitive是什么？
```
FRHICOMMAND_MACRO(FRHICommandDrawPrimitive)
{
	uint32 BaseVertexIndex;
	uint32 NumPrimitives;
	uint32 NumInstances;
	FORCEINLINE_DEBUGGABLE FRHICommandDrawPrimitive(uint32 InBaseVertexIndex, uint32 InNumPrimitives, uint32 InNumInstances)
		: BaseVertexIndex(InBaseVertexIndex)
		, NumPrimitives(InNumPrimitives)
		, NumInstances(InNumInstances)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

#define FRHICOMMAND_MACRO(CommandName)								\
struct PREPROCESSOR_JOIN(CommandName##String, __LINE__)				\
{																	\
	static const TCHAR* TStr() { return TEXT(#CommandName); }		\
};																	\
struct CommandName final : public FRHICommand<CommandName, PREPROCESSOR_JOIN(CommandName##String, __LINE__)>

翻译一下就是

```
struct FRHICommandDrawPrimitiveString_linenunber
{
    static const TCHAR* TStr() { return TEXT(FRHICommandDrawPrimitiveString); }
}

struct FRHICommandDrawPrimitive final : public FRHICommand<FRHICommandDrawPrimitive, FRHICommandDrawPrimitiveString_linenunber)>
```

那FRHICommand是什么呢？h很简单就是执行ExecuteAndDestruct，调用ThisCmd->Execute(CmdList);完成使命。然后销毁自己。
```
struct FRHICommandBase
{
	FRHICommandBase* Next = nullptr;
	virtual void ExecuteAndDestruct(FRHICommandListBase& CmdList, FRHICommandListDebugContext& DebugContext) = 0;
};

template<typename TCmd, typename NameType = FUnnamedRhiCommand>
struct FRHICommand : public FRHICommandBase
{
#if RHICOMMAND_CALLSTACK
	uint64 StackFrames[16];

	FRHICommand()
	{
		FPlatformStackWalk::CaptureStackBackTrace(StackFrames, 16);
	}
#endif

	void ExecuteAndDestruct(FRHICommandListBase& CmdList, FRHICommandListDebugContext& Context) override final
	{
#if CPUPROFILERTRACE_ENABLED
		static uint16 __CpuProfilerEventSpecId;
		if (__CpuProfilerEventSpecId == 0)
		{
			__CpuProfilerEventSpecId = FCpuProfilerTrace::OutputEventType(NameType::TStr(), CpuProfilerGroup_Default);
		}

		extern RHI_API int32 GRHICmdTraceEvents;
		struct FConditionalTraceScope
		{
			FConditionalTraceScope(const uint16 InSpecId) : SpecId(InSpecId)
			{
				if (SpecId) FCpuProfilerTrace::OutputBeginEvent(SpecId);
			}
			~FConditionalTraceScope()
			{
				if (SpecId) FCpuProfilerTrace::OutputEndEvent();
			}
			const uint16 SpecId;
		} TraceScope(GRHICmdTraceEvents ? __CpuProfilerEventSpecId : 0);
#endif // CPUPROFILERTRACE_ENABLED

		TCmd *ThisCmd = static_cast<TCmd*>(this);
#if RHI_COMMAND_LIST_DEBUG_TRACES
		ThisCmd->StoreDebugInfo(Context);
#endif
		ThisCmd->Execute(CmdList);
		ThisCmd->~TCmd();
	}

	virtual void StoreDebugInfo(FRHICommandListDebugContext& Context) {};
};
```
很好理解把命令做个封装，然后把数据填到Command里。

```
UE4Editor-D3D11RHI.dll!FD3D11DynamicRHI::RHIDrawPrimitive(unsigned int BaseVertexIndex, unsigned int NumPrimitives, unsigned int NumInstances) 行 1588	C++
[内联框架] UE4Editor-RHI.dll!FRHICommandDrawPrimitive::Execute(FRHICommandListBase &) 行 235	C++
UE4Editor-RHI.dll!FRHICommand<FRHICommandDrawPrimitive,FRHICommandDrawPrimitiveString965>::ExecuteAndDestruct(FRHICommandListBase & CmdList, FRHICommandListDebugContext & Context) 行 726	C++
>	UE4Editor-RHI.dll!FRHICommandListExecutor::ExecuteInner_DoExecute(FRHICommandListBase & CmdList) 行 351	C++
UE4Editor-RHI.dll!FRHICommandListExecutor::ExecuteInner(FRHICommandListBase & CmdList) 行 622	C++
UE4Editor-RHI.dll!FRHICommandListExecutor::ExecuteList(FRHICommandListBase & CmdList) 行 645	C++
[内联框架] UE4Editor-RHI.dll!FRHICommandListBase::Flush() 行 21	C++
[内联框架] UE4Editor-RHI.dll!FRHICommandListBase::{dtor}() 行 878	C++
UE4Editor-RHI.dll!FRHICommandWaitForAndSubmitSubList::Execute(FRHICommandListBase & CmdList) 行 1078	C++
UE4Editor-RHI.dll!FRHICommand<FRHICommandWaitForAndSubmitSubList,FRHICommandWaitForAndSubmitSubListString1044>::ExecuteAndDestruct(FRHICommandListBase & CmdList, FRHICommandListDebugContext & Context) 行 726	C++
UE4Editor-RHI.dll!FRHICommandListExecutor::ExecuteInner_DoExecute(FRHICommandListBase & CmdList) 行 351	C++

从 Flush开始

FORCEINLINE_DEBUGGABLE void FRHICommandListBase::Flush()
{
	if (HasCommands())
	{
		check(!IsImmediate());
		GRHICommandList.ExecuteList(*this);
	}
}


在 >	UE4Editor-RHI.dll!FRHICommandListExecutor::ExecuteInner(FRHICommandListBase & CmdList)中，需要先交换CommandList
static_assert(sizeof(FRHICommandList) == sizeof(FRHICommandListImmediate), "We are memswapping FRHICommandList and FRHICommandListImmediate; they need to be swappable.");
SwapCmdList->ExchangeCmdList(CmdList);
CmdList.GPUMask = SwapCmdList->GPUMask;

并将任务交给了一个Task执行，这块的意思是等RenderThreadSublistDispatchTask / RHIThreadTask的任务执行完，再执行FDispatchRHIThreadTask/FExecuteRHIThreadTask任务
//if we use a FDispatchRHIThreadTask, we must have it pass an event along to the FExecuteRHIThreadTask it will spawn so that fences can know which event to wait on for execution completion
//before the dispatch completes.
//if we use a FExecuteRHIThreadTask directly we pass the same event just to keep things consistent.
if (AllOutstandingTasks.Num() || RenderThreadSublistDispatchTask.GetReference())
{
    Prereq.Append(AllOutstandingTasks);
    AllOutstandingTasks.Reset();
    if (RenderThreadSublistDispatchTask.GetReference())
    {
        Prereq.Add(RenderThreadSublistDispatchTask);
    }
    RenderThreadSublistDispatchTask = TGraphTask<FDispatchRHIThreadTask>::CreateTask(&Prereq, ENamedThreads::GetRenderThread()).ConstructAndDispatchWhenReady(SwapCmdList, bAsyncSubmit);
}
else
{
    check(!RenderThreadSublistDispatchTask.GetReference()); // if we are doing submits, there better not be any of these in flight since then the RHIThreadTask would get out of order.
    if (RHIThreadTask.GetReference())
    {
        Prereq.Add(RHIThreadTask);
    }
    PrevRHIThreadTask = RHIThreadTask;
    RHIThreadTask = TGraphTask<FExecuteRHIThreadTask>::CreateTask(&Prereq, ENamedThreads::GetRenderThread()).ConstructAndDispatchWhenReady(SwapCmdList);
}

之后会由TaskGraph执行CommandList。

UE4Editor-RHI.dll!FRHICommandListExecutor::ExecuteInner_DoExecute(FRHICommandListBase & CmdList) 行 351	C++
UE4Editor-RHI.dll!FExecuteRHIThreadTask::DoTask(ENamedThreads::Type CurrentThread, const TRefCountPtr<FGraphEvent> & MyCompletionGraphEvent) 行 410	C++
UE4Editor-RHI.dll!TGraphTask<FExecuteRHIThreadTask>::ExecuteTask(TArray<FBaseGraphTask *,TSizedDefaultAllocator<32> > & NewTasks, ENamedThreads::Type CurrentThread) 行 847	C++

void FRHICommandListExecutor::ExecuteInner_DoExecute(FRHICommandListBase& CmdList)
{
    while (Iter.HasCommandsLeft())
    {
        FRHICommandBase* Cmd = Iter.NextCommand();
        GCurrentCommand = Cmd;
        //FPlatformMisc::Prefetch(Cmd->Next);
        Cmd->ExecuteAndDestruct(CmdList, DebugContext);
    }

    CmdList.Reset();
}
```
ExchangeCmdList这块无所顾虑，因为是一个swap的是一个空的commandList。所以不需要同步。

看了TaskGraph还没什么发现，
- https://zhuanlan.zhihu.com/p/38881269

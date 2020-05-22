
RHI结构的文章中里讲到了，RHICommandList通过执行GetContext()->SetTexture()或GDynamicRHI->CreateVertexBuffer()来调用RHI的API。

下面我们详细看一下CommandList的结构是怎么样的。

感觉有点晕，有的是直接调用的，有的是封装到命令队列里的。

### FRHICommandListBase
主要做的是同步对命令的分配等相关的工作。
```
class RHI_API FRHICommandListBase : public FNoncopyable

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

### FRHICommandList
```
class RHI_API FRHICommandList : public FRHICommandListBase
{
public:
    template <typename TShaderRHI>
	FORCEINLINE_DEBUGGABLE void SetShaderParameter(TShaderRHI* Shader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
	{
		//check(IsOutsideRenderPass());
		if (Bypass())
		{
			GetContext().RHISetShaderParameter(Shader, BufferIndex, BaseIndex, NumBytes, NewValue);
			return;
		}
		void* UseValue = Alloc(NumBytes, 16);
		FMemory::Memcpy(UseValue, NewValue, NumBytes);
		ALLOC_COMMAND(FRHICommandSetShaderParameter<TShaderRHI, ECmdList::EGfx>)(Shader, BufferIndex, BaseIndex, NumBytes, UseValue);
	}
}

	FORCEINLINE_DEBUGGABLE void DrawPrimitive(uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances)
	{
		//check(IsOutsideRenderPass());
		if (Bypass())
		{
			GetContext().RHIDrawPrimitive(BaseVertexIndex, NumPrimitives, NumInstances);
			return;
		}
		ALLOC_COMMAND(FRHICommandDrawPrimitive)(BaseVertexIndex, NumPrimitives, NumInstances);
	}

    FORCEINLINE_DEBUGGABLE void SetGraphicsPipelineState(class FGraphicsPipelineState* GraphicsPipelineState)
	{
		//check(IsOutsideRenderPass());
		if (Bypass())
		{
			extern RHI_API FRHIGraphicsPipelineState* ExecuteSetGraphicsPipelineState(class FGraphicsPipelineState* GraphicsPipelineState);
			FRHIGraphicsPipelineState* RHIGraphicsPipelineState = ExecuteSetGraphicsPipelineState(GraphicsPipelineState);
			GetContext().RHISetGraphicsPipelineState(RHIGraphicsPipelineState);
			return;
		}
		ALLOC_COMMAND(FRHICommandSetGraphicsPipelineState)(GraphicsPipelineState);
	}
```

### VertexBuffer的Create和Lock
```
FVertexBufferRHIRef FOpenGLDynamicRHI::RHICreateVertexBuffer(uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	const void *Data = NULL;

	// If a resource array was provided for the resource, create the resource pre-populated
	if(CreateInfo.ResourceArray)
	{
		check(Size == CreateInfo.ResourceArray->GetResourceDataSize());
		Data = CreateInfo.ResourceArray->GetResourceData();
	}

	TRefCountPtr<FOpenGLVertexBuffer> VertexBuffer = new FOpenGLVertexBuffer(0, Size, InUsage, Data);
	
	if (CreateInfo.ResourceArray)
	{
		CreateInfo.ResourceArray->Discard();
	}
	
	return VertexBuffer.GetReference();
}

TOpenGLBuffer(uint32 InStride,uint32 InSize,uint32 InUsage,
		const void *InData = NULL, bool bStreamedDraw = false, GLuint ResourceToUse = 0, uint32 ResourceSize = 0)
	: BaseType(InStride,InSize,InUsage)
	, Resource(0)
	, ModificationCount(0)
	, bIsLocked(false)
	, bIsLockReadOnly(false)
	, bStreamDraw(bStreamedDraw)
	, bLockBufferWasAllocated(false)
	, LockSize(0)
	, LockOffset(0)
	, LockBuffer(NULL)
	, RealSize(InSize)
	{

		RealSize = ResourceSize ? ResourceSize : InSize;

		if( (FOpenGL::SupportsVertexAttribBinding() && OpenGLConsoleVariables::bUseVAB) || !( InUsage & BUF_ZeroStride ) )
		{
			FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

			if (ShouldRunGLRenderContextOpOnThisThread(RHICmdList))
			{
				CreateGLBuffer(InData, ResourceToUse, ResourceSize);
			}
			else
			{
				void* BuffData = nullptr;
				if (InData)
				{
					BuffData = RHICmdList.Alloc(RealSize, 16);
					FMemory::Memcpy(BuffData, InData, RealSize);
				}
				TransitionFence.Reset();
				ALLOC_COMMAND_CL(RHICmdList, FRHICommandGLCommand)([=]() 
				{
					CreateGLBuffer(BuffData, ResourceToUse, ResourceSize); 
					TransitionFence.WriteAssertFence();
				});
				TransitionFence.SetRHIThreadFence();

			}
		}
	}



void* FOpenGLDynamicRHI::LockVertexBuffer_BottomOfPipe(FRHICommandListImmediate& RHICmdList, FRHIVertexBuffer* VertexBufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	check(Size > 0);
	RHITHREAD_GLCOMMAND_PROLOGUE();

	VERIFY_GL_SCOPE();
	FOpenGLVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
	if( !(FOpenGL::SupportsVertexAttribBinding() && OpenGLConsoleVariables::bUseVAB) && VertexBuffer->GetUsage() & BUF_ZeroStride )
	{
		check( Offset + Size <= VertexBuffer->GetSize() );
		// We assume we are only using the first elements from the VB, so we can later on read this memory and create an expanded version of this zero stride buffer
		check(Offset == 0);
		return (void*)( (uint8*)VertexBuffer->GetZeroStrideBuffer() + Offset );
	}
	else
	{
		if (VertexBuffer->IsDynamic() && LockMode == EResourceLockMode::RLM_WriteOnly)
		{
			void *Staging = GetAllocation(VertexBuffer, Size, Offset);
			if (Staging)
			{
				return Staging;
			}
		}
		return (void*)VertexBuffer->Lock(Offset, Size, LockMode == EResourceLockMode::RLM_ReadOnly, VertexBuffer->IsDynamic());
	}
	RHITHREAD_GLCOMMAND_EPILOGUE_RETURN(void*);
}

#define RHITHREAD_GLCOMMAND_PROLOGUE() auto GLCommand= [&]() {

#define RHITHREAD_GLCOMMAND_EPILOGUE_RETURN(x) };\
    if (RHICmdList.Bypass() ||  !IsRunningRHIInSeparateThread() || IsInRHIThread())\
    {\
        return GLCommand();\
    }\
    else\
    {\
        x ReturnValue = (x)0;\
        ALLOC_COMMAND_CL(RHICmdList, FRHICommandGLCommand)([&ReturnValue, GLCommand = MoveTemp(GLCommand)]() { ReturnValue = GLCommand(); }); \
        RHITHREAD_GLTRACE_BLOCKING;\
        RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);\
        return ReturnValue;\
    }\


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
break;
```

### FRHICommandListImmediate
对应DX12中的D3D12_COMMAND_LIST_TYPE_DIRECT，跟渲染相关的命令。
``` c++

void* FOpenGLDynamicRHI::LockVertexBuffer_BottomOfPipe(FRHICommandListImmediate& RHICmdList, FRHIVertexBuffer* VertexBufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	check(Size > 0);
	RHITHREAD_GLCOMMAND_PROLOGUE();

	VERIFY_GL_SCOPE();
	FOpenGLVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
	if( !(FOpenGL::SupportsVertexAttribBinding() && OpenGLConsoleVariables::bUseVAB) && VertexBuffer->GetUsage() & BUF_ZeroStride )
	{
		check( Offset + Size <= VertexBuffer->GetSize() );
		// We assume we are only using the first elements from the VB, so we can later on read this memory and create an expanded version of this zero stride buffer
		check(Offset == 0);
		return (void*)( (uint8*)VertexBuffer->GetZeroStrideBuffer() + Offset );
	}
	else
	{
		if (VertexBuffer->IsDynamic() && LockMode == EResourceLockMode::RLM_WriteOnly)
		{
			void *Staging = GetAllocation(VertexBuffer, Size, Offset);
			if (Staging)
			{
				return Staging;
			}
		}
		return (void*)VertexBuffer->Lock(Offset, Size, LockMode == EResourceLockMode::RLM_ReadOnly, VertexBuffer->IsDynamic());
	}
	RHITHREAD_GLCOMMAND_EPILOGUE_RETURN(void*);
}

#define RHITHREAD_GLCOMMAND_EPILOGUE_RETURN(x) };\
		if (RHICmdList.Bypass() ||  !IsRunningRHIInSeparateThread() || IsInRHIThread())\
		{\
			return GLCommand();\
		}\
		else\
		{\
			x ReturnValue = (x)0;\
			ALLOC_COMMAND_CL(RHICmdList, FRHICommandGLCommand)([&ReturnValue, GLCommand = MoveTemp(GLCommand)]() { ReturnValue = GLCommand(); }); \
			RHITHREAD_GLTRACE_BLOCKING;\
			RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);\
			return ReturnValue;\
		}\

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

void* FOpenGLDynamicRHI:CreateVertexBuffer()
{
    ALLOC_COMMAND_CL(RHICmdList, FRHICommandGLCommand)([=]() 
    {
        CreateGLBuffer(BuffData, ResourceToUse, ResourceSize); 
        TransitionFence.WriteAssertFence();
    });
}


FVertexBufferRHIRef FDynamicRHI::CreateVertexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	CSV_SCOPED_TIMING_STAT(RHITStalls, CreateVertexBuffer_RenderThread);
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateVertexBuffer(Size, InUsage, CreateInfo);
}

class RHI_API FRHICommandListImmediate : public FRHICommandList;

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

struct FRHICommandDrawPrimitive final : public FRHICommand < FRHICommandDrawPrimitive, FRHICommandDrawPrimitiveString_linenunber >

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
	void ExecuteAndDestruct(FRHICommandListBase& CmdList, FRHICommandListDebugContext& Context) override final
	{

		TCmd *ThisCmd = static_cast<TCmd*>(this);
		ThisCmd->Execute(CmdList);
		ThisCmd->~TCmd();
	}
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


RenderingThread等待 RHI线程。
RHI线程的Task等待上一帧的执行完。

void FRHICommandListBase::WaitForRHIThreadTasks()
{
	check(IsImmediate() && IsInRenderingThread());
	bool bAsyncSubmit = CVarRHICmdAsyncRHIThreadDispatch.GetValueOnRenderThread() > 0;
	ENamedThreads::Type RenderThread_Local = ENamedThreads::GetRenderThread_Local();
	if (bAsyncSubmit)
	{
		if (RenderThreadSublistDispatchTask.GetReference() && RenderThreadSublistDispatchTask->IsComplete())
		{
			RenderThreadSublistDispatchTask = nullptr;
		}
		while (RenderThreadSublistDispatchTask.GetReference())
		{
			SCOPE_CYCLE_COUNTER(STAT_ExplicitWaitRHIThread_Dispatch);
			if (FTaskGraphInterface::Get().IsThreadProcessingTasks(RenderThread_Local))
			{
				// we have to spin here because all task threads might be stalled, meaning the fire event anythread task might not be hit.
				// todo, add a third queue
				SCOPE_CYCLE_COUNTER(STAT_SpinWaitRHIThread);
				while (!RenderThreadSublistDispatchTask->IsComplete())
				{
					FPlatformProcess::SleepNoStats(0);
				}
			}
			else
			{
				FTaskGraphInterface::Get().WaitUntilTaskCompletes(RenderThreadSublistDispatchTask, RenderThread_Local);
			}
			if (RenderThreadSublistDispatchTask.GetReference() && RenderThreadSublistDispatchTask->IsComplete())
			{
				RenderThreadSublistDispatchTask = nullptr;
			}
		}
		// now we can safely look at RHIThreadTask
	}
	if (RHIThreadTask.GetReference() && RHIThreadTask->IsComplete())
	{
		RHIThreadTask = nullptr;
		PrevRHIThreadTask = nullptr;
	}
	while (RHIThreadTask.GetReference())
	{
		SCOPE_CYCLE_COUNTER(STAT_ExplicitWaitRHIThread);
		if (FTaskGraphInterface::Get().IsThreadProcessingTasks(RenderThread_Local))
		{
			// we have to spin here because all task threads might be stalled, meaning the fire event anythread task might not be hit.
			// todo, add a third queue
			SCOPE_CYCLE_COUNTER(STAT_SpinWaitRHIThread);
			while (!RHIThreadTask->IsComplete())
			{
				FPlatformProcess::SleepNoStats(0);
			}
		}
		else
		{
			FTaskGraphInterface::Get().WaitUntilTaskCompletes(RHIThreadTask, RenderThread_Local);
		}
		if (RHIThreadTask.GetReference() && RHIThreadTask->IsComplete())
		{
			RHIThreadTask = nullptr;
			PrevRHIThreadTask = nullptr;
		}
	}
}
```
ExchangeCmdList这块无所顾虑，因为是一个swap的是一个空的commandList。所以不需要同步。

看了TaskGraph还没什么发现，
- https://zhuanlan.zhihu.com/p/38881269

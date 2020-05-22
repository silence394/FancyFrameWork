### VertexBuffer的Create和Lock
- lock会阻塞Render线程
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
```
### Render和RHI线程的同步
- Render线程发布Task，让RHI线程执行。
- RHI线程执行命令，需要等待上一次render发布的命令执行完毕，才能继续执行。
- Render等待上一帧的RHI命令执行完毕。

```
void FRHICommandListExecutor::ExecuteInner(FRHICommandListBase& CmdList)
{
	if (RHIThreadTask.GetReference())
	{
		Prereq.Add(RHIThreadTask);
	}
	PrevRHIThreadTask = RHIThreadTask;
	RHIThreadTask = TGraphTask<FExecuteRHIThreadTask>::CreateTask(&Prereq, ENamedThreads::GetRenderThread()).ConstructAndDispatchWhenReady(SwapCmdList);
}

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

	check(IsImmediate() && IsInRenderingThread());
	if (Bypass())
	{
		GetContext().RHIEndDrawingViewport(Viewport, bPresent, bLockToVsync);
	}
	else
	{
		ALLOC_COMMAND(FRHICommandEndDrawingViewport)(Viewport, bPresent, bLockToVsync);

		if (IsRunningRHIInSeparateThread())
		{
			// Insert a fence to prevent the renderthread getting more than a frame ahead of the RHIThread
			GRHIThreadEndDrawingViewportFences[GRHIThreadEndDrawingViewportFenceIndex] = static_cast<FRHICommandListImmediate*>(this)->RHIThreadFence();
		}
		// if we aren't running an RHIThread, there is no good reason to buffer this frame advance stuff and that complicates state management, so flush everything out now
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_EndDrawingViewport_Dispatch);
			FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
		}
	}

	if (IsRunningRHIInSeparateThread())
	{
		// Wait on the previous frame's RHI thread fence (we never want the rendering thread to get more than a frame ahead)
		uint32 PreviousFrameFenceIndex = 1 - GRHIThreadEndDrawingViewportFenceIndex;
		FGraphEventRef& LastFrameFence = GRHIThreadEndDrawingViewportFences[PreviousFrameFenceIndex];
		FRHICommandListExecutor::WaitOnRHIThreadFence(LastFrameFence);
		GRHIThreadEndDrawingViewportFences[PreviousFrameFenceIndex] = nullptr;
		GRHIThreadEndDrawingViewportFenceIndex = PreviousFrameFenceIndex;
	}

	RHIAdvanceFrameForGetViewportBackBuffer(Viewport);
}
```

### RHI架构
- 结构以D3D11为基础
- 上层只创建RHI
- 全局的commandListExecutor管理Commandlist
- 上层不关心Context.
```
class RHI_API FDynamicRHI
{
	virtual FRasterizerStateRHIRef RHICreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer) = 0;
	virtual FVertexBufferRHIRef CreateVertexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo);
}

class IRHIComputeContext;
class IRHICommandContext : public IRHIComputeContext;
class IRHICommandContextPSOFallback : public IRHICommandContext;

class D3D11RHI_API FD3D11DynamicRHI : public FDynamicRHI, public IRHICommandContextPSOFallback
{
	IRHICommandContext* FD3D11DynamicRHI::RHIGetDefaultContext()
	{
		return this;
	}
}

class FD3D12CommandContextBase : public IRHICommandContext, public FD3D12AdapterChild;
class FD3D12CommandContext : public FD3D12CommandContextBase, public FD3D12DeviceChild;

class FD3D12DynamicRHI : public FDynamicRHI
{
	IRHICommandContext* RHIGetDefaultContext()
	{
		FD3D12Adapter& Adapter = GetAdapter();

		IRHICommandContext* DefaultCommandContext = nullptr;	
		if (GNumExplicitGPUsForRendering > 1)
		{
			DefaultCommandContext = static_cast<IRHICommandContext*>(&Adapter.GetDefaultContextRedirector());
		}
		else // Single GPU path
		{
			FD3D12Device* Device = Adapter.GetDevice(0);
			DefaultCommandContext = static_cast<IRHICommandContext*>(&Device->GetDefaultCommandContext());
		}

		check(DefaultCommandContext);
		return DefaultCommandContext;
	}
}

Adapter、Device都是D3D12独有的封装。
class FD3D12Adapter : public FNoncopyable
{
	// Each of these devices represents a physical GPU 'Node'
	FD3D12Device* Devices[MAX_NUM_GPUS];
}

class FD3D12Device
{
	void CreateCommandContexts();
	inline FD3D12CommandListManager& GetCommandListManager() { return *CommandListManager; }
	inline FD3D12CommandListManager& GetCopyCommandListManager() { return *CopyCommandListManager; }
	inline FD3D12CommandListManager& GetAsyncCommandListManager() { return *AsyncCommandListManager; }
}


void FD3D12DynamicRHIModule::FindAdapter()创建D3D12的Adaptor。

FD3D12DynamicRHI::Init()中调用Adapter->InitializeDevices(); //在这里创建和初始化devices。
			Devices[GPUIndex] = new FD3D12Device(FRHIGPUMask::FromIndex(GPUIndex), this);
			Devices[GPUIndex]->Initialize();
				// Device的initialize中，创建Context.
				CreateCommandContexts();
						const uint32 NumContexts = FTaskGraphInterface::Get().GetNumWorkerThreads() + 1;
						CommandContextArray.Reserve(NumContexts);

 上层只管理RHI。
FDynamicRHI* PlatformCreateDynamicRHI()
FDynamicRHI* FD3D12DynamicRHIModule::CreateRHI(ERHIFeatureLevel::Type RequestedFeatureLevel)

全局的commandlistExecutor
class RHI_API FRHICommandListExecutor
{
	static inline FRHICommandListImmediate& GetImmediateCommandList();
	static inline FRHIAsyncComputeCommandListImmediate& GetImmediateAsyncComputeCommandList();

	void ExecuteList(FRHICommandListBase& CmdList);
	void ExecuteList(FRHICommandListImmediate& CmdList);
	FRHICommandListImmediate CommandListImmediate;
	FRHIAsyncComputeCommandListImmediate AsyncComputeCmdListImmediate;
}

extern RHI_API FRHICommandListExecutor GRHICommandList;
```
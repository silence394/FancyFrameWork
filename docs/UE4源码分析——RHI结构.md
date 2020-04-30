### rhi.h
表示rhi的硬件情况
```
extern RHI_API bool GSupportsShaderFramebufferFetch;

/** true if mobile depth & stencil fetch is supported */
extern RHI_API bool GSupportsShaderDepthStencilFetch;

/** true if RQT_AbsoluteTime is supported by RHICreateRenderQuery */
extern RHI_API bool GSupportsTimestampRenderQueries;

/** true if RQT_AbsoluteTime is supported by RHICreateRenderQuery */
extern RHI_API bool GRHISupportsGPUTimestampBubblesRemoval;

/** true if RHIGetGPUFrameCycles removes CPu generated bubbles. */
extern RHI_API bool GRHISupportsFrameCyclesBubblesRemoval;

/** true if the GPU supports hidden surface removal in hardware. */
extern RHI_API bool GHardwareHiddenSurfaceRemoval;

/** true if the RHI supports asynchronous creation of texture resources */
extern RHI_API bool GRHISupportsAsyncTextureCreation;

/** true if the RHI supports quad topology (PT_QuadList). */
extern RHI_API bool GRHISupportsQuadTopology;

/** true if the RHI supports rectangular topology (PT_RectList). */
extern RHI_API bool GRHISupportsRectTopology;

/** true if the RHI supports 64 bit uint atomics. */
extern RHI_API bool GRHISupportsAtomicUInt64;

extern RHI_API int32 GMaxTextureSamplers;
FORCEINLINE uint32 GetMaxTextureSamplers()
{
	return GMaxTextureSamplers;
}


class FBlendStateInitializerRHI
{
public:

	struct FRenderTarget
	{
		enum
		{
			NUM_STRING_FIELDS = 7
		};
		TEnumAsByte<EBlendOperation> ColorBlendOp;
		TEnumAsByte<EBlendFactor> ColorSrcBlend;
		TEnumAsByte<EBlendFactor> ColorDestBlend;
		TEnumAsByte<EBlendOperation> AlphaBlendOp;
		TEnumAsByte<EBlendFactor> AlphaSrcBlend;
		TEnumAsByte<EBlendFactor> AlphaDestBlend;
		TEnumAsByte<EColorWriteMask> ColorWriteMask;
		
		FRenderTarget(
			EBlendOperation InColorBlendOp = BO_Add,
			EBlendFactor InColorSrcBlend = BF_One,
			EBlendFactor InColorDestBlend = BF_Zero,
			EBlendOperation InAlphaBlendOp = BO_Add,
			EBlendFactor InAlphaSrcBlend = BF_One,
			EBlendFactor InAlphaDestBlend = BF_Zero,
			EColorWriteMask InColorWriteMask = CW_RGBA
			)
		: ColorBlendOp(InColorBlendOp)
		, ColorSrcBlend(InColorSrcBlend)
		, ColorDestBlend(InColorDestBlend)
		, AlphaBlendOp(InAlphaBlendOp)
		, AlphaSrcBlend(InAlphaSrcBlend)
		, AlphaDestBlend(InAlphaDestBlend)
		, ColorWriteMask(InColorWriteMask)
		{}
		
		friend FArchive& operator<<(FArchive& Ar,FRenderTarget& RenderTarget)
		{
			Ar << RenderTarget.ColorBlendOp;
			Ar << RenderTarget.ColorSrcBlend;
			Ar << RenderTarget.ColorDestBlend;
			Ar << RenderTarget.AlphaBlendOp;
			Ar << RenderTarget.AlphaSrcBlend;
			Ar << RenderTarget.AlphaDestBlend;
			Ar << RenderTarget.ColorWriteMask;
			return Ar;
		}
		RHI_API FString ToString() const;
		RHI_API void FromString(const TArray<FString>& Parts, int32 Index);


	};


};



struct FRHIDrawIndirectParameters
{
	uint32 VertexCountPerInstance;
	uint32 InstanceCount;
	uint32 StartVertexLocation;
	uint32 StartInstanceLocation;
};

```
### DynamicRHI.H
RHI的接口：创建资源、lock、unlock、update
- 资源释放的地方在哪儿呢？（用析构触发，而不是release函数）
```

	virtual FGraphicsPipelineStateRHIRef RHICreateGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer)
	{
		return new FRHIGraphicsPipelineStateFallBack(Initializer);
	}

extern RHI_API FDynamicRHI* GDynamicRHI;

FORCEINLINE FSamplerStateRHIRef RHICreateSamplerState(const FSamplerStateInitializerRHI& Initializer)
{
	return GDynamicRHI->RHICreateSamplerState(Initializer);
}

FORCEINLINE FRasterizerStateRHIRef RHICreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer)
{
	return GDynamicRHI->RHICreateRasterizerState(Initializer);
}

class RHI_API FDynamicRHI
{
public:
    virtual void Init() = 0;
    virtual FSamplerStateRHIRef RHICreateSamplerState(const FSamplerStateInitializerRHI& Initializer) = 0;

	// FlushType: Thread safe
	virtual FRasterizerStateRHIRef RHICreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer) = 0;

	// FlushType: Thread safe
	virtual FDepthStencilStateRHIRef RHICreateDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer) = 0;

	// FlushType: Thread safe
	virtual FBlendStateRHIRef RHICreateBlendState(const FBlendStateInitializerRHI& Initializer) = 0;

	// FlushType: Wait RHI Thread
	virtual FVertexDeclarationRHIRef RHICreateVertexDeclaration(const FVertexDeclarationElementList& Elements) = 0;

    virtual FPixelShaderRHIRef RHICreatePixelShader(const TArray<uint8>& Code) = 0;

    virtual FStagingBufferRHIRef RHICreateStagingBuffer()
	{
		return new FGenericRHIStagingBuffer();
	}

    virtual void* RHILockTexture2D(FRHITexture2D* Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail) = 0;

    virtual FVertexBufferRHIRef CreateAndLockVertexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo, void*& OutDataBuffer);
	virtual FIndexBufferRHIRef CreateAndLockIndexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo, void*& OutDataBuffer);

	virtual FVertexBufferRHIRef CreateVertexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo);
	virtual FStructuredBufferRHIRef CreateStructuredBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo);
	virtual FShaderResourceViewRHIRef CreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIVertexBuffer* VertexBuffer, uint32 Stride, uint8 Format);
	virtual FShaderResourceViewRHIRef CreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIIndexBuffer* Buffer);
	virtual FTexture2DRHIRef AsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture2D, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus);
	virtual ETextureReallocationStatus FinalizeAsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture2D, bool bBlockUntilCompleted);
	virtual ETextureReallocationStatus CancelAsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture2D, bool bBlockUntilCompleted);
	virtual FIndexBufferRHIRef CreateIndexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo);
	virtual FVertexShaderRHIRef CreateVertexShader_RenderThread(class FRHICommandListImmediate& RHICmdList, const TArray<uint8>& Code);
	virtual FVertexShaderRHIRef CreateVertexShader_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIShaderLibrary* Library, FSHAHash Hash);
	virtual FPixelShaderRHIRef CreatePixelShader_RenderThread(class FRHICommandListImmediate& RHICmdList, const TArray<uint8>& Code);
	virtual FPixelShaderRHIRef CreatePixelShader_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIShaderLibrary* Library, FSHAHash Hash);
	virtual FGeometryShaderRHIRef CreateGeometryShader_RenderThread(class FRHICommandListImmediate& RHICmdList, const TArray<uint8>& Code);
	virtual FGeometryShaderRHIRef CreateGeometryShader_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIShaderLibrary* Library, FSHAHash Hash);
	virtual FComputeShaderRHIRef CreateComputeShader_RenderThread(class FRHICommandListImmediate& RHICmdList, const TArray<uint8>& Code);
	virtual FComputeShaderRHIRef CreateComputeShader_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIShaderLibrary* Library, FSHAHash Hash);
}
```

### 实现
#### d3d11
```
class D3D11RHI_API FD3D11DynamicRHI : public FDynamicRHI, public IRHICommandContextPSOFallback
{

}

// Command Context for RHIs that do not support real Graphics Pipelines.
class IRHICommandContextPSOFallback : public IRHICommandContext
{
public:
	/**
	* Set bound shader state. This will set the vertex decl/shader, and pixel shader
	* @param BoundShaderState - state resource
	*/
	virtual void RHISetBoundShaderState(FRHIBoundShaderState* BoundShaderState) = 0;

	virtual void RHISetDepthStencilState(FRHIDepthStencilState* NewState, uint32 StencilRef) = 0;

	virtual void RHISetRasterizerState(FRHIRasterizerState* NewState) = 0;

    virtual void RHISetGraphicsPipelineState(FRHIGraphicsPipelineState* GraphicsState) override
	{
		FRHIGraphicsPipelineStateFallBack* FallbackGraphicsState = static_cast<FRHIGraphicsPipelineStateFallBack*>(GraphicsState);
		FGraphicsPipelineStateInitializer& PsoInit = FallbackGraphicsState->Initializer;

		RHISetBoundShaderState(
			RHICreateBoundShaderState(
				PsoInit.BoundShaderState.VertexDeclarationRHI,
				PsoInit.BoundShaderState.VertexShaderRHI,
				PsoInit.BoundShaderState.HullShaderRHI,
				PsoInit.BoundShaderState.DomainShaderRHI,
				PsoInit.BoundShaderState.PixelShaderRHI,
				PsoInit.BoundShaderState.GeometryShaderRHI
			).GetReference()
		);

		RHISetDepthStencilState(FallbackGraphicsState->Initializer.DepthStencilState, 0);
		RHISetRasterizerState(FallbackGraphicsState->Initializer.RasterizerState);
		RHISetBlendState(FallbackGraphicsState->Initializer.BlendState, FLinearColor(1.0f, 1.0f, 1.0f));
		if (GSupportsDepthBoundsTest)
		{
			RHIEnableDepthBoundsTest(FallbackGraphicsState->Initializer.bDepthBounds);
		}
	}
}

这个结构为了使不支持PSO的RHI如DX11，opengl，有途径设置各种state。

实现部分：
FSamplerStateRHIRef FD3D11DynamicRHI::RHICreateSamplerState(const FSamplerStateInitializerRHI& Initializer)
{
    TRefCountPtr<ID3D11SamplerState> SamplerStateHandle;
	VERIFYD3D11RESULT_EX(Direct3DDevice->CreateSamplerState(&SamplerDesc, SamplerStateHandle.GetInitReference()), Direct3DDevice);

    FD3D11SamplerState* SamplerState = new FD3D11SamplerState;
	SamplerState->Resource = SamplerStateHandle;
	// Manually add reference as we control the creation/destructions
	SamplerState->AddRef();
	GSamplerStateCache.Add(SamplerStateHandle.GetReference(), SamplerState);
	return SamplerState;
}

```

#### D3D12
```
上面代码中的RHISetGraphicsPipelineState，FD3D12DynamicRHI中并没有找到。说明跟还有其他的结构在调用。
class FD3D12DynamicRHI : public FDynamicRHI
{
    friend class FD3D12CommandContext;

	static FD3D12DynamicRHI* SingleD3DRHI;
}

FSamplerStateRHIRef FD3D12DynamicRHI::RHICreateSamplerState(const FSamplerStateInitializerRHI& Initializer)
{
	FD3D12Adapter* Adapter = &GetAdapter();

	return Adapter->CreateLinkedObject<FD3D12SamplerState>(FRHIGPUMask::All(), [&](FD3D12Device* Device)
	{
		return Device->CreateSampler(Initializer);
	});
}

FD3D12SamplerState* FD3D12Device::CreateSampler(const FSamplerStateInitializerRHI& Initializer)
{
    TRefCountPtr<FD3D12SamplerState>* PreviouslyCreated = SamplerMap.Find(SamplerDesc);
	if (PreviouslyCreated)
	{
		return PreviouslyCreated->GetReference();
	}
	else
	{
		// 16-bit IDs are used for faster hashing
		check(SamplerID < 0xffff);

		FD3D12SamplerState* NewSampler = new FD3D12SamplerState(this, SamplerDesc, static_cast<uint16>(SamplerID));

		SamplerMap.Add(SamplerDesc, NewSampler);

		SamplerID++;

		return NewSampler;
	}
}
```

RHISetGraphicsPipelineState
```
class IRHICommandContext : public IRHIComputeContext
{
    virtual void RHISetGraphicsPipelineState(FRHIGraphicsPipelineState* GraphicsState) = 0;
}

class FD3D12CommandContext : public FD3D12CommandContextBase, public FD3D12DeviceChild
{
    virtual void RHISetGraphicsPipelineState(FRHIGraphicsPipelineState* GraphicsState);
}

void FD3D12CommandContext::RHISetGraphicsPipelineState(FRHIGraphicsPipelineState* GraphicsState)
{
	FD3D12GraphicsPipelineState* GraphicsPipelineState = FD3D12DynamicRHI::ResourceCast(GraphicsState);

	// TODO: [PSO API] Every thing inside this scope is only necessary to keep the PSO shadow in sync while we convert the high level to only use PSOs
	const bool bWasUsingTessellation = bUsingTessellation;
	bUsingTessellation = GraphicsPipelineState->GetHullShader() && GraphicsPipelineState->GetDomainShader();
	// Ensure the command buffers are reset to reduce the amount of data that needs to be versioned.
	VSConstantBuffer.Reset();
	PSConstantBuffer.Reset();
	HSConstantBuffer.Reset();
	DSConstantBuffer.Reset();
	GSConstantBuffer.Reset();
	// Should this be here or in RHISetComputeShader? Might need a new bDiscardSharedConstants for CS.
	CSConstantBuffer.Reset();
	// @TODO : really should only discard the constants if the shader state has actually changed.
	bDiscardSharedConstants = true;

	if (!GraphicsPipelineState->PipelineStateInitializer.bDepthBounds)
	{
		StateCache.SetDepthBounds(0.0f, 1.0f);
	}

	StateCache.SetGraphicsPipelineState(GraphicsPipelineState, bUsingTessellation != bWasUsingTessellation);
	StateCache.SetStencilRef(0);
}

这个是在RHICommandList中调用的。
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

那么commandlist 与 DX11有什么关系呢？

template <typename TRHIShader>
FORCEINLINE_DEBUGGABLE void SetShaderTexture(TRHIShader* Shader, uint32 TextureIndex, FRHITexture* Texture)
{
    //check(IsOutsideRenderPass());
    if (Bypass())
    {
        GetContext().RHISetShaderTexture(Shader, TextureIndex, Texture);
        return;
    }
    ALLOC_COMMAND(FRHICommandSetShaderTexture<TRHIShader, ECmdList::EGfx>)(Shader, TextureIndex, Texture);
}

这个函数是inline的无法下断点，那么可以下到DynamicRHI里。

GetContext()调用到了DynamicRHI中。

那么查找Context设置的地方。
GRHICommandList.GetImmediateCommandList().SetContext(GDynamicRHI->RHIGetDefaultContext());

IRHICommandContext* FD3D11DynamicRHI::RHIGetDefaultContext()
{
	return this;
}

IRHICommandContext* FD3D12DynamicRHI::RHIGetDefaultContext()
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

IRHICommandContext* FMetalDynamicRHI::RHIGetDefaultContext()
{
	return &ImmediateContext;
}

IRHICommandContext* FOpenGLDynamicRHI::RHIGetDefaultContext()
{
	return this;
}

现在就清楚了，由于opengl和dx11是继承自IRHICommandContextPSOFallback，所以本身就是一个Context，而DX12和Metal需要自己获取context。

class FD3D12Device : public FD3D12SingleNodeGPUObject, public FNoncopyable, public FD3D12AdapterChild
{
    inline FD3D12CommandContext& GetCommandContext(uint32 ThreadIndex = 0) const { return *CommandContextArray[ThreadIndex]; }
}

Metal的是这样的
FMetalRHIImmediateCommandContext ImmediateContext;

所以下一步需要弄清楚Device、Context到底都是什么用途。
```

### OpenGL

```
class OPENGLDRV_API FOpenGLDynamicRHI  final : public FDynamicRHI, public IRHICommandContextPSOFallback
{

}
在OpenGLState.cpp中
FRasterizerStateRHIRef FOpenGLDynamicRHI::RHICreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer)
{
	FOpenGLRasterizerState* RasterizerState = new FOpenGLRasterizerState;
	RasterizerState->Data.CullMode = TranslateCullMode(Initializer.CullMode);
	RasterizerState->Data.FillMode = TranslateFillMode(Initializer.FillMode);
	RasterizerState->Data.DepthBias = Initializer.DepthBias;
	RasterizerState->Data.SlopeScaleDepthBias = Initializer.SlopeScaleDepthBias;
	
	return RasterizerState;
}

void FOpenGLDynamicRHI::RHISetDepthStencilState(FRHIDepthStencilState* NewStateRHI,uint32 StencilRef)
{
	VERIFY_GL_SCOPE();
	FOpenGLDepthStencilState* NewState = ResourceCast(NewStateRHI);
	PendingState.DepthStencilState = NewState->Data;
	PendingState.StencilRef = StencilRef;
}
```

### Resource
```
class FD3D11VertexBuffer : public FRHIVertexBuffer, public FD3D11BaseShaderResource
{
public:

	TRefCountPtr<ID3D11Buffer> Resource;

	FD3D11VertexBuffer() = default;

	FD3D11VertexBuffer(ID3D11Buffer* InResource, uint32 InSize, uint32 InUsage)
	: FRHIVertexBuffer(InSize,InUsage)
	, Resource(InResource)
	{}

	virtual ~FD3D11VertexBuffer()
	{
		if (Resource)
		{
			UpdateBufferStats(Resource, false);
		}
	}
}

class FD3D12VertexBuffer : public FRHIVertexBuffer, public FD3D12BaseShaderResource, public FD3D12TransientResource, public FD3D12LinkedAdapterObject<FD3D12VertexBuffer>
{

}

template <typename BaseType, GLenum Type, BufferBindFunction BufBind>
class TOpenGLBuffer : public BaseType
{

}

```

主要看看DX12是如何组织的。

```
FVertexBufferRHIRef FD3D12DynamicRHI::RHICreateVertexBuffer(uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	if (CreateInfo.bWithoutNativeResource)
	{
		return new FD3D12VertexBuffer();
	}

	const D3D12_RESOURCE_DESC Desc = CreateVertexBufferResourceDesc(Size, InUsage);
	const uint32 Alignment = 4;

	FD3D12VertexBuffer* Buffer = GetAdapter().CreateRHIBuffer<FD3D12VertexBuffer>(nullptr, Desc, Alignment, 0, Size, InUsage, CreateInfo);
	if (Buffer->ResourceLocation.IsTransient() )
	{
		// TODO: this should ideally be set in platform-independent code, since this tracking is for the high level
		Buffer->SetCommitted(false);
	}

	return Buffer;
}

// Create部分看不懂。

void* FD3D12DynamicRHI::RHILockVertexBuffer(FRHICommandListImmediate& RHICmdList, FRHIVertexBuffer* VertexBufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	return LockBuffer(&RHICmdList, FD3D12DynamicRHI::ResourceCast(VertexBufferRHI), Offset, Size, LockMode);
}

void FD3D12DynamicRHI::RHIUnlockVertexBuffer(FRHICommandListImmediate& RHICmdList, FRHIVertexBuffer* VertexBufferRHI)
{
	UnlockBuffer(&RHICmdList, FD3D12DynamicRHI::ResourceCast(VertexBufferRHI));
}

void FD3D12DescriptorCache::SetIndexBuffer(FD3D12IndexBufferCache& Cache)
{
	CmdContext->CommandListHandle.UpdateResidency(Cache.ResidencyHandle);
	CmdContext->CommandListHandle->IASetIndexBuffer(&Cache.CurrentIndexBufferView);
}

void FD3D12DescriptorCache::SetVertexBuffers(FD3D12VertexBufferCache& Cache)
{
	const uint32 Count = Cache.MaxBoundVertexBufferIndex + 1;
	if (Count == 0)
	{
		return; // No-op
	}

	CmdContext->CommandListHandle.UpdateResidency(Cache.ResidencyHandles, Count);
	CmdContext->CommandListHandle->IASetVertexBuffers(0, Count, Cache.CurrentVertexBufferViews);
}

D3D12的StateCache, D3D12Allocator需要研究。D3D12交给用户负责资源的管理。这个需要深入研究。
```

### 官方文档
- https://docs.unrealengine.com/en-US/Programming/Rendering/index.html
#### Graphical Commands and the Command List
The commands enqueued by the Render Thread are instances of structs derived from the FRHICommand template. They override the Execute function, which is called during the translation process, and store any data that they require. Commands can be submitted to the GPU differently on different platforms (for example, submitted multiple times per frame, all at once at the end of the frame, and so on) based on heuristics for that platform's optimal performance, or they can be submitted by issuing the FRHISubmitCommandsHint command.

The main interface used in translation is the IRHICommandContext. There is a derived RHICommandContext for each platform and API. **During translation, the RHICommandList is given an RHICommandContext to operate on, and each command's Execute function calls into the RHICommandContext API.** The command's context is responsible for state-shadowing, validation, and any API-specific details necessary to perform the given operation.

#### Synchronization
Synchronization of the renderer between the GameThread, RenderThread, RHI Thread, and the GPU is a complex topic. At the highest level, Unreal Engine 4 is normally configured as a "single frame behind" renderer. This means that the RenderThread is processing Frame N while the GameThread processes Frame N+1, except in cases where the RenderThread runs faster than the GameThread. The addition of the RHI Thread complicates this slightly, in that the RenderThread is able move ahead of the RHI Thread by completing visibility calculations for Frame N+1 while the RHI Thread is processing Frame N. The end result is that, while the GameThread is processing Frame N+1, the RenderThread may be processing commands for Frame N or Frame N+1, and the RHI Thread may also be translating commands from Frame N or Frame N+1, depending on execution times. These guarantees are arbitrated by FFrameEndSync and the FRHICommandListImmediate function called RHIThreadFence. It is also guaranteed that, regardless of how parallelization is configured, the order of submission of commands to the GPU is unchanged from the order the commands would have been submitted in a single-threaded renderer. This is required to ensure consistency and must be maintained if rendering code is changed.

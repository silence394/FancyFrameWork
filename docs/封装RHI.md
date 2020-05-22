# 封装RHI

###### RHI

​	初始化一些硬件信息

###### IRHIComputeContext

​	RHIPushEvent

​	RHIPopEvent

​	RHISubmitCommandsHint

###### IRHICommandContext：IRHIComputeContext

​	RHIBeginDrawingViewport

​	RHIEndDrawingViewport

​	RHIBeginFrame

​	RHIEndFrame

​	RHIBeginScene

​	RHIEndScene

​	RHISetStreamSource

​	RHISetViewport

​	RHIDrawPrimitive

​	RHIDrawIndexedPrimitive

​	

###### DynamicRHI.h

​	CreateBuffer

​	LockBuffer

​	UnlockBuffer

###### FOpenGLDynamicRHI：DynamicRHI.h，IRHICommandContext

​	

​	
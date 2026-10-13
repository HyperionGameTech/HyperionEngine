
#ifndef DAWN_DAWN_PROC_TABLE_H_
#define DAWN_DAWN_PROC_TABLE_H_

#include "dawn/webgpu.h"

// Note: Often allocated as a static global. Do not add a complex constructor.
typedef struct DawnProcTable {
    uint8_t version[20]{};

    WGPUProcCreateInstance createInstance = nullptr;
    WGPUProcGetInstanceFeatures getInstanceFeatures = nullptr;
    WGPUProcGetInstanceLimits getInstanceLimits = nullptr;
    WGPUProcHasInstanceFeature hasInstanceFeature = nullptr;
    WGPUProcGetProcAddress getProcAddress = nullptr;

    WGPUProcAdapterCreateDevice adapterCreateDevice = nullptr;
    WGPUProcAdapterGetFeatures adapterGetFeatures = nullptr;
    WGPUProcAdapterGetFormatCapabilities adapterGetFormatCapabilities = nullptr;
    WGPUProcAdapterGetInfo adapterGetInfo = nullptr;
    WGPUProcAdapterGetInstance adapterGetInstance = nullptr;
    WGPUProcAdapterGetLimits adapterGetLimits = nullptr;
    WGPUProcAdapterHasFeature adapterHasFeature = nullptr;
    WGPUProcAdapterRequestDevice adapterRequestDevice = nullptr;
    WGPUProcAdapterAddRef adapterAddRef = nullptr;
    WGPUProcAdapterRelease adapterRelease = nullptr;

    WGPUProcAdapterInfoFreeMembers adapterInfoFreeMembers = nullptr;

    WGPUProcAdapterPropertiesMemoryHeapsFreeMembers adapterPropertiesMemoryHeapsFreeMembers = nullptr;

    WGPUProcAdapterPropertiesSubgroupMatrixConfigsFreeMembers adapterPropertiesSubgroupMatrixConfigsFreeMembers = nullptr;

    WGPUProcBindGroupSetLabel bindGroupSetLabel = nullptr;
    WGPUProcBindGroupAddRef bindGroupAddRef = nullptr;
    WGPUProcBindGroupRelease bindGroupRelease = nullptr;

    WGPUProcBindGroupLayoutSetLabel bindGroupLayoutSetLabel = nullptr;
    WGPUProcBindGroupLayoutAddRef bindGroupLayoutAddRef = nullptr;
    WGPUProcBindGroupLayoutRelease bindGroupLayoutRelease = nullptr;

    WGPUProcBufferCreateTexelView bufferCreateTexelView = nullptr;
    WGPUProcBufferDestroy bufferDestroy = nullptr;
    WGPUProcBufferGetConstMappedRange bufferGetConstMappedRange = nullptr;
    WGPUProcBufferGetMappedRange bufferGetMappedRange = nullptr;
    WGPUProcBufferGetMapState bufferGetMapState = nullptr;
    WGPUProcBufferGetSize bufferGetSize = nullptr;
    WGPUProcBufferGetUsage bufferGetUsage = nullptr;
    WGPUProcBufferMapAsync bufferMapAsync = nullptr;
    WGPUProcBufferReadMappedRange bufferReadMappedRange = nullptr;
    WGPUProcBufferSetLabel bufferSetLabel = nullptr;
    WGPUProcBufferUnmap bufferUnmap = nullptr;
    WGPUProcBufferWriteMappedRange bufferWriteMappedRange = nullptr;
    WGPUProcBufferAddRef bufferAddRef = nullptr;
    WGPUProcBufferRelease bufferRelease = nullptr;

    WGPUProcCommandBufferSetLabel commandBufferSetLabel = nullptr;
    WGPUProcCommandBufferAddRef commandBufferAddRef = nullptr;
    WGPUProcCommandBufferRelease commandBufferRelease = nullptr;

    WGPUProcCommandEncoderBeginComputePass commandEncoderBeginComputePass = nullptr;
    WGPUProcCommandEncoderBeginRenderPass commandEncoderBeginRenderPass = nullptr;
    WGPUProcCommandEncoderClearBuffer commandEncoderClearBuffer = nullptr;
    WGPUProcCommandEncoderCopyBufferToBuffer commandEncoderCopyBufferToBuffer = nullptr;
    WGPUProcCommandEncoderCopyBufferToTexture commandEncoderCopyBufferToTexture = nullptr;
    WGPUProcCommandEncoderCopyTextureToBuffer commandEncoderCopyTextureToBuffer = nullptr;
    WGPUProcCommandEncoderCopyTextureToTexture commandEncoderCopyTextureToTexture = nullptr;
    WGPUProcCommandEncoderFinish commandEncoderFinish = nullptr;
    WGPUProcCommandEncoderInjectValidationError commandEncoderInjectValidationError = nullptr;
    WGPUProcCommandEncoderInsertDebugMarker commandEncoderInsertDebugMarker = nullptr;
    WGPUProcCommandEncoderPopDebugGroup commandEncoderPopDebugGroup = nullptr;
    WGPUProcCommandEncoderPushDebugGroup commandEncoderPushDebugGroup = nullptr;
    WGPUProcCommandEncoderResolveQuerySet commandEncoderResolveQuerySet = nullptr;
    WGPUProcCommandEncoderSetLabel commandEncoderSetLabel = nullptr;
    WGPUProcCommandEncoderWriteBuffer commandEncoderWriteBuffer = nullptr;
    WGPUProcCommandEncoderWriteTimestamp commandEncoderWriteTimestamp = nullptr;
    WGPUProcCommandEncoderAddRef commandEncoderAddRef = nullptr;
    WGPUProcCommandEncoderRelease commandEncoderRelease = nullptr;

    WGPUProcComputePassEncoderDispatchWorkgroups computePassEncoderDispatchWorkgroups = nullptr;
    WGPUProcComputePassEncoderDispatchWorkgroupsIndirect computePassEncoderDispatchWorkgroupsIndirect = nullptr;
    WGPUProcComputePassEncoderEnd computePassEncoderEnd = nullptr;
    WGPUProcComputePassEncoderInsertDebugMarker computePassEncoderInsertDebugMarker = nullptr;
    WGPUProcComputePassEncoderPopDebugGroup computePassEncoderPopDebugGroup = nullptr;
    WGPUProcComputePassEncoderPushDebugGroup computePassEncoderPushDebugGroup = nullptr;
    WGPUProcComputePassEncoderSetBindGroup computePassEncoderSetBindGroup = nullptr;
    WGPUProcComputePassEncoderSetImmediates computePassEncoderSetImmediates = nullptr;
    WGPUProcComputePassEncoderSetLabel computePassEncoderSetLabel = nullptr;
    WGPUProcComputePassEncoderSetPipeline computePassEncoderSetPipeline = nullptr;
    WGPUProcComputePassEncoderSetResourceTable computePassEncoderSetResourceTable = nullptr;
    WGPUProcComputePassEncoderWriteTimestamp computePassEncoderWriteTimestamp = nullptr;
    WGPUProcComputePassEncoderAddRef computePassEncoderAddRef = nullptr;
    WGPUProcComputePassEncoderRelease computePassEncoderRelease = nullptr;

    WGPUProcComputePipelineGetBindGroupLayout computePipelineGetBindGroupLayout = nullptr;
    WGPUProcComputePipelineSetLabel computePipelineSetLabel = nullptr;
    WGPUProcComputePipelineAddRef computePipelineAddRef = nullptr;
    WGPUProcComputePipelineRelease computePipelineRelease = nullptr;

    WGPUProcDawnDrmFormatCapabilitiesFreeMembers dawnDrmFormatCapabilitiesFreeMembers = nullptr;

    WGPUProcDeviceCreateBindGroup deviceCreateBindGroup = nullptr;
    WGPUProcDeviceCreateBindGroupLayout deviceCreateBindGroupLayout = nullptr;
    WGPUProcDeviceCreateBuffer deviceCreateBuffer = nullptr;
    WGPUProcDeviceCreateCommandEncoder deviceCreateCommandEncoder = nullptr;
    WGPUProcDeviceCreateComputePipeline deviceCreateComputePipeline = nullptr;
    WGPUProcDeviceCreateComputePipelineAsync deviceCreateComputePipelineAsync = nullptr;
    WGPUProcDeviceCreateErrorBuffer deviceCreateErrorBuffer = nullptr;
    WGPUProcDeviceCreateErrorComputePipeline deviceCreateErrorComputePipeline = nullptr;
    WGPUProcDeviceCreateErrorExternalTexture deviceCreateErrorExternalTexture = nullptr;
    WGPUProcDeviceCreateErrorRenderPipeline deviceCreateErrorRenderPipeline = nullptr;
    WGPUProcDeviceCreateErrorShaderModule deviceCreateErrorShaderModule = nullptr;
    WGPUProcDeviceCreateErrorTexture deviceCreateErrorTexture = nullptr;
    WGPUProcDeviceCreateExternalTexture deviceCreateExternalTexture = nullptr;
    WGPUProcDeviceCreatePipelineLayout deviceCreatePipelineLayout = nullptr;
    WGPUProcDeviceCreateQuerySet deviceCreateQuerySet = nullptr;
    WGPUProcDeviceCreateRenderBundleEncoder deviceCreateRenderBundleEncoder = nullptr;
    WGPUProcDeviceCreateRenderPipeline deviceCreateRenderPipeline = nullptr;
    WGPUProcDeviceCreateRenderPipelineAsync deviceCreateRenderPipelineAsync = nullptr;
    WGPUProcDeviceCreateResourceTable deviceCreateResourceTable = nullptr;
    WGPUProcDeviceCreateSampler deviceCreateSampler = nullptr;
    WGPUProcDeviceCreateShaderModule deviceCreateShaderModule = nullptr;
    WGPUProcDeviceCreateTexture deviceCreateTexture = nullptr;
    WGPUProcDeviceDestroy deviceDestroy = nullptr;
    WGPUProcDeviceForceLoss deviceForceLoss = nullptr;
    WGPUProcDeviceGetAdapter deviceGetAdapter = nullptr;
    WGPUProcDeviceGetAdapterInfo deviceGetAdapterInfo = nullptr;
    WGPUProcDeviceGetAHardwareBufferProperties deviceGetAHardwareBufferProperties = nullptr;
    WGPUProcDeviceGetFeatures deviceGetFeatures = nullptr;
    WGPUProcDeviceGetLimits deviceGetLimits = nullptr;
    WGPUProcDeviceGetLostFuture deviceGetLostFuture = nullptr;
    WGPUProcDeviceGetQueue deviceGetQueue = nullptr;
    WGPUProcDeviceHasFeature deviceHasFeature = nullptr;
    WGPUProcDeviceImportSharedBufferMemory deviceImportSharedBufferMemory = nullptr;
    WGPUProcDeviceImportSharedFence deviceImportSharedFence = nullptr;
    WGPUProcDeviceImportSharedTextureMemory deviceImportSharedTextureMemory = nullptr;
    WGPUProcDeviceInjectError deviceInjectError = nullptr;
    WGPUProcDevicePopErrorScope devicePopErrorScope = nullptr;
    WGPUProcDevicePushErrorScope devicePushErrorScope = nullptr;
    WGPUProcDeviceSetLabel deviceSetLabel = nullptr;
    WGPUProcDeviceSetLoggingCallback deviceSetLoggingCallback = nullptr;
    WGPUProcDeviceTick deviceTick = nullptr;
    WGPUProcDeviceValidateTextureDescriptor deviceValidateTextureDescriptor = nullptr;
    WGPUProcDeviceAddRef deviceAddRef = nullptr;
    WGPUProcDeviceRelease deviceRelease = nullptr;

    WGPUProcExternalTextureDestroy externalTextureDestroy = nullptr;
    WGPUProcExternalTextureExpire externalTextureExpire = nullptr;
    WGPUProcExternalTextureRefresh externalTextureRefresh = nullptr;
    WGPUProcExternalTextureSetLabel externalTextureSetLabel = nullptr;
    WGPUProcExternalTextureAddRef externalTextureAddRef = nullptr;
    WGPUProcExternalTextureRelease externalTextureRelease = nullptr;

    WGPUProcInstanceCreateSurface instanceCreateSurface = nullptr;
    WGPUProcInstanceGetWGSLLanguageFeatures instanceGetWGSLLanguageFeatures = nullptr;
    WGPUProcInstanceHasWGSLLanguageFeature instanceHasWGSLLanguageFeature = nullptr;
    WGPUProcInstanceProcessEvents instanceProcessEvents = nullptr;
    WGPUProcInstanceRequestAdapter instanceRequestAdapter = nullptr;
    WGPUProcInstanceWaitAny instanceWaitAny = nullptr;
    WGPUProcInstanceAddRef instanceAddRef = nullptr;
    WGPUProcInstanceRelease instanceRelease = nullptr;

    WGPUProcPipelineLayoutSetLabel pipelineLayoutSetLabel = nullptr;
    WGPUProcPipelineLayoutAddRef pipelineLayoutAddRef = nullptr;
    WGPUProcPipelineLayoutRelease pipelineLayoutRelease = nullptr;

    WGPUProcQuerySetDestroy querySetDestroy = nullptr;
    WGPUProcQuerySetGetCount querySetGetCount = nullptr;
    WGPUProcQuerySetGetType querySetGetType = nullptr;
    WGPUProcQuerySetSetLabel querySetSetLabel = nullptr;
    WGPUProcQuerySetAddRef querySetAddRef = nullptr;
    WGPUProcQuerySetRelease querySetRelease = nullptr;

    WGPUProcQueueCopyExternalTextureForBrowser queueCopyExternalTextureForBrowser = nullptr;
    WGPUProcQueueCopyTextureForBrowser queueCopyTextureForBrowser = nullptr;
    WGPUProcQueueOnSubmittedWorkDone queueOnSubmittedWorkDone = nullptr;
    WGPUProcQueueSetLabel queueSetLabel = nullptr;
    WGPUProcQueueSubmit queueSubmit = nullptr;
    WGPUProcQueueWriteBuffer queueWriteBuffer = nullptr;
    WGPUProcQueueWriteTexture queueWriteTexture = nullptr;
    WGPUProcQueueAddRef queueAddRef = nullptr;
    WGPUProcQueueRelease queueRelease = nullptr;

    WGPUProcRenderBundleSetLabel renderBundleSetLabel = nullptr;
    WGPUProcRenderBundleAddRef renderBundleAddRef = nullptr;
    WGPUProcRenderBundleRelease renderBundleRelease = nullptr;

    WGPUProcRenderBundleEncoderDraw renderBundleEncoderDraw = nullptr;
    WGPUProcRenderBundleEncoderDrawIndexed renderBundleEncoderDrawIndexed = nullptr;
    WGPUProcRenderBundleEncoderDrawIndexedIndirect renderBundleEncoderDrawIndexedIndirect = nullptr;
    WGPUProcRenderBundleEncoderDrawIndirect renderBundleEncoderDrawIndirect = nullptr;
    WGPUProcRenderBundleEncoderFinish renderBundleEncoderFinish = nullptr;
    WGPUProcRenderBundleEncoderInsertDebugMarker renderBundleEncoderInsertDebugMarker = nullptr;
    WGPUProcRenderBundleEncoderPopDebugGroup renderBundleEncoderPopDebugGroup = nullptr;
    WGPUProcRenderBundleEncoderPushDebugGroup renderBundleEncoderPushDebugGroup = nullptr;
    WGPUProcRenderBundleEncoderSetBindGroup renderBundleEncoderSetBindGroup = nullptr;
    WGPUProcRenderBundleEncoderSetImmediates renderBundleEncoderSetImmediates = nullptr;
    WGPUProcRenderBundleEncoderSetIndexBuffer renderBundleEncoderSetIndexBuffer = nullptr;
    WGPUProcRenderBundleEncoderSetLabel renderBundleEncoderSetLabel = nullptr;
    WGPUProcRenderBundleEncoderSetPipeline renderBundleEncoderSetPipeline = nullptr;
    WGPUProcRenderBundleEncoderSetVertexBuffer renderBundleEncoderSetVertexBuffer = nullptr;
    WGPUProcRenderBundleEncoderAddRef renderBundleEncoderAddRef = nullptr;
    WGPUProcRenderBundleEncoderRelease renderBundleEncoderRelease = nullptr;

    WGPUProcRenderPassEncoderBeginOcclusionQuery renderPassEncoderBeginOcclusionQuery = nullptr;
    WGPUProcRenderPassEncoderDraw renderPassEncoderDraw = nullptr;
    WGPUProcRenderPassEncoderDrawIndexed renderPassEncoderDrawIndexed = nullptr;
    WGPUProcRenderPassEncoderDrawIndexedIndirect renderPassEncoderDrawIndexedIndirect = nullptr;
    WGPUProcRenderPassEncoderDrawIndirect renderPassEncoderDrawIndirect = nullptr;
    WGPUProcRenderPassEncoderEnd renderPassEncoderEnd = nullptr;
    WGPUProcRenderPassEncoderEndOcclusionQuery renderPassEncoderEndOcclusionQuery = nullptr;
    WGPUProcRenderPassEncoderExecuteBundles renderPassEncoderExecuteBundles = nullptr;
    WGPUProcRenderPassEncoderInsertDebugMarker renderPassEncoderInsertDebugMarker = nullptr;
    WGPUProcRenderPassEncoderMultiDrawIndexedIndirect renderPassEncoderMultiDrawIndexedIndirect = nullptr;
    WGPUProcRenderPassEncoderMultiDrawIndirect renderPassEncoderMultiDrawIndirect = nullptr;
    WGPUProcRenderPassEncoderPixelLocalStorageBarrier renderPassEncoderPixelLocalStorageBarrier = nullptr;
    WGPUProcRenderPassEncoderPopDebugGroup renderPassEncoderPopDebugGroup = nullptr;
    WGPUProcRenderPassEncoderPushDebugGroup renderPassEncoderPushDebugGroup = nullptr;
    WGPUProcRenderPassEncoderSetBindGroup renderPassEncoderSetBindGroup = nullptr;
    WGPUProcRenderPassEncoderSetBlendConstant renderPassEncoderSetBlendConstant = nullptr;
    WGPUProcRenderPassEncoderSetImmediates renderPassEncoderSetImmediates = nullptr;
    WGPUProcRenderPassEncoderSetIndexBuffer renderPassEncoderSetIndexBuffer = nullptr;
    WGPUProcRenderPassEncoderSetLabel renderPassEncoderSetLabel = nullptr;
    WGPUProcRenderPassEncoderSetPipeline renderPassEncoderSetPipeline = nullptr;
    WGPUProcRenderPassEncoderSetResourceTable renderPassEncoderSetResourceTable = nullptr;
    WGPUProcRenderPassEncoderSetScissorRect renderPassEncoderSetScissorRect = nullptr;
    WGPUProcRenderPassEncoderSetStencilReference renderPassEncoderSetStencilReference = nullptr;
    WGPUProcRenderPassEncoderSetVertexBuffer renderPassEncoderSetVertexBuffer = nullptr;
    WGPUProcRenderPassEncoderSetViewport renderPassEncoderSetViewport = nullptr;
    WGPUProcRenderPassEncoderWriteTimestamp renderPassEncoderWriteTimestamp = nullptr;
    WGPUProcRenderPassEncoderAddRef renderPassEncoderAddRef = nullptr;
    WGPUProcRenderPassEncoderRelease renderPassEncoderRelease = nullptr;

    WGPUProcRenderPipelineGetBindGroupLayout renderPipelineGetBindGroupLayout = nullptr;
    WGPUProcRenderPipelineSetLabel renderPipelineSetLabel = nullptr;
    WGPUProcRenderPipelineAddRef renderPipelineAddRef = nullptr;
    WGPUProcRenderPipelineRelease renderPipelineRelease = nullptr;

    WGPUProcResourceTableDestroy resourceTableDestroy = nullptr;
    WGPUProcResourceTableGetSize resourceTableGetSize = nullptr;
    WGPUProcResourceTableInsert resourceTableInsert = nullptr;
    WGPUProcResourceTableRemove resourceTableRemove = nullptr;
    WGPUProcResourceTableSetLabel resourceTableSetLabel = nullptr;
    WGPUProcResourceTableUpdate resourceTableUpdate = nullptr;
    WGPUProcResourceTableAddRef resourceTableAddRef = nullptr;
    WGPUProcResourceTableRelease resourceTableRelease = nullptr;

    WGPUProcSamplerSetLabel samplerSetLabel = nullptr;
    WGPUProcSamplerAddRef samplerAddRef = nullptr;
    WGPUProcSamplerRelease samplerRelease = nullptr;

    WGPUProcShaderModuleGetCompilationInfo shaderModuleGetCompilationInfo = nullptr;
    WGPUProcShaderModuleSetLabel shaderModuleSetLabel = nullptr;
    WGPUProcShaderModuleAddRef shaderModuleAddRef = nullptr;
    WGPUProcShaderModuleRelease shaderModuleRelease = nullptr;

    WGPUProcSharedBufferMemoryBeginAccess sharedBufferMemoryBeginAccess = nullptr;
    WGPUProcSharedBufferMemoryCreateBuffer sharedBufferMemoryCreateBuffer = nullptr;
    WGPUProcSharedBufferMemoryEndAccess sharedBufferMemoryEndAccess = nullptr;
    WGPUProcSharedBufferMemoryGetProperties sharedBufferMemoryGetProperties = nullptr;
    WGPUProcSharedBufferMemoryIsDeviceLost sharedBufferMemoryIsDeviceLost = nullptr;
    WGPUProcSharedBufferMemorySetLabel sharedBufferMemorySetLabel = nullptr;
    WGPUProcSharedBufferMemoryAddRef sharedBufferMemoryAddRef = nullptr;
    WGPUProcSharedBufferMemoryRelease sharedBufferMemoryRelease = nullptr;

    WGPUProcSharedBufferMemoryEndAccessStateFreeMembers sharedBufferMemoryEndAccessStateFreeMembers = nullptr;

    WGPUProcSharedFenceExportInfo sharedFenceExportInfo = nullptr;
    WGPUProcSharedFenceSetLabel sharedFenceSetLabel = nullptr;
    WGPUProcSharedFenceAddRef sharedFenceAddRef = nullptr;
    WGPUProcSharedFenceRelease sharedFenceRelease = nullptr;

    WGPUProcSharedTextureMemoryBeginAccess sharedTextureMemoryBeginAccess = nullptr;
    WGPUProcSharedTextureMemoryCreateTexture sharedTextureMemoryCreateTexture = nullptr;
    WGPUProcSharedTextureMemoryEndAccess sharedTextureMemoryEndAccess = nullptr;
    WGPUProcSharedTextureMemoryGetProperties sharedTextureMemoryGetProperties = nullptr;
    WGPUProcSharedTextureMemoryIsDeviceLost sharedTextureMemoryIsDeviceLost = nullptr;
    WGPUProcSharedTextureMemorySetLabel sharedTextureMemorySetLabel = nullptr;
    WGPUProcSharedTextureMemoryAddRef sharedTextureMemoryAddRef = nullptr;
    WGPUProcSharedTextureMemoryRelease sharedTextureMemoryRelease = nullptr;

    WGPUProcSharedTextureMemoryEndAccessStateFreeMembers sharedTextureMemoryEndAccessStateFreeMembers = nullptr;

    WGPUProcSupportedFeaturesFreeMembers supportedFeaturesFreeMembers = nullptr;

    WGPUProcSupportedInstanceFeaturesFreeMembers supportedInstanceFeaturesFreeMembers = nullptr;

    WGPUProcSupportedWGSLLanguageFeaturesFreeMembers supportedWGSLLanguageFeaturesFreeMembers = nullptr;

    WGPUProcSurfaceConfigure surfaceConfigure = nullptr;
    WGPUProcSurfaceGetCapabilities surfaceGetCapabilities = nullptr;
    WGPUProcSurfaceGetCurrentTexture surfaceGetCurrentTexture = nullptr;
    WGPUProcSurfacePresent surfacePresent = nullptr;
    WGPUProcSurfaceSetLabel surfaceSetLabel = nullptr;
    WGPUProcSurfaceUnconfigure surfaceUnconfigure = nullptr;
    WGPUProcSurfaceAddRef surfaceAddRef = nullptr;
    WGPUProcSurfaceRelease surfaceRelease = nullptr;

    WGPUProcSurfaceCapabilitiesFreeMembers surfaceCapabilitiesFreeMembers = nullptr;

    WGPUProcTexelBufferViewSetLabel texelBufferViewSetLabel = nullptr;
    WGPUProcTexelBufferViewAddRef texelBufferViewAddRef = nullptr;
    WGPUProcTexelBufferViewRelease texelBufferViewRelease = nullptr;

    WGPUProcTextureCreateErrorView textureCreateErrorView = nullptr;
    WGPUProcTextureCreateView textureCreateView = nullptr;
    WGPUProcTextureDestroy textureDestroy = nullptr;
    WGPUProcTextureGetDepthOrArrayLayers textureGetDepthOrArrayLayers = nullptr;
    WGPUProcTextureGetDimension textureGetDimension = nullptr;
    WGPUProcTextureGetFormat textureGetFormat = nullptr;
    WGPUProcTextureGetHeight textureGetHeight = nullptr;
    WGPUProcTextureGetMipLevelCount textureGetMipLevelCount = nullptr;
    WGPUProcTextureGetSampleCount textureGetSampleCount = nullptr;
    WGPUProcTextureGetTextureBindingViewDimension textureGetTextureBindingViewDimension = nullptr;
    WGPUProcTextureGetUsage textureGetUsage = nullptr;
    WGPUProcTextureGetWidth textureGetWidth = nullptr;
    WGPUProcTextureSetLabel textureSetLabel = nullptr;
    WGPUProcTextureSetOwnershipForMemoryDump textureSetOwnershipForMemoryDump = nullptr;
    WGPUProcTextureAddRef textureAddRef = nullptr;
    WGPUProcTextureRelease textureRelease = nullptr;

    WGPUProcTextureViewSetLabel textureViewSetLabel = nullptr;
    WGPUProcTextureViewAddRef textureViewAddRef = nullptr;
    WGPUProcTextureViewRelease textureViewRelease = nullptr;


} DawnProcTable;

#endif  // DAWN_DAWN_PROC_TABLE_H_

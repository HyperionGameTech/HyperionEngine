/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#ifdef HYP_WEB

#include <pthread.h>

#include <type_traits>

namespace Hyperion {

extern pthread_t g_webGPUDeviceThread;

inline bool IsOnWebGPUDeviceThread()
{
    return g_webGPUDeviceThread == pthread_t {}
        || pthread_equal(pthread_self(), g_webGPUDeviceThread);
}

void RunOnWebGPUDeviceThread(void (*function)(void*), void* arg);

template <class Function, class... Args>
inline auto WebGPUCall(Function function, Args... args) -> decltype(function(args...))
{
    using ReturnType = decltype(function(args...));

    if (IsOnWebGPUDeviceThread())
    {
        return function(args...);
    }

    if constexpr (std::is_void_v<ReturnType>)
    {
        auto closure = [&]()
        {
            function(args...);
        };

        RunOnWebGPUDeviceThread(
            [](void* closurePtr)
            {
                (*static_cast<decltype(closure)*>(closurePtr))();
            },
            &closure);
    }
    else
    {
        ReturnType result {};

        auto closure = [&]()
        {
            result = function(args...);
        };

        RunOnWebGPUDeviceThread(
            [](void* closurePtr)
            {
                (*static_cast<decltype(closure)*>(closurePtr))();
            },
            &closure);

        return result;
    }
}

} // namespace Hyperion

//////////////////////////////////////

#define wgpuAdapterGetInfo(...) ::Hyperion::WebGPUCall(&::wgpuAdapterGetInfo __VA_OPT__(, ) __VA_ARGS__)
#define wgpuAdapterGetLimits(...) ::Hyperion::WebGPUCall(&::wgpuAdapterGetLimits __VA_OPT__(, ) __VA_ARGS__)
#define wgpuAdapterHasFeature(...) ::Hyperion::WebGPUCall(&::wgpuAdapterHasFeature __VA_OPT__(, ) __VA_ARGS__)
#define wgpuAdapterInfoFreeMembers(...) ::Hyperion::WebGPUCall(&::wgpuAdapterInfoFreeMembers __VA_OPT__(, ) __VA_ARGS__)
#define wgpuAdapterRelease(...) ::Hyperion::WebGPUCall(&::wgpuAdapterRelease __VA_OPT__(, ) __VA_ARGS__)
#define wgpuAdapterRequestDevice(...) ::Hyperion::WebGPUCall(&::wgpuAdapterRequestDevice __VA_OPT__(, ) __VA_ARGS__)
#define wgpuBindGroupLayoutRelease(...) ::Hyperion::WebGPUCall(&::wgpuBindGroupLayoutRelease __VA_OPT__(, ) __VA_ARGS__)
#define wgpuBindGroupRelease(...) ::Hyperion::WebGPUCall(&::wgpuBindGroupRelease __VA_OPT__(, ) __VA_ARGS__)
#define wgpuBufferGetConstMappedRange(...) ::Hyperion::WebGPUCall(&::wgpuBufferGetConstMappedRange __VA_OPT__(, ) __VA_ARGS__)
#define wgpuBufferMapAsync(...) ::Hyperion::WebGPUCall(&::wgpuBufferMapAsync __VA_OPT__(, ) __VA_ARGS__)
#define wgpuBufferRelease(...) ::Hyperion::WebGPUCall(&::wgpuBufferRelease __VA_OPT__(, ) __VA_ARGS__)
#define wgpuBufferSetLabel(...) ::Hyperion::WebGPUCall(&::wgpuBufferSetLabel __VA_OPT__(, ) __VA_ARGS__)
#define wgpuBufferUnmap(...) ::Hyperion::WebGPUCall(&::wgpuBufferUnmap __VA_OPT__(, ) __VA_ARGS__)
#define wgpuCommandBufferRelease(...) ::Hyperion::WebGPUCall(&::wgpuCommandBufferRelease __VA_OPT__(, ) __VA_ARGS__)
#define wgpuCommandEncoderBeginComputePass(...) ::Hyperion::WebGPUCall(&::wgpuCommandEncoderBeginComputePass __VA_OPT__(, ) __VA_ARGS__)
#define wgpuCommandEncoderBeginRenderPass(...) ::Hyperion::WebGPUCall(&::wgpuCommandEncoderBeginRenderPass __VA_OPT__(, ) __VA_ARGS__)
#define wgpuCommandEncoderCopyBufferToBuffer(...) ::Hyperion::WebGPUCall(&::wgpuCommandEncoderCopyBufferToBuffer __VA_OPT__(, ) __VA_ARGS__)
#define wgpuCommandEncoderCopyBufferToTexture(...) ::Hyperion::WebGPUCall(&::wgpuCommandEncoderCopyBufferToTexture __VA_OPT__(, ) __VA_ARGS__)
#define wgpuCommandEncoderCopyTextureToBuffer(...) ::Hyperion::WebGPUCall(&::wgpuCommandEncoderCopyTextureToBuffer __VA_OPT__(, ) __VA_ARGS__)
#define wgpuCommandEncoderCopyTextureToTexture(...) ::Hyperion::WebGPUCall(&::wgpuCommandEncoderCopyTextureToTexture __VA_OPT__(, ) __VA_ARGS__)
#define wgpuCommandEncoderFinish(...) ::Hyperion::WebGPUCall(&::wgpuCommandEncoderFinish __VA_OPT__(, ) __VA_ARGS__)
#define wgpuCommandEncoderRelease(...) ::Hyperion::WebGPUCall(&::wgpuCommandEncoderRelease __VA_OPT__(, ) __VA_ARGS__)
#define wgpuComputePassEncoderDispatchWorkgroups(...) ::Hyperion::WebGPUCall(&::wgpuComputePassEncoderDispatchWorkgroups __VA_OPT__(, ) __VA_ARGS__)
#define wgpuComputePassEncoderDispatchWorkgroupsIndirect(...) ::Hyperion::WebGPUCall(&::wgpuComputePassEncoderDispatchWorkgroupsIndirect __VA_OPT__(, ) __VA_ARGS__)
#define wgpuComputePassEncoderEnd(...) ::Hyperion::WebGPUCall(&::wgpuComputePassEncoderEnd __VA_OPT__(, ) __VA_ARGS__)
#define wgpuComputePassEncoderRelease(...) ::Hyperion::WebGPUCall(&::wgpuComputePassEncoderRelease __VA_OPT__(, ) __VA_ARGS__)
#define wgpuComputePassEncoderSetBindGroup(...) ::Hyperion::WebGPUCall(&::wgpuComputePassEncoderSetBindGroup __VA_OPT__(, ) __VA_ARGS__)
#define wgpuComputePassEncoderSetPipeline(...) ::Hyperion::WebGPUCall(&::wgpuComputePassEncoderSetPipeline __VA_OPT__(, ) __VA_ARGS__)
#define wgpuComputePipelineRelease(...) ::Hyperion::WebGPUCall(&::wgpuComputePipelineRelease __VA_OPT__(, ) __VA_ARGS__)
#define wgpuCreateInstance(...) ::Hyperion::WebGPUCall(&::wgpuCreateInstance __VA_OPT__(, ) __VA_ARGS__)
#define wgpuDeviceCreateBindGroup(...) ::Hyperion::WebGPUCall(&::wgpuDeviceCreateBindGroup __VA_OPT__(, ) __VA_ARGS__)
#define wgpuDeviceCreateBindGroupLayout(...) ::Hyperion::WebGPUCall(&::wgpuDeviceCreateBindGroupLayout __VA_OPT__(, ) __VA_ARGS__)
#define wgpuDeviceCreateBuffer(...) ::Hyperion::WebGPUCall(&::wgpuDeviceCreateBuffer __VA_OPT__(, ) __VA_ARGS__)
#define wgpuDeviceCreateCommandEncoder(...) ::Hyperion::WebGPUCall(&::wgpuDeviceCreateCommandEncoder __VA_OPT__(, ) __VA_ARGS__)
#define wgpuDeviceCreateComputePipeline(...) ::Hyperion::WebGPUCall(&::wgpuDeviceCreateComputePipeline __VA_OPT__(, ) __VA_ARGS__)
#define wgpuDeviceCreatePipelineLayout(...) ::Hyperion::WebGPUCall(&::wgpuDeviceCreatePipelineLayout __VA_OPT__(, ) __VA_ARGS__)
#define wgpuDeviceCreateRenderPipeline(...) ::Hyperion::WebGPUCall(&::wgpuDeviceCreateRenderPipeline __VA_OPT__(, ) __VA_ARGS__)
#define wgpuDeviceCreateSampler(...) ::Hyperion::WebGPUCall(&::wgpuDeviceCreateSampler __VA_OPT__(, ) __VA_ARGS__)
#define wgpuDeviceCreateShaderModule(...) ::Hyperion::WebGPUCall(&::wgpuDeviceCreateShaderModule __VA_OPT__(, ) __VA_ARGS__)
#define wgpuDeviceCreateTexture(...) ::Hyperion::WebGPUCall(&::wgpuDeviceCreateTexture __VA_OPT__(, ) __VA_ARGS__)
#define wgpuDeviceDestroy(...) ::Hyperion::WebGPUCall(&::wgpuDeviceDestroy __VA_OPT__(, ) __VA_ARGS__)
#define wgpuDeviceGetLimits(...) ::Hyperion::WebGPUCall(&::wgpuDeviceGetLimits __VA_OPT__(, ) __VA_ARGS__)
#define wgpuDeviceGetQueue(...) ::Hyperion::WebGPUCall(&::wgpuDeviceGetQueue __VA_OPT__(, ) __VA_ARGS__)
#define wgpuDeviceRelease(...) ::Hyperion::WebGPUCall(&::wgpuDeviceRelease __VA_OPT__(, ) __VA_ARGS__)
#define wgpuInstanceCreateSurface(...) ::Hyperion::WebGPUCall(&::wgpuInstanceCreateSurface __VA_OPT__(, ) __VA_ARGS__)
#define wgpuInstanceProcessEvents(...) ::Hyperion::WebGPUCall(&::wgpuInstanceProcessEvents __VA_OPT__(, ) __VA_ARGS__)
#define wgpuInstanceRelease(...) ::Hyperion::WebGPUCall(&::wgpuInstanceRelease __VA_OPT__(, ) __VA_ARGS__)
#define wgpuInstanceRequestAdapter(...) ::Hyperion::WebGPUCall(&::wgpuInstanceRequestAdapter __VA_OPT__(, ) __VA_ARGS__)
#define wgpuInstanceWaitAny(...) ::Hyperion::WebGPUCall(&::wgpuInstanceWaitAny __VA_OPT__(, ) __VA_ARGS__)
#define wgpuPipelineLayoutRelease(...) ::Hyperion::WebGPUCall(&::wgpuPipelineLayoutRelease __VA_OPT__(, ) __VA_ARGS__)
#define wgpuQueueOnSubmittedWorkDone(...) ::Hyperion::WebGPUCall(&::wgpuQueueOnSubmittedWorkDone __VA_OPT__(, ) __VA_ARGS__)
#define wgpuQueueRelease(...) ::Hyperion::WebGPUCall(&::wgpuQueueRelease __VA_OPT__(, ) __VA_ARGS__)
#define wgpuQueueSubmit(...) ::Hyperion::WebGPUCall(&::wgpuQueueSubmit __VA_OPT__(, ) __VA_ARGS__)
#define wgpuQueueWriteBuffer(...) ::Hyperion::WebGPUCall(&::wgpuQueueWriteBuffer __VA_OPT__(, ) __VA_ARGS__)
#define wgpuRenderPassEncoderDraw(...) ::Hyperion::WebGPUCall(&::wgpuRenderPassEncoderDraw __VA_OPT__(, ) __VA_ARGS__)
#define wgpuRenderPassEncoderDrawIndexed(...) ::Hyperion::WebGPUCall(&::wgpuRenderPassEncoderDrawIndexed __VA_OPT__(, ) __VA_ARGS__)
#define wgpuRenderPassEncoderDrawIndexedIndirect(...) ::Hyperion::WebGPUCall(&::wgpuRenderPassEncoderDrawIndexedIndirect __VA_OPT__(, ) __VA_ARGS__)
#define wgpuRenderPassEncoderEnd(...) ::Hyperion::WebGPUCall(&::wgpuRenderPassEncoderEnd __VA_OPT__(, ) __VA_ARGS__)
#define wgpuRenderPassEncoderRelease(...) ::Hyperion::WebGPUCall(&::wgpuRenderPassEncoderRelease __VA_OPT__(, ) __VA_ARGS__)
#define wgpuRenderPassEncoderSetBindGroup(...) ::Hyperion::WebGPUCall(&::wgpuRenderPassEncoderSetBindGroup __VA_OPT__(, ) __VA_ARGS__)
#define wgpuRenderPassEncoderSetIndexBuffer(...) ::Hyperion::WebGPUCall(&::wgpuRenderPassEncoderSetIndexBuffer __VA_OPT__(, ) __VA_ARGS__)
#define wgpuRenderPassEncoderSetPipeline(...) ::Hyperion::WebGPUCall(&::wgpuRenderPassEncoderSetPipeline __VA_OPT__(, ) __VA_ARGS__)
#define wgpuRenderPassEncoderSetScissorRect(...) ::Hyperion::WebGPUCall(&::wgpuRenderPassEncoderSetScissorRect __VA_OPT__(, ) __VA_ARGS__)
#define wgpuRenderPassEncoderSetStencilReference(...) ::Hyperion::WebGPUCall(&::wgpuRenderPassEncoderSetStencilReference __VA_OPT__(, ) __VA_ARGS__)
#define wgpuRenderPassEncoderSetVertexBuffer(...) ::Hyperion::WebGPUCall(&::wgpuRenderPassEncoderSetVertexBuffer __VA_OPT__(, ) __VA_ARGS__)
#define wgpuRenderPassEncoderSetViewport(...) ::Hyperion::WebGPUCall(&::wgpuRenderPassEncoderSetViewport __VA_OPT__(, ) __VA_ARGS__)
#define wgpuRenderPipelineRelease(...) ::Hyperion::WebGPUCall(&::wgpuRenderPipelineRelease __VA_OPT__(, ) __VA_ARGS__)
#define wgpuSamplerRelease(...) ::Hyperion::WebGPUCall(&::wgpuSamplerRelease __VA_OPT__(, ) __VA_ARGS__)
#define wgpuSamplerSetLabel(...) ::Hyperion::WebGPUCall(&::wgpuSamplerSetLabel __VA_OPT__(, ) __VA_ARGS__)
#define wgpuShaderModuleRelease(...) ::Hyperion::WebGPUCall(&::wgpuShaderModuleRelease __VA_OPT__(, ) __VA_ARGS__)
#define wgpuSurfaceConfigure(...) ::Hyperion::WebGPUCall(&::wgpuSurfaceConfigure __VA_OPT__(, ) __VA_ARGS__)
#define wgpuSurfaceGetCurrentTexture(...) ::Hyperion::WebGPUCall(&::wgpuSurfaceGetCurrentTexture __VA_OPT__(, ) __VA_ARGS__)
#define wgpuSurfacePresent(...) ::Hyperion::WebGPUCall(&::wgpuSurfacePresent __VA_OPT__(, ) __VA_ARGS__)
#define wgpuSurfaceRelease(...) ::Hyperion::WebGPUCall(&::wgpuSurfaceRelease __VA_OPT__(, ) __VA_ARGS__)
#define wgpuSurfaceUnconfigure(...) ::Hyperion::WebGPUCall(&::wgpuSurfaceUnconfigure __VA_OPT__(, ) __VA_ARGS__)
#define wgpuTextureCreateView(...) ::Hyperion::WebGPUCall(&::wgpuTextureCreateView __VA_OPT__(, ) __VA_ARGS__)
#define wgpuTextureRelease(...) ::Hyperion::WebGPUCall(&::wgpuTextureRelease __VA_OPT__(, ) __VA_ARGS__)
#define wgpuTextureSetLabel(...) ::Hyperion::WebGPUCall(&::wgpuTextureSetLabel __VA_OPT__(, ) __VA_ARGS__)
#define wgpuTextureViewRelease(...) ::Hyperion::WebGPUCall(&::wgpuTextureViewRelease __VA_OPT__(, ) __VA_ARGS__)

//////////////////////////////////////

#endif // HYP_WEB

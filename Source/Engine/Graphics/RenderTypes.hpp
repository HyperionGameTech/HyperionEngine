/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/HashCode.hpp>
#include <Core/Constants.hpp>
#include <Core/Types.hpp>

#include <Core/reflection/Handle.hpp>

/// Type definitions used for specific rendering backends.
/// For example, if HYP_VULKAN is defined, 'GraphicsPipeline' becomes 'VulkanGraphicsPipeline'
/// The base classes will remain as eg 'GraphicsPipelineBase'

namespace Hyperion {

#define DECLARE_GFX_TYPE_BASE(T)        \
    class T##Base;                      \
                                        \
    using T##BaseRef = Handle<T##Base>; \
    using T##BaseWeakRef = WeakHandle<T##Base>;

#if HYP_VULKAN
#define DECLARE_GFX_TYPE(T)                                                             \
    DECLARE_GFX_TYPE_BASE(T);                                                           \
                                                                                        \
    class Vulkan##T;                                                                    \
                                                                                        \
    using T = Vulkan##T;                                                                \
                                                                                        \
    using Vulkan##T##Ref = Handle<Vulkan##T>;                                           \
    using Vulkan##T##WeakRef = WeakHandle<Vulkan##T>;                                   \
                                                                                        \
    using T##Ref = Vulkan##T##Ref;                                                      \
    using T##WeakRef = Vulkan##T##WeakRef;                                              \

#define DECLARE_VULKAN_GFX_TYPE(T)            \
    class Vulkan##T;                          \
                                              \
    using Vulkan##T##Ref = Handle<Vulkan##T>; \
    using Vulkan##T##WeakRef = WeakHandle<Vulkan##T>;

#elif HYP_DX12
#define DECLARE_GFX_TYPE(T)                                                             \
    DECLARE_GFX_TYPE_BASE(T);                                                           \
                                                                                        \
    class DX12##T;                                                                      \
                                                                                        \
    using T = DX12##T;                                                                  \
                                                                                        \
    using DX12##T##Ref = Handle<DX12##T>;                                               \
    using DX12##T##WeakRef = WeakHandle<DX12##T>;                                       \
                                                                                        \
    using T##Ref = DX12##T##Ref;                                                        \
    using T##WeakRef = DX12##T##WeakRef;                                                \

#define DECLARE_DX12_GFX_TYPE(T)                \
    class DX12##T;                              \
                                                \
    using DX12##T##Ref = Handle<DX12##T>;       \
    using DX12##T##WeakRef = WeakHandle<DX12##T>;
#endif

DECLARE_GFX_TYPE(Device);
DECLARE_GFX_TYPE(AsyncCompute);
DECLARE_GFX_TYPE(Swapchain);
DECLARE_GFX_TYPE(GpuImage);
DECLARE_GFX_TYPE(GpuImageView);
DECLARE_GFX_TYPE(Sampler);
DECLARE_GFX_TYPE(GpuBuffer);
DECLARE_GFX_TYPE(Frame);
DECLARE_GFX_TYPE(Framebuffer);
DECLARE_GFX_TYPE(CommandBuffer);
DECLARE_GFX_TYPE(Attachment);
DECLARE_GFX_TYPE(ComputePipeline);
DECLARE_GFX_TYPE(GraphicsPipeline);
DECLARE_GFX_TYPE(RayTracingPipeline);
DECLARE_GFX_TYPE(ShaderInstance);
DECLARE_GFX_TYPE(DescriptorSet);
DECLARE_GFX_TYPE(DescriptorTable);
DECLARE_GFX_TYPE(GpuBlas);
DECLARE_GFX_TYPE(GpuTlas);
DECLARE_GFX_TYPE(TextureViewCache);

#if HYP_VULKAN

DECLARE_VULKAN_GFX_TYPE(Semaphore);
DECLARE_VULKAN_GFX_TYPE(Fence);
DECLARE_VULKAN_GFX_TYPE(RenderPass);

#undef DECLARE_VULKAN_GFX_TYPE

#elif HYP_DX12

DECLARE_DX12_GFX_TYPE(Fence);
DECLARE_DX12_GFX_TYPE(RayTracingPipeline);
DECLARE_DX12_GFX_TYPE(GpuBlas);
DECLARE_DX12_GFX_TYPE(GpuTlas);

#undef DECLARE_DX12_GFX_TYPE

#endif

#undef DECLARE_GFX_TYPE

} // namespace Hyperion

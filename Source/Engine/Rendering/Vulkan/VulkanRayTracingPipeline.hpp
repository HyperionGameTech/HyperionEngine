/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/RayTracingPipeline.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Rendering/Vulkan/VulkanPipeline.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Map.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class VulkanShaderInstance;

enum class ShaderModuleType : uint8;

HYP_CLASS(NoScriptBindings)
class VulkanRayTracingPipeline final : public RayTracingPipelineBase, public VulkanPipelineBase
{
    HYP_OBJECT_BODY(VulkanRayTracingPipeline);

public:
    VulkanRayTracingPipeline();
    explicit VulkanRayTracingPipeline(const VulkanShaderInstanceRef& shader);
    ~VulkanRayTracingPipeline() override;

    bool IsCreated() const override
    {
        return VulkanPipelineBase::IsCreated();
    }

    RendererResult Create() override;

    void Bind(VulkanCommandBuffer* commandBuffer) override;
    void TraceRays(VulkanCommandBuffer* commandBuffer, const Vec3u& extent) const override;

#ifdef HYP_RHI_DEBUG_NAMES
    void SetDebugName(Name name) override
    {
        VulkanPipelineBase::SetDebugName(name);
        m_debugName = name;
    }
#endif

private:
    struct ShaderBindingTableEntry
    {
        VulkanGpuBufferRef buffer;
        VkStridedDeviceAddressRegionKHR stridedDeviceAddressRegion;
    };

    struct
    {
        VkStridedDeviceAddressRegionKHR rayGen {};
        VkStridedDeviceAddressRegionKHR rayMiss {};
        VkStridedDeviceAddressRegionKHR closestHit {};
        VkStridedDeviceAddressRegionKHR callable {};
    } m_shaderBindingTableEntries;

    using ShaderBindingTableMap = Map<ShaderModuleType, ShaderBindingTableEntry, VulkanAllocator>;

    RendererResult CreateShaderBindingTables(VulkanShaderInstance* shader);
    RendererResult CreateShaderBindingTableEntry(uint32 numShaders, ShaderBindingTableEntry& out);

    ShaderBindingTableMap m_shaderBindingTableBuffers;
};

} // namespace Hyperion

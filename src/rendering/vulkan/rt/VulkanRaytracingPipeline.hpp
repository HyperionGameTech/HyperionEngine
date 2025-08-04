/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/rt/RenderRaytracingPipeline.hpp>
#include <rendering/vulkan/VulkanPipeline.hpp>
#include <rendering/Shared.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/HashMap.hpp>

#include <Types.hpp>

namespace hyperion {
class VulkanShader;

class VulkanRaytracingPipeline final : public RaytracingPipelineBase, public VulkanPipelineBase
{
public:
    HYP_API VulkanRaytracingPipeline();
    HYP_API VulkanRaytracingPipeline(const VulkanShaderRef& shader, const VulkanDescriptorTableRef& descriptorTable);
    HYP_API virtual ~VulkanRaytracingPipeline() override;

    HYP_API virtual RendererResult Create() override;
    HYP_API virtual RendererResult Destroy() override;

    HYP_API virtual void Bind(CommandBufferBase* commandBuffer) override;
    HYP_API virtual void TraceRays(CommandBufferBase* commandBuffer, const Vec3u& extent) const override;

    HYP_API virtual void SetPushConstants(const void* data, SizeType size) override;

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

    using ShaderBindingTableMap = HashMap<ShaderModuleType, ShaderBindingTableEntry>;

    RendererResult CreateShaderBindingTables(VulkanShader* shader);
    RendererResult CreateShaderBindingTableEntry(uint32 numShaders, ShaderBindingTableEntry& out);

    ShaderBindingTableMap m_shaderBindingTableBuffers;
};

} // namespace hyperion

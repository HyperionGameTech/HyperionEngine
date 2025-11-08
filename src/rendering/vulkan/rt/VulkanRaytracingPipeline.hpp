/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/raytracing/RenderRaytracingPipeline.hpp>
#include <rendering/vulkan/VulkanPipeline.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/HashMap.hpp>

#include <core/Types.hpp>

namespace hyperion {

class VulkanShader;

HYP_CLASS(NoScriptBindings)
class VulkanRaytracingPipeline final : public RaytracingPipelineBase, public VulkanPipelineBase
{
    HYP_OBJECT_BODY(VulkanRaytracingPipeline);

public:
    VulkanRaytracingPipeline();
    VulkanRaytracingPipeline(const VulkanShaderRef& shader, const VulkanDescriptorTableRef& descriptorTable);
    virtual ~VulkanRaytracingPipeline() override;

    virtual bool IsCreated() const override
    {
        return VulkanPipelineBase::IsCreated();
    }

    virtual RendererResult Create() override;

    virtual void Bind(CommandBufferBase* commandBuffer) override;
    virtual void TraceRays(CommandBufferBase* commandBuffer, const Vec3u& extent) const override;

    virtual void SetPushConstants(const void* data, SizeType size) override;

#ifdef HYP_DEBUG_MODE
    virtual void SetDebugName(Name name) override
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

    using ShaderBindingTableMap = HashMap<ShaderModuleType, ShaderBindingTableEntry>;

    RendererResult CreateShaderBindingTables(VulkanShader* shader);
    RendererResult CreateShaderBindingTableEntry(uint32 numShaders, ShaderBindingTableEntry& out);

    ShaderBindingTableMap m_shaderBindingTableBuffers;
};

} // namespace hyperion

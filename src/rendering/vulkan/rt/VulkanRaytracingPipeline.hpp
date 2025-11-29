/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/raytracing/RenderRaytracingPipeline.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/vulkan/VulkanPipeline.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/HashMap.hpp>

#include <core/Types.hpp>

namespace hyperion {

class VulkanShader;
enum ShaderModuleType : uint32;

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

    virtual void Bind(VulkanCommandBuffer* commandBuffer) override;
    virtual void TraceRays(VulkanCommandBuffer* commandBuffer, const Vec3u& extent) const override;

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

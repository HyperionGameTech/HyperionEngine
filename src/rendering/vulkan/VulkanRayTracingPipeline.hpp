/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/RayTracingPipeline.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/vulkan/VulkanPipeline.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/HashMap.hpp>

#include <core/Types.hpp>

namespace Hyperion {

class VulkanShader;

enum class ShaderModuleType : uint8;

HYP_CLASS(NoScriptBindings)
class VulkanRayTracingPipeline final : public RayTracingPipelineBase, public VulkanPipelineBase
{
    HYP_OBJECT_BODY(VulkanRayTracingPipeline);

public:
    VulkanRayTracingPipeline();
    explicit VulkanRayTracingPipeline(const VulkanShaderRef& shader);
    ~VulkanRayTracingPipeline() override;

    bool IsCreated() const override
    {
        return VulkanPipelineBase::IsCreated();
    }

    RendererResult Create() override;

    void Bind(VulkanCommandBuffer* commandBuffer) override;
    void TraceRays(VulkanCommandBuffer* commandBuffer, const Vec3u& extent) const override;

    void SetPushConstants(const void* data, SizeType size) override;

#ifdef HYP_DEBUG_MODE
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

    using ShaderBindingTableMap = HashMap<ShaderModuleType, ShaderBindingTableEntry>;

    RendererResult CreateShaderBindingTables(VulkanShader* shader);
    RendererResult CreateShaderBindingTableEntry(uint32 numShaders, ShaderBindingTableEntry& out);

    ShaderBindingTableMap m_shaderBindingTableBuffers;
};

} // namespace Hyperion

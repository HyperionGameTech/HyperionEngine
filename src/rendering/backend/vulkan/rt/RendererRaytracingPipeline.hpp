/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_RAYTRACING_PIPELINE_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_RAYTRACING_PIPELINE_HPP

#include <rendering/backend/rt/RendererRaytracingPipeline.hpp>
#include <rendering/backend/vulkan/RendererPipeline.hpp>

#include <core/containers/Array.hpp>

#include <Types.hpp>

namespace hyperion {
namespace renderer {

class VulkanShader;

class VulkanRaytracingPipeline final : public RaytracingPipelineBase, public VulkanPipelineBase
{
public:
    HYP_API VulkanRaytracingPipeline();
    HYP_API VulkanRaytracingPipeline(const VulkanShaderRef& shader, const VulkanDescriptorTableRef& descriptor_table);
    HYP_API virtual ~VulkanRaytracingPipeline() override;

    HYP_API virtual RendererResult Create() override;
    HYP_API virtual RendererResult Destroy() override;

    HYP_API virtual void Bind(CommandBufferBase* command_buffer) override;

    HYP_API virtual void TraceRays(
        CommandBufferBase* command_buffer,
        const Vec3u& extent) const override;

    HYP_API virtual void SetPushConstants(const void* data, SizeType size) override;

private:
    struct ShaderBindingTableEntry
    {
        VulkanGPUBufferRef buffer;
        VkStridedDeviceAddressRegionKHR strided_device_address_region;
    };

    struct
    {
        VkStridedDeviceAddressRegionKHR ray_gen {};
        VkStridedDeviceAddressRegionKHR ray_miss {};
        VkStridedDeviceAddressRegionKHR closest_hit {};
        VkStridedDeviceAddressRegionKHR callable {};
    } m_shader_binding_table_entries;

    using ShaderBindingTableMap = std::unordered_map<ShaderModuleType, ShaderBindingTableEntry>;

    RendererResult CreateShaderBindingTables(VulkanShader* shader);

    RendererResult CreateShaderBindingTableEntry(
        uint32 num_shaders,
        ShaderBindingTableEntry& out);

    ShaderBindingTableMap m_shader_binding_table_buffers;
};

} // namespace renderer
} // namespace hyperion

#endif // HYPERION_RENDERER_BACKEND_VULKAN_RAYTRACING_PIPELINE_HPP

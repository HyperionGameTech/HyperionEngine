/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererPipeline.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>

#include <core/containers/FlatSet.hpp>
#include <core/memory/Memory.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

#pragma region PipelinePlatformImpl

Array<VkDescriptorSetLayout> PipelinePlatformImpl<Platform::VULKAN>::GetDescriptorSetLayouts() const
{
    AssertThrowMsg(self->m_descriptor_table.IsValid(), "Invalid DescriptorTable provided to Pipeline");

    Array<VkDescriptorSetLayout> used_layouts;
    used_layouts.Reserve(self->m_descriptor_table->GetSets()[0].Size());

    for (const DescriptorSetRef<Platform::VULKAN> &descriptor_set : self->m_descriptor_table->GetSets()[0]) {
        AssertThrow(descriptor_set != nullptr);

        used_layouts.PushBack(descriptor_set->GetPlatformImpl().GetVkDescriptorSetLayout());
    }

    return used_layouts;
}

#pragma endregion PipelinePlatformImpl

#pragma region Pipeline

template <>
Pipeline<Platform::VULKAN>::Pipeline()
    : m_platform_impl { this }
{
}

template <>
Pipeline<Platform::VULKAN>::Pipeline(const ShaderRef<Platform::VULKAN> &shader, const DescriptorTableRef<Platform::VULKAN> &descriptor_table)
    : m_platform_impl { this },
      m_shader(shader),
      m_descriptor_table(descriptor_table)
{
}

template <>
Pipeline<Platform::VULKAN>::~Pipeline()
{
    AssertThrowMsg(m_platform_impl.handle == VK_NULL_HANDLE, "Expected pipeline to have been destroyed");
    AssertThrowMsg(m_platform_impl.layout == VK_NULL_HANDLE, "Expected layout to have been destroyed");
}

template <>
RendererResult Pipeline<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    SafeRelease(std::move(m_descriptor_table));

    if (m_platform_impl.handle != VK_NULL_HANDLE) {
        vkDestroyPipeline(device->GetDevice(), m_platform_impl.handle, nullptr);
        m_platform_impl.handle = VK_NULL_HANDLE;
    }

    if (m_platform_impl.layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device->GetDevice(), m_platform_impl.layout, nullptr);
        m_platform_impl.layout = VK_NULL_HANDLE;
    }

    HYPERION_RETURN_OK;
}

template <>
bool Pipeline<Platform::VULKAN>::IsCreated() const
{
    return m_platform_impl.handle != VK_NULL_HANDLE;
}

template <>
void Pipeline<Platform::VULKAN>::SetDescriptorTable(const DescriptorTableRef<Platform::VULKAN> &descriptor_table)
{
    m_descriptor_table = descriptor_table;
}

template <>
void Pipeline<Platform::VULKAN>::SetShader(const ShaderRef<Platform::VULKAN> &shader)
{
    m_shader = shader;
}

template <>
void Pipeline<Platform::VULKAN>::SetPushConstants(const void *data, SizeType size)
{
    AssertThrowMsg(size <= 128, "Push constant data size exceeds 128 bytes");

    m_push_constants = PushConstantData(data, size);
}

#pragma endregion Pipeline

} // namespace platform
} // namespace renderer
} // namespace hyperion

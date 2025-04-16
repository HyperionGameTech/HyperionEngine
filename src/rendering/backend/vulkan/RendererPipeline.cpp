/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/RendererPipeline.hpp>
#include <rendering/backend/vulkan/VulkanRenderingAPI.hpp>

#include <core/containers/FlatSet.hpp>

#include <core/memory/Memory.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {

extern IRenderingAPI *g_rendering_api;

namespace renderer {

static inline VulkanRenderingAPI *GetRenderingAPI()
{
    return static_cast<VulkanRenderingAPI *>(g_rendering_api);
}

#pragma region VulkanPipelineBase

VulkanPipelineBase::VulkanPipelineBase()
    : m_handle(VK_NULL_HANDLE),
      m_layout(VK_NULL_HANDLE)
{
}

VulkanPipelineBase::~VulkanPipelineBase()
{
    AssertThrowMsg(m_handle == VK_NULL_HANDLE, "Expected pipeline to have been destroyed");
    AssertThrowMsg(m_layout == VK_NULL_HANDLE, "Expected layout to have been destroyed");
}

RendererResult VulkanPipelineBase::Destroy()
{
    if (m_handle != VK_NULL_HANDLE) {
        vkDestroyPipeline(GetRenderingAPI()->GetDevice()->GetDevice(), m_handle, nullptr);
        m_handle = VK_NULL_HANDLE;
    }

    if (m_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(GetRenderingAPI()->GetDevice()->GetDevice(), m_layout, nullptr);
        m_layout = VK_NULL_HANDLE;
    }

    HYPERION_RETURN_OK;
}

bool VulkanPipelineBase::IsCreated() const
{
    return m_handle != VK_NULL_HANDLE;
}

void VulkanPipelineBase::SetPushConstants(const void *data, SizeType size)
{
    AssertThrowMsg(size <= 128, "Push constant data size exceeds 128 bytes");

    m_push_constants = PushConstantData(data, size);
}

#pragma endregion VulkanPipelineBase

} // namespace renderer
} // namespace hyperion

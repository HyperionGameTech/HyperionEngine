/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/rhi/CmdList.hpp>
#include <rendering/backend/RendererFrame.hpp>

#if defined(HYP_DEBUG_MODE) && defined(HYP_VULKAN)
// for debugging
#include <rendering/backend/vulkan/RendererFramebuffer.hpp>
#include <rendering/backend/vulkan/RendererGraphicsPipeline.hpp>
#endif

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {

#pragma region CmdList

CmdList::CmdList()
{
}

CmdList::~CmdList()
{
    for (CmdBase* command : m_commands)
    {
        FreeCommand(command);
    }
}

void CmdList::Prepare(FrameBase* frame)
{
    AssertThrow(frame != nullptr);

    for (CmdBase* command : m_commands)
    {
        command->Prepare(frame);
    }
}

void CmdList::Execute(const CommandBufferRef& cmd)
{
    AssertThrow(cmd != nullptr);

    for (CmdBase* command : m_commands)
    {
        command->Execute(cmd);

        FreeCommand(command);
    }

    m_commands.Clear();
}

void CmdList::FreeCommand(CmdBase* command)
{
    AssertThrow(command != nullptr);

    CmdMemoryPoolBase* pool = command->m_pool_handle.pool;
    AssertThrow(pool != nullptr);

    pool->FreeCommand(command);
}

#pragma endregion CmdList

#pragma region BindDescriptorSet

void BindDescriptorSet::Prepare(FrameBase* frame)
{
    AssertThrow(m_descriptor_set->IsCreated());

    frame->MarkDescriptorSetUsed(m_descriptor_set);
}

#pragma endregion BindDescriptorSet

#pragma region BindDescriptorTable

void BindDescriptorTable::Prepare(FrameBase* frame)
{
    for (const DescriptorSetRef& descriptor_set : m_descriptor_table->GetSets()[frame->GetFrameIndex()])
    {
        if (descriptor_set->GetLayout().IsTemplate())
        {
            continue;
        }

        AssertThrow(descriptor_set->IsCreated());

        frame->MarkDescriptorSetUsed(descriptor_set);
    }
}

#pragma endregion BindDescriptorTable

#pragma region BeginFramebuffer

#ifdef HYP_DEBUG_MODE
static FramebufferBase* g_active_framebuffer = nullptr;

BeginFramebuffer::BeginFramebuffer(const FramebufferRef& framebuffer, uint32 frame_index)
    : m_framebuffer(framebuffer),
      m_frame_index(frame_index)
{
    AssertThrowMsg(!g_active_framebuffer, "Cannot begin framebuffer: already in a framebuffer");
    g_active_framebuffer = framebuffer.Get();
}

void BeginFramebuffer::Prepare(FrameBase* frame)
{
}
#endif

#pragma endregion BeginFramebuffer

#pragma region EndFramebuffer

#ifdef HYP_DEBUG_MODE

EndFramebuffer::EndFramebuffer(const FramebufferRef& framebuffer, uint32 frame_index)
    : m_framebuffer(framebuffer),
      m_frame_index(frame_index)
{
    AssertThrowMsg(g_active_framebuffer, "Cannot end framebuffer: not in a framebuffer");
    AssertThrowMsg(g_active_framebuffer == framebuffer.Get(), "Cannot end framebuffer: mismatched framebuffer");
    g_active_framebuffer = nullptr;
}

void EndFramebuffer::Prepare(FrameBase* frame)
{
}
#endif

#pragma endregion EndFramebuffer

#pragma region BindGraphicsPipeline

#ifdef HYP_DEBUG_MODE

BindGraphicsPipeline::BindGraphicsPipeline(const GraphicsPipelineRef& pipeline, Vec2i viewport_offset, Vec2u viewport_extent)
    : m_pipeline(pipeline),
      m_viewport_offset(viewport_offset),
      m_viewport_extent(viewport_extent)
{
    AssertThrowMsg(g_active_framebuffer != nullptr, "Cannot bind graphics pipeline: not in a framebuffer");
    //     AssertThrowMsg(pipeline->GetFramebuffers().FindAs(g_active_framebuffer) != pipeline->GetFramebuffers().End(), "Cannot bind graphics pipeline: framebuffer does not match pipeline");
    // #ifdef HYP_VULKAN
    //     VulkanFramebuffer* vulkan_framebuffer = static_cast<VulkanFramebuffer*>(g_active_framebuffer);
    //     VulkanGraphicsPipeline* vulkan_pipeline = static_cast<VulkanGraphicsPipeline*>(pipeline.Get());

    //     AssertThrowMsg(vulkan_framebuffer->GetRenderPass()->GetAttachments()[0]->GetFormat() == vulkan_pipeline->GetRenderPass()->GetAttachments()[0]->GetFormat(),
    //         "Cannot bind graphics pipeline: render pass does not match framebuffer render pass");
    // #endif
}

BindGraphicsPipeline::BindGraphicsPipeline(const GraphicsPipelineRef& pipeline)
    : m_pipeline(pipeline)
{
    AssertThrowMsg(g_active_framebuffer != nullptr, "Cannot bind graphics pipeline: not in a framebuffer");
    //     AssertThrowMsg(pipeline->GetFramebuffers().FindAs(g_active_framebuffer) != pipeline->GetFramebuffers().End(), "Cannot bind graphics pipeline: framebuffer does not match pipeline");
    // #ifdef HYP_VULKAN
    //     VulkanFramebuffer* vulkan_framebuffer = static_cast<VulkanFramebuffer*>(g_active_framebuffer);
    //     VulkanGraphicsPipeline* vulkan_pipeline = static_cast<VulkanGraphicsPipeline*>(pipeline.Get());

    //     AssertThrowMsg(vulkan_framebuffer->GetRenderPass()->GetAttachments()[0]->GetFormat() == vulkan_pipeline->GetRenderPass()->GetAttachments()[0]->GetFormat(),
    //         "Cannot bind graphics pipeline: render pass does not match framebuffer render pass");
    // #endif
}

#endif

#pragma endregion BindGraphicsPipeline

} // namespace hyperion

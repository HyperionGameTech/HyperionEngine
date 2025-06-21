/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/rhi/RHICommandList.hpp>
#include <rendering/backend/RendererFrame.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {

#pragma region RHICommandList

RHICommandList::RHICommandList()
{
}

RHICommandList::~RHICommandList()
{
    for (RHICommandBase* command : m_commands)
    {
        FreeCommand(command);
    }
}

void RHICommandList::Prepare(FrameBase* frame)
{
    AssertThrow(frame != nullptr);

    for (RHICommandBase* command : m_commands)
    {
        command->Prepare(frame);
    }
}

void RHICommandList::Execute(const CommandBufferRef& cmd)
{
    AssertThrow(cmd != nullptr);

    for (RHICommandBase* command : m_commands)
    {
        command->Execute(cmd);

        FreeCommand(command);
    }

    m_commands.Clear();
}

void RHICommandList::FreeCommand(RHICommandBase* command)
{
    AssertThrow(command != nullptr);

    RHICommandMemoryPoolBase* pool = command->m_pool_handle.pool;
    AssertThrow(pool != nullptr);

    pool->FreeCommand(command);
}

#pragma endregion RHICommandList

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
static bool g_is_in_framebuffer = false;

BeginFramebuffer::BeginFramebuffer(const FramebufferRef& framebuffer, uint32 frame_index)
    : m_framebuffer(framebuffer),
      m_frame_index(frame_index)
{
    AssertThrowMsg(!g_is_in_framebuffer, "Cannot begin framebuffer: already in a framebuffer");
    g_is_in_framebuffer = true;
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
    AssertThrowMsg(g_is_in_framebuffer, "Cannot end framebuffer: not in a framebuffer");
    g_is_in_framebuffer = false;
}

void EndFramebuffer::Prepare(FrameBase* frame)
{
}
#endif

#pragma endregion EndFramebuffer

} // namespace hyperion

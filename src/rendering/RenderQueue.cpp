/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderQueue.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/Mesh.hpp>

#if defined(HYP_DEBUG_MODE) && defined(HYP_VULKAN)
// for debugging
#include <rendering/vulkan/VulkanFramebuffer.hpp>
#include <rendering/vulkan/VulkanGraphicsPipeline.hpp>
#endif

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <util/MeshBuilder.hpp>

namespace hyperion {

#pragma region RenderQueue

RenderQueue::RenderQueue()
    : m_offset(0)
{
}

RenderQueue::~RenderQueue()
{
    Assert(m_cmdHeaders.Empty(), "RenderQueue destroyed with pending commands!");
}

void RenderQueue::Prepare(FrameBase* frame)
{
    Assert(frame != nullptr);

    for (CmdHeader& cmdHeader : m_cmdHeaders)
    {
        CmdBase* cmdDataPtr = reinterpret_cast<CmdBase*>(m_buffer.Data() + cmdHeader.dataOffset);
        AssertDebug(cmdHeader.dataOffset < m_buffer.Size());

        cmdHeader.prepareFnPtr(cmdDataPtr, frame);
    }
}

void RenderQueue::Execute(CommandBufferBase* commandBuffer)
{
    AssertDebug(commandBuffer != nullptr);

    for (CmdHeader& cmdHeader : m_cmdHeaders)
    {
        AssertDebug(cmdHeader.dataOffset < m_buffer.Size());
        CmdBase* cmdDataPtr = reinterpret_cast<CmdBase*>(m_buffer.Data() + cmdHeader.dataOffset);

        cmdHeader.invokeFnPtr(cmdDataPtr, commandBuffer);
    }

    m_cmdHeaders.Clear();
    m_offset = 0;
}

#pragma endregion RenderQueue

#pragma region BindDescriptorSet

void BindDescriptorSet::PrepareStatic(CmdBase* cmd, FrameBase* frame)
{
    BindDescriptorSet* cmdCasted = static_cast<BindDescriptorSet*>(cmd);

    Assert(cmdCasted->m_descriptorSet->IsCreated());

    frame->MarkDescriptorSetUsed(cmdCasted->m_descriptorSet);
}

#pragma endregion BindDescriptorSet

#pragma region BindDescriptorTable

void BindDescriptorTable::PrepareStatic(CmdBase* cmd, FrameBase* frame)
{
    BindDescriptorTable* cmdCasted = static_cast<BindDescriptorTable*>(cmd);

    for (const DescriptorSetRef& descriptorSet : cmdCasted->m_descriptorTable->GetSets()[frame->GetFrameIndex()])
    {
        if (descriptorSet->GetLayout().IsTemplate())
        {
            continue;
        }

        Assert(descriptorSet->IsCreated());

        frame->MarkDescriptorSetUsed(descriptorSet);
    }
}

#pragma endregion BindDescriptorTable

#pragma region BeginFramebuffer

#ifdef HYP_DEBUG_MODE
static FramebufferBase* g_activeFramebuffer = nullptr;

BeginFramebuffer::BeginFramebuffer(FramebufferBase* framebuffer)
    : m_framebuffer(framebuffer)
{
    Assert(!g_activeFramebuffer, "Cannot begin framebuffer: already in a framebuffer");
    g_activeFramebuffer = framebuffer;
}

void BeginFramebuffer::PrepareStatic(CmdBase* cmd, FrameBase* frame)
{
}
#endif

#pragma endregion BeginFramebuffer

#pragma region EndFramebuffer

#ifdef HYP_DEBUG_MODE

EndFramebuffer::EndFramebuffer(FramebufferBase* framebuffer)
    : m_framebuffer(framebuffer)
{
    Assert(g_activeFramebuffer, "Cannot end framebuffer: not in a framebuffer");
    Assert(g_activeFramebuffer == framebuffer, "Cannot end framebuffer: mismatched framebuffer");
    g_activeFramebuffer = nullptr;
}

void EndFramebuffer::PrepareStatic(CmdBase* cmd, FrameBase* frame)
{
}
#endif

#pragma endregion EndFramebuffer

#pragma region BindGraphicsPipeline

#ifdef HYP_DEBUG_MODE

BindGraphicsPipeline::BindGraphicsPipeline(GraphicsPipelineBase* pipeline, Vec2i viewportOffset, Vec2u viewportExtent)
    : m_pipeline(pipeline),
      m_viewportOffset(viewportOffset),
      m_viewportExtent(viewportExtent)
{
    Assert(g_activeFramebuffer != nullptr, "Cannot bind graphics pipeline: not in a framebuffer");
}

BindGraphicsPipeline::BindGraphicsPipeline(GraphicsPipelineBase* pipeline)
    : m_pipeline(pipeline)
{
    Assert(g_activeFramebuffer != nullptr, "Cannot bind graphics pipeline: not in a framebuffer");
}

#endif

void BindGraphicsPipeline::PrepareStatic(CmdBase* cmd, FrameBase*)
{
    BindGraphicsPipeline* cmdCasted = static_cast<BindGraphicsPipeline*>(cmd);

    if (cmdCasted->m_pipeline)
    {
        cmdCasted->m_pipeline->lastFrame = RenderApi_GetFrameCounter();
    }
}

#pragma endregion BindGraphicsPipeline

#pragma region DrawQuad

static Handle<Mesh> g_quadMesh;

void DrawQuad::InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer)
{
    if (HYP_UNLIKELY(!g_quadMesh))
    {
        g_quadMesh = MeshBuilder::Quad();
        InitObject(g_quadMesh);

        Threads::CurrentThreadObject()->AtExit([]()
            {
                g_quadMesh.Reset();
            });
    }

    commandBuffer->BindIndexBuffer(g_quadMesh->GetIndexBuffer());
    commandBuffer->BindVertexBuffer(g_quadMesh->GetVertexBuffer());
    commandBuffer->DrawIndexed(6);

    static_assert(std::is_trivially_destructible_v<DrawQuad>);

    // reinterpret_cast<DrawQuad*>(cmd)->~DrawQuad();
}

#pragma endregion DrawQuad

} // namespace hyperion

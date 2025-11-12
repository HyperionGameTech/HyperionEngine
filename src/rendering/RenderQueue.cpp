/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <rendering/RenderQueue.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/Mesh.hpp>

#if defined(HYP_DEBUG_MODE) && defined(HYP_VULKAN)
// for debugging
#include <rendering/vulkan/VulkanFramebuffer.hpp>
#include <rendering/vulkan/VulkanGraphicsPipeline.hpp>
#include <rendering/vulkan/VulkanCommandBuffer.hpp>
#endif

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <util/MeshBuilder.hpp>

namespace hyperion {

#pragma region RenderQueue

template <>
void RenderQueue::Prepare(FrameBase* frame)
{
    Assert(frame != nullptr);

    for (CmdHeader& cmdHeader : m_cmdHeaders)
    {
        CmdBase* cmdDataPtr = reinterpret_cast<CmdBase*>(m_buffer.Data() + cmdHeader.offset);
        AssertDebug(cmdHeader.offset < m_buffer.Size());

        cmdHeader.prepareFnPtr(cmdDataPtr, frame);
    }
}

template <>
void RenderQueue::Execute(CommandBufferBase* commandBuffer)
{
    AssertDebug(commandBuffer != nullptr);

    for (CmdHeader& cmdHeader : m_cmdHeaders)
    {
        AssertDebug(cmdHeader.offset < m_buffer.Size());
        CmdBase* cmdDataPtr = reinterpret_cast<CmdBase*>(m_buffer.Data() + cmdHeader.offset);

        cmdHeader.invokeFnPtr(cmdDataPtr, commandBuffer);
    }

    m_cmdHeaders.Clear();
    m_offset = 0;
}

#pragma endregion RenderQueue

#pragma region BindDescriptorSet

BindDescriptorSet::BindDescriptorSet(DescriptorSetBase* descriptorSet, GraphicsPipelineBase* pipeline, const DescriptorSetOffsetMap& offsets)
    : m_descriptorSet(descriptorSet),
      m_graphicsPipeline(pipeline),
      m_offsets(offsets),
      m_pipelineType(0) // 0 = Graphics
{
    AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
    AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");

    m_bindIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(descriptorSet->GetLayout().GetName());
    AssertDebug(m_bindIndex != ~0u, "Invalid bind index for descriptor set {}", descriptorSet->GetLayout().GetName());
}

BindDescriptorSet::BindDescriptorSet(DescriptorSetBase* descriptorSet, GraphicsPipelineBase* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex)
    : m_descriptorSet(descriptorSet),
      m_graphicsPipeline(pipeline),
      m_offsets(offsets),
      m_bindIndex(bindIndex),
      m_pipelineType(0) // 0 = Graphics
{
    AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
    AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");
    AssertDebug(m_bindIndex != ~0u, "Invalid bind index");
}

BindDescriptorSet::BindDescriptorSet(DescriptorSetBase* descriptorSet, ComputePipelineBase* pipeline, const DescriptorSetOffsetMap& offsets)
    : m_descriptorSet(descriptorSet),
      m_computePipeline(pipeline),
      m_offsets(offsets),
      m_pipelineType(1) // 1 = Compute
{
    AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
    AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");

    m_bindIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(descriptorSet->GetLayout().GetName());
    AssertDebug(m_bindIndex != ~0u, "Invalid bind index for descriptor set {}", descriptorSet->GetLayout().GetName());
}

BindDescriptorSet::BindDescriptorSet(DescriptorSetBase* descriptorSet, ComputePipelineBase* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex)
    : m_descriptorSet(descriptorSet),
      m_computePipeline(pipeline),
      m_offsets(offsets),
      m_bindIndex(bindIndex),
      m_pipelineType(1) // 1 = Compute
{
    AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
    AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");
    AssertDebug(m_bindIndex != ~0u, "Invalid bind index");
}

BindDescriptorSet::BindDescriptorSet(DescriptorSetBase* descriptorSet, RaytracingPipelineBase* pipeline, const DescriptorSetOffsetMap& offsets)
    : m_descriptorSet(descriptorSet),
      m_raytracingPipeline(pipeline),
      m_offsets(offsets),
      m_pipelineType(2) // 2 = Raytracing
{
    AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
    AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");

    m_bindIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(descriptorSet->GetLayout().GetName());
    AssertDebug(m_bindIndex != ~0u, "Invalid bind index for descriptor set {}", descriptorSet->GetLayout().GetName());
}

BindDescriptorSet::BindDescriptorSet(DescriptorSetBase* descriptorSet, RaytracingPipelineBase* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex)
    : m_descriptorSet(descriptorSet),
      m_raytracingPipeline(pipeline),
      m_offsets(offsets),
      m_bindIndex(bindIndex),
      m_pipelineType(2) // 2 = Raytracing
{
    AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
    AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");
    AssertDebug(m_bindIndex != ~0u, "Invalid bind index");
}

void BindDescriptorSet::PrepareStatic(CmdBase* cmd, FrameBase* frame)
{
    BindDescriptorSet* cmdCasted = static_cast<BindDescriptorSet*>(cmd);

    Assert(cmdCasted->m_descriptorSet->IsCreated());

    frame->MarkDescriptorSetUsed(cmdCasted->m_descriptorSet);
}

void BindDescriptorSet::InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer)
{
    BindDescriptorSet* cmdCasted = static_cast<BindDescriptorSet*>(cmd);

    switch (cmdCasted->m_pipelineType)
    {
    case 0: // Graphics
        cmdCasted->m_descriptorSet->Bind(commandBuffer, cmdCasted->m_graphicsPipeline, cmdCasted->m_offsets, cmdCasted->m_bindIndex);
        break;
    case 1: // Compute
        cmdCasted->m_descriptorSet->Bind(commandBuffer, cmdCasted->m_computePipeline, cmdCasted->m_offsets, cmdCasted->m_bindIndex);
        break;
    case 2: // Raytracing
        cmdCasted->m_descriptorSet->Bind(commandBuffer, cmdCasted->m_raytracingPipeline, cmdCasted->m_offsets, cmdCasted->m_bindIndex);
        break;
    default:
        HYP_UNREACHABLE();
    }

    static_assert(std::is_trivially_destructible_v<BindDescriptorSet>);
    // cmdCasted->~BindDescriptorSet();
}

#pragma endregion BindDescriptorSet

#pragma region BindDescriptorTable

BindDescriptorTable::BindDescriptorTable(DescriptorTableBase* descriptorTable, GraphicsPipelineBase* graphicsPipeline, const DescriptorTableOffsetMap& offsets, uint32 frameIndex)
    : m_descriptorTable(descriptorTable),
      m_graphicsPipeline(graphicsPipeline),
      m_offsets(offsets),
      m_frameIndex(frameIndex),
      m_pipelineType(0) // 0 = Graphics
{
    AssertDebug(descriptorTable != nullptr, "Descriptor table must not be null");
}

BindDescriptorTable::BindDescriptorTable(DescriptorTableBase* descriptorTable, ComputePipelineBase* computePipeline, const DescriptorTableOffsetMap& offsets, uint32 frameIndex)
    : m_descriptorTable(descriptorTable),
      m_computePipeline(computePipeline),
      m_offsets(offsets),
      m_frameIndex(frameIndex),
      m_pipelineType(1) // 1 = Compute
{
    AssertDebug(descriptorTable != nullptr, "Descriptor table must not be null");
}

BindDescriptorTable::BindDescriptorTable(DescriptorTableBase* descriptorTable, RaytracingPipelineBase* raytracingPipeline, const DescriptorTableOffsetMap& offsets, uint32 frameIndex)
    : m_descriptorTable(descriptorTable),
      m_raytracingPipeline(raytracingPipeline),
      m_offsets(offsets),
      m_frameIndex(frameIndex),
      m_pipelineType(2) // 2 = Raytracing
{
    AssertDebug(descriptorTable != nullptr, "Descriptor table must not be null");
}

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

void BindDescriptorTable::InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer)
{
    BindDescriptorTable* cmdCasted = static_cast<BindDescriptorTable*>(cmd);

    switch (cmdCasted->m_pipelineType)
    {
    case 0: // Graphics
        cmdCasted->m_descriptorTable->Bind(commandBuffer, cmdCasted->m_frameIndex, cmdCasted->m_graphicsPipeline, cmdCasted->m_offsets);
        break;
    case 1: // Compute
        cmdCasted->m_descriptorTable->Bind(commandBuffer, cmdCasted->m_frameIndex, cmdCasted->m_computePipeline, cmdCasted->m_offsets);
        break;
    case 2: // Raytracing
        cmdCasted->m_descriptorTable->Bind(commandBuffer, cmdCasted->m_frameIndex, cmdCasted->m_raytracingPipeline, cmdCasted->m_offsets);
        break;
    default:
        HYP_UNREACHABLE();
    }

    static_assert(std::is_trivially_destructible_v<BindDescriptorTable>);
    // cmdCasted->~BindDescriptorTable();
}

#pragma endregion BindDescriptorTable

#pragma region InsertBarrier

#if defined(HYP_VULKAN) && defined(HYP_DEBUG_MODE)
void InsertBarrier::CheckNotInRenderPass(CommandBufferBase* commandBuffer) const
{
    VulkanCommandBuffer* vulkanCommandBuffer = VULKAN_CAST(commandBuffer);
    HYP_GFX_ASSERT(!vulkanCommandBuffer->IsInRenderPass());
}
#endif

#pragma endregion InsertBarrier

#pragma region BeginFramebuffer

#ifdef HYP_DEBUG_MODE
thread_local int s_framebufferCount;
thread_local FramebufferBase* s_currentFramebuffer;
#endif

BeginFramebuffer::BeginFramebuffer(FramebufferBase* framebuffer)
    : m_framebuffer(framebuffer)
{
#ifdef HYP_DEBUG_MODE
    Assert(!s_framebufferCount, "Cannot begin framebuffer: already in a framebuffer");
    s_framebufferCount++;
    s_currentFramebuffer = framebuffer;
#endif

    AssertDebug(!framebuffer->IsDeferredRecording(), "Beginning a framebuffer that is already recording");

    m_framebuffer->SetIsDeferredRecording(true);
}

void BeginFramebuffer::PrepareStatic(CmdBase* cmd, FrameBase* frame)
{
}

#pragma endregion BeginFramebuffer

#pragma region EndFramebuffer

EndFramebuffer::EndFramebuffer(FramebufferBase* framebuffer)
    : m_framebuffer(framebuffer)
{
#ifdef HYP_DEBUG_MODE
    Assert(s_framebufferCount, "Cannot end framebuffer: not in a framebuffer");
    s_framebufferCount--;

    Assert(s_currentFramebuffer == framebuffer, "Cannot end framebuffer: mismatched framebuffer");
    s_currentFramebuffer = nullptr;
#endif
    AssertDebug(framebuffer->IsDeferredRecording(), "Ending a framebuffer that is not recording");

    m_framebuffer->SetIsDeferredRecording(false);
}

void EndFramebuffer::PrepareStatic(CmdBase* cmd, FrameBase* frame)
{
}

#pragma endregion EndFramebuffer

#pragma region BindGraphicsPipeline

#ifdef HYP_DEBUG_MODE

BindGraphicsPipeline::BindGraphicsPipeline(GraphicsPipelineBase* pipeline, const Viewport& viewport)
    : m_pipeline(pipeline),
      m_viewport(viewport)
{
    Assert(s_framebufferCount, "Cannot bind graphics pipeline: not in a framebuffer");
}

BindGraphicsPipeline::BindGraphicsPipeline(GraphicsPipelineBase* pipeline, Vec2i viewportOffset, Vec2u viewportExtent)
    : m_pipeline(pipeline),
      m_viewport(Viewport { viewportExtent, viewportOffset })
{
    Assert(s_framebufferCount, "Cannot bind graphics pipeline: not in a framebuffer");
}

BindGraphicsPipeline::BindGraphicsPipeline(GraphicsPipelineBase* pipeline)
    : m_pipeline(pipeline),
      m_viewport()
{
    Assert(s_framebufferCount, "Cannot bind graphics pipeline: not in a framebuffer");
}

#endif

void BindGraphicsPipeline::PrepareStatic(CmdBase* cmd, FrameBase*)
{
    BindGraphicsPipeline* cmdCasted = static_cast<BindGraphicsPipeline*>(cmd);

    if (cmdCasted->m_pipeline)
    {
        cmdCasted->m_pipeline->lastFrame = RenderApi::GetFrameCounter();
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
        g_quadMesh->SetFlags(MF_VIEW_INDEPENDENT);
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

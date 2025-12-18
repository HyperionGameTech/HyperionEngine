/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/RenderQueue.hpp>
#include <rendering/Frame.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/GraphicsPipeline.hpp>
#include <rendering/ComputePipeline.hpp>
#include <rendering/Mesh.hpp>

#include <rendering/raytracing/RenderRaytracingPipeline.hpp>

#include <util/MeshBuilder.hpp>

namespace hyperion {

#pragma region RenderQueue

template <>
void RenderQueue::Prepare(Frame* frame)
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
void RenderQueue::Execute(CommandBuffer* commandBuffer)
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

BindDescriptorSet::BindDescriptorSet(DescriptorSet* descriptorSet, GraphicsPipeline* pipeline, const DescriptorSetOffsetMap& offsets)
    : m_descriptorSet(descriptorSet),
      m_graphicsPipeline(pipeline),
      m_offsets(offsets),
      m_pipelineType(0) // 0 = Graphics
{
    AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
    AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");

    AssertDebug(pipeline && pipeline->GetShader());

    m_bindIndex = pipeline->GetShader()->GetCompiledShader()->GetDescriptorTableDeclaration()->GetDescriptorSetIndex(descriptorSet->GetLayout().GetName());
    AssertDebug(m_bindIndex != ~0u, "Invalid bind index for descriptor set {}", descriptorSet->GetLayout().GetName());
}

BindDescriptorSet::BindDescriptorSet(DescriptorSet* descriptorSet, GraphicsPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex)
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

BindDescriptorSet::BindDescriptorSet(DescriptorSet* descriptorSet, ComputePipeline* pipeline, const DescriptorSetOffsetMap& offsets)
    : m_descriptorSet(descriptorSet),
      m_computePipeline(pipeline),
      m_offsets(offsets),
      m_pipelineType(1) // 1 = Compute
{
    AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
    AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");

    AssertDebug(pipeline && pipeline->GetShader());

    m_bindIndex = pipeline->GetShader()->GetCompiledShader()->GetDescriptorTableDeclaration()->GetDescriptorSetIndex(descriptorSet->GetLayout().GetName());
    AssertDebug(m_bindIndex != ~0u, "Invalid bind index for descriptor set {}", descriptorSet->GetLayout().GetName());
}

BindDescriptorSet::BindDescriptorSet(DescriptorSet* descriptorSet, ComputePipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex)
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

BindDescriptorSet::BindDescriptorSet(DescriptorSet* descriptorSet, RaytracingPipeline* pipeline, const DescriptorSetOffsetMap& offsets)
    : m_descriptorSet(descriptorSet),
      m_raytracingPipeline(pipeline),
      m_offsets(offsets),
      m_pipelineType(2) // 2 = Raytracing
{
    AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
    AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");

    AssertDebug(pipeline && pipeline->GetShader());

    m_bindIndex = pipeline->GetShader()->GetCompiledShader()->GetDescriptorTableDeclaration()->GetDescriptorSetIndex(descriptorSet->GetLayout().GetName());
    AssertDebug(m_bindIndex != ~0u, "Invalid bind index for descriptor set {}", descriptorSet->GetLayout().GetName());
}

BindDescriptorSet::BindDescriptorSet(DescriptorSet* descriptorSet, RaytracingPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex)
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

void BindDescriptorSet::PrepareStatic(CmdBase* cmd, Frame* frame)
{
    BindDescriptorSet* cmdCasted = static_cast<BindDescriptorSet*>(cmd);

    Assert(cmdCasted->m_descriptorSet->IsCreated());

    frame->MarkDescriptorSetUsed(cmdCasted->m_descriptorSet);
}

void BindDescriptorSet::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
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

BindDescriptorTable::BindDescriptorTable(DescriptorTable* descriptorTable, GraphicsPipeline* graphicsPipeline, const DescriptorTableOffsetMap& offsets, uint32 frameIndex)
    : m_descriptorTable(descriptorTable),
      m_graphicsPipeline(graphicsPipeline),
      m_offsets(offsets),
      m_frameIndex(frameIndex),
      m_pipelineType(0) // 0 = Graphics
{
    AssertDebug(descriptorTable != nullptr, "Descriptor table must not be null");
}

BindDescriptorTable::BindDescriptorTable(DescriptorTable* descriptorTable, ComputePipeline* computePipeline, const DescriptorTableOffsetMap& offsets, uint32 frameIndex)
    : m_descriptorTable(descriptorTable),
      m_computePipeline(computePipeline),
      m_offsets(offsets),
      m_frameIndex(frameIndex),
      m_pipelineType(1) // 1 = Compute
{
    AssertDebug(descriptorTable != nullptr, "Descriptor table must not be null");
}

BindDescriptorTable::BindDescriptorTable(DescriptorTable* descriptorTable, RaytracingPipeline* raytracingPipeline, const DescriptorTableOffsetMap& offsets, uint32 frameIndex)
    : m_descriptorTable(descriptorTable),
      m_raytracingPipeline(raytracingPipeline),
      m_offsets(offsets),
      m_frameIndex(frameIndex),
      m_pipelineType(2) // 2 = Raytracing
{
    AssertDebug(descriptorTable != nullptr, "Descriptor table must not be null");
}

void BindDescriptorTable::PrepareStatic(CmdBase* cmd, Frame* frame)
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

void BindDescriptorTable::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
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
void InsertBarrier::CheckNotInRenderPass(CommandBuffer* commandBuffer) const
{
    HYP_GFX_ASSERT(!commandBuffer->IsInRenderPass());
}
#endif

#pragma endregion InsertBarrier

#pragma region BeginFramebuffer

#ifdef HYP_DEBUG_MODE
thread_local int s_framebufferCount;
thread_local Framebuffer* s_currentFramebuffer;
#endif

BeginFramebuffer::BeginFramebuffer(Framebuffer* framebuffer)
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

void BeginFramebuffer::PrepareStatic(CmdBase* cmd, Frame* frame)
{
}

#pragma endregion BeginFramebuffer

#pragma region EndFramebuffer

EndFramebuffer::EndFramebuffer(Framebuffer* framebuffer)
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

void EndFramebuffer::PrepareStatic(CmdBase* cmd, Frame* frame)
{
}

#pragma endregion EndFramebuffer

#pragma region BindGraphicsPipeline

#ifdef HYP_DEBUG_MODE

BindGraphicsPipeline::BindGraphicsPipeline(GraphicsPipeline* pipeline, const Viewport& viewport)
    : m_pipeline(pipeline),
      m_viewport(viewport)
{
    Assert(s_framebufferCount, "Cannot bind graphics pipeline: not in a framebuffer");
}

BindGraphicsPipeline::BindGraphicsPipeline(GraphicsPipeline* pipeline, Vec2i viewportOffset, Vec2u viewportExtent)
    : m_pipeline(pipeline),
      m_viewport(Viewport { viewportExtent, viewportOffset })
{
    Assert(s_framebufferCount, "Cannot bind graphics pipeline: not in a framebuffer");
}

BindGraphicsPipeline::BindGraphicsPipeline(GraphicsPipeline* pipeline)
    : m_pipeline(pipeline),
      m_viewport()
{
    Assert(s_framebufferCount, "Cannot bind graphics pipeline: not in a framebuffer");
}

#endif

void BindGraphicsPipeline::PrepareStatic(CmdBase* cmd, Frame*)
{
    BindGraphicsPipeline* cmdCasted = static_cast<BindGraphicsPipeline*>(cmd);

    if (cmdCasted->m_pipeline)
    {
        cmdCasted->m_pipeline->lastFrame = RenderApi::GetFrameCounter();
    }
}

void BindGraphicsPipeline::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    BindGraphicsPipeline* cmdCasted = static_cast<BindGraphicsPipeline*>(cmd);

    if (cmdCasted->m_viewport.position != Vec2i(0, 0) || cmdCasted->m_viewport.extent != Vec2u(0, 0))
    {
        cmdCasted->m_pipeline->Bind(commandBuffer, cmdCasted->m_viewport.position, cmdCasted->m_viewport.extent);
    }
    else
    {
        cmdCasted->m_pipeline->Bind(commandBuffer);
    }

    static_assert(std::is_trivially_destructible_v<BindGraphicsPipeline>);
    // cmdCasted->~BindGraphicsPipeline();
}

#pragma endregion BindGraphicsPipeline

#pragma region BindComputePipeline

void BindComputePipeline::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    BindComputePipeline* cmdCasted = static_cast<BindComputePipeline*>(cmd);

    cmdCasted->m_pipeline->Bind(commandBuffer);

    static_assert(std::is_trivially_destructible_v<BindComputePipeline>);
    // cmdCasted->~BindComputePipeline();
}

#pragma endregion BindComputePipeline

#pragma region BindRaytracingPipeline

void BindRaytracingPipeline::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    BindRaytracingPipeline* cmdCasted = static_cast<BindRaytracingPipeline*>(cmd);

    cmdCasted->m_pipeline->Bind(commandBuffer);

    static_assert(std::is_trivially_destructible_v<BindRaytracingPipeline>);
    // cmdCasted->~BindRaytracingPipeline();
}

#pragma endregion BindRaytracingPipeline

#pragma region DispatchCompute

void DispatchCompute::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    DispatchCompute* cmdCasted = static_cast<DispatchCompute*>(cmd);

    cmdCasted->m_pipeline->Dispatch(commandBuffer, cmdCasted->m_workgroupCount);

    static_assert(std::is_trivially_destructible_v<DispatchCompute>);
    // cmdCasted->~DispatchCompute();
}

#pragma endregion DispatchCompute

#pragma region TraceRays

void TraceRays::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    TraceRays* cmdCasted = static_cast<TraceRays*>(cmd);

    cmdCasted->m_pipeline->TraceRays(commandBuffer, cmdCasted->m_workgroupCount);

    static_assert(std::is_trivially_destructible_v<TraceRays>);
    // cmdCasted->~TraceRays();
}

#pragma endregion TraceRays

#pragma region DrawQuad

static Handle<Mesh> g_quadMesh;

void DrawQuad::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    if (HYP_UNLIKELY(!g_quadMesh))
    {
        g_quadMesh = MeshBuilder::Quad();
        g_quadMesh->SetFlags(MF_VIEW_INDEPENDENT);
        InitObject(g_quadMesh);

        CurrentThreadObject()->AtExit([]()
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

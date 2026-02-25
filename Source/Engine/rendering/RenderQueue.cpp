/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/RenderQueue.hpp>
#include <rendering/Frame.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/DescriptorSetCache.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/GraphicsPipeline.hpp>
#include <rendering/ComputePipeline.hpp>
#include <rendering/ShaderInstance.hpp>
#include <rendering/Shader.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/RenderGroup.hpp>

#include <rendering/RayTracingPipeline.hpp>
#include <rendering/AccelerationStructure.hpp>

#include <rendering/util/ShaderCompiler.hpp>

#include <scene/View.hpp>

#include <util/MeshBuilder.hpp>

namespace Hyperion {

#pragma region RenderQueue

template <>
void RenderQueue::Prepare(Frame* frame)
{
    Assert(frame != nullptr);

    for (CmdHeader& cmdHeader : m_cmdHeaders)
    {
        CmdBase* cmdDataPtr = reinterpret_cast<CmdBase*>(m_buffer.Data() + cmdHeader.offset);
        AssertDebug(cmdHeader.offset < m_buffer.Size());

        if (cmdHeader.prepareFnPtr != nullptr)
        {
            cmdHeader.prepareFnPtr(cmdDataPtr, frame);
        }
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

    m_bindIndex = pipeline->GetShader()->GetShader()->GetDescriptorTableDeclaration()->GetDescriptorSetIndex(descriptorSet->GetLayout().GetName());
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

    m_bindIndex = pipeline->GetShader()->GetShader()->GetDescriptorTableDeclaration()->GetDescriptorSetIndex(descriptorSet->GetLayout().GetName());
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

BindDescriptorSet::BindDescriptorSet(DescriptorSet* descriptorSet, RayTracingPipeline* pipeline, const DescriptorSetOffsetMap& offsets)
    : m_descriptorSet(descriptorSet),
      m_rayTracingPipeline(pipeline),
      m_offsets(offsets),
      m_pipelineType(2) // 2 = RayTracing
{
    AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
    AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");

    AssertDebug(pipeline && pipeline->GetShader());

    m_bindIndex = pipeline->GetShader()->GetShader()->GetDescriptorTableDeclaration()->GetDescriptorSetIndex(descriptorSet->GetLayout().GetName());
    AssertDebug(m_bindIndex != ~0u, "Invalid bind index for descriptor set {}", descriptorSet->GetLayout().GetName());
}

BindDescriptorSet::BindDescriptorSet(DescriptorSet* descriptorSet, RayTracingPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex)
    : m_descriptorSet(descriptorSet),
      m_rayTracingPipeline(pipeline),
      m_offsets(offsets),
      m_bindIndex(bindIndex),
      m_pipelineType(2) // 2 = RayTracing
{
    AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
    AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");
    AssertDebug(m_bindIndex != ~0u, "Invalid bind index");
}

void BindDescriptorSet::PrepareStatic(CmdBase* cmd, Frame* frame)
{
    BindDescriptorSet* cmdCasted = static_cast<BindDescriptorSet*>(cmd);

    Assert(cmdCasted->m_descriptorSet->IsCreated());
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
    case 2: // RayTracing
        cmdCasted->m_descriptorSet->Bind(commandBuffer, cmdCasted->m_rayTracingPipeline, cmdCasted->m_offsets, cmdCasted->m_bindIndex);
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

BindDescriptorTable::BindDescriptorTable(DescriptorTable* descriptorTable, RayTracingPipeline* rayTracingPipeline, const DescriptorTableOffsetMap& offsets, uint32 frameIndex)
    : m_descriptorTable(descriptorTable),
      m_rayTracingPipeline(rayTracingPipeline),
      m_offsets(offsets),
      m_frameIndex(frameIndex),
      m_pipelineType(2) // 2 = RayTracing
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
    case 2: // RayTracing
        cmdCasted->m_descriptorTable->Bind(commandBuffer, cmdCasted->m_frameIndex, cmdCasted->m_rayTracingPipeline, cmdCasted->m_offsets);
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
    Assert(!commandBuffer->IsInRenderPass());
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

void BeginFramebuffer::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    BeginFramebuffer* cmdCasted = static_cast<BeginFramebuffer*>(cmd);

    cmdCasted->m_framebuffer->BeginCapture(commandBuffer);

    static_assert(std::is_trivially_destructible_v<BeginFramebuffer>);
    // cmdCasted->~BeginFramebuffer();
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

void EndFramebuffer::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    EndFramebuffer* cmdCasted = static_cast<EndFramebuffer*>(cmd);

    cmdCasted->m_framebuffer->EndCapture(commandBuffer);

    static_assert(std::is_trivially_destructible_v<EndFramebuffer>);
    // cmdCasted->~EndFramebuffer();
}

#pragma endregion EndFramebuffer

#pragma region ClearFramebuffer

void ClearFramebuffer::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    ClearFramebuffer* cmdCasted = static_cast<ClearFramebuffer*>(cmd);

    cmdCasted->m_framebuffer->Clear(commandBuffer);

    static_assert(std::is_trivially_destructible_v<ClearFramebuffer>);
    // cmdCasted->~ClearFramebuffer();
}

#pragma endregion ClearFramebuffer

#pragma region BindGraphicsPipeline

void BindGraphicsPipeline::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    BindGraphicsPipeline* cmdCasted = static_cast<BindGraphicsPipeline*>(cmd);
    
    cmdCasted->m_pipeline->lastFrame = GetFrameCounter();

    if (cmdCasted->m_viewport.position != Vec2i(0, 0) || cmdCasted->m_viewport.extent != Vec2u(0, 0))
    {
        cmdCasted->m_pipeline->Bind(commandBuffer, cmdCasted->m_viewport.position, cmdCasted->m_viewport.extent);
    }
    else
    {
        cmdCasted->m_pipeline->Bind(commandBuffer);
    }

    //// temporary, will be removed once everything operates through CommitDrawState().
    //RenderInterface::State& state = g_renderInterface->state;
    //state.Reset();

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

#pragma region BindRayTracingPipeline

void BindRayTracingPipeline::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    BindRayTracingPipeline* cmdCasted = static_cast<BindRayTracingPipeline*>(cmd);

    cmdCasted->m_pipeline->Bind(commandBuffer);

    static_assert(std::is_trivially_destructible_v<BindRayTracingPipeline>);
    // cmdCasted->~BindRayTracingPipeline();
}

#pragma endregion BindRayTracingPipeline

#pragma region DispatchCompute

void DispatchCompute::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    DispatchCompute* cmdCasted = static_cast<DispatchCompute*>(cmd);

    ComputePipeline* pipeline = cmdCasted->m_pipeline;

    if (pipeline == nullptr)
    {
        g_renderInterface->CommitPipelineState(PSO_Compute, commandBuffer);
        
        pipeline = g_renderInterface->state.prevComputePipeline;
        AssertDebug(pipeline != nullptr, "No compute pipeline set, call SetCurrentShader before DispatchCompute() without pipeline passed");
    }

    pipeline->Dispatch(commandBuffer, cmdCasted->m_workgroupCount);

    static_assert(std::is_trivially_destructible_v<DispatchCompute>);
    // cmdCasted->~DispatchCompute();
}

#pragma endregion DispatchCompute

#pragma region TraceRays

void TraceRays::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    TraceRays* cmdCasted = static_cast<TraceRays*>(cmd);

    RayTracingPipeline* pipeline = cmdCasted->m_pipeline;
    
    if (pipeline == nullptr)
    {
        g_renderInterface->CommitPipelineState(PSO_RayTracing, commandBuffer);

        pipeline = g_renderInterface->state.prevRayTracingPipeline;
        AssertDebug(pipeline != nullptr, "No rayTracing pipeline set, call SetCurrentShader before TraceRays() without pipeline passed");
    }

    pipeline->TraceRays(commandBuffer, cmdCasted->m_workgroupCount);

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
        g_quadMesh->SetFlags(MeshFlags::ViewIndependent);
        InitObject(g_quadMesh);

        CurrentThreadObject()->AddOnExitCallback([]()
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

#pragma region SetStencilState

void SetStencilState::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    SetStencilState* cmdCasted = static_cast<SetStencilState*>(cmd);

    RenderInterface::State& state = g_renderInterface->state;

    if (state.stencilReference != cmdCasted->m_referenceValue
        || state.stencilCompareMask != cmdCasted->m_compareMask
        || state.stencilWriteMask != cmdCasted->m_writeMask)
    {
        // set stencil state
        state.stencilReference = cmdCasted->m_referenceValue;
        state.stencilCompareMask = cmdCasted->m_compareMask;
        state.stencilWriteMask = cmdCasted->m_writeMask;

        // invalidate pipeline state
        state.prevGraphicsPipeline = nullptr;

        state.dirtyUniforms |= (state.validUniforms | state.dirtyBufferOffsets);
        state.validUniforms = 0;

        Memory::Fill(state.prevBoundDescriptorSets, 0, sizeof(state.prevBoundDescriptorSets));
    }

    static_assert(std::is_trivially_destructible_v<SetStencilState>);
    // cmdCasted->~SetStencilState();
}

#pragma endregion SetStencilState

#pragma region SetCurrentShader

void SetCurrentShader::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetCurrentShader* cmdCasted = static_cast<SetCurrentShader*>(cmd);

    RenderInterface::State& state = g_renderInterface->state;

    ShaderDesc& shaderDesc = cmdCasted->shaderDesc;

    // merge shared global properties with the one we're setting
    MergeGlobalShaderProperties(shaderDesc.properties);

    state.attributes.SetShaderName(shaderDesc.name);
    state.attributes.SetShaderProperties(shaderDesc.properties);

    static_assert(std::is_trivially_destructible_v<SetCurrentShader>);
    // cmdCasted->~SetCurrentShader();
}

#pragma endregion SetCurrentShader

#pragma region SetCurrentView

void SetCurrentView::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetCurrentView* cmdCasted = static_cast<SetCurrentView*>(cmd);

    Framebuffer* framebuffer = nullptr;

    g_renderInterface->state.renderTargetDesc = cmdCasted->renderTargetDesc;
    g_renderInterface->state.viewport = cmdCasted->viewport;

    static_assert(std::is_trivially_destructible_v<SetCurrentView>);
    // cmdCasted->~SetCurrentView();
}

#pragma endregion SetCurrentView

#pragma region SetTopology

void SetTopology::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetTopology* cmdCasted = static_cast<SetTopology*>(cmd);

    if (g_renderInterface->state.attributes.GetMeshAttributes().topology == cmdCasted->topology)
        return;

    g_renderInterface->state.attributes.GetMeshAttributes().topology = cmdCasted->topology;
    g_renderInterface->state.attributes.Invalidate();
    
    static_assert(std::is_trivially_destructible_v<SetTopology>);
    // cmdCasted->~SetTopology();
}

#pragma endregion SetTopology

#pragma region SetVertexAttributes

void SetVertexAttributes::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetVertexAttributes* cmdCasted = static_cast<SetVertexAttributes*>(cmd);

    RenderInterface::State& state = g_renderInterface->state;

    if (state.attributes.GetMeshAttributes().vertexAttributes == cmdCasted->vertexAttributes)
        return;

    state.attributes.GetMeshAttributes().vertexAttributes = cmdCasted->vertexAttributes;

    state.attributes.Invalidate();
    
    static_assert(std::is_trivially_destructible_v<SetVertexAttributes>);
    // cmdCasted->~SetVertexAttributes();
}

#pragma endregion SetVertexAttributes

#pragma region SetCurrentBlendFunction

void SetCurrentBlendFunction::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetCurrentBlendFunction* cmdCasted = static_cast<SetCurrentBlendFunction*>(cmd);

    if (g_renderInterface->state.attributes.GetMaterialAttributes().blendFunction == cmdCasted->blendFunction)
        return;

    g_renderInterface->state.attributes.GetMaterialAttributes().blendFunction = cmdCasted->blendFunction;
    g_renderInterface->state.attributes.Invalidate();
    
    static_assert(std::is_trivially_destructible_v<SetCurrentBlendFunction>);
    // cmdCasted->~SetCurrentBlendFunction();
}

#pragma endregion SetCurrentBlendFunction

#pragma region SetDepthWrite

void SetDepthWrite::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetDepthWrite* cmdCasted = static_cast<SetDepthWrite*>(cmd);

    if (cmdCasted->depthWrite)
    {
        if (g_renderInterface->state.attributes.GetMaterialAttributes().flags & MAF_DEPTH_WRITE)
            return;

        g_renderInterface->state.attributes.GetMaterialAttributes().flags |= MAF_DEPTH_WRITE;
    }
    else
    {
        if (!(g_renderInterface->state.attributes.GetMaterialAttributes().flags & MAF_DEPTH_WRITE))
            return;

        g_renderInterface->state.attributes.GetMaterialAttributes().flags &= ~MAF_DEPTH_WRITE;
    }

    g_renderInterface->state.attributes.Invalidate();
    
    static_assert(std::is_trivially_destructible_v<SetDepthWrite>);
    // cmdCasted->~SetDepthWrite();
}

#pragma endregion SetDepthWrite

#pragma region SetDepthTest

void SetDepthTest::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetDepthTest* cmdCasted = static_cast<SetDepthTest*>(cmd);

    if (cmdCasted->depthTest)
    {
        if (g_renderInterface->state.attributes.GetMaterialAttributes().flags & MAF_DEPTH_TEST)
            return;

        g_renderInterface->state.attributes.GetMaterialAttributes().flags |= MAF_DEPTH_TEST;
    }
    else
    {
        if (!(g_renderInterface->state.attributes.GetMaterialAttributes().flags & MAF_DEPTH_TEST))
            return;

        g_renderInterface->state.attributes.GetMaterialAttributes().flags &= ~MAF_DEPTH_TEST;
    }

    g_renderInterface->state.attributes.Invalidate();
    
    static_assert(std::is_trivially_destructible_v<SetDepthTest>);
    // cmdCasted->~SetDepthTest();
}

#pragma endregion SetDepthTest

#pragma region SetStencilTest

void SetStencilTest::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetStencilTest* cmdCasted = static_cast<SetStencilTest*>(cmd);

    if (cmdCasted->stencilTest)
    {
        if (g_renderInterface->state.attributes.GetMaterialAttributes().flags & MAF_STENCIL_TEST)
            return;

        g_renderInterface->state.attributes.GetMaterialAttributes().flags |= MAF_STENCIL_TEST;
    }
    else
    {
        if (!(g_renderInterface->state.attributes.GetMaterialAttributes().flags & MAF_STENCIL_TEST))
            return;

        g_renderInterface->state.attributes.GetMaterialAttributes().flags &= ~MAF_STENCIL_TEST;
    }

    g_renderInterface->state.attributes.Invalidate();
    
    static_assert(std::is_trivially_destructible_v<SetStencilTest>);
    // cmdCasted->~SetStencilTest();
}

#pragma endregion SetStencilTest

#pragma region SetStencilFunction

void SetStencilFunction::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetStencilFunction* cmdCasted = static_cast<SetStencilFunction*>(cmd);

    if (g_renderInterface->state.attributes.GetMaterialAttributes().stencilFunction == cmdCasted->stencilFunction)
        return;

    g_renderInterface->state.attributes.GetMaterialAttributes().stencilFunction = cmdCasted->stencilFunction;
    g_renderInterface->state.attributes.Invalidate();
    
    static_assert(std::is_trivially_destructible_v<SetStencilFunction>);
    // cmdCasted->~SetStencilFunction();
}

#pragma endregion SetStencilFunction

#pragma region SetFillMode

void SetFillMode::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetFillMode* cmdCasted = static_cast<SetFillMode*>(cmd);

    if (g_renderInterface->state.attributes.GetMaterialAttributes().fillMode == cmdCasted->fillMode)
        return;

    g_renderInterface->state.attributes.GetMaterialAttributes().fillMode = cmdCasted->fillMode;
    g_renderInterface->state.attributes.Invalidate();
    
    static_assert(std::is_trivially_destructible_v<SetFillMode>);
    // cmdCasted->~SetFillMode();
}

#pragma endregion SetFillMode

#pragma region SetFaceCullMode

void SetFaceCullMode::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetFaceCullMode* cmdCasted = static_cast<SetFaceCullMode*>(cmd);

    if (g_renderInterface->state.attributes.GetMaterialAttributes().cullFaces == cmdCasted->faceCullMode)
        return;

    g_renderInterface->state.attributes.GetMaterialAttributes().cullFaces = cmdCasted->faceCullMode;
    g_renderInterface->state.attributes.Invalidate();
    
    static_assert(std::is_trivially_destructible_v<SetFaceCullMode>);
    // cmdCasted->~SetFaceCullMode();
}

#pragma endregion SetFaceCullMode

#pragma region SetShaderUniform

void SetShaderUniform::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    SetShaderUniform* cmdCasted = static_cast<SetShaderUniform*>(cmd);
    
    RenderInterface& ri = *g_renderInterface;
    RenderInterface::State& state = ri.state;

    ShaderUniform& uniform = state.shaderUniforms[cmdCasted->uniformIndex];

    if (uniform != cmdCasted->uniform                                   // uniform packet differs
        || !(state.validUniforms & (1u << cmdCasted->uniformIndex)))    // previous bound is invalid
    {
        uniform = cmdCasted->uniform;

        state.dirtyUniforms |= (1u << cmdCasted->uniformIndex);
    }

    if (cmdCasted->uniform.type == ShaderUniform::UT_Buffer)
    {
        // strides differ for buffer; needs rebind
        if (state.shaderUniformBufferOffsetStrides[cmdCasted->uniformIndex] != cmdCasted->shaderDataOffset.stride)
        {
            state.dirtyUniforms |= (1u << cmdCasted->uniformIndex);
        }

        // buffer offset + stride updating
        state.shaderUniformBufferOffsets[cmdCasted->uniformIndex] = cmdCasted->shaderDataOffset.offset;
        state.shaderUniformBufferOffsetStrides[cmdCasted->uniformIndex] = cmdCasted->shaderDataOffset.stride;

        state.dirtyBufferOffsets |= (1u << cmdCasted->uniformIndex);
    }
    else
    {
        // unset dirty buffer offset bit if it is not a buffer
        state.dirtyBufferOffsets &= ~(1u << cmdCasted->uniformIndex);
    }

    static_assert(std::is_trivially_destructible_v<SetShaderUniform>);
    // cmdCasted->~SetShaderUniform();
}

#pragma endregion SetShaderUniform

#pragma region CommitDrawState

void CommitDrawState::InvokeStatic(CmdBase*, CommandBuffer* commandBuffer)
{
    g_renderInterface->CommitDrawState(commandBuffer);

    static_assert(std::is_trivially_destructible_v<CommitDrawState>);
    // cmdCasted->~CommitDrawState();
}

#pragma endregion CommitDrawState

} // namespace Hyperion

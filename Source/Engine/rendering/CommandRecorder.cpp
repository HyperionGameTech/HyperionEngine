/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/CommandRecorder.hpp>
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

#pragma region CommandRecorder

template <>
void CommandRecorder::Prepare(Frame* frame)
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
void CommandRecorder::Execute(CommandBuffer* commandBuffer)
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
    m_writableState.Release();
}

#pragma endregion CommandRecorder

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

#pragma region SetCurrentFramebuffer

thread_local Framebuffer* s_currentFramebuffer;

SetCurrentFramebuffer::SetCurrentFramebuffer(Framebuffer* framebuffer)
    : m_framebuffer(framebuffer)
{
    if (m_framebuffer != nullptr)
    {
        m_framebuffer->SetIsDeferredRecording(true);
    }

    if (s_currentFramebuffer != nullptr && s_currentFramebuffer != m_framebuffer)
    {
        s_currentFramebuffer->SetIsDeferredRecording(false);
    }

    s_currentFramebuffer = m_framebuffer;
}

void SetCurrentFramebuffer::PrepareStatic(CmdBase* cmd, Frame* frame)
{
}

void SetCurrentFramebuffer::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    SetCurrentFramebuffer* cmdCasted = static_cast<SetCurrentFramebuffer*>(cmd);

    RenderInterface::State& state = g_renderInterface->state;

    if (cmdCasted->m_framebuffer == nullptr && state.boundFramebuffer != nullptr)
    {
        // end render pass if we are in one.
        // otherwise, we don't call BeginCapture() until CommitDrawState...
        // this lets us transition textures before we actually begin the pass
        state.boundFramebuffer->EndCapture(commandBuffer);
        state.boundFramebuffer = nullptr;
    }

    state.framebuffer = cmdCasted->m_framebuffer;

    static_assert(std::is_trivially_destructible_v<SetCurrentFramebuffer>);
    // cmdCasted->~SetCurrentFramebuffer();
}

#pragma endregion SetCurrentFramebuffer

#pragma region ClearFramebuffer

void ClearFramebuffer::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    ClearFramebuffer* cmdCasted = static_cast<ClearFramebuffer*>(cmd);
    
    AssertDebug(cmdCasted->framebuffer != nullptr);

    RenderInterface::State& state = g_renderInterface->state;

    state.framebuffer = cmdCasted->framebuffer;

    if (state.boundFramebuffer != cmdCasted->framebuffer)
    {
        if (state.boundFramebuffer != nullptr)
        {
            // end render pass if we are in one and not same as the one we want to bind.
            state.boundFramebuffer->EndCapture(commandBuffer);
            state.boundFramebuffer = nullptr;
        }
        
        // begin pass
        state.boundFramebuffer = cmdCasted->framebuffer;
        
        cmdCasted->framebuffer->BeginCapture(commandBuffer);
    }

    if (int(cmdCasted->rect.x1) - int(cmdCasted->rect.x0) == 0
        && int(cmdCasted->rect.y1) - int(cmdCasted->rect.y0) == 0)
    {
        cmdCasted->framebuffer->Clear(commandBuffer, cmdCasted->attachmentsMask);
    }
    else
    {
        cmdCasted->framebuffer->Clear(commandBuffer, cmdCasted->rect, cmdCasted->attachmentsMask);
    }

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

#pragma region SetCurrentViewport

void SetCurrentViewport::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetCurrentViewport* cmdCasted = static_cast<SetCurrentViewport*>(cmd);

    Framebuffer* framebuffer = nullptr;

    g_renderInterface->state.viewport = cmdCasted->viewport;

    static_assert(std::is_trivially_destructible_v<SetCurrentViewport>);
    // cmdCasted->~SetCurrentViewport();
}

#pragma endregion SetCurrentViewport

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

#pragma region SetDepthBias

void SetDepthBias::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetDepthBias* cmdCasted = static_cast<SetDepthBias*>(cmd);

    RenderInterface::State& state = g_renderInterface->state;

    const bool enableDepthBias = cmdCasted->depthBias != 0;

    if (enableDepthBias)
    {
        if ((state.attributes.GetMaterialAttributes().flags & MAF_DEPTH_BIAS)
            && state.attributes.GetMaterialAttributes().depthBias == cmdCasted->depthBias
            && MathUtil::ApproxEqual(state.attributes.GetMaterialAttributes().depthBiasSlope, cmdCasted->depthBiasSlope))
        {
            return;
        }

        state.attributes.GetMaterialAttributes().flags |= MAF_DEPTH_BIAS;
        state.attributes.GetMaterialAttributes().depthBias = cmdCasted->depthBias;
        state.attributes.GetMaterialAttributes().depthBiasSlope = cmdCasted->depthBiasSlope;
    }
    else
    {
        if (!(state.attributes.GetMaterialAttributes().flags & MAF_DEPTH_BIAS))
            return;

        state.attributes.GetMaterialAttributes().flags &= ~MAF_DEPTH_BIAS;
    }

    state.attributes.Invalidate();
    
    static_assert(std::is_trivially_destructible_v<SetDepthBias>);
    // cmdCasted->~SetDepthBias();
}

#pragma endregion SetDepthBias

#pragma region SetDepthClamp

void SetDepthClamp::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetDepthClamp* cmdCasted = static_cast<SetDepthClamp*>(cmd);

    if (cmdCasted->depthClamp)
    {
        if (g_renderInterface->state.attributes.GetMaterialAttributes().flags & MAF_DEPTH_CLAMP)
            return;

        g_renderInterface->state.attributes.GetMaterialAttributes().flags |= MAF_DEPTH_CLAMP;
    }
    else
    {
        if (!(g_renderInterface->state.attributes.GetMaterialAttributes().flags & MAF_DEPTH_CLAMP))
            return;

        g_renderInterface->state.attributes.GetMaterialAttributes().flags &= ~MAF_DEPTH_CLAMP;
    }

    g_renderInterface->state.attributes.Invalidate();
    
    static_assert(std::is_trivially_destructible_v<SetDepthClamp>);
    // cmdCasted->~SetDepthClamp();
}

#pragma endregion SetDepthClamp

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

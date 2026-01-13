/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/RenderQueue.hpp>
#include <rendering/Frame.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/DescriptorSetCache.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/GraphicsPipeline.hpp>
#include <rendering/ComputePipeline.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/RenderGroup.hpp>

#include <rendering/raytracing/RenderRaytracingPipeline.hpp>
#include <rendering/raytracing/RenderAccelerationStructure.hpp>

#include <rendering/util/ShaderCompiler.hpp>

#include <scene/View.hpp>

#include <util/MeshBuilder.hpp>

namespace Hyperion {

HYP_DISABLE_OPTIMIZATION;

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

void BindGraphicsPipeline::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    BindGraphicsPipeline* cmdCasted = static_cast<BindGraphicsPipeline*>(cmd);
    
    cmdCasted->m_pipeline->lastFrame = RenderApi::GetFrameCounter();

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

#pragma region SetStencilState

void SetStencilState::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    SetStencilState* cmdCasted = static_cast<SetStencilState*>(cmd);

    g_renderInterface->state.stencilReference = cmdCasted->m_referenceValue;
    g_renderInterface->state.stencilCompareMask = cmdCasted->m_compareMask;
    g_renderInterface->state.stencilWriteMask = cmdCasted->m_writeMask;

    static_assert(std::is_trivially_destructible_v<SetStencilState>);
    // cmdCasted->~SetStencilState();
}

#pragma endregion SetStencilState

#pragma region SetCurrentShader

void SetCurrentShader::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetCurrentShader* cmdCasted = static_cast<SetCurrentShader*>(cmd);

    g_renderInterface->state.shader = cmdCasted->m_shader;

    static_assert(std::is_trivially_destructible_v<SetCurrentShader>);
    // cmdCasted->~SetCurrentShader();
}

#pragma endregion SetCurrentShader

#pragma region SetCurrentView

void SetCurrentView::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetCurrentView* cmdCasted = static_cast<SetCurrentView*>(cmd);

    Framebuffer* framebuffer = nullptr;

    if (!cmdCasted->m_view || !(framebuffer = cmdCasted->m_view->GetOutputTarget().GetFramebuffer()))
    {
        g_renderInterface->state.renderTargetDesc = {};
    }
    else
    {
        g_renderInterface->state.renderTargetDesc = framebuffer->GetRenderTargetDesc();
    }

    if (cmdCasted->m_view)
    {
        g_renderInterface->state.viewport = cmdCasted->m_view->GetViewport();
    }

    static_assert(std::is_trivially_destructible_v<SetCurrentView>);
    // cmdCasted->~SetCurrentView();
}

#pragma endregion SetCurrentView

#pragma region SetCurrentRenderGroup

void SetCurrentRenderGroup::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetCurrentRenderGroup* cmdCasted = static_cast<SetCurrentRenderGroup*>(cmd);

    g_renderInterface->state.renderGroup = cmdCasted->m_renderGroup;

    static_assert(std::is_trivially_destructible_v<SetCurrentRenderGroup>);
    // cmdCasted->~SetCurrentRenderGroup();
}

#pragma endregion SetCurrentRenderGroup

#pragma region SetShaderUniform

void SetShaderUniform::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetShaderUniform* cmdCasted = static_cast<SetShaderUniform*>(cmd);
    
    RenderInterface& ri = *g_renderInterface;
    RenderInterface::State& state = ri.state;

    ShaderUniform& uniform = state.shaderUniforms[cmdCasted->uniformIndex];

    if (uniform != cmdCasted->uniform || !(state.validUniforms & (1u << cmdCasted->uniformIndex)))
    {
        uniform = cmdCasted->uniform;

        state.dirtyUniforms |= (1u << cmdCasted->uniformIndex);
    }
    else if (uniform.type == ShaderUniform::UT_Buffer && cmdCasted->bufferOffset != state.shaderUniformBufferOffsets[cmdCasted->uniformIndex])
    {
        // buffer offset only updating
        state.shaderUniformBufferOffsets[cmdCasted->uniformIndex] = cmdCasted->bufferOffset;
        state.dirtyBufferOffsets |= (1u << cmdCasted->uniformIndex);
    }

    static_assert(std::is_trivially_destructible_v<SetShaderUniform>);
    // cmdCasted->~SetShaderUniform();
}

#pragma endregion SetShaderUniform

void SetShaderUniforms::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    SetShaderUniforms* cmdCasted = static_cast<SetShaderUniforms*>(cmd);
    
    for (uint32 i = 0; i < cmdCasted->count; i++)
    {
        SetShaderUniform setUniformCmd(cmdCasted->offset + i, cmdCasted->uniforms[i]);
        SetShaderUniform::InvokeStatic(&setUniformCmd, commandBuffer);
    }

    static_assert(std::is_trivially_destructible_v<SetShaderUniforms>);
    // cmdCasted->~SetShaderUniforms();
}

#pragma region SetShaderUniforms

#pragma endregion SetShaderUniforms

#pragma region CommitDrawState

static const RenderableAttributeSet s_defaultAttributes;

HYP_DISABLE_OPTIMIZATION;
void CommitDrawState::InvokeStatic(CmdBase*, CommandBuffer* commandBuffer)
{
    RenderInterface& ri = *g_renderInterface;
    RenderInterface::State& state = ri.state;

    GraphicsPipeline* pipeline = nullptr;

    if (!state.prevGraphicsPipeline
        || !state.prevGraphicsPipeline->MatchesSignature(
                state.shader,
                &state.renderTargetDesc,
                state.renderGroup
                    ? state.renderGroup->GetRenderableAttributes()
                    : s_defaultAttributes
            ))
    {
        GraphicsPipelineCacheHandle cacheHandle;
        
        ri.graphicsPipelineCache->GetOrCreate(
            state.shader,
            &state.renderTargetDesc,
            state.renderGroup
                ? state.renderGroup->GetRenderableAttributes()
                : s_defaultAttributes,
            cacheHandle);

        pipeline = *cacheHandle;

        BindGraphicsPipeline bindCmd(pipeline, state.viewport);
        BindGraphicsPipeline::InvokeStatic(&bindCmd, commandBuffer);

        state.prevGraphicsPipeline = pipeline;
        
        Memory::MemSet(state.prevBoundDescriptorSets, 0, sizeof(state.prevBoundDescriptorSets));
        
        // invalidate
        state.dirtyUniforms |= (state.validUniforms | state.dirtyBufferOffsets);
        state.validUniforms = 0;
    }
    else
    {
        pipeline = state.prevGraphicsPipeline;
    }

    // keep dirtyBufferOffsets '1' bits only if NOT a dirty uniform (indicating it needs rebinding)
    state.dirtyBufferOffsets &= ~(state.dirtyUniforms);
    
    Shader* shader = pipeline->GetShader();
    CompiledShader* compiledShader = shader->GetCompiledShader();

    AssertDebug(compiledShader != nullptr);

    const DescriptorTableDeclaration* tableDecl = compiledShader->GetDescriptorTableDeclaration();
    AssertDebug(tableDecl != nullptr);

    uint32 setsToBindMask = 0;
    uint32 ownedSetsMask = 0;
    uint32 dirtySetsMask = 0;

    static const auto FetchDescriptorSet = [](const DescriptorSetDeclaration& dsDecl, bool& outIsGlobalDS) -> DescriptorSet*
    {
        outIsGlobalDS = false;

        // global reference (TRANSITIONAL, WILL BE REMOVED EVENTUALLY)
        if (dsDecl.flags & DescriptorSetDeclarationFlags::REFERENCE)
        {
            if (dsDecl.flags & DescriptorSetDeclarationFlags::TEMPLATE)
            {
                const DescriptorSetDeclaration* refDsDecl = g_renderInterface->globalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration(dsDecl.name);
                AssertDebug(refDsDecl != nullptr);
                    
                DescriptorSetLayout layout { refDsDecl };
                return g_renderInterface->descriptorSetCache->GetOrCreate(layout);
            }

            outIsGlobalDS = true;

            return g_renderInterface->globalDescriptorTable->GetDescriptorSet(dsDecl.name, g_renderBackend->GetCurrentFrame()->GetFrameIndex());
        }
        else
        {
            DescriptorSetLayout layout { &dsDecl };
            return g_renderInterface->descriptorSetCache->GetOrCreate(layout);
        }
    };
    
    constexpr uint32 MaxDynamicOffsetsPerSet = 8;
    constexpr uint32 MaxDescriptorSetsBound = 8;

    DescriptorSet* setsToBind[MaxDescriptorSetsBound] {};
    uint8 bufferOffsets[MaxDynamicOffsetsPerSet][MaxDescriptorSetsBound] {}; // index of ShaderUniform
    uint8 bufferOffsetCounts[MaxDescriptorSetsBound] {};

    // Set descriptors
    if ((state.dirtyUniforms | state.dirtyBufferOffsets) != 0)
    {
        FOR_EACH_BIT((state.dirtyUniforms | state.dirtyBufferOffsets), uniformIndex)
        {
            const ShaderUniform& uniform = state.shaderUniforms[uniformIndex];

            // @TODO: Optimize the hell out of this so no more linear search and then checking REFERENCE flag and fetching from global ..

            const DescriptorDeclaration* decl = nullptr;

            const DescriptorSetDeclaration* foundSetDecl = nullptr;             // original

            for (const DescriptorSetDeclaration& setDecl : tableDecl->elements)
            {
                const DescriptorSetDeclaration* pSetDecl = &setDecl;

                if (setDecl.flags & DescriptorSetDeclarationFlags::REFERENCE)
                {
                    pSetDecl = g_renderInterface->globalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration(setDecl.name);
                    AssertDebug(pSetDecl != nullptr);
                }

                decl = pSetDecl->FindDescriptorDeclaration(uniform.name);

                if (decl)
                {
                    foundSetDecl = &setDecl;

                    break;
                }
            }

            if (decl)
            {
                bool isGlobalDS = false;

                const uint32 setIndex = foundSetDecl->setIndex;
                AssertDebug(setIndex < MaxDescriptorSetsBound);

                // differentiate between buffer offset changes and actual uniform data changes
                if (uniform.type == ShaderUniform::UT_Buffer && (state.dirtyBufferOffsets & (1u << uniformIndex)))
                {
                    // only a buffer offset update; keep the previous bound set.
                    setsToBind[setIndex] = state.prevBoundDescriptorSets[setIndex];

                    AssertDebug(setsToBind[setIndex] != nullptr);
                    setsToBindMask |= (1u << setIndex);
                }
                else if (!(ownedSetsMask & (1u << setIndex)))
                {
                    setsToBind[setIndex] = FetchDescriptorSet(*foundSetDecl, isGlobalDS);
                    AssertDebug(setsToBind[setIndex] != nullptr);

                    // is 'owned' if it is not a reference to a global descriptor set
                    const bool isOwnedSet = !(foundSetDecl->flags & DescriptorSetDeclarationFlags::REFERENCE);

                    if (isOwnedSet)
                    {
                        ownedSetsMask |= (1u << setIndex);

                        // not created yet; needs to be handled
                        if (!setsToBind[setIndex]->IsCreated())
                        {
                            dirtySetsMask |= (1u << setIndex);
                        }

                        // check if we need to re-visit buffers that only had buffer offsets changed,
                        // since we grabbed a fresh set.
                        if (bufferOffsetCounts[setIndex] != 0)
                        {
                            for (uint32 offsetIdx = 0; offsetIdx < MaxDynamicOffsetsPerSet; offsetIdx++)
                            {
                                const uint8 bufferShaderUniformIndex = bufferOffsets[setIndex][offsetIdx];
                                AssertDebug(bufferShaderUniformIndex < ArraySize(state.shaderUniforms));

                                const ShaderUniform& bufferUniform = state.shaderUniforms[bufferShaderUniformIndex];
                                AssertDebug(bufferUniform.type == ShaderUniform::UT_Buffer);

                                setsToBind[setIndex]->SetElement(bufferUniform.name, MakeStrongRef(bufferUniform.buffer));
                                
                                dirtySetsMask |= (1u << setIndex);
                            }
                        }
                    }

                    AssertDebug(setsToBind[setIndex] != nullptr);
                    setsToBindMask |= (1u << setIndex);
                }

                // For dirtySetsMask, we:
                // Mark the set as needing Update() called if:
                //   -- is NOT a global descriptor set (these are updated before the frame is submitted)
                //   -- is NOT just a buffer offset update (dirtyBufferOffsets would be wiped if the uniform actually changed)
                
                switch (uniform.type)
                {
                case ShaderUniform::UT_Buffer:
                    if (!(state.dirtyBufferOffsets & (1u << uniformIndex)))
                    {
                        setsToBind[setIndex]->SetElement(uniform.name, MakeStrongRef(uniform.buffer));

                        dirtySetsMask |= (1u << setIndex);
                    }

                    if (decl->isDynamic)
                    {
                        const uint8 offsetIndex = bufferOffsetCounts[setIndex]++;
                        AssertDebug(offsetIndex <= MaxDynamicOffsetsPerSet);

                        bufferOffsets[setIndex][offsetIndex] = (uint8)uniformIndex;
                    }

                    break;
                case ShaderUniform::UT_ImageView:
                    setsToBind[setIndex]->SetElement(uniform.name, MakeStrongRef(uniform.imageView));
                    
                    dirtySetsMask |= (1u << setIndex);

                    break;
                case ShaderUniform::UT_Sampler:
                    setsToBind[setIndex]->SetElement(uniform.name, MakeStrongRef(uniform.sampler));
                    
                    dirtySetsMask |= (1u << setIndex);

                    break;
                case ShaderUniform::UT_Tlas:
                    setsToBind[setIndex]->SetElement(uniform.name, MakeStrongRef(uniform.tlas));
                    
                    dirtySetsMask |= (1u << setIndex);

                    break;
                default:
                    HYP_UNREACHABLE();
                }

                state.validUniforms |= (1u << uniformIndex);
                state.dirtyUniforms &= ~(1u << uniformIndex);
                state.dirtyBufferOffsets &= ~(1u << uniformIndex);
            }
        }
    }
    
    // now, we need to rebind sets that have NOT been modified (for example, in case of the first binding of graphics pipeline)
    for (uint32 setIndex = 0; setIndex < uint32(tableDecl->elements.Size()); setIndex++)
    {
        if (setsToBindMask & (1u << setIndex))
            continue;

        if (!state.prevBoundDescriptorSets[setIndex])
        {
            // need to bind it again anyway if no prev descriptor set here.
            bool isGlobalDS = false;

            setsToBind[setIndex] = FetchDescriptorSet(tableDecl->elements[setIndex], isGlobalDS);
            AssertDebug(setsToBind[setIndex] != nullptr);

            if (!setsToBind[setIndex]->IsCreated())
                dirtySetsMask |= (1u << setIndex);
                
            setsToBindMask |= (1u << setIndex);
        }
    }
       
    FOR_EACH_BIT(dirtySetsMask, setIndex)
    {
        DescriptorSet* ds = setsToBind[setIndex];
        AssertDebug(ds != nullptr);

        if (!ds->IsCreated())
        {
            Assert(ds->Create());
        }
        else
        {
            bool isDirty = false;
            ds->UpdateDirtyState(&isDirty);

            if (isDirty)
            {
                AssertDebug(ownedSetsMask & (1u << setIndex),
                    "Updating set {} not owned by us! Could cause data access race!",
                    ds->GetDebugName());

                ds->Update();
            }
        }
    }

    // bind descriptor sets
    if (setsToBindMask != 0)
    {
        FOR_EACH_BIT(setsToBindMask, setIndex)
        {
            DescriptorSet* ds = setsToBind[setIndex];
            AssertDebug(ds != nullptr);

            DescriptorSetOffsetMap offsets {};
            for (uint8 bufferOffsetIndex = 0; bufferOffsetIndex < bufferOffsetCounts[setIndex]; bufferOffsetIndex++)
            {
                const uint8 shaderUniformIndex = bufferOffsets[setIndex][bufferOffsetIndex];

                const ShaderUniform& uniform = state.shaderUniforms[shaderUniformIndex];
                AssertDebug(uniform.type == ShaderUniform::UT_Buffer);

                const uint32 bufferOffset = state.shaderUniformBufferOffsets[shaderUniformIndex];

                offsets.Add(uniform.name, bufferOffset);
            }

            BindDescriptorSet bindCmd(ds, pipeline, offsets, setIndex);
            BindDescriptorSet::InvokeStatic(&bindCmd, commandBuffer);

            state.prevBoundDescriptorSets[setIndex] = ds;
        }
    }

    // debugging
    auto& dsDecls = tableDecl->elements;
    
    for (uint32 i = 0; i < dsDecls.Size(); i++)
    {
        Assert(state.prevBoundDescriptorSets[i] != nullptr
            && state.prevBoundDescriptorSets[i]->GetLayout().GetDeclaration()->name == dsDecls[i].name,
            "Invalid descriptor set binding for index {} (name : {})", i, dsDecls[i].name);
    }

    static_assert(std::is_trivially_destructible_v<CommitDrawState>);
    // cmdCasted->~CommitDrawState();
}

#pragma endregion CommitDrawState

} // namespace Hyperion

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

    // temporary, will be removed once everything operates through CommitDrawState().
    RenderInterface::State& state = g_renderInterface->state;
    state.Reset();

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
    
    if (cmdCasted->uniform.type == ShaderUniform::UT_Buffer)
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

        RenderInterface::State prevState = state;

        BindGraphicsPipeline bindCmd(pipeline, state.viewport);
        BindGraphicsPipeline::InvokeStatic(&bindCmd, commandBuffer);

        state = prevState;

        state.prevGraphicsPipeline = pipeline;
        
        // invalidate all uniforms on pipeline change
        state.dirtyUniforms |= (state.validUniforms | state.dirtyBufferOffsets);
        state.validUniforms = 0;
        
        Memory::MemSet(state.prevBoundDescriptorSets, 0, sizeof(state.prevBoundDescriptorSets));
    }
    else
    {
        pipeline = state.prevGraphicsPipeline;
    }

    Shader* shader = pipeline->GetShader();
    CompiledShader* compiledShader = shader->GetCompiledShader();

    AssertDebug(compiledShader != nullptr);

    const DescriptorTableDeclaration* tableDecl = compiledShader->GetDescriptorTableDeclaration();
    AssertDebug(tableDecl != nullptr);

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
    
    constexpr uint32 MaxDynamicOffsetsPerSet = 16;
    constexpr uint32 MaxDescriptorSetsBound = 4;

    DescriptorSet* setsToBind[MaxDescriptorSetsBound] {};

    uint32 bufferOffsets[MaxDescriptorSetsBound][MaxDynamicOffsetsPerSet] {};
    uint8 bufferOffsetCounts[MaxDescriptorSetsBound] {};

#define IS_BIT_SET(bits, bitIdx) ((bits) & (1u << bitIdx))

    uint8 uniformIndexToSetIndex[RenderInterface::State::MaxShaderUniforms];
    Memory::MemSet(uniformIndexToSetIndex, ubyte(-1), sizeof(uniformIndexToSetIndex));

    enum DescriptorSetState : uint8
    {
        DSS_NotDirty = 0x0,
        DSS_BufferOffsetChanged = 0x1,
        DSS_Dirty = 0x2
    };

    uint8 dsStates[MaxDescriptorSetsBound] = { };
    uint8 dsIndices = 0;

    // set up uniform index to sets mapping
    TBitset<FixedAllocator<2>> bits { state.dirtyUniforms | state.dirtyBufferOffsets | state.validUniforms };

    for (auto currBit = bits.Begin(); bits.AnyBitsSet(); currBit = bits.Begin())
    {
        const uint8 uniformIndex = (uint8)*currBit;
        const ShaderUniform& uniform = state.shaderUniforms[uniformIndex];

        const DescriptorDeclaration* decl = nullptr;

        const DescriptorSetDeclaration* foundSetDecl = nullptr;

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

        if (!decl)
        {
            // not found; skip
            state.dirtyUniforms &= ~(1u << uniformIndex);
            state.dirtyBufferOffsets &= ~(1u << uniformIndex);
            //state.validUniforms &= ~(1u << uniformIndex);

            bits.Set(currBit, false);

            continue;
        }

        const uint8 setIndex = uint8(foundSetDecl->setIndex);

        if (IS_BIT_SET(state.dirtyUniforms, uniformIndex))
        {
            if (!(dsStates[setIndex] & DSS_Dirty))
            {
                bool isGlobal;
                setsToBind[setIndex] = FetchDescriptorSet(*foundSetDecl, isGlobal);
                AssertDebug(setsToBind[setIndex] != nullptr);
                
                dsStates[setIndex] |= DSS_Dirty;
            }
        }
        
        if (IS_BIT_SET(state.dirtyBufferOffsets, uniformIndex))
        {
            if (!setsToBind[setIndex])
            {
                if (state.prevBoundDescriptorSets[setIndex])
                {
                    setsToBind[setIndex] = state.prevBoundDescriptorSets[setIndex];
                }
                else
                {
                    bool isGlobal;
                    setsToBind[setIndex] = FetchDescriptorSet(*foundSetDecl, isGlobal);
                    AssertDebug(setsToBind[setIndex] != nullptr);

                    dsStates[setIndex] |= DSS_Dirty;
                }
            }

            dsStates[setIndex] |= DSS_BufferOffsetChanged;
        }

        dsIndices |= uint8(1u << setIndex);

        uniformIndexToSetIndex[uniformIndex] = setIndex;

        bits.Set(currBit, false);
    }

    // valid uniforms / buffer offset updates need to be rebound if the set is dirty
    FOR_EACH_BIT(state.validUniforms | state.dirtyBufferOffsets, uniformIndex)
    {
        const uint8 setIndex = uniformIndexToSetIndex[uniformIndex];

        if (dsStates[setIndex] & DSS_Dirty)
        {
            state.dirtyUniforms |= (1u << uniformIndex);

            //state.validUniforms &= ~(1u << uniformIndex);
        }
    }

    // remaining valid uniforms need to be included in buffer offset updating if we are to update their sets' dynamic offsets.
    FOR_EACH_BIT(state.validUniforms, uniformIndex)
    {
        if (state.shaderUniforms[uniformIndex].type != ShaderUniform::UT_Buffer)
            continue;

        const uint8 setIndex = uniformIndexToSetIndex[uniformIndex];

        if ((dsStates[setIndex] & (DSS_BufferOffsetChanged | DSS_Dirty)) == DSS_BufferOffsetChanged)
        {
            state.dirtyBufferOffsets |= (1u << uniformIndex);

            //state.validUniforms &= ~(1u << uniformIndex);
        }
    }

    if (state.dirtyUniforms)
    {
        bits.Clear();
        bits |= { state.dirtyUniforms };

        // Set dirty descriptors
        for (auto currBit = bits.Begin(); bits.AnyBitsSet(); currBit = bits.Begin())
        {
            const uint8 uniformIndex = (uint8)*currBit;
            const ShaderUniform& uniform = state.shaderUniforms[uniformIndex];

            uint8 setIndex = uniformIndexToSetIndex[uniformIndex];
            AssertDebug(setIndex != uint8(-1));

            DescriptorSet* ds = setsToBind[setIndex];
            AssertDebug(ds != nullptr);

            AssertDebug(dsStates[setIndex] & DSS_Dirty);

            switch (uniform.type)
            {
            case ShaderUniform::UT_Buffer:
                ds->SetElement(uniform.name, uniform.buffer);

                state.dirtyBufferOffsets |= (1u << uniformIndex);

                break;
            case ShaderUniform::UT_ImageView:
                ds->SetElement(uniform.name, uniform.imageView);

                break;
            case ShaderUniform::UT_Sampler:
                ds->SetElement(uniform.name, uniform.sampler);

                break;
            case ShaderUniform::UT_Tlas:
                ds->SetElement(uniform.name, uniform.tlas);

                break;
            default:
                HYP_UNREACHABLE();
            }

            bits.Set(currBit, false);
        }
    }

    uint32 offsetValidityMasks[MaxDescriptorSetsBound] = { };

    if (state.dirtyBufferOffsets)
    {
        bits.Clear();
        bits |= state.dirtyBufferOffsets;

        for (auto currBit = bits.Begin(); bits.AnyBitsSet(); currBit = bits.Begin())
        {
            const uint8 uniformIndex = (uint8)*currBit;
            const ShaderUniform& uniform = state.shaderUniforms[uniformIndex];

            uint8 setIndex = uniformIndexToSetIndex[uniformIndex];
            AssertDebug(setIndex != uint8(-1));

            DescriptorSet* ds = setsToBind[setIndex];
            AssertDebug(ds != nullptr);

            // this isn't ideal
            uint8 descriptorIndex = ds->GetLayout().GetElement(uniform.name)->binding;

            bufferOffsets[setIndex][descriptorIndex] = state.shaderUniformBufferOffsets[uniformIndex];
            bufferOffsetCounts[setIndex]++;

            offsetValidityMasks[setIndex] |= (1u << descriptorIndex);

            bits.Set(currBit, false);
        }
    }
    
    // now, we need to rebind sets that have NOT been modified (for example, in case of the first binding of graphics pipeline)
    for (uint32 setIndex = 0; setIndex < uint32(tableDecl->elements.Size()); setIndex++)
    {
        if (!setsToBind[setIndex])
        {
            // need to bind it again anyway if no prev descriptor set here.
            if (!state.prevBoundDescriptorSets[setIndex])
            {
                bool isGlobalDS = false;
                setsToBind[setIndex] = FetchDescriptorSet(tableDecl->elements[setIndex], isGlobalDS);

                if (!setsToBind[setIndex]->IsCreated())
                {
                    // just create it here, we have nothing to bind for it
                    Assert(setsToBind[setIndex]->Create());
                }
            }
        }
    }

    // debug
    Array<ShaderUniform> validUniforms;
    validUniforms.Resize(RenderInterface::State::MaxShaderUniforms);
    FOR_EACH_BIT(state.validUniforms, i)
    {
        validUniforms[i] = state.shaderUniforms[i];
    }
    
    // debug
    Array<ShaderUniform> dirtyUniforms;
    dirtyUniforms.Resize(RenderInterface::State::MaxShaderUniforms);
    FOR_EACH_BIT(state.dirtyUniforms, i)
    {
        dirtyUniforms[i] = state.shaderUniforms[i];
    }

    // bind descriptor sets
    for (uint8 setIndex = 0; setIndex < MaxDescriptorSetsBound; setIndex++)
    {
        DescriptorSet* ds = setsToBind[setIndex];
            
        if (!ds)
            continue;

        if (dsStates[setIndex] & DSS_Dirty)
        {
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
                    ds->Update();
                }
            }
        }
        
        uint32 compactedOffsets[MaxDynamicOffsetsPerSet] = {};
        uint8 compactedCount = 0;

        if (bufferOffsetCounts[setIndex] != 0)
        {
            // offset are in order, but we need to compact them to remove gaps

            FOR_EACH_BIT(offsetValidityMasks[setIndex], i)
            {
                compactedOffsets[compactedCount++] = bufferOffsets[setIndex][i];
            }
        }

        ds->Bind(commandBuffer, pipeline, compactedOffsets, compactedCount, setIndex);

        state.prevBoundDescriptorSets[setIndex] = ds;
    }
                

    state.validUniforms |= state.dirtyUniforms;
    state.dirtyUniforms = 0;
    state.dirtyBufferOffsets = 0;

    // debugging
    auto& dsDecls = tableDecl->elements;
    
    for (uint32 i = 0; i < dsDecls.Size(); i++)
    {
        Assert(state.prevBoundDescriptorSets[i] != nullptr
            && state.prevBoundDescriptorSets[i]->GetLayout().GetDeclaration()->name == dsDecls[i].name,
            "Invalid descriptor set binding for index {} (name : {})", i, dsDecls[i].name);
    }

#undef IS_BIT_SET

    static_assert(std::is_trivially_destructible_v<CommitDrawState>);
    // cmdCasted->~CommitDrawState();
}

#pragma endregion CommitDrawState

} // namespace Hyperion

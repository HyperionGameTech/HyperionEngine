/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderGroup.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/env_grid/EnvGridRenderer.hpp>
#include <rendering/env_probe/EnvProbeRenderer.hpp>
#include <rendering/IndirectDraw.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/RenderGraphicsPipeline.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/RenderBackend.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <scene/Entity.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/Light.hpp>
#include <scene/View.hpp>

#include <scene/animation/Skeleton.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/utilities/ForEach.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>
#include <core/profiling/PerformanceClock.hpp>

#include <core/Defines.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>
#include <core/Constants.hpp>

#ifdef HYP_VULKAN
#include <rendering/vulkan/VulkanStructs.hpp>
#endif

namespace hyperion {

#pragma region RenderGroup

RenderGroup::RenderGroup()
    : HypObjectBase(),
      m_flags(RenderGroupFlags::NONE)
{
}

RenderGroup::RenderGroup(const ShaderRef& shader, const RenderableAttributeSet& renderableAttributes, EnumFlags<RenderGroupFlags> flags)
    : HypObjectBase(),
      m_flags(flags),
      m_shader(shader),
      m_renderableAttributes(renderableAttributes)
{
}

RenderGroup::RenderGroup(const ShaderRef& shader, const RenderableAttributeSet& renderableAttributes, const DescriptorTableRef& descriptorTable, EnumFlags<RenderGroupFlags> flags)
    : HypObjectBase(),
      m_flags(flags),
      m_shader(shader),
      m_descriptorTable(descriptorTable),
      m_renderableAttributes(renderableAttributes)
{
}

RenderGroup::~RenderGroup()
{
    SafeDelete(std::move(m_shader));
    SafeDelete(std::move(m_descriptorTable));
}

void RenderGroup::SetShader(const ShaderRef& shader)
{
    HYP_SCOPE;

    SafeDelete(std::move(m_shader));

    m_shader = shader;
}

void RenderGroup::SetRenderableAttributes(const RenderableAttributeSet& renderableAttributes)
{
    m_renderableAttributes = renderableAttributes;
}

void RenderGroup::Init()
{
    HYP_SCOPE;

    AddDelegateHandler(g_engineDriver->GetDelegates().OnShutdown.Bind([this]()
        {
            HYP_SCOPE;

            SafeDelete(std::move(m_shader));
            SafeDelete(std::move(m_descriptorTable));
        }));

    // If parallel rendering is globally disabled, disable it for this RenderGroup
    if (!g_renderBackend->GetRenderConfig().parallelRendering)
    {
        m_flags &= ~RenderGroupFlags::PARALLEL_RENDERING;
    }

    if (!g_renderBackend->GetRenderConfig().indirectRendering)
    {
        m_flags &= ~RenderGroupFlags::INDIRECT_RENDERING;
    }

    SetReady(true);
}

GraphicsPipelineCacheHandle RenderGroup::CreateGraphicsPipeline(PassData* pd, IDrawCallCollectionImpl* drawCallCollectionImpl) const
{
    HYP_SCOPE;

    Assert(pd != nullptr);
    Assert(drawCallCollectionImpl != nullptr);

    PerformanceClock clock;
    clock.Start();

    Handle<View> view = pd->view.Lock();
    Assert(view.IsValid());
    Assert(view->GetOutputTarget().IsValid());

    Assert(m_shader.IsValid());

    DescriptorTableRef descriptorTable = m_descriptorTable;

    if (!descriptorTable.IsValid())
    {
        const DescriptorTableDeclaration& descriptorTableDecl = m_shader->GetCompiledShader()->GetDescriptorTableDeclaration();

        descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);
        descriptorTable->SetDebugName(NAME_FMT("DescriptorTable_{}", m_shader->GetCompiledShader()->GetName()));

        // Setup instancing buffers if "Instancing" descriptor set exists
        const uint32 instancingDescriptorSetIndex = descriptorTable->GetDescriptorSetIndex("Instancing");

        if (instancingDescriptorSetIndex != ~0u)
        {
            for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
            {
                const GpuBufferRef& gpuBuffer = drawCallCollectionImpl->GetGpuBufferHolder()->GetBuffer(frameIndex);
                Assert(gpuBuffer.IsValid());

                const DescriptorSetRef& instancingDescriptorSet = descriptorTable->GetDescriptorSet("Instancing", frameIndex);
                Assert(instancingDescriptorSet.IsValid());

                instancingDescriptorSet->SetElement("EntityInstanceBatchesBuffer", gpuBuffer);
            }
        }

        DeferCreate(descriptorTable);
    }

    Assert(descriptorTable.IsValid());

    GraphicsPipelineCacheHandle cacheHandle = g_renderGlobalState->graphicsPipelineCache->GetOrCreate(
        m_shader,
        descriptorTable,
        { &view->GetOutputTarget().GetFramebuffer(m_renderableAttributes.GetMaterialAttributes().bucket), 1 },
        m_renderableAttributes);

    clock.Stop();
    HYP_LOG(Rendering, Debug, "Created graphics pipeline ({} ms)", clock.ElapsedMs());

    return cacheHandle;
}

template <class T, class OutArray, typename = std::enable_if_t<std::is_base_of_v<DrawCallBase, T>>>
static void DivideDrawCalls(Span<const T> drawCalls, uint32 numBatches, OutArray& outDividedDrawCalls)
{
    HYP_SCOPE;

    outDividedDrawCalls.Clear();

    const uint32 numDrawCalls = uint32(drawCalls.Size());

    // Make sure we don't try to divide into more batches than we have draw calls
    numBatches = MathUtil::Min(numBatches, numDrawCalls);
    outDividedDrawCalls.Resize(numBatches);

    const uint32 numDrawCallsDivided = (numDrawCalls + numBatches - 1) / numBatches;

    uint32 drawCallIndex = 0;

    for (uint32 containerIndex = 0; containerIndex < numBatches; containerIndex++)
    {
        const uint32 diffToNextOrEnd = MathUtil::Min(numDrawCallsDivided, numDrawCalls - drawCallIndex);

        outDividedDrawCalls[containerIndex] = {
            drawCalls.Begin() + drawCallIndex,
            drawCalls.Begin() + drawCallIndex + diffToNextOrEnd
        };

        // sanity check
        Assert(drawCalls.Size() >= drawCallIndex + diffToNextOrEnd);

        drawCallIndex += diffToNextOrEnd;
    }
}

static void ValidatePipelineState(const RenderSetup& renderSetup, const GraphicsPipelineRef& pipeline)
{
#if 0
    HYP_SCOPE;

    Assert(renderSetup.IsValid());
    Assert(pipeline.IsValid());

    Assert(renderSetup.passData != nullptr);

    const Handle<View> view = renderSetup.passData->view.Lock();
    Assert(view.IsValid());

    const ViewOutputTarget& outputTarget = view->GetOutputTarget();
    Assert(outputTarget.IsValid());

    // Pipeline state validation: Does the pipeline framebuffer match the output target?
    const Array<FramebufferRef>& pipelineFramebuffers = pipeline->GetFramebuffers();

    for (uint32 i = 0; i < pipelineFramebuffers.Size(); ++i)
    {
        AssertDebug(pipelineFramebuffers[i] == outputTarget.GetFramebuffers()[i],
            "Pipeline framebuffer at index {} does not match output target framebuffer at index {}",
            i, i);
    }
#endif
}

template <bool UseIndirectRendering>
static void RenderAll(
    FrameBase* frame,
    const RenderSetup& renderSetup,
    const GraphicsPipelineRef& pipeline,
    IndirectRenderer* indirectRenderer,
    const DrawCallCollection& drawCallCollection)
{
    HYP_SCOPE;

    if constexpr (UseIndirectRendering)
    {
        AssertDebug(indirectRenderer != nullptr);
    }

    static const bool useBindlessTextures = g_renderBackend->GetRenderConfig().bindlessTextures;

    if (drawCallCollection.instancedDrawCalls.Empty() && drawCallCollection.drawCalls.Empty())
    {
        // No draw calls to render
        return;
    }

    ValidatePipelineState(renderSetup, pipeline);

    const uint32 frameIndex = frame->GetFrameIndex();

    const uint32 globalDescriptorSetIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex("Global");
    const DescriptorSetRef& globalDescriptorSet = pipeline->GetDescriptorTable()->GetDescriptorSet("Global", frameIndex);

    const uint32 viewDescriptorSetIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex("View");

    const uint32 materialDescriptorSetIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex("Material");
    const DescriptorSetRef& materialDescriptorSet = useBindlessTextures
        ? pipeline->GetDescriptorTable()->GetDescriptorSet("Material", frameIndex)
        : DescriptorSetRef::empty;

    const uint32 entityDescriptorSetIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex("Object");
    const DescriptorSetRef& entityDescriptorSet = pipeline->GetDescriptorTable()->GetDescriptorSet("Object", frameIndex);

    const uint32 instancingDescriptorSetIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex("Instancing");
    const DescriptorSetRef& instancingDescriptorSet = pipeline->GetDescriptorTable()->GetDescriptorSet("Instancing", frameIndex);

    RenderStatsCounts counts;

    frame->renderQueue << BindGraphicsPipeline(pipeline);

    if (globalDescriptorSetIndex != ~0u)
    {
        frame->renderQueue << BindDescriptorSet(
            globalDescriptorSet,
            pipeline,
            { { "CamerasBuffer", ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) },
                { "EnvGridsBuffer", ShaderDataOffset<EnvGridShaderData>(renderSetup.envGrid, 0) },
                { "CurrentLight", ShaderDataOffset<LightShaderData>(renderSetup.light, 0) },
                { "CurrentEnvProbe", ShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe, 0) } },
            globalDescriptorSetIndex);
    }

    if (viewDescriptorSetIndex != ~0u)
    {
        Assert(renderSetup.passData != nullptr);

        frame->renderQueue << BindDescriptorSet(
            renderSetup.passData->descriptorSets[frameIndex],
            pipeline,
            {},
            viewDescriptorSetIndex);
    }

    // Bind textures globally (bindless)
    if (materialDescriptorSetIndex != ~0u && useBindlessTextures)
    {
        frame->renderQueue << BindDescriptorSet(
            materialDescriptorSet,
            pipeline,
            {},
            materialDescriptorSetIndex);
    }

    const DrawCallBase* prevDrawCall = nullptr;

    for (const DrawCall& drawCall : drawCallCollection.drawCalls)
    {
        if (entityDescriptorSet.IsValid())
        {
            AssertDebug(drawCall.entityId.GetTypeId() == TypeId::ForType<Entity>());

            DescriptorSetOffsetMap offsets({
                { "SkeletonsBuffer", ShaderDataOffset<SkeletonShaderData>(drawCall.skeleton, 0) },
                { "CurrentObject", ShaderDataOffset<EntityShaderData>(drawCall.entityId.ToIndex()) }
            });

            if (g_renderBackend->GetRenderConfig().uniqueDrawCallPerMaterial)
            {
                offsets.Add("MaterialsBuffer", ShaderDataOffset<MaterialShaderData>(drawCall.material, 0));
            }

            frame->renderQueue << BindDescriptorSet(entityDescriptorSet, pipeline, offsets, entityDescriptorSetIndex);
        }

        // Bind material descriptor set
        if (materialDescriptorSetIndex != ~0u && !useBindlessTextures)
        {
            const DescriptorSetRef& materialDescriptorSet = g_renderGlobalState->materialDescriptorSetManager->ForBoundMaterial(drawCall.material, frame->GetFrameIndex());

            frame->renderQueue << BindDescriptorSet(materialDescriptorSet, pipeline, {}, materialDescriptorSetIndex);
        }

        if (!prevDrawCall || prevDrawCall->mesh != drawCall.mesh)
        {
            frame->renderQueue << BindVertexBuffer(drawCall.mesh->GetVertexBuffer());
            frame->renderQueue << BindIndexBuffer(drawCall.mesh->GetIndexBuffer());
        }

        if (UseIndirectRendering && drawCall.drawCommandIndex != ~0u)
        {
            frame->renderQueue << DrawIndexedIndirect(
                indirectRenderer->GetDrawState().GetIndirectBuffer(frameIndex),
                drawCall.drawCommandIndex * uint32(sizeof(IndirectDrawCommand)));
        }
        else
        {
            frame->renderQueue << DrawIndexed(drawCall.mesh->NumIndices(), 1);
        }

        prevDrawCall = &drawCall;

        counts[ERS_DRAW_CALLS]++;
        counts[ERS_TRIANGLES] += drawCall.mesh->NumIndices() / 3;
    }

    for (const InstancedDrawCall& drawCall : drawCallCollection.instancedDrawCalls)
    {
        EntityInstanceBatch* entityInstanceBatch = drawCall.batch;
        AssertDebug(entityInstanceBatch != nullptr);

        Assert(instancingDescriptorSet.IsValid());

        if (entityDescriptorSet.IsValid())
        {
            DescriptorSetOffsetMap offsets({
                { "SkeletonsBuffer", ShaderDataOffset<SkeletonShaderData>(drawCall.skeleton, 0) }
            });

            if (g_renderBackend->GetRenderConfig().uniqueDrawCallPerMaterial)
            {
                offsets.Add("MaterialsBuffer", ShaderDataOffset<MaterialShaderData>(drawCall.material, 0));
            }

            frame->renderQueue << BindDescriptorSet(
                entityDescriptorSet,
                pipeline,
                offsets,
                entityDescriptorSetIndex);
        }

        // Bind material descriptor set
        if (materialDescriptorSetIndex != ~0u && !useBindlessTextures)
        {
            const DescriptorSetRef& materialDescriptorSet = g_renderGlobalState->materialDescriptorSetManager->ForBoundMaterial(drawCall.material, frameIndex);

            frame->renderQueue << BindDescriptorSet(
                materialDescriptorSet,
                pipeline,
                {},
                materialDescriptorSetIndex);
        }

        const SizeType offset = entityInstanceBatch->batchIndex * drawCallCollection.impl->GetStructSize();

        frame->renderQueue << BindDescriptorSet(
            instancingDescriptorSet,
            pipeline,
            { { "EntityInstanceBatchesBuffer", uint32(offset) } },
            instancingDescriptorSetIndex);

        if (!prevDrawCall || prevDrawCall->mesh != drawCall.mesh)
        {
            frame->renderQueue << BindVertexBuffer(drawCall.mesh->GetVertexBuffer());
            frame->renderQueue << BindIndexBuffer(drawCall.mesh->GetIndexBuffer());
        }

        if (UseIndirectRendering && drawCall.drawCommandIndex != ~0u)
        {
            frame->renderQueue << DrawIndexedIndirect(
                indirectRenderer->GetDrawState().GetIndirectBuffer(frameIndex),
                drawCall.drawCommandIndex * uint32(sizeof(IndirectDrawCommand)));
        }
        else
        {
            frame->renderQueue << DrawIndexed(drawCall.mesh->NumIndices(), entityInstanceBatch->numEntities);
        }

        prevDrawCall = &drawCall;

        counts[ERS_DRAW_CALLS]++;
        counts[ERS_INSTANCED_DRAW_CALLS]++;
        counts[ERS_TRIANGLES] += drawCall.mesh->NumIndices() / 3;
    }

    RenderApi_AddRenderStats(counts);
}

template <bool UseIndirectRendering>
static void RenderAll_Parallel(
    FrameBase* frame,
    const RenderSetup& renderSetup,
    const GraphicsPipelineRef& pipeline,
    IndirectRenderer* indirectRenderer,
    const DrawCallCollection& drawCallCollection,
    ParallelRenderingState* parallelRenderingState)
{
    HYP_SCOPE;

    if constexpr (UseIndirectRendering)
    {
        AssertDebug(indirectRenderer != nullptr);
    }

    AssertDebug(parallelRenderingState != nullptr);

    static const bool useBindlessTextures = g_renderBackend->GetRenderConfig().bindlessTextures;

    if (drawCallCollection.instancedDrawCalls.Empty() && drawCallCollection.drawCalls.Empty())
    {
        // No draw calls to render
        return;
    }

    ValidatePipelineState(renderSetup, pipeline);

    const uint32 frameIndex = frame->GetFrameIndex();

    const uint32 globalDescriptorSetIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex("Global");
    const DescriptorSetRef& globalDescriptorSet = pipeline->GetDescriptorTable()->GetDescriptorSet("Global", frameIndex);

    const uint32 viewDescriptorSetIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex("View");
    const uint32 materialDescriptorSetIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex("Material");

    RenderQueue& rootQueue = parallelRenderingState->rootQueue;

    rootQueue << BindGraphicsPipeline(pipeline);

    if (globalDescriptorSetIndex != ~0u)
    {
        rootQueue << BindDescriptorSet(
            globalDescriptorSet,
            pipeline,
            { { "CamerasBuffer", ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) },
                { "EnvGridsBuffer", ShaderDataOffset<EnvGridShaderData>(renderSetup.envGrid, 0) },
                { "CurrentLight", ShaderDataOffset<LightShaderData>(renderSetup.light, 0) },
                { "CurrentEnvProbe", ShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe, 0) } },
            globalDescriptorSetIndex);
    }

    if (viewDescriptorSetIndex != ~0u)
    {
        Assert(renderSetup.passData != nullptr);

        rootQueue << BindDescriptorSet(
            renderSetup.passData->descriptorSets[frameIndex],
            pipeline,
            {},
            viewDescriptorSetIndex);
    }

    // Bind textures globally (bindless)
    if (materialDescriptorSetIndex != ~0u && useBindlessTextures)
    {
        const DescriptorSetRef& materialDescriptorSet = pipeline->GetDescriptorTable()->GetDescriptorSet("Material", frameIndex);
        AssertDebug(materialDescriptorSet.IsValid());

        rootQueue << BindDescriptorSet(
            materialDescriptorSet,
            pipeline,
            {},
            materialDescriptorSetIndex);
    }

    // Store the proc in the parallel rendering state so that it doesn't get destroyed until we're done with it
    if (drawCallCollection.drawCalls.Any())
    {
        DivideDrawCalls(drawCallCollection.drawCalls.ToSpan(), parallelRenderingState->numBatches, parallelRenderingState->drawCalls);

        ProcRef<void(Span<const DrawCall>, uint32, uint32)> proc = parallelRenderingState->drawCallProcs.EmplaceBack([frameIndex, parallelRenderingState, &pipeline, indirectRenderer, materialDescriptorSetIndex](Span<const DrawCall> drawCalls, uint32 index, uint32)
            {
                if (!drawCalls)
                {
                    return;
                }

                RenderQueue& renderQueue = parallelRenderingState->localQueues[index];

                const uint32 entityDescriptorSetIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex("Object");
                const DescriptorSetRef& entityDescriptorSet = pipeline->GetDescriptorTable()->GetDescriptorSet("Object", frameIndex);

                const DrawCallBase* prevDrawCall = nullptr;

                for (const DrawCall& drawCall : drawCalls)
                {
                    if (entityDescriptorSet.IsValid())
                    {
                        AssertDebug(drawCall.entityId.GetTypeId() == TypeId::ForType<Entity>());

                        DescriptorSetOffsetMap offsets({
                            { "SkeletonsBuffer", ShaderDataOffset<SkeletonShaderData>(drawCall.skeleton, 0) },
                            { "CurrentObject", ShaderDataOffset<EntityShaderData>(drawCall.entityId.ToIndex()) }
                        });

                        if (g_renderBackend->GetRenderConfig().uniqueDrawCallPerMaterial)
                        {
                            offsets.Add("MaterialsBuffer", ShaderDataOffset<MaterialShaderData>(drawCall.material, 0));
                        }

                        renderQueue << BindDescriptorSet(entityDescriptorSet, pipeline, offsets, entityDescriptorSetIndex);
                    }

                    // Bind material descriptor set
                    if (materialDescriptorSetIndex != ~0u && !useBindlessTextures)
                    {
                        const DescriptorSetRef& materialDescriptorSet = g_renderGlobalState->materialDescriptorSetManager->ForBoundMaterial(drawCall.material, frameIndex);

                        renderQueue << BindDescriptorSet(materialDescriptorSet, pipeline, {}, materialDescriptorSetIndex);
                    }

                    if (!prevDrawCall || prevDrawCall->mesh != drawCall.mesh)
                    {
                        renderQueue << BindVertexBuffer(drawCall.mesh->GetVertexBuffer());
                        renderQueue << BindIndexBuffer(drawCall.mesh->GetIndexBuffer());
                    }

                    if (UseIndirectRendering && drawCall.drawCommandIndex != ~0u)
                    {
                        renderQueue << DrawIndexedIndirect(
                            indirectRenderer->GetDrawState().GetIndirectBuffer(frameIndex),
                            drawCall.drawCommandIndex * uint32(sizeof(IndirectDrawCommand)));
                    }
                    else
                    {
                        renderQueue << DrawIndexed(drawCall.mesh->NumIndices(), 1);
                    }

                    parallelRenderingState->renderStatsCounts[index][ERS_TRIANGLES] += drawCall.mesh->NumIndices() / 3;
                    parallelRenderingState->renderStatsCounts[index][ERS_DRAW_CALLS]++;

                    prevDrawCall = &drawCall;
                }
            });

        TaskSystem::GetInstance().ParallelForEach_Batch(
            *parallelRenderingState->taskBatch,
            parallelRenderingState->numBatches,
            parallelRenderingState->drawCalls,
            std::move(proc));
    }

    if (drawCallCollection.instancedDrawCalls.Any())
    {
        DivideDrawCalls(drawCallCollection.instancedDrawCalls.ToSpan(), parallelRenderingState->numBatches, parallelRenderingState->instancedDrawCalls);

        ProcRef<void(Span<const InstancedDrawCall>, uint32, uint32)> proc = parallelRenderingState->instancedDrawCallProcs.EmplaceBack([frameIndex, parallelRenderingState, &drawCallCollection, &pipeline, indirectRenderer, materialDescriptorSetIndex](Span<const InstancedDrawCall> drawCalls, uint32 index, uint32)
            {
                if (!drawCalls)
                {
                    return;
                }

                RenderQueue& renderQueue = parallelRenderingState->localQueues[index];

                const uint32 entityDescriptorSetIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex("Object");
                const DescriptorSetRef& entityDescriptorSet = pipeline->GetDescriptorTable()->GetDescriptorSet("Object", frameIndex);

                const uint32 instancingDescriptorSetIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex("Instancing");
                const DescriptorSetRef& instancingDescriptorSet = pipeline->GetDescriptorTable()->GetDescriptorSet("Instancing", frameIndex);

                const DrawCallBase* prevDrawCall = nullptr;

                AssertDebug(instancingDescriptorSet.IsValid());

                for (const InstancedDrawCall& drawCall : drawCalls)
                {
                    EntityInstanceBatch* entityInstanceBatch = drawCall.batch;
                    AssertDebug(entityInstanceBatch != nullptr);

                    if (entityDescriptorSet.IsValid())
                    {
                        DescriptorSetOffsetMap offsets({
                            { "SkeletonsBuffer", ShaderDataOffset<SkeletonShaderData>(drawCall.skeleton, 0) }
                        });

                        if (g_renderBackend->GetRenderConfig().uniqueDrawCallPerMaterial)
                        {
                            offsets.Add("MaterialsBuffer", ShaderDataOffset<MaterialShaderData>(drawCall.material, 0));
                        }

                        renderQueue << BindDescriptorSet(
                            entityDescriptorSet,
                            pipeline,
                            offsets,
                            entityDescriptorSetIndex);
                    }

                    // Bind material descriptor set
                    if (materialDescriptorSetIndex != ~0u && !useBindlessTextures)
                    {
                        const DescriptorSetRef& materialDescriptorSet = g_renderGlobalState->materialDescriptorSetManager->ForBoundMaterial(drawCall.material, frameIndex);

                        renderQueue << BindDescriptorSet(
                            materialDescriptorSet,
                            pipeline,
                            {},
                            materialDescriptorSetIndex);
                    }

                    const SizeType offset = entityInstanceBatch->batchIndex * drawCallCollection.impl->GetStructSize();

                    renderQueue << BindDescriptorSet(
                        instancingDescriptorSet,
                        pipeline,
                        { { "EntityInstanceBatchesBuffer", uint32(offset) } },
                        instancingDescriptorSetIndex);

                    if (!prevDrawCall || prevDrawCall->mesh != drawCall.mesh)
                    {
                        renderQueue << BindVertexBuffer(drawCall.mesh->GetVertexBuffer());
                        renderQueue << BindIndexBuffer(drawCall.mesh->GetIndexBuffer());
                    }

                    if (UseIndirectRendering && drawCall.drawCommandIndex != ~0u)
                    {
                        renderQueue << DrawIndexedIndirect(
                            indirectRenderer->GetDrawState().GetIndirectBuffer(frameIndex),
                            drawCall.drawCommandIndex * uint32(sizeof(IndirectDrawCommand)));
                    }
                    else
                    {
                        renderQueue << DrawIndexed(drawCall.mesh->NumIndices(), entityInstanceBatch->numEntities);
                    }

                    prevDrawCall = &drawCall;

                    parallelRenderingState->renderStatsCounts[index][ERS_TRIANGLES] += drawCall.mesh->NumIndices() / 3;
                    parallelRenderingState->renderStatsCounts[index][ERS_DRAW_CALLS]++;
                    parallelRenderingState->renderStatsCounts[index][ERS_INSTANCED_DRAW_CALLS]++;
                }
            });

        TaskSystem::GetInstance().ParallelForEach_Batch(
            *parallelRenderingState->taskBatch,
            parallelRenderingState->numBatches,
            parallelRenderingState->instancedDrawCalls,
            std::move(proc));
    }
}

void RenderGroup::PerformRendering(
    FrameBase* frame,
    const RenderSetup& renderSetup,
    const DrawCallCollection& drawCallCollection,
    IndirectRenderer* indirectRenderer,
    ParallelRenderingState* parallelRenderingState)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);
    AssertReady();

    AssertDebug(renderSetup.IsValid(), "RenderSetup must be valid for rendering");
    AssertDebug(renderSetup.HasView(), "RenderSetup must have a valid View for rendering");
    AssertDebug(renderSetup.passData != nullptr, "RenderSetup must have valid PassData for rendering!");
    
    static const bool isIndirectRenderingEnabled = g_renderBackend->GetRenderConfig().indirectRendering;

    const bool useIndirectRendering = isIndirectRenderingEnabled
        && m_flags[RenderGroupFlags::INDIRECT_RENDERING]
        && (renderSetup.passData && renderSetup.passData->cullData.depthPyramidImageView);
    
    if (drawCallCollection.drawCalls.Empty() && drawCallCollection.instancedDrawCalls.Empty())
    {
        // No draw calls to render; skip pipeline / cache fetch
        return;
    }

    auto* cacheEntry = renderSetup.passData->renderGroupCache.TryGet(Id().ToIndex());

    if (!cacheEntry)
    {
        cacheEntry = &*renderSetup.passData->renderGroupCache.Emplace(Id().ToIndex());

        *cacheEntry = PassData::RenderGroupCacheEntry {
            WeakHandleFromThis(),
            CreateGraphicsPipeline(renderSetup.passData, drawCallCollection.impl)
        };
    }

    if (!cacheEntry->cacheHandle.IsAlive())
    {
        // fetch a new graphics pipeline if it is dead
        cacheEntry->cacheHandle = CreateGraphicsPipeline(renderSetup.passData, drawCallCollection.impl);
    }

    if (useIndirectRendering)
    {
        if (m_flags & RenderGroupFlags::PARALLEL_RENDERING)
        {
            RenderAll_Parallel<true>(
                frame,
                renderSetup,
                *cacheEntry->cacheHandle,
                indirectRenderer,
                drawCallCollection,
                parallelRenderingState);
        }
        else
        {
            RenderAll<true>(
                frame,
                renderSetup,
                *cacheEntry->cacheHandle,
                indirectRenderer,
                drawCallCollection);
        }
    }
    else
    {
        if (m_flags & RenderGroupFlags::PARALLEL_RENDERING)
        {
            AssertDebug(parallelRenderingState != nullptr);

            RenderAll_Parallel<false>(
                frame,
                renderSetup,
                *cacheEntry->cacheHandle,
                indirectRenderer,
                drawCallCollection,
                parallelRenderingState);
        }
        else
        {
            RenderAll<false>(
                frame,
                renderSetup,
                *cacheEntry->cacheHandle,
                indirectRenderer,
                drawCallCollection);
        }
    }

#if defined(HYP_ENABLE_RENDER_STATS) && defined(HYP_ENABLE_RENDER_STATS_COUNTERS)
    RenderStatsCounts counts;
    counts[ERS_RENDER_GROUPS] = 1;

    RenderApi_AddRenderStats(counts);
#endif
}

#pragma endregion RenderGroup

} // namespace hyperion

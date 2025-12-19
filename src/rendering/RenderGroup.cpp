/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/RenderGroup.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/IndirectDraw.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/GraphicsPipeline.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/RenderBackend.hpp>

#include <rendering/renderers/DeferredRenderer.hpp>
#include <rendering/renderers/EnvGridRenderer.hpp>
#include <rendering/renderers/EnvProbeRenderer.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <scene/Entity.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/Light.hpp>
#include <scene/View.hpp>

#include <scene/animation/Skeleton.hpp>

#include <core/utilities/ForEach.hpp>

#include <core/profiling/PerformanceClock.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/EngineStats.hpp>

#ifdef HYP_VULKAN
#include <rendering/vulkan/VulkanStructs.hpp>

#endif

#include <RenderGroup.generated.inl>

namespace hyperion {

// #define HYP_MATERIAL_DEBUG 1
// #define HYP_GRAPHICS_PIPELINE_TIMING_DEBUG 1

extern EngineStatCounter<uint32> g_statDrawCalls;
extern EngineStatCounter<uint32> g_statInstancedDrawCalls;
extern EngineStatCounter<uint32> g_statTriangles;
extern EngineStatCounter<uint32> g_statRenderGroups;

#pragma region RenderGroup

RenderGroup::RenderGroup()
    : ObjectBase(),
      m_flags(RenderGroupFlags::NONE)
{
}

RenderGroup::RenderGroup(const ShaderRef& shader, const RenderableAttributeSet& renderableAttributes, EnumFlags<RenderGroupFlags> flags)
    : ObjectBase(),
      m_flags(flags),
      m_shader(shader),
      m_renderableAttributes(renderableAttributes)
{
}

RenderGroup::RenderGroup(const ShaderRef& shader, const RenderableAttributeSet& renderableAttributes, const DescriptorTableRef& descriptorTable, EnumFlags<RenderGroupFlags> flags)
    : ObjectBase(),
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

    ObjectBase::Init();

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

GraphicsPipelineCacheHandle RenderGroup::CreateGraphicsPipeline(
    PassData* pd,
    EntityBatchAllocatorBase* batchAllocator) const
{
    HYP_SCOPE;

    Assert(pd != nullptr);
    Assert(batchAllocator != nullptr);

#if HYP_GRAPHICS_PIPELINE_TIMING_DEBUG
    PerformanceClock clock;
    clock.Start();
#endif

    Handle<View> view = pd->view.Lock();
    Assert(view.IsValid());
    Assert(view->GetOutputTarget().IsValid());

    Assert(m_shader.IsValid());

    GraphicsPipelineCacheHandle cacheHandle = g_renderInterface->graphicsPipelineCache->GetOrCreate(
        m_shader,
        { &view->GetOutputTarget().GetFramebuffer(m_renderableAttributes.GetMaterialAttributes().bucket), 1 },
        m_renderableAttributes);

#if HYP_GRAPHICS_PIPELINE_TIMING_DEBUG
    clock.Stop();

    HYP_LOG(Rendering, Debug, "Created graphics pipeline ({} ms)", clock.ElapsedMs());
#endif

    return cacheHandle;
}

template <class OutArray>
static void DivideDrawCalls(SizeType numDrawCalls, uint32 numBatches, OutArray& outDividedDrawCalls)
{
    HYP_SCOPE;

    outDividedDrawCalls.Clear();

    if (numDrawCalls == 0)
    {
        return;
    }

    // Make sure we don't try to divide into more batches than we have draw calls
    numBatches = MathUtil::Min(numBatches, uint32(numDrawCalls));
    outDividedDrawCalls.Resize(numBatches);

    const uint32 numDrawCallsDivided = (uint32(numDrawCalls) + numBatches - 1) / numBatches;

    SizeType drawCallIndex = 0;

    for (uint32 containerIndex = 0; containerIndex < numBatches; containerIndex++)
    {
        const SizeType diffToNextOrEnd = MathUtil::Min(SizeType(numDrawCallsDivided), numDrawCalls - drawCallIndex);

        outDividedDrawCalls[containerIndex] = DrawCallRange {
            drawCallIndex,
            diffToNextOrEnd
        };

        // sanity check
        Assert(numDrawCalls >= drawCallIndex + diffToNextOrEnd);

        drawCallIndex += diffToNextOrEnd;
    }
}

static void ValidatePipelineState(const RenderSetup& renderSetup, const GraphicsPipelineRef& pipeline)
{
#if 0
    HYP_SCOPE;

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
    Frame* frame,
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

    const uint32 globalDescriptorSetIndex = pipeline->GetDescriptorSetIndex("Global"_sh);
    const uint32 viewDescriptorSetIndex = pipeline->GetDescriptorSetIndex("View"_sh);
    const uint32 materialDescriptorSetIndex = pipeline->GetDescriptorSetIndex("Material"_sh);
    const uint32 entityDescriptorSetIndex = pipeline->GetDescriptorSetIndex("Entity"_sh);
    const uint32 instancingDescriptorSetIndex = pipeline->GetDescriptorSetIndex("Instancing"_sh);

    const DescriptorSetRef& globalDescriptorSet = g_renderInterface->globalDescriptorTable->GetDescriptorSet("Global"_sh, frameIndex);
    const DescriptorSetRef& materialDescriptorSet = useBindlessTextures ? g_renderInterface->globalDescriptorTable->GetDescriptorSet("Material"_sh, frameIndex) : DescriptorSetRef::Null();
    const DescriptorSetRef& entityDescriptorSet = g_renderInterface->globalDescriptorTable->GetDescriptorSet("Entity"_sh, frameIndex);
    const DescriptorSetRef& instancingDescriptorSet = drawCallCollection.instancingDescriptorSets[frameIndex];

    RenderGroup* renderGroup = drawCallCollection.renderGroup;
    const RenderableAttributeSet& renderableAttributes = renderGroup->GetRenderableAttributes();

    const MeshAttributes& meshAttributes = renderableAttributes.GetMeshAttributes();
    const MaterialAttributes& materialAttributes = renderableAttributes.GetMaterialAttributes();

    if (materialAttributes.stencilReference != 0)
    {
        frame->renderQueue << SetStencilState(materialAttributes.stencilReference, 0xFF, 0xFF);
    }

    frame->renderQueue << BindGraphicsPipeline(pipeline, renderSetup.view->GetViewport());

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

    Mesh* prevMesh = nullptr;

    const DrawCallStorage& drawCalls = drawCallCollection.drawCalls;
    for (SizeType i = 0; i < drawCalls.Size(); i++)
    {
        if (entityDescriptorSet.IsValid())
        {
            AssertDebug(drawCalls.entityIds[i].GetTypeId() == TypeId::ForType<Entity>());

            DescriptorSetOffsetMap offsets = {
                { "SkeletonsBuffer", ShaderDataOffset<SkeletonShaderData>(drawCalls.skeletons[i], 0) },
                { "CurrentEntity", ShaderDataOffset<EntityShaderData>(drawCalls.entityIds[i].ToIndex()) }
            };

            if (g_renderBackend->GetRenderConfig().uniqueDrawCallPerMaterial)
            {
                offsets.Add("MaterialsBuffer", ShaderDataOffset<MaterialShaderData>(drawCalls.materials[i], 0));
            }

            frame->renderQueue << BindDescriptorSet(entityDescriptorSet, pipeline, offsets, entityDescriptorSetIndex);
        }

        // Bind material descriptor set
        if (materialDescriptorSetIndex != ~0u && !useBindlessTextures)
        {
            const DescriptorSetRef& materialDescriptorSet = g_renderInterface->materialDescriptorSetManager->ForBoundMaterial(drawCalls.materials[i], frame->GetFrameIndex());

            frame->renderQueue << BindDescriptorSet(materialDescriptorSet, pipeline, {}, materialDescriptorSetIndex);
        }

        if (!prevMesh || prevMesh != drawCalls.meshes[i])
        {
            frame->renderQueue << BindVertexBuffer(drawCalls.meshes[i]->GetVertexBuffer());
            frame->renderQueue << BindIndexBuffer(drawCalls.meshes[i]->GetIndexBuffer());

#if HYP_MATERIAL_DEBUG
            AssertDebug(drawCalls.materials[i] != nullptr && drawCalls.materials[i]->IsReady());
            if (!drawCalls.materials[i]->GetTexture(MaterialTextureKey::ALBEDO_MAP))
            {
                HYP_LOG(Rendering, Warning, "Rendering instanced draw call with material '{}' that has no albedo map bound!", drawCalls.materials[i]->GetName());
            }
#endif
        }

        if (UseIndirectRendering && drawCalls.drawCommandIndices[i] != ~0u)
        {
            frame->renderQueue << DrawIndexedIndirect(
                indirectRenderer->GetDrawState().GetIndirectBuffer(frameIndex),
                drawCalls.drawCommandIndices[i] * uint32(sizeof(IndirectDrawCommand)));
        }
        else
        {
            frame->renderQueue << DrawIndexed(drawCalls.numIndices[i], 1);
        }

        prevMesh = drawCalls.meshes[i];

        g_statDrawCalls++;
        g_statTriangles += drawCalls.numIndices[i] / 3;
    }

    const InstancedDrawCallStorage& instancedDrawCalls = drawCallCollection.instancedDrawCalls;

    if (instancedDrawCalls.Any())
    {
        AssertDebug(instancingDescriptorSet.IsValid(),
            "RenderGroup for shader '{}' is missing instancing descriptor set required for instanced draw calls!",
            renderGroup->GetShader()->GetCompiledShader()->GetName());
    }

    for (SizeType i = 0; i < instancedDrawCalls.Size(); i++)
    {
        EntityInstanceBatch* entityInstanceBatch = instancedDrawCalls.batches[i];
        AssertDebug(entityInstanceBatch != nullptr);

        if (entityDescriptorSet.IsValid())
        {
            DescriptorSetOffsetMap offsets({ { "SkeletonsBuffer", ShaderDataOffset<SkeletonShaderData>(instancedDrawCalls.skeletons[i], 0) } });

            if (g_renderBackend->GetRenderConfig().uniqueDrawCallPerMaterial)
            {
                offsets.Add("MaterialsBuffer", ShaderDataOffset<MaterialShaderData>(instancedDrawCalls.materials[i], 0));
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
            const DescriptorSetRef& materialDescriptorSet = g_renderInterface->materialDescriptorSetManager->ForBoundMaterial(instancedDrawCalls.materials[i], frameIndex);

            frame->renderQueue << BindDescriptorSet(
                materialDescriptorSet,
                pipeline,
                {},
                materialDescriptorSetIndex);
        }

        const SizeType offset = entityInstanceBatch->batchIndex * drawCallCollection.batchAllocator->GetStructSize();

        frame->renderQueue << BindDescriptorSet(
            instancingDescriptorSet,
            pipeline,
            { { "EntityInstanceBatchesBuffer", uint32(offset) } },
            instancingDescriptorSetIndex);

        if (!prevMesh || prevMesh != instancedDrawCalls.meshes[i])
        {
            frame->renderQueue << BindVertexBuffer(instancedDrawCalls.meshes[i]->GetVertexBuffer());
            frame->renderQueue << BindIndexBuffer(instancedDrawCalls.meshes[i]->GetIndexBuffer());

#if HYP_MATERIAL_DEBUG
            AssertDebug(instancedDrawCalls.materials[i] != nullptr && instancedDrawCalls.materials[i]->IsReady());
            if (!instancedDrawCalls.materials[i]->GetTexture(MaterialTextureKey::ALBEDO_MAP))
            {
                HYP_LOG(Rendering, Warning, "Rendering instanced draw call with material '{}' that has no albedo map bound!", instancedDrawCalls.materials[i]->GetName());
            }
#endif
        }

        if (UseIndirectRendering && instancedDrawCalls.drawCommandIndices[i] != ~0u)
        {
            frame->renderQueue << DrawIndexedIndirect(
                indirectRenderer->GetDrawState().GetIndirectBuffer(frameIndex),
                instancedDrawCalls.drawCommandIndices[i] * uint32(sizeof(IndirectDrawCommand)));
        }
        else
        {
            frame->renderQueue << DrawIndexed(instancedDrawCalls.numIndices[i], entityInstanceBatch->numEntities);
        }

        prevMesh = instancedDrawCalls.meshes[i];

        g_statDrawCalls++;
        g_statInstancedDrawCalls++;
        g_statTriangles += instancedDrawCalls.numIndices[i] / 3;
    }
}

template <bool UseIndirectRendering>
static void RenderAll_Parallel(
    Frame* frame,
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

    const uint32 globalDescriptorSetIndex = pipeline->GetDescriptorSetIndex("Global"_sh);
    const uint32 viewDescriptorSetIndex = pipeline->GetDescriptorSetIndex("View"_sh);
    const uint32 materialDescriptorSetIndex = pipeline->GetDescriptorSetIndex("Material"_sh);

    const DescriptorSetRef& globalDescriptorSet = g_renderInterface->globalDescriptorTable->GetDescriptorSet("Global"_sh, frameIndex);

    RenderQueue& rootQueue = parallelRenderingState->rootQueue;

    RenderGroup* renderGroup = drawCallCollection.renderGroup;
    const RenderableAttributeSet& renderableAttributes = renderGroup->GetRenderableAttributes();

    const MeshAttributes& meshAttributes = renderableAttributes.GetMeshAttributes();
    const MaterialAttributes& materialAttributes = renderableAttributes.GetMaterialAttributes();

    if (materialAttributes.stencilReference != 0)
    {
        rootQueue << SetStencilState(materialAttributes.stencilReference, 0xFF, 0xFF);
    }

    rootQueue << BindGraphicsPipeline(pipeline, renderSetup.view->GetViewport());

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
        const DescriptorSetRef& materialDescriptorSet = g_renderInterface->globalDescriptorTable->GetDescriptorSet("Material"_sh, frameIndex);
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
        DivideDrawCalls(drawCallCollection.drawCalls.Size(), parallelRenderingState->numBatches, parallelRenderingState->drawCalls);

        ProcRef<void(DrawCallRange, uint32, uint32)> proc = parallelRenderingState->drawCallProcs.EmplaceBack([frameIndex, parallelRenderingState, &drawCallCollection, &pipeline, indirectRenderer, materialDescriptorSetIndex](DrawCallRange range, uint32 index, uint32 batchIndex)
            {
                if (range.count == 0)
                {
                    return;
                }

                auto& renderQueue = *parallelRenderingState->localQueues[batchIndex];

                const uint32 entityDescriptorSetIndex = pipeline->GetDescriptorSetIndex("Entity");
                const DescriptorSetRef& entityDescriptorSet = g_renderInterface->globalDescriptorTable->GetDescriptorSet("Entity"_sh, frameIndex);

                const DrawCallStorage& drawCalls = drawCallCollection.drawCalls;

                Mesh* prevMesh = nullptr;

                for (SizeType i = range.start; i < range.start + range.count; i++)
                {
                    if (entityDescriptorSet.IsValid())
                    {
                        AssertDebug(drawCalls.entityIds[i].GetTypeId() == TypeId::ForType<Entity>());

                        DescriptorSetOffsetMap offsets = {
                            { "SkeletonsBuffer", ShaderDataOffset<SkeletonShaderData>(drawCalls.skeletons[i], 0) },
                            { "CurrentEntity", ShaderDataOffset<EntityShaderData>(drawCalls.entityIds[i].ToIndex()) }
                        };

                        if (g_renderBackend->GetRenderConfig().uniqueDrawCallPerMaterial)
                        {
                            offsets.Add("MaterialsBuffer", ShaderDataOffset<MaterialShaderData>(drawCalls.materials[i], 0));
                        }

                        renderQueue << BindDescriptorSet(entityDescriptorSet, pipeline, offsets, entityDescriptorSetIndex);
                    }

                    // Bind material descriptor set
                    if (materialDescriptorSetIndex != ~0u && !useBindlessTextures)
                    {
                        const DescriptorSetRef& materialDescriptorSet = g_renderInterface->materialDescriptorSetManager->ForBoundMaterial(drawCalls.materials[i], frameIndex);

                        renderQueue << BindDescriptorSet(materialDescriptorSet, pipeline, {}, materialDescriptorSetIndex);
                    }

                    if (!prevMesh || prevMesh != drawCalls.meshes[i])
                    {
                        renderQueue << BindVertexBuffer(drawCalls.meshes[i]->GetVertexBuffer());
                        renderQueue << BindIndexBuffer(drawCalls.meshes[i]->GetIndexBuffer());

#if HYP_MATERIAL_DEBUG
                        AssertDebug(drawCalls.materials[i] != nullptr && drawCalls.materials[i]->IsReady());
                        if (!drawCalls.materials[i]->GetTexture(MaterialTextureKey::ALBEDO_MAP))
                        {
                            HYP_LOG(Rendering, Warning, "Rendering instanced draw call with material '{}' that has no albedo map bound!", drawCalls.materials[i]->GetName());
                        }
#endif
                    }

                    if (UseIndirectRendering && drawCalls.drawCommandIndices[i] != ~0u)
                    {
                        renderQueue << DrawIndexedIndirect(
                            indirectRenderer->GetDrawState().GetIndirectBuffer(frameIndex),
                            drawCalls.drawCommandIndices[i] * uint32(sizeof(IndirectDrawCommand)));
                    }
                    else
                    {
                        renderQueue << DrawIndexed(drawCalls.numIndices[i], 1);
                    }

                    parallelRenderingState->statValues[index][g_statTriangles] += drawCalls.numIndices[i] / 3;
                    parallelRenderingState->statValues[index][g_statDrawCalls]++;

                    prevMesh = drawCalls.meshes[i];
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
        DivideDrawCalls(drawCallCollection.instancedDrawCalls.Size(), parallelRenderingState->numBatches, parallelRenderingState->instancedDrawCalls);

        ProcRef<void(DrawCallRange, uint32, uint32)> proc = parallelRenderingState->instancedDrawCallProcs.EmplaceBack([frameIndex, parallelRenderingState, &drawCallCollection, &pipeline, indirectRenderer, materialDescriptorSetIndex](DrawCallRange range, uint32 index, uint32 batchIndex)
            {
                if (range.count == 0)
                {
                    return;
                }

                auto& renderQueue = *parallelRenderingState->localQueues[batchIndex];

                const uint32 entityDescriptorSetIndex = pipeline->GetDescriptorSetIndex("Entity"_sh);
                const uint32 instancingDescriptorSetIndex = pipeline->GetDescriptorSetIndex("Instancing"_sh);

                const DescriptorSetRef& entityDescriptorSet = g_renderInterface->globalDescriptorTable->GetDescriptorSet("Entity"_sh, frameIndex);

                const DescriptorSetRef& instancingDescriptorSet = drawCallCollection.instancingDescriptorSets[batchIndex];
                AssertDebug(instancingDescriptorSet.IsValid());

                const InstancedDrawCallStorage& instancedDrawCalls = drawCallCollection.instancedDrawCalls;

                Mesh* prevMesh = nullptr;

                for (SizeType i = range.start; i < range.start + range.count; i++)
                {
                    EntityInstanceBatch* entityInstanceBatch = instancedDrawCalls.batches[i];
                    AssertDebug(entityInstanceBatch != nullptr);

                    if (entityDescriptorSet.IsValid())
                    {
                        DescriptorSetOffsetMap offsets({ { "SkeletonsBuffer", ShaderDataOffset<SkeletonShaderData>(instancedDrawCalls.skeletons[i], 0) } });

                        if (g_renderBackend->GetRenderConfig().uniqueDrawCallPerMaterial)
                        {
                            offsets.Add("MaterialsBuffer"_sh, ShaderDataOffset<MaterialShaderData>(instancedDrawCalls.materials[i], 0));
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
                        const DescriptorSetRef& materialDescriptorSet = g_renderInterface->materialDescriptorSetManager->ForBoundMaterial(instancedDrawCalls.materials[i], frameIndex);

                        renderQueue << BindDescriptorSet(
                            materialDescriptorSet,
                            pipeline,
                            {},
                            materialDescriptorSetIndex);
                    }

                    const SizeType offset = entityInstanceBatch->batchIndex * drawCallCollection.batchAllocator->GetStructSize();

                    renderQueue << BindDescriptorSet(
                        instancingDescriptorSet,
                        pipeline,
                        { { "EntityInstanceBatchesBuffer", uint32(offset) } },
                        instancingDescriptorSetIndex);

                    if (!prevMesh || prevMesh != instancedDrawCalls.meshes[i])
                    {
                        renderQueue << BindVertexBuffer(instancedDrawCalls.meshes[i]->GetVertexBuffer());
                        renderQueue << BindIndexBuffer(instancedDrawCalls.meshes[i]->GetIndexBuffer());

#if HYP_MATERIAL_DEBUG
                        AssertDebug(instancedDrawCalls.materials[i] != nullptr && instancedDrawCalls.materials[i]->IsReady());
                        if (!instancedDrawCalls.materials[i]->GetTexture(MaterialTextureKey::ALBEDO_MAP))
                        {
                            HYP_LOG(Rendering, Warning, "Rendering instanced draw call with material '{}' that has no albedo map bound!", instancedDrawCalls.materials[i]->GetName());
                        }
#endif
                    }

                    if (UseIndirectRendering && instancedDrawCalls.drawCommandIndices[i] != ~0u)
                    {
                        renderQueue << DrawIndexedIndirect(
                            indirectRenderer->GetDrawState().GetIndirectBuffer(frameIndex),
                            instancedDrawCalls.drawCommandIndices[i] * uint32(sizeof(IndirectDrawCommand)));
                    }
                    else
                    {
                        renderQueue << DrawIndexed(instancedDrawCalls.numIndices[i], entityInstanceBatch->numEntities);
                    }

                    prevMesh = instancedDrawCalls.meshes[i];

                    parallelRenderingState->statValues[index][g_statTriangles] += instancedDrawCalls.numIndices[i] / 3;
                    parallelRenderingState->statValues[index][g_statDrawCalls]++;
                    parallelRenderingState->statValues[index][g_statInstancedDrawCalls]++;
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
    Frame* frame,
    const RenderSetup& renderSetup,
    DrawCallCollection& drawCallCollection,
    IndirectRenderer* indirectRenderer,
    ParallelRenderingState* parallelRenderingState)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);
    AssertReady();

    AssertDebug(renderSetup.world && renderSetup.view);
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
    bool isNewlyCreated = false;

    if (!cacheEntry)
    {
        cacheEntry = &*renderSetup.passData->renderGroupCache.Emplace(Id().ToIndex());

        *cacheEntry = PassData::RenderGroupCacheEntry {
            WeakHandleFromThis(),
            CreateGraphicsPipeline(renderSetup.passData, drawCallCollection.batchAllocator)
        };

        isNewlyCreated = true;
    }

    if (!cacheEntry->cacheHandle.IsAlive())
    {
        // fetch a new graphics pipeline if it is dead
        cacheEntry->cacheHandle = CreateGraphicsPipeline(renderSetup.passData, drawCallCollection.batchAllocator);

        isNewlyCreated = true;
    }

    // Setup instancing descriptor set if "Instancing" descriptor set exists in the shader.
    if (drawCallCollection.instancedDrawCalls.Any() && !drawCallCollection.instancingDescriptorSets[frame->GetFrameIndex()])
    {
        const DescriptorTableDeclaration* descriptorTableDecl = m_shader->GetCompiledShader()->GetDescriptorTableDeclaration();
        Assert(descriptorTableDecl != nullptr);

        const DescriptorSetDeclaration* instancingDescriptorSetDecl = descriptorTableDecl->FindDescriptorSetDeclaration("Instancing"_sh);
        Assert(instancingDescriptorSetDecl != nullptr);

        const GpuBufferRef& gpuBuffer = drawCallCollection.batchAllocator->GetGpuBufferHolder()->GetBuffer(frame->GetFrameIndex());
        Assert(gpuBuffer.IsValid());

        DescriptorSetRef& descriptorSet = drawCallCollection.instancingDescriptorSets[frame->GetFrameIndex()];
        descriptorSet = g_renderBackend->MakeDescriptorSet(DescriptorSetLayout(instancingDescriptorSetDecl));
        descriptorSet->SetElement("EntityInstanceBatchesBuffer"_sh, gpuBuffer);
        Assert(descriptorSet->Create());
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

    g_statRenderGroups++;
}

#pragma endregion RenderGroup

} // namespace hyperion

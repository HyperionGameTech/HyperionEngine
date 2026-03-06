/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/RenderGroup.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/MaterialTextureCache.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/IndirectDraw.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/TextureViewCache.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/Texture.hpp>
#include <rendering/GraphicsPipeline.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/PlaceholderData.hpp>

#include <rendering/shadows/ShadowMapAllocator.hpp>

#include <rendering/renderers/DeferredRenderer.hpp>
#include <rendering/renderers/EnvProbeRenderer.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <scene/Entity.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/Light.hpp>
#include <scene/View.hpp>

#include <scene/animation/Skeleton.hpp>

#include <Core/utilities/ForEach.hpp>

#include <Core/profiling/PerformanceClock.hpp>

#include <Core/threading/ThreadLocalStorage.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/EngineStats.hpp>

// For IndirectDrawCommand
#if HYP_VULKAN
#include <rendering/vulkan/VulkanStructs.hpp>
#elif HYP_DX12
#include <rendering/dx12/DX12Structs.hpp>
#endif

namespace Hyperion {

// #define HYP_MATERIAL_DEBUG 1
// #define HYP_GRAPHICS_PIPELINE_TIMING_DEBUG 1

extern EngineStatCounter<uint32> g_statDrawCalls;
extern EngineStatCounter<uint32> g_statInstancedDrawCalls;
extern EngineStatCounter<uint32> g_statTriangles;
extern EngineStatCounter<uint32> g_statRenderGroups;

#pragma region RenderGroup

template <class OutArray>
static void DivideDrawCalls(size_t numDrawCalls, uint32 numBatches, OutArray& outDividedDrawCalls)
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

    size_t drawCallIndex = 0;

    for (uint32 containerIndex = 0; containerIndex < numBatches; containerIndex++)
    {
        const size_t diffToNextOrEnd = MathUtil::Min(size_t(numDrawCallsDivided), numDrawCalls - drawCallIndex);

        outDividedDrawCalls[containerIndex] = DrawCallRange {
            drawCallIndex,
            diffToNextOrEnd
        };

        // sanity check
        Assert(numDrawCalls >= drawCallIndex + diffToNextOrEnd);

        drawCallIndex += diffToNextOrEnd;
    }
}

template <bool UseIndirectRendering>
static void RenderAll(
    Frame* frame,
    const RenderSetup& renderSetup,
    IndirectRenderer* indirectRenderer,
    const DrawCallCollection& drawCallCollection)
{
    HYP_SCOPE;

    if constexpr (UseIndirectRendering)
    {
        AssertDebug(indirectRenderer != nullptr);
    }

    static const bool s_useBindlessTextures = g_renderInterface->GetRenderConfig().bindlessTextures;

    if (drawCallCollection.instancedDrawCalls.Empty() && drawCallCollection.drawCalls.Empty())
    {
        // No draw calls to render
        return;
    }

    const uint32 frameIndex = frame->GetFrameIndex();

    const RenderGroup& renderGroup = drawCallCollection.renderGroup;
    const RenderableAttributeSet& renderableAttributes = renderGroup.renderableAttributes;

    const MeshAttributes& meshAttributes = renderableAttributes.GetMeshAttributes();
    const MaterialAttributes& materialAttributes = renderableAttributes.GetMaterialAttributes();

    CommandRecorder& cr = frame->cr;

    uint32 numShaderUniforms = 0;
    
    cr << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinearMipmap());
    cr << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());

    cr << SetShaderUniform(numShaderUniforms++, "CamerasBuffer"_sh, g_renderInterface->gpuBuffers[GRB_CAMERAS]->GetBuffer(frameIndex), TShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()));
    
    cr << SetShaderUniform(numShaderUniforms++, "EntitiesBuffer"_sh, g_renderInterface->gpuBuffers[GRB_ENTITIES]->GetBuffer(frameIndex));

    cr << SetShaderUniform(numShaderUniforms++, "WorldsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_WORLDS]->GetBuffer(frameIndex));
    
    cr << SetShaderUniform(numShaderUniforms++, "ShadowMapsTextureArray"_sh, g_renderInterface->shadowMapAllocator->GetAtlasImageView()); 
    cr << SetShaderUniform(numShaderUniforms++, "PointLightShadowMapsTextureArray"_sh, g_renderInterface->shadowMapAllocator->GetPointLightShadowMapImageView());
    
    cr << SetShaderUniform(numShaderUniforms++, "EnvProbesTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(g_renderInterface->envProbesTexture));
    cr << SetShaderUniform(numShaderUniforms++, "EnvProbesBuffer"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frameIndex));
    
    if (renderSetup.light != nullptr)
        cr << SetShaderUniform(numShaderUniforms++, "CurrentLight"_sh, g_renderInterface->gpuBuffers[GRB_LIGHTS]->GetBuffer(frameIndex), TShaderDataOffset<LightShaderData>(renderSetup.light));
    else
        cr << SetShaderUniform(numShaderUniforms++, "CurrentLight"_sh, g_renderInterface->gpuBuffers[GRB_LIGHTS]->GetBuffer(frameIndex), TShaderDataOffset<LightShaderData>(0));
    
    if (renderSetup.envProbe != nullptr)
        cr << SetShaderUniform(numShaderUniforms++, "CurrentEnvProbe"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frameIndex), TShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe));
    else
        cr << SetShaderUniform(numShaderUniforms++, "CurrentEnvProbe"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frameIndex), TShaderDataOffset<EnvProbeShaderData>(0));

    DeferredRendererPassData* dpd = ObjCast<DeferredRendererPassData>(renderSetup.passData);
    if (dpd != nullptr)
    {
        cr << SetShaderUniform(numShaderUniforms++, "GBufferMipChain"_sh, g_renderInterface->textureViewCache->GetOrCreate(dpd->mipChain));
    }

    Mesh* prevMesh = nullptr;

    const DrawCallStorage& drawCalls = drawCallCollection.drawCalls;
    for (size_t i = 0; i < drawCalls.Size(); i++)
    {
        AssertDebug(drawCalls.entityIds[i].GetTypeId() == TypeId::ForType<Entity>());

        const uint32 materialBoundIndex = RetrieveResourceBinding(drawCalls.materials[i]);
        AssertDebug(materialBoundIndex != ~0u);

        uint32 numDrawCallUniforms = numShaderUniforms;

        cr << SetShaderUniform(numDrawCallUniforms++, "CurrentEntity"_sh,
            g_renderInterface->gpuBuffers[GRB_ENTITIES]->GetBuffer(frameIndex),
            TShaderDataOffset<EntityShaderData>(drawCalls.entityIds[i].ToIndex()));

        cr << SetShaderUniform(numDrawCallUniforms++, "MaterialsBuffer"_sh,
            g_renderInterface->gpuBuffers[GRB_MATERIALS]->GetBuffer(frameIndex),
            TShaderDataOffset<MaterialShaderData>(materialBoundIndex));
                        
        if (drawCalls.skeletons[i] != nullptr)
        {
            cr << SetShaderUniform(numDrawCallUniforms++, "SkeletonsBuffer"_sh,
                g_renderInterface->gpuBuffers[GRB_SKELETONS]->GetBuffer(frameIndex),
                TShaderDataOffset<SkeletonShaderData>(drawCalls.skeletons[i]));
        }
        
        if (!s_useBindlessTextures)
        {
            const uint32 textureMask = drawCallCollection.renderGroup.renderableAttributes.GetMaterialAttributes().textureMask;

            if (textureMask != 0)
            {
                RenderProxyMaterial* materialProxy = static_cast<RenderProxyMaterial*>(GetRenderProxy(drawCalls.materials[i]));
                AssertDebug(materialProxy != nullptr);

                Span<const GpuImageViewRef> imageViews = g_renderInterface->materialTextureCache->imageViews.Get(materialBoundIndex);
                AssertDebug(imageViews.Size() >= materialProxy->boundTextures.Size());

                FOR_EACH_BIT(textureMask, bit)
                {
                    const Name textureUniformName = Material::s_textureNames[bit];

                    cr << SetShaderUniform(numDrawCallUniforms++,
                        textureUniformName,
                        imageViews[materialProxy->boundTextureIndices[bit]]);
                }
            }
        }
        
        cr << CommitDrawState();

        if (!prevMesh || prevMesh != drawCalls.meshes[i])
        {
            cr << BindVertexBuffer(drawCalls.meshes[i]->GetVertexBuffer());
            cr << BindIndexBuffer(drawCalls.meshes[i]->GetIndexBuffer());

#if HYP_MATERIAL_DEBUG
            AssertDebug(drawCalls.materials[i] != nullptr && drawCalls.materials[i]->IsReady());
            if (!drawCalls.materials[i]->GetTexture(MaterialTextureKey::Diffuse))
            {
                HYP_LOG(Rendering, Warning, "Rendering instanced draw call with material '{}' that has no albedo map bound!", drawCalls.materials[i]->GetName());
            }
#endif
        }

        if (UseIndirectRendering && drawCalls.drawCommandIndices[i] != ~0u)
        {
            cr << DrawIndexedIndirect(
                indirectRenderer->GetDrawState().GetIndirectBuffer(frameIndex),
                drawCalls.drawCommandIndices[i] * uint32(sizeof(IndirectDrawCommand)));
        }
        else
        {
            cr << DrawIndexed(drawCalls.numIndices[i], 1);
        }

        prevMesh = drawCalls.meshes[i];

        g_statDrawCalls++;
        g_statTriangles += drawCalls.numIndices[i] / 3;
    }

    const InstancedDrawCallStorage& instancedDrawCalls = drawCallCollection.instancedDrawCalls;

    for (size_t i = 0; i < instancedDrawCalls.Size(); i++)
    {
        uint32 numDrawCallUniforms = numShaderUniforms;

        EntityInstanceBatch* entityInstanceBatch = instancedDrawCalls.batches[i];
        AssertDebug(entityInstanceBatch != nullptr);
        
        const uint32 stride = drawCallCollection.batchAllocator->GetStructSize();

        cr << SetShaderUniform(numDrawCallUniforms++, "EntityInstanceBatchesBuffer"_sh,
            drawCallCollection.batchAllocator->GetGpuBufferHolder()->GetBuffer(frameIndex),
            ShaderDataOffset(entityInstanceBatch->batchIndex * stride, stride));

        const uint32 materialBoundIndex = RetrieveResourceBinding(instancedDrawCalls.materials[i]);
        AssertDebug(materialBoundIndex != ~0u);

        cr << SetShaderUniform(numDrawCallUniforms++, "MaterialsBuffer"_sh,
            g_renderInterface->gpuBuffers[GRB_MATERIALS]->GetBuffer(frameIndex),
            TShaderDataOffset<MaterialShaderData>(materialBoundIndex));
                        
        if (instancedDrawCalls.skeletons[i] != nullptr)
        {
            cr << SetShaderUniform(numDrawCallUniforms++, "SkeletonsBuffer"_sh,
                g_renderInterface->gpuBuffers[GRB_SKELETONS]->GetBuffer(frameIndex),
                TShaderDataOffset<SkeletonShaderData>(instancedDrawCalls.skeletons[i]));
        }
        
        if (!s_useBindlessTextures)
        {
            const uint32 textureMask = drawCallCollection.renderGroup.renderableAttributes.GetMaterialAttributes().textureMask;

            if (textureMask != 0)
            {
                RenderProxyMaterial* materialProxy = static_cast<RenderProxyMaterial*>(GetRenderProxy(instancedDrawCalls.materials[i]));
                AssertDebug(materialProxy != nullptr);

                Span<const GpuImageViewRef> imageViews = g_renderInterface->materialTextureCache->imageViews.Get(materialBoundIndex);
                AssertDebug(imageViews.Size() >= materialProxy->boundTextures.Size());

                FOR_EACH_BIT(textureMask, bit)
                {
                    const Name textureUniformName = Material::s_textureNames[bit];

                    cr << SetShaderUniform(numDrawCallUniforms++,
                        textureUniformName,
                        imageViews[materialProxy->boundTextureIndices[bit]]);
                }
            }
        }
        
        cr << CommitDrawState();

        if (!prevMesh || prevMesh != instancedDrawCalls.meshes[i])
        {
            cr << BindVertexBuffer(instancedDrawCalls.meshes[i]->GetVertexBuffer());
            cr << BindIndexBuffer(instancedDrawCalls.meshes[i]->GetIndexBuffer());

#if HYP_MATERIAL_DEBUG
            AssertDebug(instancedDrawCalls.materials[i] != nullptr && instancedDrawCalls.materials[i]->IsReady());
            if (!instancedDrawCalls.materials[i]->GetTexture(MaterialTextureKey::Diffuse))
            {
                HYP_LOG(Rendering, Warning, "Rendering instanced draw call with material '{}' that has no albedo map bound!", instancedDrawCalls.materials[i]->GetName());
            }
#endif
        }

        if (UseIndirectRendering && instancedDrawCalls.drawCommandIndices[i] != ~0u)
        {
            cr << DrawIndexedIndirect(
                indirectRenderer->GetDrawState().GetIndirectBuffer(frameIndex),
                instancedDrawCalls.drawCommandIndices[i] * uint32(sizeof(IndirectDrawCommand)));
        }
        else
        {
            cr << DrawIndexed(instancedDrawCalls.numIndices[i], entityInstanceBatch->numEntities);
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

    static const bool s_useBindlessTextures = g_renderInterface->GetRenderConfig().bindlessTextures;

    if (drawCallCollection.instancedDrawCalls.Empty() && drawCallCollection.drawCalls.Empty())
    {
        // No draw calls to render
        return;
    }

    const uint32 frameIndex = frame->GetFrameIndex();

    const RenderGroup& renderGroup = drawCallCollection.renderGroup;
    const RenderableAttributeSet& renderableAttributes = renderGroup.renderableAttributes;

    const MeshAttributes& meshAttributes = renderableAttributes.GetMeshAttributes();
    const MaterialAttributes& materialAttributes = renderableAttributes.GetMaterialAttributes();

    CommandRecorder& cr = parallelRenderingState->cr;
    
    uint32 numShaderUniforms = 0;
    
    cr << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinearMipmap());
    cr << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());

    cr << SetShaderUniform(numShaderUniforms++, "CamerasBuffer"_sh, g_renderInterface->gpuBuffers[GRB_CAMERAS]->GetBuffer(frameIndex), TShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()));

    cr << SetShaderUniform(numShaderUniforms++, "EntitiesBuffer"_sh, g_renderInterface->gpuBuffers[GRB_ENTITIES]->GetBuffer(frameIndex));

    cr << SetShaderUniform(numShaderUniforms++, "WorldsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_WORLDS]->GetBuffer(frameIndex));

    cr << SetShaderUniform(numShaderUniforms++, "ShadowMapsTextureArray"_sh, g_renderInterface->shadowMapAllocator->GetAtlasImageView()); 
    cr << SetShaderUniform(numShaderUniforms++, "PointLightShadowMapsTextureArray"_sh, g_renderInterface->shadowMapAllocator->GetPointLightShadowMapImageView());

    cr << SetShaderUniform(numShaderUniforms++, "EnvProbesTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(g_renderInterface->envProbesTexture));
    cr << SetShaderUniform(numShaderUniforms++, "EnvProbesBuffer"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frameIndex));

    if (renderSetup.light != nullptr)
        cr << SetShaderUniform(numShaderUniforms++, "CurrentLight"_sh, g_renderInterface->gpuBuffers[GRB_LIGHTS]->GetBuffer(frameIndex), TShaderDataOffset<LightShaderData>(renderSetup.light));
    else
        cr << SetShaderUniform(numShaderUniforms++, "CurrentLight"_sh, g_renderInterface->gpuBuffers[GRB_LIGHTS]->GetBuffer(frameIndex), TShaderDataOffset<LightShaderData>(0));
    
    if (renderSetup.envProbe != nullptr)
        cr << SetShaderUniform(numShaderUniforms++, "CurrentEnvProbe"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frameIndex), TShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe));
    else
        cr << SetShaderUniform(numShaderUniforms++, "CurrentEnvProbe"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frameIndex), TShaderDataOffset<EnvProbeShaderData>(0));

    DeferredRendererPassData* dpd = ObjCast<DeferredRendererPassData>(renderSetup.passData);
    if (dpd != nullptr)
    {
        cr << SetShaderUniform(numShaderUniforms++, "GBufferMipChain"_sh, g_renderInterface->textureViewCache->GetOrCreate(dpd->mipChain));
    }

    // Store the proc in the parallel rendering state so that it doesn't get destroyed until we're done with it
    if (drawCallCollection.drawCalls.Any())
    {
        DivideDrawCalls(drawCallCollection.drawCalls.Size(), parallelRenderingState->numBatches, parallelRenderingState->drawCalls);

        ProcRef<void(DrawCallRange, uint32, uint32)> proc = parallelRenderingState->drawCallProcs.EmplaceBack([frameIndex, numShaderUniforms, parallelRenderingState, &drawCallCollection, indirectRenderer](DrawCallRange range, uint32 index, uint32 batchIndex)
            {
                if (range.count == 0)
                {
                    return;
                }

                auto& cr = *parallelRenderingState->threadLocalRecorders[GetCurrentThreadIndex()];

                const DrawCallStorage& drawCalls = drawCallCollection.drawCalls;

                Mesh* prevMesh = nullptr;

                for (size_t i = range.start; i < range.start + range.count; i++)
                {
                    AssertDebug(drawCalls.entityIds[i].GetTypeId() == TypeId::ForType<Entity>());

                    uint32 numDrawCallUniforms = numShaderUniforms;

                    const uint32 materialBoundIndex = RetrieveResourceBinding(drawCalls.materials[i]);
                    AssertDebug(materialBoundIndex != ~0u);

                    cr << SetShaderUniform(numDrawCallUniforms++, "CurrentEntity"_sh,
                        g_renderInterface->gpuBuffers[GRB_ENTITIES]->GetBuffer(frameIndex),
                        TShaderDataOffset<EntityShaderData>(drawCalls.entityIds[i].ToIndex()));

                    cr << SetShaderUniform(numDrawCallUniforms++, "MaterialsBuffer"_sh,
                        g_renderInterface->gpuBuffers[GRB_MATERIALS]->GetBuffer(frameIndex),
                        TShaderDataOffset<MaterialShaderData>(materialBoundIndex));
                        
                    if (drawCalls.skeletons[i] != nullptr)
                    {
                        cr << SetShaderUniform(numDrawCallUniforms++, "SkeletonsBuffer"_sh,
                            g_renderInterface->gpuBuffers[GRB_SKELETONS]->GetBuffer(frameIndex),
                            TShaderDataOffset<SkeletonShaderData>(drawCalls.skeletons[i]));
                    }

                    if (!s_useBindlessTextures)
                    {
                        const uint32 textureMask = drawCallCollection.renderGroup.renderableAttributes.GetMaterialAttributes().textureMask;

                        if (textureMask != 0)
                        {
                            RenderProxyMaterial* materialProxy = static_cast<RenderProxyMaterial*>(GetRenderProxy(drawCalls.materials[i]));
                            AssertDebug(materialProxy != nullptr);

                            Span<const GpuImageViewRef> imageViews = g_renderInterface->materialTextureCache->imageViews.Get(materialBoundIndex);
                            AssertDebug(imageViews.Size() >= materialProxy->boundTextures.Size());

                            FOR_EACH_BIT(textureMask, bit)
                            {
                                const Name textureUniformName = Material::s_textureNames[bit];

                                cr << SetShaderUniform(numDrawCallUniforms++,
                                    textureUniformName,
                                    imageViews[materialProxy->boundTextureIndices[bit]]);
                            }
                        }
                    }
        
                    cr << CommitDrawState();

                    if (!prevMesh || prevMesh != drawCalls.meshes[i])
                    {
                        cr << BindVertexBuffer(drawCalls.meshes[i]->GetVertexBuffer());
                        cr << BindIndexBuffer(drawCalls.meshes[i]->GetIndexBuffer());

#if HYP_MATERIAL_DEBUG
                        AssertDebug(drawCalls.materials[i] != nullptr && drawCalls.materials[i]->IsReady());
                        if (!drawCalls.materials[i]->GetTexture(MaterialTextureKey::Diffuse))
                        {
                            HYP_LOG(Rendering, Warning, "Rendering instanced draw call with material '{}' that has no albedo map bound!", drawCalls.materials[i]->GetName());
                        }
#endif
                    }

                    if (UseIndirectRendering && drawCalls.drawCommandIndices[i] != ~0u)
                    {
                        cr << DrawIndexedIndirect(
                            indirectRenderer->GetDrawState().GetIndirectBuffer(frameIndex),
                            drawCalls.drawCommandIndices[i] * uint32(sizeof(IndirectDrawCommand)));
                    }
                    else
                    {
                        cr << DrawIndexed(drawCalls.numIndices[i], 1);
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

        ProcRef<void(DrawCallRange, uint32, uint32)> proc = parallelRenderingState->instancedDrawCallProcs.EmplaceBack([frameIndex, numShaderUniforms, parallelRenderingState, &drawCallCollection, indirectRenderer](DrawCallRange range, uint32 index, uint32 batchIndex)
            {
                if (range.count == 0)
                {
                    return;
                }

                auto& cr = *parallelRenderingState->threadLocalRecorders[GetCurrentThreadIndex()];

                const InstancedDrawCallStorage& instancedDrawCalls = drawCallCollection.instancedDrawCalls;

                Mesh* prevMesh = nullptr;

                for (size_t i = range.start; i < range.start + range.count; i++)
                {
                    uint32 numDrawCallUniforms = numShaderUniforms;

                    EntityInstanceBatch* entityInstanceBatch = instancedDrawCalls.batches[i];
                    AssertDebug(entityInstanceBatch != nullptr);

                    const uint32 stride = drawCallCollection.batchAllocator->GetStructSize();
        
                    cr << SetShaderUniform(numDrawCallUniforms++, "EntityInstanceBatchesBuffer"_sh,
                        drawCallCollection.batchAllocator->GetGpuBufferHolder()->GetBuffer(frameIndex),
                        ShaderDataOffset(entityInstanceBatch->batchIndex * stride, stride));

                    const uint32 materialBoundIndex = RetrieveResourceBinding(instancedDrawCalls.materials[i]);
                    AssertDebug(materialBoundIndex != ~0u);

                    cr << SetShaderUniform(numDrawCallUniforms++, "MaterialsBuffer"_sh,
                        g_renderInterface->gpuBuffers[GRB_MATERIALS]->GetBuffer(frameIndex),
                        TShaderDataOffset<MaterialShaderData>(materialBoundIndex));
                        
                    if (instancedDrawCalls.skeletons[i] != nullptr)
                    {
                        cr << SetShaderUniform(numDrawCallUniforms++, "SkeletonsBuffer"_sh,
                            g_renderInterface->gpuBuffers[GRB_SKELETONS]->GetBuffer(frameIndex),
                            TShaderDataOffset<SkeletonShaderData>(instancedDrawCalls.skeletons[i]));
                    }
                    
                    if (!s_useBindlessTextures)
                    {
                        const uint32 textureMask = drawCallCollection.renderGroup.renderableAttributes.GetMaterialAttributes().textureMask;

                        if (textureMask != 0)
                        {
                            RenderProxyMaterial* materialProxy = static_cast<RenderProxyMaterial*>(GetRenderProxy(instancedDrawCalls.materials[i]));
                            AssertDebug(materialProxy != nullptr);

                            Span<const GpuImageViewRef> imageViews = g_renderInterface->materialTextureCache->imageViews.Get(materialBoundIndex);
                            AssertDebug(imageViews.Size() >= materialProxy->boundTextures.Size());

                            FOR_EACH_BIT(textureMask, bit)
                            {
                                const Name textureUniformName = Material::s_textureNames[bit];

                                cr << SetShaderUniform(numDrawCallUniforms++,
                                    textureUniformName,
                                    imageViews[materialProxy->boundTextureIndices[bit]]);
                            }
                        }
                    }
        
                    cr << CommitDrawState();

                    if (!prevMesh || prevMesh != instancedDrawCalls.meshes[i])
                    {
                        cr << BindVertexBuffer(instancedDrawCalls.meshes[i]->GetVertexBuffer());
                        cr << BindIndexBuffer(instancedDrawCalls.meshes[i]->GetIndexBuffer());

#if HYP_MATERIAL_DEBUG
                        AssertDebug(instancedDrawCalls.materials[i] != nullptr && instancedDrawCalls.materials[i]->IsReady());
                        if (!instancedDrawCalls.materials[i]->GetTexture(MaterialTextureKey::Diffuse))
                        {
                            HYP_LOG(Rendering, Warning, "Rendering instanced draw call with material '{}' that has no albedo map bound!", instancedDrawCalls.materials[i]->GetName());
                        }
#endif
                    }

                    if (UseIndirectRendering && instancedDrawCalls.drawCommandIndices[i] != ~0u)
                    {
                        cr << DrawIndexedIndirect(
                            indirectRenderer->GetDrawState().GetIndirectBuffer(frameIndex),
                            instancedDrawCalls.drawCommandIndices[i] * uint32(sizeof(IndirectDrawCommand)));
                    }
                    else
                    {
                        cr << DrawIndexed(instancedDrawCalls.numIndices[i], entityInstanceBatch->numEntities);
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
    ParallelRenderingState* parallelRenderingState) const
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world && renderSetup.view);
    AssertDebug(renderSetup.passData != nullptr, "RenderSetup must have valid PassData for rendering!");

    Framebuffer* framebuffer = renderSetup.framebuffer;
    
    if (!framebuffer)
    {
        framebuffer = renderSetup.view->GetOutputTarget().GetFramebuffer();
    }

    AssertDebug(framebuffer != nullptr);

    static const bool isIndirectRenderingEnabled = g_renderInterface->GetRenderConfig().indirectRendering;

    const bool useIndirectRendering = isIndirectRenderingEnabled
        && flags[RenderGroupFlags::INDIRECT_RENDERING]
        && (renderSetup.passData && renderSetup.passData->cullData.depthPyramidImageView);

    if (drawCallCollection.drawCalls.Empty() && drawCallCollection.instancedDrawCalls.Empty())
    {
        // No draw calls to render; skip pipeline / cache fetch
        return;
    }
    
    const uint8 stencilReference = renderableAttributes.GetMaterialAttributes().stencilReference;

    CommandRecorder* pRecorder = &frame->cr;

    if (flags & RenderGroupFlags::PARALLEL_RENDERING)
    {
        AssertDebug(parallelRenderingState != nullptr);

        pRecorder = &parallelRenderingState->cr;
    }

    CommandRecorder& cr = *pRecorder;
    
    cr << SetTopology(renderableAttributes.GetMeshAttributes().topology);
    cr << SetVertexAttributes(renderableAttributes.GetMeshAttributes().vertexAttributes);
    
    cr << SetCurrentViewport(renderSetup.viewport);
    
    cr << SetCurrentShader(ShaderDesc(
        renderableAttributes.GetMaterialAttributes().shaderName,
        renderableAttributes.GetMaterialAttributes().shaderProperties));

    cr << SetFillMode(renderableAttributes.GetMaterialAttributes().fillMode);
    cr << SetFaceCullMode(renderableAttributes.GetMaterialAttributes().cullFaces);
    
    cr << SetCurrentBlendFunction(renderableAttributes.GetMaterialAttributes().blendFunction);

    cr << SetDepthTest(bool(renderableAttributes.GetMaterialAttributes().flags & MAF_DEPTH_TEST));
    cr << SetDepthWrite(bool(renderableAttributes.GetMaterialAttributes().flags & MAF_DEPTH_WRITE));
    cr << SetDepthClamp(bool(renderableAttributes.GetMaterialAttributes().flags & MAF_DEPTH_CLAMP));

    if (renderableAttributes.GetMaterialAttributes().flags & MAF_DEPTH_BIAS)
    {
        cr << SetDepthBias(
            renderableAttributes.GetMaterialAttributes().depthBias,
            renderableAttributes.GetMaterialAttributes().depthBiasSlope);
    }

    cr << SetStencilTest(bool(renderableAttributes.GetMaterialAttributes().flags & MAF_STENCIL_TEST));
    cr << SetStencilFunction(renderableAttributes.GetMaterialAttributes().stencilFunction);

    if (stencilReference != 0)
    {
        // apply stencil state before render (write)
        cr << SetStencilState(stencilReference, 0x0, 0xFF);
    }

    if (useIndirectRendering)
    {
        if (flags & RenderGroupFlags::PARALLEL_RENDERING)
        {
            RenderAll_Parallel<true>(
                frame,
                renderSetup,
                indirectRenderer,
                drawCallCollection,
                parallelRenderingState);
        }
        else
        {
            RenderAll<true>(
                frame,
                renderSetup,
                indirectRenderer,
                drawCallCollection);
        }
    }
    else
    {
        if (flags & RenderGroupFlags::PARALLEL_RENDERING)
        {
            RenderAll_Parallel<false>(
                frame,
                renderSetup,
                indirectRenderer,
                drawCallCollection,
                parallelRenderingState);
        }
        else
        {
            RenderAll<false>(
                frame,
                renderSetup,
                indirectRenderer,
                drawCallCollection);
        }
    }

    g_statRenderGroups++;
}

#pragma endregion RenderGroup

} // namespace Hyperion

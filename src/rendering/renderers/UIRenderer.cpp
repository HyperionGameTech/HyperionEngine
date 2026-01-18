/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/RenderGroup.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/FinalPass.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/Frame.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/GraphicsPipeline.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/Mesh.hpp>

#include <rendering/renderers/DeferredRenderer.hpp>
#include <rendering/renderers/UIRenderer.hpp>

#include <scene/View.hpp>

#include <ui/font/FontAtlas.hpp>

#include <ui/UIStage.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/filesystem/FsUtil.hpp>

#include <engine/EngineDriver.hpp>

#include <UIRenderer.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

HYP_API extern const char* LookupTypeName(const TypeId& typeId);

#pragma region UIRenderCollector

static RenderableAttributeSet GetMergedRenderableAttributes(const RenderableAttributeSet& entityAttributes, const Optional<RenderableAttributeSet>& overrideAttributes)
{
    HYP_NAMED_SCOPE("Rebuild UI Proxy Groups: GetMergedRenderableAttributes");

    RenderableAttributeSet attributes = entityAttributes;

    if (overrideAttributes.HasValue())
    {
        if (ShaderCacheId shaderCacheId = overrideAttributes->GetShaderCacheId(); shaderCacheId != InvalidShaderCacheId)
        {
            attributes.SetShaderCacheId(shaderCacheId);
        }

        const ShaderDefinition* shaderDefinition = g_shaderManager->GetShaderDefinition(attributes.GetShaderCacheId());
        AssertDebug(shaderDefinition != nullptr, "Could not find ShaderDefinition for ShaderCacheId value {}", attributes.GetShaderCacheId());

        // Check for varying vertex attributes on the override shader compared to the entity's vertex
        // attributes. If there is not a match, we should switch to a version of the override shader that
        // has matching vertex attribs.
        const VertexAttributeSet meshVertexAttributes = attributes.GetMeshAttributes().vertexAttributes;

        MaterialAttributes newMaterialAttributes = static_cast<MaterialAttributes>(overrideAttributes->GetMaterialAttributes());
        newMaterialAttributes.shaderDefinition = *shaderDefinition;

        if (meshVertexAttributes != newMaterialAttributes.shaderDefinition.GetProperties().GetRequiredVertexAttributes())
        {
            newMaterialAttributes.shaderDefinition.properties.SetRequiredVertexAttributes(meshVertexAttributes);
        }

        // do not override bucket!
        newMaterialAttributes.bucket = attributes.GetMaterialAttributes().bucket;

        attributes.SetMaterialAttributes(RuntimeMaterialAttributes(newMaterialAttributes));
    }

    return attributes;
}

static void BuildRenderGroupsOrdered(RenderCollector& renderCollector, RenderProxyList& rpl, const Array<Pair<ObjId<Entity>, int>>& meshEntityOrdering, const Optional<RenderableAttributeSet>& overrideAttributes)
{
    renderCollector.Clear(/* freeMemory */ false);

    for (const Pair<ObjId<Entity>, int>& pair : meshEntityOrdering)
    {
        RenderProxyMesh* meshProxy = rpl.GetMeshEntities().GetProxy(pair.first);
        AssertDebug(meshProxy != nullptr);

        if (!meshProxy)
        {
            continue;
        }

#ifdef HYP_DEBUG_MODE
        AssertDebug(meshProxy->entity.Id().GetTypeId() == TypeId::ForType<Entity>(),
            "Cannot use Entity subclass as MeshEntity, indices would overlap! Class: {}",
            LookupTypeName(meshProxy->entity.Id().GetTypeId()));
#endif

        Mesh* mesh = meshProxy->mesh;
        Material* material = meshProxy->material;

        if (!mesh || !material)
        {
            continue;
        }

        RenderableAttributeSet attributes = GetMergedRenderableAttributes(RenderableAttributeSet { mesh->GetMeshAttributes(), material->GetRenderAttributes() }, overrideAttributes);

        // \TODO Optimize. may not be needed since we set textureMask when textures on material are changed,
        // So we may already have the mappings separated so that ui objects with/without textures already are binned appropriately.
        // in that case, this logic could be moved below into the `if (!rg.IsValid())` part, minimizing calls in hot path
        if (const Handle<Texture>& albedoTexture = material->GetTexture(MaterialTextureKey::ALBEDO_MAP))
        {
            if (albedoTexture != g_renderInterface->placeholderData->defaultTexture2d)
            {
                ShaderDefinition shaderDefinition;
                if (const ShaderDefinition* currentDefinition = g_shaderManager->GetShaderDefinition(attributes.GetShaderCacheId()))
                {
                    shaderDefinition = *currentDefinition;
                }


                shaderDefinition.GetProperties().Set(NAME("TEXTURED"));

                attributes.SetShaderCacheId(g_shaderManager->GetShaderCacheId(shaderDefinition, /* createIfNotExists */ true));
            }
        }

        const RenderBucket rb = attributes.GetMaterialAttributes().bucket;

        attributes.SetLayerIndex(pair.second);

        DrawCallCollectionMapping& mapping = renderCollector.mappingsByBucket[rb][attributes];
        Handle<RenderGroup>& rg = mapping.renderGroup;

        if (!rg.IsValid())
        {
            ShaderDefinition shaderDefinition;
            if (const ShaderDefinition* currentDefinition = g_shaderManager->GetShaderDefinition(attributes.GetShaderCacheId()))
            {
                shaderDefinition = *currentDefinition;
            }

            ShaderRef shader = g_shaderManager->GetOrCreate(shaderDefinition);
            Assert(shader.IsValid());

            rg = CreateObject<RenderGroup>(shader, attributes, RenderGroupFlags::NONE);

#ifdef HYP_DEBUG_MODE
            if (!rg.IsValid())
            {
                HYP_LOG(UI, Error, "Render group not valid for attribute set {}!", attributes.GetHashCode().Value());

                continue;
            }
#endif

            InitObject(rg);
        }

        mapping.meshProxies.Set(meshProxy->entity.Id().ToIndex(), meshProxy);
    }
}

void UIRenderCollector::ExecuteDrawCalls(Frame* frame, const RenderSetup& renderSetup, const FramebufferRef& framebuffer, uint32 bucketBits)
{
    HYP_SCOPE;

    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.HasWorld() && renderSetup.HasView());

    RenderProxyList& rpl = RenderApi::GetConsumerProxyList(renderSetup.view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    const uint32 frameIndex = frame->GetFrameIndex();

    if (framebuffer.IsValid())
    {
        frame->renderQueue << BeginFramebuffer(framebuffer);
    }

    using IteratorType = FlatMap<RenderableAttributeSet, DrawCallCollectionMapping>::Iterator;
    Array<IteratorType> iterators;

    for (auto& mappings : mappingsByBucket)
    {
        for (auto& it : mappings)
        {
            iterators.PushBack(&it);
        }
    }

    {
        HYP_NAMED_SCOPE("Sort proxy groups by layer");

        std::sort(iterators.Begin(), iterators.End(), [](IteratorType lhs, IteratorType rhs) -> bool
            {
                return lhs->first.GetLayerIndex() < rhs->first.GetLayerIndex();
            });
    }

    for (SizeType index = 0; index < iterators.Size(); index++)
    {
        auto& it = *iterators[index];

        const RenderableAttributeSet& attributes = it.first;

        DrawCallCollectionMapping& mapping = it.second;
        Assert(mapping.IsValid());

        const Handle<RenderGroup>& renderGroup = mapping.renderGroup;
        Assert(renderGroup.IsValid());

        DrawCallCollection& drawCallCollection = mapping.drawCallCollection;

        ParallelRenderingState* parallelRenderingState = nullptr;

        if (renderGroup->GetFlags() & RenderGroupFlags::PARALLEL_RENDERING)
        {
            parallelRenderingState = AcquireNextParallelRenderingState();
        }

        renderGroup->PerformRendering(frame, renderSetup, drawCallCollection, nullptr, parallelRenderingState);

        if (parallelRenderingState != nullptr)
        {
            AssertDebug(parallelRenderingState->taskBatch != nullptr);

            TaskSystem::GetInstance().EnqueueBatch(parallelRenderingState->taskBatch);
        }
    }

    // Wait for all parallel rendering tasks to finish
    CommitParallelRenderingState(frame->renderQueue);

    if (framebuffer.IsValid())
    {
        frame->renderQueue << EndFramebuffer(framebuffer);
    }
}

#pragma endregion UIRenderCollector

#pragma region UIRenderer

UIRenderer::UIRenderer(const Handle<View>& view)
    : m_view(view)
{
    AssertDebug(view.IsValid());
}

void UIRenderer::Initialize()
{
}

void UIRenderer::Shutdown()
{
}

void UIRenderer::RenderFrame(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const Handle<UIRendererPassData>& pd = ObjCast<UIRendererPassData>(FetchViewPassData(m_view));
    AssertDebug(pd != nullptr);

    RenderSetup rs = renderSetup;
    rs.view = m_view.Get();
    rs.passData = pd;

    RenderProxyList& rpl = RenderApi::GetConsumerProxyList(m_view);
    rpl.BeginRead();

    const ViewOutputTarget& outputTarget = m_view->GetOutputTarget();
    Assert(outputTarget.IsValid());

    BuildRenderGroupsOrdered(renderCollector, rpl, rpl.meshEntityOrdering, {});

    rpl.EndRead();

    renderCollector.BuildDrawCalls(0);
    renderCollector.ExecuteDrawCalls(frame, rs, outputTarget.GetFramebuffer(), 0);
}

Handle<PassData> UIRenderer::CreateViewPassData(View* view, PassDataExt&)
{
    Handle<UIRendererPassData> pd = CreateObject<UIRendererPassData>();

    pd->view = MakeWeakRef(view);
    pd->viewport = view->GetViewport();

    HYP_LOG(UI, Debug, "Creating UI pass data with viewport size {}", pd->viewport.extent);

    return pd;
}

#pragma endregion UIRenderer

} // namespace Hyperion

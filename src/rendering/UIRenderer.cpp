/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/UIRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/FinalPass.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/RenderStats.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/Mesh.hpp>

#include <rendering/font/FontAtlas.hpp>

#include <rendering/RenderFrame.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/RenderGraphicsPipeline.hpp>

#include <scene/View.hpp>

#include <ui/UIStage.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/Logger.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/filesystem/FsUtil.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

#pragma region UIRenderCollector

static RenderableAttributeSet GetMergedRenderableAttributes(const RenderableAttributeSet& entityAttributes, const Optional<RenderableAttributeSet>& overrideAttributes)
{
    HYP_NAMED_SCOPE("Rebuild UI Proxy Groups: GetMergedRenderableAttributes");

    RenderableAttributeSet attributes = entityAttributes;

    if (overrideAttributes.HasValue())
    {
        if (const ShaderDefinition& overrideShaderDefinition = overrideAttributes->GetShaderDefinition())
        {
            attributes.SetShaderDefinition(overrideShaderDefinition);
        }

        ShaderDefinition shaderDefinition = overrideAttributes->GetShaderDefinition().IsValid()
            ? overrideAttributes->GetShaderDefinition()
            : attributes.GetShaderDefinition();

#ifdef HYP_DEBUG_MODE
        Assert(shaderDefinition.IsValid());
#endif

        // Check for varying vertex attributes on the override shader compared to the entity's vertex
        // attributes. If there is not a match, we should switch to a version of the override shader that
        // has matching vertex attribs.
        const VertexAttributeSet meshVertexAttributes = attributes.GetMeshAttributes().vertexAttributes;

        if (meshVertexAttributes != shaderDefinition.GetProperties().GetRequiredVertexAttributes())
        {
            shaderDefinition.properties.SetRequiredVertexAttributes(meshVertexAttributes);
        }

        MaterialAttributes newMaterialAttributes = overrideAttributes->GetMaterialAttributes();
        newMaterialAttributes.shaderDefinition = shaderDefinition;
        // do not override bucket!
        newMaterialAttributes.bucket = attributes.GetMaterialAttributes().bucket;

        attributes.SetMaterialAttributes(newMaterialAttributes);
    }

    return attributes;
}

static void BuildRenderGroups(RenderCollector& renderCollector, RenderProxyList& rpl, const Array<Pair<ObjId<Entity>, int>>& proxyDepths, const Optional<RenderableAttributeSet>& overrideAttributes)
{
    renderCollector.Clear();

    for (const Pair<ObjId<Entity>, int>& pair : proxyDepths)
    {
        RenderProxyMesh* meshProxy = rpl.GetMeshEntities().GetProxy(pair.first);
        AssertDebug(meshProxy != nullptr);

        if (!meshProxy)
        {
            continue;
        }

        const Handle<Mesh>& mesh = meshProxy->mesh;
        const Handle<Material>& material = meshProxy->material;

        if (!mesh.IsValid() || !material.IsValid())
        {
            continue;
        }

        RenderableAttributeSet attributes = GetMergedRenderableAttributes(RenderableAttributeSet { mesh->GetMeshAttributes(), material->GetRenderAttributes() }, overrideAttributes);

        if (const Handle<Texture>& albedoTexture = material->GetTexture(MaterialTextureKey::ALBEDO_MAP))
        {
            if (albedoTexture != g_renderGlobalState->placeholderData->defaultTexture2d)
            {
                ShaderDefinition shaderDefinition = attributes.GetShaderDefinition();
                shaderDefinition.GetProperties().Set(NAME("TEXTURED"));
                attributes.SetShaderDefinition(shaderDefinition);
            }
        }

        const RenderBucket rb = attributes.GetMaterialAttributes().bucket;

        attributes.SetDrawableLayer(pair.second);

        DrawCallCollectionMapping& mapping = renderCollector.mappingsByBucket[rb][attributes];
        Handle<RenderGroup>& rg = mapping.renderGroup;

        if (!rg.IsValid())
        {
            ShaderDefinition shaderDefinition = attributes.GetShaderDefinition();
            shaderDefinition.GetProperties().Set(NAME("INSTANCING"));

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

        const int subclassIndex = GetSubclassIndex(TypeId::ForType<Entity>(), meshProxy->entity.Id().GetTypeId());
        Assert(subclassIndex >= 0 && subclassIndex < mapping.meshProxiesBySubtype.Size());

        mapping.meshProxiesBySubtype[subclassIndex].Set(meshProxy->entity.Id().ToIndex(), meshProxy);
    }
}

void UIRenderCollector::ExecuteDrawCalls(FrameBase* frame, const RenderSetup& renderSetup, const FramebufferRef& framebuffer, uint32 bucketBits)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(renderSetup.view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    const uint32 frameIndex = frame->GetFrameIndex();

    if (framebuffer.IsValid())
    {
        frame->renderQueue << BeginFramebuffer(framebuffer);
    }

    using IteratorType = FlatMap<RenderableAttributeSet, DrawCallCollectionMapping>::ConstIterator;
    Array<IteratorType> iterators;

    for (const auto& mappings : mappingsByBucket)
    {
        for (const auto& it : mappings)
        {
            iterators.PushBack(&it);
        }
    }

    {
        HYP_NAMED_SCOPE("Sort proxy groups by layer");

        std::sort(iterators.Begin(), iterators.End(), [](IteratorType lhs, IteratorType rhs) -> bool
            {
                return lhs->first.GetDrawableLayer() < rhs->first.GetDrawableLayer();
            });
    }

    // HYP_LOG(UI, Debug, " UI rendering {} render groups", iterators.Size());

    for (SizeType index = 0; index < iterators.Size(); index++)
    {
        const auto& it = *iterators[index];

        const RenderableAttributeSet& attributes = it.first;
        const DrawCallCollectionMapping& mapping = it.second;
        Assert(mapping.IsValid());

        const Handle<RenderGroup>& renderGroup = mapping.renderGroup;
        Assert(renderGroup.IsValid());

        const DrawCallCollection& drawCallCollection = mapping.drawCallCollection;

        // debugging
        for (const DrawCall& drawCall : drawCallCollection.drawCalls)
        {
            AssertDebug(RenderApi_RetrieveResourceBinding(drawCall.material) != ~0u);
        }
        for (const InstancedDrawCall& drawCall : drawCallCollection.instancedDrawCalls)
        {
            AssertDebug(RenderApi_RetrieveResourceBinding(drawCall.material) != ~0u);
        }

        ParallelRenderingState* parallelRenderingState = nullptr;

        if (renderGroup->GetFlags() & RenderGroupFlags::PARALLEL_RENDERING)
        {
            parallelRenderingState = AcquireNextParallelRenderingState();
        }

        // Don't count draw calls for UI
        SuppressRenderStatsScope suppressRenderStatsScope;

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

void UIRenderer::RenderFrame(FrameBase* frame, const RenderSetup& renderSetup)
{
    const Handle<UIPassData>& pd = ObjCast<UIPassData>(FetchViewPassData(m_view));
    AssertDebug(pd != nullptr);

    RenderSetup rs = renderSetup;
    rs.view = m_view.Get();
    rs.passData = pd;

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(m_view);
    rpl.BeginRead();

    // RenderCollector& renderCollector = RenderApi_GetRenderCollector(m_view);

    if (pd->viewport != m_view->GetViewport())
    {
        /// @TODO: Implement me!

        HYP_LOG(UI, Warning, "UIRenderer: Viewport size changed from {} to {}, resizing view pass data",
            pd->viewport.extent, m_view->GetViewport().extent);
    }

    // Don't include UI rendering in global render stats
    SuppressRenderStatsScope suppressRenderStatsScope;

    const ViewOutputTarget& outputTarget = m_view->GetOutputTarget();
    Assert(outputTarget.IsValid());

    // renderCollector.BuildRenderGroups(m_view, rpl);
    ::hyperion::BuildRenderGroups(renderCollector, rpl, rpl.meshEntityOrdering, {});

    rpl.EndRead();

    renderCollector.BuildDrawCalls(0);
    renderCollector.ExecuteDrawCalls(frame, rs, outputTarget.GetFramebuffer(), 0);
}

Handle<PassData> UIRenderer::CreateViewPassData(View* view, PassDataExt&)
{
    Handle<UIPassData> pd = CreateObject<UIPassData>();

    pd->view = MakeWeakRef(view);
    pd->viewport = view->GetViewport();

    HYP_LOG(UI, Debug, "Creating UI pass data with viewport size {}", pd->viewport.extent);

    return pd;
}

#pragma endregion UIRenderer

} // namespace hyperion

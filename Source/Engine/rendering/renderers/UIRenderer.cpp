/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/RenderGroup.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/FinalPass.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/Frame.hpp>
#include <rendering/Mesh.hpp>

#include <rendering/renderers/DeferredRenderer.hpp>
#include <rendering/renderers/UIRenderer.hpp>

#include <rendering/util/ShaderPropertyDictionary.hpp>

#include <scene/View.hpp>

#include <ui/font/FontAtlas.hpp>

#include <ui/UIStage.hpp>

#include <Core/utilities/DeferredScope.hpp>

#include <Core/filesystem/FsUtil.hpp>

#include <engine/EngineDriver.hpp>

#include <UIRenderer.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

HYP_API extern const char* LookupTypeName(const TypeId& typeId);

#pragma region UIRenderCollector

static const ShaderPropertyId s_propTextured = InternShaderProperty(ShaderProperty(NAME("TEXTURED")));
static const ShaderPropertyId s_propUIText = InternShaderProperty(ShaderProperty(NAME("UI_TEXT")));

static RenderableAttributeSet GetMergedRenderableAttributes(
    const RenderableAttributeSet& inAttributes,
    const Optional<RenderableAttributeSet>& overrideAttributes)
{
    HYP_NAMED_SCOPE("Rebuild UI Proxy Groups: GetMergedRenderableAttributes");

    RenderableAttributeSet attributes = inAttributes;

    if (overrideAttributes.HasValue())
    {
        MaterialAttributes newMaterialAttributes = overrideAttributes->GetMaterialAttributes();
        // do not override bucket!
        newMaterialAttributes.bucket = attributes.GetMaterialAttributes().bucket;

        attributes.SetMaterialAttributes(newMaterialAttributes);

        AssertDebug(attributes.GetShaderName().IsValid());
    }

    return attributes;
}

static void BuildRenderGroupsOrdered(
    RenderCollector& renderCollector,
    RenderProxyList& rpl,
    const Array<Pair<ObjId<Entity>, int>>& meshEntityOrdering,
    const Optional<RenderableAttributeSet>& overrideAttributes)
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

#if HYP_DEBUG_MODE
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

        // @FIXME Thread safe?
        RenderableAttributeSet attributes = GetMergedRenderableAttributes(
            RenderableAttributeSet { mesh->GetMeshAttributes(), material->GetRenderAttributes() },
            overrideAttributes);

        if (const Handle<Texture>& albedoTexture = material->GetTexture(MaterialTextureKey::Diffuse); albedoTexture.IsValid())
        {
            if (albedoTexture != g_renderInterface->placeholderData->defaultTexture2d)
            {
                ShaderPropertySet newProperties = attributes.GetShaderProperties();
                newProperties.Add(s_propTextured);

                attributes.SetShaderProperties(newProperties);
            }
        }

        if (const MaterialParameter& textParam = material->GetParameter(MaterialParameterKey::MATERIAL_KEY_RESERVED2); int(textParam) == 1) // text
        {
            ShaderPropertySet newProperties = attributes.GetShaderProperties();
            newProperties.Add(s_propUIText);

            attributes.SetShaderProperties(newProperties);
        }

        const RenderBucket bucket = attributes.GetMaterialAttributes().bucket;

        attributes.SetLayerIndex(pair.second);

        DrawCallCollection& drawCallCollection = renderCollector.mappingsByBucket[uint32(bucket)][attributes];
        RenderGroup& rg = drawCallCollection.renderGroup;

        if (!rg.valid)
        {
            rg.valid = true;
            rg.renderableAttributes = attributes;
            rg.flags = RenderGroupFlags::NONE;

            drawCallCollection.batchAllocator = renderCollector.batchAllocator;
        }

        drawCallCollection.meshProxies.Set(meshProxy->entity.Id().ToIndex(), meshProxy);
    }
}

void UIRenderCollector::ExecuteDrawCalls(Frame* frame, const RenderSetup& renderSetup, Framebuffer* framebuffer, uint32 bucketBits)
{
    HYP_SCOPE;

    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.HasWorld() && renderSetup.HasView());

    RenderProxyList& rpl = GetConsumerProxyList(renderSetup.view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    const uint32 frameIndex = frame->GetFrameIndex();

    if (framebuffer != nullptr)
    {
        frame->renderQueue << SetCurrentFramebuffer(framebuffer);
    }

    using IteratorType = FlatMap<RenderableAttributeSet, DrawCallCollection>::Iterator;
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

    for (size_t index = 0; index < iterators.Size(); index++)
    {
        auto& it = *iterators[index];

        const RenderableAttributeSet& attributes = it.first;

        DrawCallCollection& drawCallCollection = it.second;
        Assert(drawCallCollection.IsValid());

        RenderGroup& renderGroup = drawCallCollection.renderGroup;
        Assert(renderGroup.valid);

        ParallelRenderingState* parallelRenderingState = nullptr;

        if (renderGroup.flags & RenderGroupFlags::PARALLEL_RENDERING)
        {
            parallelRenderingState = AcquireNextParallelRenderingState();
        }

        renderGroup.PerformRendering(frame, renderSetup, drawCallCollection, nullptr, parallelRenderingState);

        if (parallelRenderingState != nullptr)
        {
            AssertDebug(parallelRenderingState->taskBatch != nullptr);

            TaskSystem::GetInstance().EnqueueBatch(parallelRenderingState->taskBatch);
        }
    }

    // Wait for all parallel rendering tasks to finish
    CommitParallelRenderingState(frame->renderQueue);

    if (framebuffer != nullptr)
    {
        frame->renderQueue << SetCurrentFramebuffer(nullptr);
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

    UIRendererPassData* pd = ObjCast<UIRendererPassData>(FetchViewPassData(m_view));
    AssertDebug(pd != nullptr);

    RenderSetup rs = renderSetup.Fork();
    rs.view = m_view.Get();
    rs.viewport = Viewport { m_view->GetViewDesc().renderTargetDesc.extent };
    rs.passData = pd;
    
    Framebuffer* framebuffer = rs.framebuffer;
    
    if (!framebuffer)
    {
        framebuffer = m_view->GetOutputTarget().GetFramebuffer();
    }

    AssertDebug(framebuffer != nullptr);

    RenderProxyList& rpl = GetConsumerProxyList(m_view);
    rpl.BeginRead();

    BuildRenderGroupsOrdered(renderCollector, rpl, rpl.meshEntityOrdering, {});

    rpl.EndRead();

    renderCollector.BuildDrawCalls(0);
    renderCollector.ExecuteDrawCalls(frame, rs, framebuffer, 0);
}

PassData* UIRenderer::CreateViewPassData(View* view, PassDataExt&)
{
    UIRendererPassData* pd = new UIRendererPassData();

    pd->view = MakeWeakRef(view);

    return pd;
}

#pragma endregion UIRenderer

} // namespace Hyperion

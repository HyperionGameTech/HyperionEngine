/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

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
#include <rendering/MaterialInstance.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderGroupCache.hpp>

#include <rendering/passes/DeferredPass.hpp>
#include <rendering/passes/UIPass.hpp>

#include <rendering/util/ShaderPropertyDictionary.hpp>

#include <scene/View.hpp>

#include <ui/font/FontAtlas.hpp>

#include <ui/UIStage.hpp>

#include <Core/utilities/DeferredScope.hpp>

#include <Core/filesystem/FsUtil.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/CVarManager.hpp>

#include <UIPass.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

HYP_API extern const char* LookupTypeName(const TypeId& typeId);

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
    Span<const Pair<ObjId<Entity>, int>> meshEntityOrdering,
    const Optional<RenderableAttributeSet>& overrideAttributes)
{
    renderCollector.Clear(/* freeMemory */ false);

    RenderGroupCache& attributeRegistry = *RI.renderGroupCache;

    for (const Pair<ObjId<Entity>, int>& pair : meshEntityOrdering)
    {
        RenderProxyMesh* meshProxy = rpl.GetMeshEntities().GetProxy(pair.first);
        AssertDebug(meshProxy != nullptr);

        if (!meshProxy)
        {
            continue;
        }

        Mesh* mesh = meshProxy->mesh;
        MaterialInstance* material = meshProxy->material;

        if (!mesh || !material)
        {
            continue;
        }

        // @FIXME Thread safe?
        RenderableAttributeSet attributes = GetMergedRenderableAttributes(
            RenderableAttributeSet { mesh->GetMeshAttributes(), material->GetAttributes() },
            overrideAttributes);

        if (const Handle<Texture>& albedoTexture = material->GetTexture(MaterialTextureKey::Diffuse); albedoTexture.IsValid())
        {
            if (albedoTexture != RI.placeholderData->defaultTexture2d)
            {
                ShaderPropertySet newProperties = attributes.GetShaderProperties();
                newProperties.Add(s_propTextured);

                attributes.SetShaderProperties(newProperties);
            }
        }

        if (material->GetParameters().userParams.x > 0.0f) // it is text if this is set
        {
            ShaderPropertySet newProperties = attributes.GetShaderProperties();
            newProperties.Add(s_propUIText);

            attributes.SetShaderProperties(newProperties);
        }

        attributes.SetLayerIndex(pair.second);

        const RenderableAttributeHandle handle = attributeRegistry.GetOrCreate(attributes);

        DrawCallCollection& drawCallCollection = renderCollector.mappingsByBucket[uint32(handle.GetBucket())][handle.GetIndex()];

        if (!drawCallCollection.isInit)
        {
            drawCallCollection.attributes = attributes;
            drawCallCollection.flags = RenderGroupFlags::NONE;

            drawCallCollection.batchAllocator = renderCollector.batchAllocator;

            drawCallCollection.isInit = true;
            drawCallCollection.suppressStats = true;
        }

        drawCallCollection.meshProxies.Set(meshProxy->entity.Id().ToIndex(), meshProxy);
    }
}

void UIRenderCollector::ExecuteDrawCalls(Frame* frame, const RenderSetup& renderSetup, Framebuffer* framebuffer, uint32 bucketBits)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.HasWorld() && renderSetup.HasView());

    if (bucketBits == 0)
    {
        bucketBits = AllRenderBucketsMask;
    }

    RenderProxyList& rpl = GetConsumerProxyList(renderSetup.view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    if (framebuffer != nullptr)
    {
        frame->cr << SetCurrentFramebuffer(framebuffer);
    }

    RenderGroupCache& attributeRegistry = *RI.renderGroupCache;

    using IteratorType = BinnedDrawCallCollections::Iterator;

    Array<IteratorType, RenderAllocator> iterators;

    for (BinnedDrawCallCollections& mappings : mappingsByBucket)
    {
        iterators.Reserve(iterators.Size() + mappings.Count());

        for (auto it = mappings.Begin(); it != mappings.End(); ++it)
        {
            iterators.PushBack(it);
        }
    }

    {
        HYP_NAMED_SCOPE("Sort proxy groups by layer");

        std::sort(iterators.Begin(), iterators.End(), [](IteratorType lhs, IteratorType rhs) -> bool
            {
                return lhs->attributes.GetLayerIndex() < rhs->attributes.GetLayerIndex();
            });
    }

    // set these to null after rendering
    static Array<ParallelRenderingState**> s_parallelRenderingStatesToNullify;
    s_parallelRenderingStatesToNullify.Reserve(32);
    s_parallelRenderingStatesToNullify.Clear();

    for (size_t index = 0; index < iterators.Size(); index++)
    {
        DrawCallCollection& drawCallCollection = *iterators[index];
        Assert(drawCallCollection.isInit);

        if (drawCallCollection.flags & RenderGroupFlags::PARALLEL_COLLECTION)
        {
            drawCallCollection.parallelRenderingState = AcquireNextParallelRenderingState(uint8(drawCallCollection.attributes.GetMaterialAttributes().bucket));
        }

        PerformRendering(frame, renderSetup, drawCallCollection);

        if (drawCallCollection.parallelRenderingState != nullptr)
        {
            s_parallelRenderingStatesToNullify.PushBack(&drawCallCollection.parallelRenderingState);

            AssertDebug(drawCallCollection.parallelRenderingState->taskBatch != nullptr);
            TaskSystem::GetInstance().EnqueueBatch(drawCallCollection.parallelRenderingState->taskBatch);
        }
    }

    FOR_EACH_BIT(bucketBits, bit)
    {
        Commit(frame->cr, uint8(bit));
    }

    if (s_parallelRenderingStatesToNullify.Any())
    {
        for (ParallelRenderingState** pp : s_parallelRenderingStatesToNullify)
        {
            *pp = nullptr;
        }
    }

    if (framebuffer != nullptr)
    {
        frame->cr << SetCurrentFramebuffer(nullptr);
    }
}

#pragma endregion UIRenderCollector

#pragma region UIPass

UIPass::UIPass()
{
}

void UIPass::Initialize()
{
}

void UIPass::Shutdown()
{
}

void UIPass::RenderFrame(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    Assert(renderSetup.view != nullptr);

    UIPassData* pd = DynamicCast<UIPassData>(FetchViewPassData(renderSetup.view));
    AssertDebug(pd != nullptr);

    if (!pd->renderCollector.batchAllocator)
    {
        const Class* entityBatchClass = renderSetup.view->GetViewDesc().entityBatchClass;
        Assert(entityBatchClass != nullptr);

        pd->renderCollector.batchAllocator = GetOrCreateEntityBatchAllocator(entityBatchClass->GetTypeId());
        Assert(pd->renderCollector.batchAllocator != nullptr);
    }

    {
        RenderProxyList& rpl = GetConsumerProxyList(renderSetup.view);
        rpl.BeginRead();

        BuildRenderGroupsOrdered(pd->renderCollector, rpl, rpl.meshEntityOrdering.ToSpan(), {});

        rpl.EndRead();
    }

    RenderSetup rs = renderSetup.Fork();
    rs.passData = pd;

    pd->renderCollector.CollectRenderables(0);
    pd->renderCollector.ExecuteDrawCalls(frame, rs, nullptr, 0);
}

PassData* UIPass::CreateViewPassData(View* view, PassDataExt&)
{
    UIPassData* pd = new UIPassData();

    pd->view = MakeWeakRef(view);

    return pd;
}

#pragma endregion UIPass

} // namespace Hyperion

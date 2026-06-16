/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderConfig.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/Buffers.hpp>
#include <Rendering/FinalPass.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/MaterialInstance.hpp>
#include <Rendering/RenderGroup.hpp>
#include <Rendering/RenderGroupCache.hpp>

#include <Rendering/Passes/DeferredPass.hpp>
#include <Rendering/Passes/UIPass.hpp>

#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#include <Scene/View.hpp>

#include <UI/Font/FontAtlas.hpp>

#include <UI/UIStage.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Core/FileSystem/FsUtil.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/CVarManager.hpp>
#include <Framework/EngineStats.hpp>

#include <UIPass.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(UI);

static EngineStatGpuTimer s_statFillUI("Rendering/GPU/FillUI");

CORE_API extern const char* LookupTypeName(const TypeId& typeId);

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

            drawCallCollection.renderProxyList = &rpl;

            drawCallCollection.isInit = true;
            drawCallCollection.suppressStats = true;
        }

        drawCallCollection.meshProxies.Set(meshProxy->entity.Id().ToIndex(), meshProxy);
    }
}

void UIRenderCollector::ExecuteDrawCalls(Frame* frame, const RenderSetup& renderSetup, Framebuffer* framebuffer, uint32 bucketBits)
{
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
    static Array<ParallelRenderingState**> s_parallelRenderingStatesToNull;
    s_parallelRenderingStatesToNull.Reserve(32);
    s_parallelRenderingStatesToNull.Resize(0);

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
            s_parallelRenderingStatesToNull.PushBack(&drawCallCollection.parallelRenderingState);

            AssertDebug(drawCallCollection.parallelRenderingState->taskBatch != nullptr);
            TaskSystem::GetInstance().EnqueueBatch(drawCallCollection.parallelRenderingState->taskBatch);
        }
    }

    FOR_EACH_BIT(bucketBits, bit)
    {
        Commit(frame->cr, uint8(bit));
    }

    if (s_parallelRenderingStatesToNull.Any())
    {
        for (ParallelRenderingState** pp : s_parallelRenderingStatesToNull)
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
    AssertOnThread(g_renderThread);

    ENGINE_STAT_GPU_SCOPE(&s_statFillUI);

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

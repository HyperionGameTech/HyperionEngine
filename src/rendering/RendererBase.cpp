#include <RenderingPch.hpp>

#include <rendering/RendererBase.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderGroup.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <scene/View.hpp>

#include <core/threading/Threads.hpp>

#include <core/profiling/PerformanceClock.hpp>

#include <RendererBase.generated.inl>

namespace hyperion {

const RenderSetup& NullRenderSetup()
{
    static const RenderSetup s_nullRenderSetup;

    return s_nullRenderSetup;
}

#pragma region PassData

PassData::~PassData()
{
    HYP_LOG(Rendering, Debug, "Destroying PassData");

    if (next != nullptr)
    {
        delete next;
        next = nullptr;
    }

    // no need to SafeDelete() the graphics pipelines as they are managed by the global graphics pipeline cache.

    SafeDelete(std::move(descriptorSets));
}

int PassData::CullUnusedGraphicsPipelines(int maxIter)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    // Ensures the iterator is valid: the Iterator type for SparsePagedArray will find the next available slot in the constructor
    // elements may have been added in the middle or removed in the meantime.
    // elements that were added will be handled after the next time this loops around; elements that were removed will be skipped over to find the next valid entry.
    renderGroupCacheIterator = typename RenderGroupCache::Iterator(
        &renderGroupCache,
        renderGroupCacheIterator.page,
        renderGroupCacheIterator.elem);

    int numCycles = 0;

    for (; numCycles < maxIter; numCycles++)
    {
        // Loop around to the beginning of the container when the end is reached.
        if (renderGroupCacheIterator == renderGroupCache.End())
        {
            renderGroupCacheIterator = renderGroupCache.Begin();

            // no elements if still at end
            if (renderGroupCacheIterator == renderGroupCache.End())
            {
                break;
            }
        }

        RenderGroupCacheEntry& entry = *renderGroupCacheIterator;

        // check refcount is zero without lock
        if (entry.renderGroup.GetUnsafe()->GetObjectHeader_Internal()->GetRefCountStrong() == 0)
        {
            HYP_LOG(Rendering, Debug, "Removing graphics pipeline for RenderGroup '{}' as it is no longer valid.", entry.renderGroup.Id());

            renderGroupCacheIterator = renderGroupCache.Erase(renderGroupCacheIterator);

            continue;
        }

        ++renderGroupCacheIterator;
    }

    return numCycles;
}

#pragma endregion PassData

#pragma region RendererBase

struct NullPassDataExt final : PassDataExt
{
    PassDataExt* Clone() override
    {
        HYP_FAIL("Should not Clone() NullPassDataExt!");
    }
};

RendererBase::RendererBase()
    : m_viewPassDataCleanupIterator(m_viewPassData.End())
{
}

RendererBase::~RendererBase()
{
}

int RendererBase::RunCleanupCycle(int maxIter)
{
    // default impl, run for views
    return RunCleanupCycle(m_viewPassData, maxIter, &m_viewPassDataCleanupIterator);
}

int RendererBase::RunCleanupCycle(PassDataMap& passData, int maxIter, typename PassDataMap::Iterator* pIter)
{
    typename PassDataMap::Iterator tmpIterator;

    if (!pIter)
    {
        pIter = &tmpIterator;
    }

    typename PassDataMap::Iterator& iter = *pIter;

    // Ensures the iterator is valid: the Iterator type for SparsePagedArray will find the next available slot in the constructor
    // elements may have been added in the middle or removed in the meantime.
    // elements that were added will be handled after the next time this loops around; elements that were removed will be skipped over to find the next valid entry.
    iter = typename PassDataMap::Iterator(
        &passData,
        iter.page,
        iter.elem);

    const typename PassDataMap::Iterator startIterator = iter; // the iterator we started at - use it to check that we don't do duplicate checks

    int numCycles = 0;
    for (; numCycles < maxIter; ++numCycles)
    {
        // Loop around to the beginning of the container when the end is reached.
        if (iter == passData.End())
        {
            iter = passData.Begin();

            if (iter == passData.End())
            {
                break;
            }
        }

        Handle<PassData>& pd = *iter;

        if (!pd->view.Lock())
        {
            HYP_LOG(Rendering, Debug, "Removing PassData for View {} as it is no longer valid.", pd->view.Id());

            pd.Reset();

            iter = passData.Erase(iter);
        }
        else
        {
            pd->CullUnusedGraphicsPipelines(1000);

            ++iter;
        }

        if (iter == startIterator)
        {
            // we checked everything
            break;
        }
    }

    return numCycles;
}

const Handle<PassData>& RendererBase::TryGetViewPassData(View* view)
{
    if (!view)
    {
        return Handle<PassData>::empty;
    }

    AssertDebug(view->InstanceClass() == View::StaticClass(), "View cannot be subclassed"); // indices would get messed up

    if (Handle<PassData>* passData = m_viewPassData.TryGet(view->Id().ToIndex()))
    {
        return *passData;
    }

    return Handle<PassData>::empty;
}

const Handle<PassData>& RendererBase::FetchViewPassData(View* view, PassDataExt* ext, bool forceNew)
{
    if (!view)
    {
        return Handle<PassData>::empty;
    }

    AssertDebug(view->InstanceClass() == View::StaticClass(), "View cannot be subclassed"); // indices would get messed up

    Handle<PassData>* passDataHandle = m_viewPassData.TryGet(view->Id().ToIndex());

    if (!passDataHandle)
    {
        NullPassDataExt nullPassDataExt {};

        // call virtual function to alloc / create

        Handle<PassData> pd = CreateViewPassData(view, ext ? *ext : nullPassDataExt);
        AssertDebug(pd != nullptr);

        pd->next = ext ? ext->Clone() : nullptr;

        InitObject(pd);

        passDataHandle = &*m_viewPassData.Set(view->Id().ToIndex(), pd);
    }
    else if (forceNew || (*passDataHandle)->view.GetUnsafe() != view)
    {
        Handle<PassData>& pd = *passDataHandle;
        pd.Reset();

        NullPassDataExt nullPassDataExt {};

        pd = CreateViewPassData(view, ext ? *ext : nullPassDataExt);
        AssertDebug(pd != nullptr);

        pd->next = ext ? ext->Clone() : nullptr;

        InitObject(pd);

        m_viewPassData.Set(view->Id().ToIndex(), pd);
    }

    AssertDebug(passDataHandle != nullptr && *passDataHandle != nullptr);
    AssertDebug((*passDataHandle)->view.GetUnsafe() == view);

    return *passDataHandle;
}

#pragma region RendererBase

} // namespace hyperion

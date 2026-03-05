#include <RenderingPch.hpp>

#include <rendering/RendererBase.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/RenderGroup.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <scene/View.hpp>

#include <Core/threading/Threads.hpp>

#include <Core/profiling/PerformanceClock.hpp>

#include <RendererBase.generated.inl>

namespace Hyperion {

const RenderSetup& NullRenderSetup()
{
    static const RenderSetup s_nullRenderSetup;

    return s_nullRenderSetup;
}

#pragma region PassData

PassData::~PassData()
{
    HYP_LOG(Rendering, Verbose, "Destroying PassData");

    if (next != nullptr)
    {
        delete next;
        next = nullptr;
    }
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

        PassData* pd = *iter;

        if (pd->view.Expired())
        {
            HYP_LOG(Rendering, Verbose, "Removing PassData for View {} as it is no longer valid.", pd->view.Id());

            delete pd;

            iter = passData.Erase(iter);
        }
        else
        {
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

PassData* RendererBase::TryGetViewPassData(View* view)
{
    if (!view)
    {
        return nullptr;
    }

    AssertDebug(view->InstanceClass() == View::StaticClass(), "View cannot be subclassed"); // indices would get messed up

    if (PassData** ppPassData = m_viewPassData.TryGet(view->Id().ToIndex()))
    {
        return *ppPassData;
    }

    return nullptr;
}

PassData* RendererBase::FetchViewPassData(View* view, PassDataExt* ext, bool forceNew)
{
    if (!view)
    {
        return nullptr;
    }

    AssertDebug(view->InstanceClass() == View::StaticClass(), "View cannot be subclassed"); // indices would get messed up

    PassData** ppPassData = m_viewPassData.TryGet(view->Id().ToIndex());

    if (!ppPassData)
    {
        NullPassDataExt nullPassDataExt {};

        // call virtual function to alloc / create

        PassData* pd = CreateViewPassData(view, ext ? *ext : nullPassDataExt);
        AssertDebug(pd != nullptr);

        pd->next = ext ? ext->Clone() : nullptr;

        InitObject(pd);

        ppPassData = &*m_viewPassData.Set(view->Id().ToIndex(), pd);
    }
    else if (forceNew || (*ppPassData)->view.GetUnsafe() != view)
    {
        PassData* pd = *ppPassData;
        delete pd;

        NullPassDataExt nullPassDataExt {};

        pd = CreateViewPassData(view, ext ? *ext : nullPassDataExt);
        AssertDebug(pd != nullptr);

        pd->next = ext ? ext->Clone() : nullptr;

        InitObject(pd);

        ppPassData = &*m_viewPassData.Set(view->Id().ToIndex(), pd);
    }

    AssertDebug(ppPassData != nullptr && *ppPassData != nullptr);
    AssertDebug((*ppPassData)->view.GetUnsafe() == view);

    return *ppPassData;
}

#pragma region RendererBase

} // namespace Hyperion

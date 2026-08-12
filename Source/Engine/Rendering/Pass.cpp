/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Pass.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/GraphicsPipelineCache.hpp>
#include <Rendering/DrawCall.hpp>
#include <Rendering/RenderGroup.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Scene/View.hpp>

#include <Core/Threading/Threads.hpp>

#include <Core/Profiling/PerformanceClock.hpp>

#include <Pass.generated.inl>

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

#pragma region PassBase

struct NullPassDataExt final : PassDataExt
{
    PassDataExt* Clone() override
    {
        HYP_FAIL("Should not Clone() NullPassDataExt!");
    }
};

PassBase::PassBase()
{
}

PassBase::~PassBase()
{
    for (auto it = m_viewPassData.Begin(); it != m_viewPassData.End(); ++it)
    {
        PassData* pd = it->second;
        delete pd;
    }
}

void PassBase::OnFrameEnd(uint32 prevFrameIndex)
{
    for (auto it = m_viewPassData.Begin(); it != m_viewPassData.End();)
    {
        PassData* pd = it->second;

        if (pd == nullptr)
        {
            it = m_viewPassData.Erase(it);
            continue;
        }

        if (pd->view.Expired())
        {
            HYP_LOG(Rendering, Verbose, "Removing PassData for View {} as it is no longer valid.", pd->view.Id());

            delete pd;

            it = m_viewPassData.Erase(it);
        }
        else
        {
            ++it;
        }
    }
}

PassData* PassBase::TryGetViewPassData(View* view)
{
    if (!view)
    {
        return nullptr;
    }

    if (KeyValuePair<View*, PassData*>* it = m_viewPassData.TryGet(view))
    {
        return it->second;
    }

    return nullptr;
}

PassData* PassBase::FetchViewPassData(View* view, PassDataExt* ext, bool forceNew)
{
    if (!view)
    {
        return nullptr;
    }

    AssertDebug(view->InstanceClass() == View::StaticClass(), "View cannot be subclassed"); // indices would get messed up

    KeyValuePair<View*, PassData*>* it = m_viewPassData.TryGet(view);

    if (!it)
    {
        NullPassDataExt nullPassDataExt {};

        // call virtual function to alloc / create

        PassData* pd = CreateViewPassData(view, ext ? *ext : nullPassDataExt);
        AssertDebug(pd != nullptr);

        pd->next = ext ? ext->Clone() : nullptr;

        InitObject(pd);

        it = &*m_viewPassData.Set(view, pd).first;
    }
    else if (forceNew || it->second->view.GetUnsafe() != view)
    {
        PassData* pd = it->second;
        delete pd;

        NullPassDataExt nullPassDataExt {};

        pd = CreateViewPassData(view, ext ? *ext : nullPassDataExt);
        AssertDebug(pd != nullptr);

        pd->next = ext ? ext->Clone() : nullptr;

        InitObject(pd);

        it = &*m_viewPassData.Set(view, pd).first;
    }

    AssertDebug(it != nullptr && it->second != nullptr);
    AssertDebug(it->second->view.GetUnsafe() == view);

    return it->second;
}

#pragma region PassBase

} // namespace Hyperion

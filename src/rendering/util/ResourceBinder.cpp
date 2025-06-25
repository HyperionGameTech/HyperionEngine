#include <rendering/util/ResourceBinder.hpp>
#include <rendering/RenderGlobalState.hpp>

namespace hyperion {

ResourceBinderBase::ResourceBinderBase(RenderGlobalState* rgs)
{
    AssertDebug(rgs != nullptr);

    for (uint32 i = 0; i < RenderGlobalState::max_binders; i++)
    {
        if (rgs->ObjectBinders[i] == nullptr)
        {
            rgs->ObjectBinders[i] = this;
            return;
        }
    }

    HYP_FAIL("Failed to find a free slot in the RenderGlobalState's ObjectBinders array!");
}

} // namespace hyperion
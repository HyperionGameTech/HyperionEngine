#include <rendering/util/ResourceBinder.hpp>
#include <rendering/RenderGlobalState.hpp>

namespace hyperion {

ResourceBinderBase::ResourceBinderBase(RenderGlobalState* rgs)
{
    AssertDebug(rgs != nullptr);

    for (uint32 i = 0; i < RenderGlobalState::max_binders; i++)
    {
        if (rgs->ResourceBinders[i] == nullptr)
        {
            rgs->ResourceBinders[i] = this;
            return;
        }
    }

    HYP_FAIL("Failed to find a free slot!");
}

} // namespace hyperion
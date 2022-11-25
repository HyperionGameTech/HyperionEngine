#include <rendering/DrawProxy.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

void WaitForRenderUpdatesToComplete()
{
    HYP_FLUSH_RENDER_QUEUE();
}

} // namespace hyperion::v2
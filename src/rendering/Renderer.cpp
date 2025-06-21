#include <rendering/Renderer.hpp>

#include <core/threading/Task.hpp>
#include <core/threading/TaskSystem.hpp>

#include <core/Defines.hpp>

namespace hyperion {

const RenderSetup& NullRenderSetup()
{
    static const RenderSetup null_render_setup;
    return null_render_setup;
}

} // namespace hyperion
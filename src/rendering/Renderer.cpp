#include <rendering/Renderer.hpp>

#include <core/Defines.hpp>

namespace hyperion {

HYP_API const RenderSetup& NullRenderSetup()
{
    static const RenderSetup null_render_setup;
    return null_render_setup;
}

} // namespace hyperion
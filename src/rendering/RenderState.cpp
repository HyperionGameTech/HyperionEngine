#include <rendering/RenderState.hpp>

namespace hyperion::v2 {

const RenderBinding<Scene> RenderBinding<Scene>::empty = RenderBinding { };
const RenderBinding<Camera> RenderBinding<Camera>::empty = RenderBinding { };

} // namespace hyperion::v2
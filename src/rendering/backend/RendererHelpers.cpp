#include <rendering/backend/RendererHelpers.hpp>

#include <math/MathUtil.hpp>

namespace hyperion {
namespace renderer {
namespace helpers {

UInt MipmapSize(UInt src_size, Int lod)
{
    return MathUtil::Max(src_size >> lod, 1u);
}

} // namespace helpers
} // namespace renderer
} // namespace hyperion
/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <Rendering/RenderHelpers.hpp>

#include <Core/math/MathUtil.hpp>

namespace Hyperion {
namespace helpers {

uint32 MipmapSize(uint32 srcSize, int lod)
{
    return MathUtil::Max(srcSize >> lod, 1u);
}

} // namespace helpers

} // namespace Hyperion

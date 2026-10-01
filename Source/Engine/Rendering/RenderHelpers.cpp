/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <Rendering/RenderHelpers.hpp>

#include <Core/Math/MathUtil.hpp>

namespace Hyperion {
namespace helpers {

uint32 MipmapSize(uint32 srcSize, int lod)
{
    return MathUtil::Max(srcSize >> lod, 1u);
}

Vec3u WrapComputeGroupCount(uint32 numGroups)
{
    numGroups = MathUtil::Max(numGroups, 1u);

    const uint32 groupsX = MathUtil::Min(numGroups, MaxComputeGroupsPerDimension);

    return Vec3u { groupsX, (numGroups + groupsX - 1) / groupsX, 1 };
}

} // namespace helpers

} // namespace Hyperion

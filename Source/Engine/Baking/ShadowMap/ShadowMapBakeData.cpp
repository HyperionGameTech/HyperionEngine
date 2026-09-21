/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Baking/ShadowMap/ShadowMapBakeData.hpp>

#include <Scene/Light/Light.hpp>

namespace Hyperion {

namespace Baking {

Result BakeData<Light>::Build()
{
    Assert(m_light != nullptr);

    dimensions = Vec3u(m_light->GetShadowMapDimensions(), 1);

    AssertDebug(dimensions.Volume() > 0,
        "Shadow map dimensions must be non-zero! Dimensions: {}", dimensions);


    return {};
}

} // namespace Baking
} // namespace Hyperion

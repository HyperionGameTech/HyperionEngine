/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>

#include <Scene/Light.hpp> // For LightType

namespace Hyperion {

class ShadowMap;
struct ShadowMapData;
class RenderProxyList;

extern const StringHash GBufferTextureNames[NumGBufferTargets];

namespace DeferredRendererHelpers {

HYP_FORCE_INLINE bool CanClusterLight(LightType lightType)
{
    return lightType == LightType::Point
        || lightType == LightType::Spot;
}

void FillShadowMapData(
    ShadowMapData &outShadowMapData,
    const ShadowMap &inShadowMap,
    uint32 cascadeIndex,
    View *shadowMapViewDynamic,
    View *shadowMapViewStatic);

} // namespace DeferredRendererHelpers

} // namespace Hyperion

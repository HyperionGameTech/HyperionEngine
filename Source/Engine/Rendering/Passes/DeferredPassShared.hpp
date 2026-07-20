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

struct DirectionalLightCSMData
{
    Mat4f shadowViewMat;

    Vec4f atlasU;
    Vec4f atlasV;
    Vec4f atlasScaleX;
    Vec4f atlasScaleY;

    Vec4u atlasSlice;

    Vec4f cascadeScaleX;
    Vec4f cascadeScaleY;
    Vec4f cascadeScaleZ;

    Vec4f cascadeOffsetX;
    Vec4f cascadeOffsetY;
    Vec4f cascadeOffsetZ;
};


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

void FillShadowMapDataCSM(
    DirectionalLightCSMData *outCSMData,
    View **shadowMapViews,
    ShadowMap **shadowMaps,
    uint32 numCascades);

} // namespace DeferredRendererHelpers

} // namespace Hyperion

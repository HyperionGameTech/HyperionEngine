/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/EffectVolume.hpp>

#include <Rendering/RenderProxy.hpp>

#include <EffectVolume.generated.inl>

namespace Hyperion {

void EffectVolume::UpdateRenderProxy(RenderProxyEffectVolume* proxy)
{
    AssertDebug(proxy != nullptr);

    const BoundingBox worldAabb = GetWorldBounds();

    proxy->effectVolume = this;
    proxy->worldAabb = worldAabb;

    proxy->bufferData.aabbMin = Vec4f(worldAabb.min, 1.0f);
    proxy->bufferData.aabbMax = Vec4f(worldAabb.max, 1.0f);
}

} // namespace Hyperion

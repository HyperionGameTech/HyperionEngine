/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/LOD.hpp>

#include <Scene/Scene.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/World.hpp>
#include <Scene/View.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Framework/GameState.hpp>

namespace Hyperion {

LODViewData::LODViewData(const Camera& camera)
{
    const Mat4f& projectionMatrix = camera.GetProjectionMatrix();

    position = camera.GetWorldTranslation();
    projectionScale = projectionMatrix[1][1];
    nearClip = camera.GetNearClip();
    isOrthographic = MathUtil::Abs(projectionMatrix[3][3]) > MathUtil::epsilonF;
}


} // namespace Hyperion

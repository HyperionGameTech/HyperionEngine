/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Util/SceneHelpers.hpp>

#include <Scene/World.hpp>
#include <Scene/Scene.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/EntityTag.hpp>

#include <Scene/Camera/Camera.hpp>

namespace Hyperion {
namespace SceneHelpers {

Camera* FindMainCamera(World& world)
{
    for (Scene* scene : world.GetScenes())
    {
        Assert(scene != nullptr);
        
        if (scene->GetSceneFlags() & SceneFlags::FOREGROUND)
        {
            EntityManager* entityManager = scene->GetEntityManager();
            Assert(entityManager != nullptr);

            if (!entityManager)
            {
                continue;
            }

            for (auto [entity, _0, _1] : entityManager->GetEntitySet<EntityType<Camera>, TagComponent<EntityTag::PrimaryCamera>>())
            {
                return StaticCast<Camera>(entity);
            }
        }
    }
    
    return nullptr;
}

} // namespace SceneHelpers
} // namespace Hyperion

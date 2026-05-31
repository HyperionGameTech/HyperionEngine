/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

namespace Hyperion {

class Entity;
class World;

struct ScriptComponent;
struct GameState;

namespace EntityScripting {

template <class TWorld, class T>
static void QueryScriptedEntities(TWorld& world, T&& function)
{
    for (auto&& scene : world.GetScenes())
    {
        for (auto [entity, scriptComponent] : scene->GetEntityManager()->template GetEntitySet<ScriptComponent>())
        {
            function(entity, scriptComponent);
        }
    }
}

void InitializeEntityScript(Entity* entity, ScriptComponent& scriptComponent, const GameState& gameState);
void ShutdownEntityScript(Entity* entity, ScriptComponent& scriptComponent, const GameState& gameState);

void UpdateScriptedEntities(World& world, float delta);

} // namespace EntityScripting

} // namespace Hyperion

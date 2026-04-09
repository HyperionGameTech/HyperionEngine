/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

namespace Hyperion {

class Entity;
struct ScriptComponent;

class EntityScripting
{
public:
    static void InitEntityScriptComponent(Entity* entity, ScriptComponent& scriptComponent);
    static void DeinitEntityScriptComponent(Entity* entity, ScriptComponent& scriptComponent);
};

} // namespace Hyperion

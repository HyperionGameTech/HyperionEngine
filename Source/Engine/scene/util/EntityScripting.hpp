/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

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

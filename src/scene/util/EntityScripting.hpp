/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

namespace hyperion {

class Entity;
struct ScriptComponent;

class EntityScripting
{
public:
    static void InitEntityScriptComponent(Entity* entity, ScriptComponent& scriptComponent);
    static void DeinitEntityScriptComponent(Entity* entity, ScriptComponent& scriptComponent);
};

} // namespace hyperion

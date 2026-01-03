/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/System.hpp>
#include <scene/components/ScriptComponent.hpp>

#include <engine/GameState.hpp>

#include <core/functional/Delegate.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings, Serialize=false)
class ScriptSystem final : public SystemBase
{
    HYP_OBJECT_BODY(ScriptSystem);

public:
    ScriptSystem();
    virtual ~ScriptSystem() override = default;

    // This system does not support parallel execution because scripts may modify
    // any component in the entity manager
    virtual bool AllowParallelExecution() const override
    {
        return false;
    }

    virtual bool RequiresSimThread() const override
    {
        return true;
    }

    virtual void OnEntityAdded(Entity* entity) override;
    virtual void OnEntityRemoved(Entity* entity) override;

    virtual void Process(float delta, Span<Handle<Scene>> scenes) override;

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<ScriptComponent, ComponentAccess::READ_WRITE> {}
        };
    }

    void HandleGameStateChanged(GameStateMode gameStateMode, GameStateMode previousGameStateMode);

    void CallScriptMethod(UTF8StringView methodName);
    void CallScriptMethod(UTF8StringView methodName, ScriptComponent& target);
};

} // namespace Hyperion

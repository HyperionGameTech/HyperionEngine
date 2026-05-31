/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <scene/System.hpp>
#include <scene/components/ScriptComponent.hpp>

#include <Framework/GameState.hpp>

#include <Core/functional/Delegate.hpp>
#include <Core/memory/UniquePtr.hpp>

namespace Hyperion {

class ScriptingService;
class ScriptTracker;

HYP_CLASS(NoScriptBindings, Serialize=false)
class ScriptSystem final : public SystemBase
{
    HYP_OBJECT_BODY(ScriptSystem);

public:
    ScriptSystem();
    ~ScriptSystem() override;

    bool AllowParallelExecution() const override;
    bool RequiresSimThread() const override;
    bool AllowUpdate() const override;

    void OnAddedToWorld(World* world) override;
    void OnRemovedFromWorld(World* world) override;

    void OnEntityAdded(Entity* entity) override;
    void OnEntityRemoved(Entity* entity) override;

    void Process(float delta, Span<Handle<Scene>> scenes) override;

private:
    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<ScriptComponent, ComponentAccess::READ_WRITE> {}
        };
    }

    void HandleGameStateChanged(GameStateMode gameStateMode, GameStateMode previousGameStateMode);

    UniquePtr<ScriptingService> m_scriptingService;
    UniquePtr<ScriptTracker> m_scriptTracker;
};

} // namespace Hyperion

/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/System.hpp>
#include <Scene/Components/CharacterControllerComponent.hpp>
#include <Scene/Components/TransformComponent.hpp>

#include <Input/InputHandler.hpp>

namespace Hyperion {

HYP_CLASS()
class ENGINE_API CharacterControllerInputHandler final : public InputHandlerBase
{
    HYP_OBJECT_BODY(CharacterControllerInputHandler);

public:
    CharacterControllerInputHandler() = default;
    ~CharacterControllerInputHandler() override = default;

    Vec2f GetMovementInput() const;
    bool IsJumpPressed() const;

    bool OnKeyDown(const KeyboardEvent& evt) override;
    bool OnKeyUp(const KeyboardEvent& evt) override;

private:
    float m_forward = 0.0f;
    float m_strafe = 0.0f;
    bool m_jump = false;
};

HYP_CLASS(NoScriptBindings)
class CharacterControllerSystem final : public SystemBase
{
    HYP_OBJECT_BODY(CharacterControllerSystem);

public:
    ~CharacterControllerSystem() override = default;

    bool ShouldProcessScene(Scene* scene) const override;

    void OnEntityAdded(Entity* entity) override;
    void OnEntityRemoved(Entity* entity) override;

    void Process(float delta, Span<Handle<Scene>> scenes) override;

    bool RequiresSimThread() const override { return true; }
    bool AllowParallelExecution() const override { return false; }

private:
    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<CharacterControllerComponent, ComponentAccess::READ_WRITE> {},
            ComponentDescriptor<TransformComponent, ComponentAccess::READ_WRITE> {}
        };
    }
};

} // namespace Hyperion

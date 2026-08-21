/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/System.hpp>
#include <Scene/Components/CharacterControllerComponent.hpp>
#include <Scene/Components/TransformComponent.hpp>
#include <Scene/Components/PlayerComponent.hpp>

#include <Input/InputHandler.hpp>

namespace Hyperion {

HYP_CLASS()
class ENGINE_API CharacterControllerInputHandler final : public InputHandlerBase
{
    HYP_OBJECT_BODY(CharacterControllerInputHandler);

public:
    CharacterControllerInputHandler()
        : m_forward(0.0f),
          m_strafe(0.0f),
          m_isJumpRequested(false)
    {
    }

    explicit CharacterControllerInputHandler(const Handle<InputHandlerBase>& parentInputHandler)
        : m_parentInputHandler(parentInputHandler),
          m_forward(0.0f),
          m_strafe(0.0f),
          m_isJumpRequested(false)
    {
    }

    ~CharacterControllerInputHandler() override = default;

    HYP_FORCE_INLINE const Handle<InputHandlerBase>& GetParentInputHandler() const
    {
        return m_parentInputHandler;
    }

    HYP_FORCE_INLINE const Vec2f& GetMovementInput() const
    {
        return m_movementInput;
    }

    /// Server only!
    HYP_FORCE_INLINE void SetMovementInput(const Vec2f& movementInput)
    {
        m_movementInput = movementInput;
    }

    HYP_FORCE_INLINE bool IsJumpPressed() const
    {
        return m_isJumpRequested;
    }

    /// Server only!
    HYP_FORCE_INLINE void SetIsJumpRequested(bool isJumpRequested)
    {
        m_isJumpRequested = isJumpRequested;
    }

    bool OnKeyDown(const KeyboardEvent& evt) override;
    bool OnKeyUp(const KeyboardEvent& evt) override;

private:
    void Update();

    HYP_FIELD(Serialize)
    Handle<InputHandlerBase> m_parentInputHandler;

    float m_forward;
    float m_strafe;

    Vec2f m_movementInput;
    bool m_isJumpRequested;
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
            ComponentDescriptor<TransformComponent, ComponentAccess::READ_WRITE> {},
            ComponentDescriptor<PlayerComponent, ComponentAccess::READ, false> {}
        };
    }
};

} // namespace Hyperion

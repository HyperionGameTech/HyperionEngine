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

#include <Framework/Net/PlayerMove.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Map.hpp>

namespace Hyperion {

class Entity;

void ApplyCharacterMove(Entity* entity, CharacterControllerComponent& component, const PlayerMove& move, Vec3f& outResultTranslation);

HYP_CLASS()
class CharacterControllerInputHandler final : public InputHandlerBase
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

    HYP_FORCE_INLINE bool IsJumpPressed() const
    {
        return m_isJumpRequested;
    }

    bool OnKeyDown(const KeyboardEvent& evt) override;
    bool OnKeyUp(const KeyboardEvent& evt) override;

    // Recomputes movement/jump input from live keyboard, touch and controller state.
    void Update();

private:
    HYP_FIELD(Serialize)
    Handle<InputHandlerBase> m_parentInputHandler;

    float m_forward;
    float m_strafe;

    Vec2f m_movementInput;
    bool m_isJumpRequested;
};

struct ClientPredictionState
{
    struct BufferedMove
    {
        PlayerMove move;
        Vec3f resultTranslation; // predicted entity translation after applying this move
    };

    static constexpr uint32 MaxBufferedMoves = 128;

    Array<BufferedMove, SceneAllocator> unacknowledgedMoves;
    uint32 nextMoveId = 1;
    uint32 lastAckedMoveId = 0;
    uint32 lastSentMoveId = 0;
    Vec3f smoothingOffset = Vec3f(0.0f);
    float smoothingSecondsRemaining = 0.0f;
    float secondsSinceLastSend = 0.0f;
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

    ClientPredictionState& GetPredictionState(Entity* entity)
    {
        return m_predictionStates[entity];
    }

private:
    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<CharacterControllerComponent, ComponentAccess::READ_WRITE> {},
            ComponentDescriptor<TransformComponent, ComponentAccess::READ_WRITE> {},
            ComponentDescriptor<PlayerComponent, ComponentAccess::READ, false> {}
        };
    }

    Map<Entity*, ClientPredictionState, SceneAllocator> m_predictionStates;
};

} // namespace Hyperion

#pragma once
#include <core/Defines.hpp>

#include "Game.hpp"

#include <core/reflection/Handle.hpp>

namespace hyperion {

class Camera;
class EditorSubsystem;

namespace game {

class DefaultGameImpl;

HYP_CLASS(NoScriptBindings)
class HYP_API DefaultGame : public Game
{
    HYP_OBJECT_BODY(DefaultGame);

public:
    DefaultGame();
    DefaultGame(const DefaultGame& other) = delete;
    DefaultGame& operator=(const DefaultGame& other) = delete;
    DefaultGame(DefaultGame&& other) noexcept = delete;
    DefaultGame& operator=(DefaultGame&& other) noexcept = delete;
    virtual ~DefaultGame() override;

    virtual void Logic(float delta) override;
    virtual void OnInputEvent(const SystemEvent& event) override;

public:
    Handle<Scene> m_defaultScene;
    Handle<Camera> m_camera;

private:
    bool m_mouseLocked = false;

protected:
    virtual void Init() override;

    DefaultGameImpl* m_impl;
};
} // namespace game

using game::DefaultGame;

} // namespace hyperion

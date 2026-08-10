#pragma once

#include <Core/Defines.hpp>

#include <Framework/Game.hpp>

#include <Core/Reflection/Handle.hpp>

namespace Hyperion {

class Camera;
class DirectionalLight;
class EditorSubsystem;

namespace game {

class DefaultGameImpl;

HYP_CLASS(NoScriptBindings)
class ENGINE_API DefaultGame : public Game
{
    HYP_OBJECT_BODY(DefaultGame);

public:
    DefaultGame();

    DefaultGame(const DefaultGame& other) = delete;
    DefaultGame& operator=(const DefaultGame& other) = delete;

    DefaultGame(DefaultGame&& other) noexcept = delete;
    DefaultGame& operator=(DefaultGame&& other) noexcept = delete;

    virtual ~DefaultGame() override;

protected:
    void InitializeWorld() override;
    
    void BeforeContentLoaded() override;
    void AfterContentLoaded() override;

    void ShowLoadingScreen();
    void HideLoadingScreen();

    virtual void OnLaunch_Impl() override;
    virtual void OnUpdate_Impl(float delta) override;

    DefaultGameImpl* m_impl;

    Handle<Scene> m_defaultScene;
    Handle<Camera> m_camera;
    Handle<DirectionalLight> m_sun;
    float m_sunAngle = 0.0f;
};
} // namespace game

using game::DefaultGame;

} // namespace Hyperion

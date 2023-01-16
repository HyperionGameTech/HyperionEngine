#ifndef HYPERION_V2_GAME_H
#define HYPERION_V2_GAME_H

#include <GameCounter.hpp>
#include <core/lib/UniquePtr.hpp>
#include <input/InputManager.hpp>
#include <scene/Scene.hpp>
#include <ui/UIScene.hpp>
#include <system/SdlSystem.hpp>

namespace hyperion {
namespace renderer {

class Frame;

} // namespace renderer
} // namespace hyperion

namespace hyperion::v2 {

using renderer::Frame;

class Engine;

class Game
{
    friend class GameThread;

public:
    Game(RefCountedPtr<Application> application);
    virtual ~Game();
    
    Handle<Scene> &GetScene()
        { return m_scene; }

    const Handle<Scene> &GetScene() const
        { return m_scene; }

    virtual void Init() final;
    virtual void Update(GameCounter::TickUnit delta) final;
    virtual void Teardown();

    virtual void InitGame();

    virtual void OnFrameBegin(Frame *frame);
    virtual void OnFrameEnd(Frame *frame);

    virtual void HandleEvent(SystemEvent &&event) final;
    virtual void OnInputEvent(const SystemEvent &event);

protected:
    virtual void Logic(GameCounter::TickUnit delta) = 0;

    UIScene &GetUI()
        { return m_ui; }

    const UIScene &GetUI() const
        { return m_ui; }

    const UniquePtr<InputManager> &GetInputManager() const
        { return m_input_manager; }

    RefCountedPtr<Application> m_application;

    UniquePtr<InputManager> m_input_manager;
    Handle<Scene> m_scene;

    UIScene m_ui;

private:
    bool m_is_init;
};

} // namespace hyperion::v2

#endif
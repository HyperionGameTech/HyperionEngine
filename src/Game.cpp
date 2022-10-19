#include "Game.hpp"
#include <Engine.hpp>
#include <Threads.hpp>

#include <system/Debug.hpp>

namespace hyperion::v2 {

Game::Game()
    : m_is_init(false),
      m_input_manager(nullptr)
{
}

Game::~Game()
{
    AssertExitMsg(
        !m_is_init,
        "Expected Game to have called Teardown() before destructor call"
    );

    delete m_input_manager;
}

void Game::Init(Engine *engine, SystemWindow *window)
{
    Threads::AssertOnThread(THREAD_MAIN);

    m_input_manager = new InputManager(window);

    m_scene = engine->CreateHandle<Scene>(
        Handle<Camera>(),
        Scene::InitInfo {
            .flags = Scene::InitInfo::SCENE_FLAGS_HAS_TLAS // default it to having a top level acceleration structure for RT
        }
    );

    engine->InitObject(m_scene);

    m_is_init = true;

    static_assert(THREAD_MAIN == THREAD_RENDER,
        "InitRender must be enqueued instead of directly called if main thread != render thread!");

    InitRender(engine);
}

void Game::Update(Engine *engine, GameCounter::TickUnit delta)
{
    engine->GetWorld().Update(engine, delta);

    Logic(engine, delta);
}

void Game::InitRender(Engine *engine)
{
    Threads::AssertOnThread(THREAD_RENDER);
}

void Game::InitGame(Engine *engine)
{
    Threads::AssertOnThread(THREAD_GAME);

    m_ui.Init(engine);
}

void Game::Teardown(Engine *engine)
{
    engine->GetWorld().RemoveScene(m_scene);
    m_scene.Reset();

    m_is_init = false;
}

void Game::HandleEvent(Engine *engine, SystemEvent &event)
{
    if (m_input_manager == nullptr) {
        return;
    }

    m_input_manager->CheckEvent(&event);

    switch (event.GetType()) {
        case SystemEventType::EVENT_SHUTDOWN:
            engine->RequestStop();

            break;
        default:
            if (engine->game_thread.IsRunning()) {
                engine->game_thread.GetScheduler().Enqueue([this, engine, event](...) {
                    OnInputEvent(engine, event);

                    HYPERION_RETURN_OK;
                });
            }

            break;
    }
}

void Game::OnInputEvent(Engine *engine, const SystemEvent &event)
{
    Threads::AssertOnThread(THREAD_GAME);

    switch (event.GetType()) {
        case SystemEventType::EVENT_MOUSESCROLL:
        {
            if (m_scene && m_scene->GetCamera()) {
                int wheel_x,
                    wheel_y;

                event.GetMouseWheel(&wheel_x, &wheel_y);

                m_scene->GetCamera()->PushCommand(CameraCommand {
                    .command = CameraCommand::CAMERA_COMMAND_SCROLL,
                    .scroll_data = {
                        .wheel_x = wheel_x,
                        .wheel_y = wheel_y
                    }
                });
            }

            break;
        }
        case SystemEventType::EVENT_MOUSEMOTION:
        {
            const auto &mouse_position = m_input_manager->GetMousePosition();

            int mouse_x = mouse_position.x.load(),
                mouse_y = mouse_position.y.load();

            float mx, my;

            int window_width,
                window_height;

            m_input_manager->GetWindow()->GetSize(&window_width, &window_height);

            mx = (static_cast<float>(mouse_x) - static_cast<float>(window_width) * 0.5f) / (static_cast<float>(window_width));
            my = (static_cast<float>(mouse_y) - static_cast<float>(window_height) * 0.5f) / (static_cast<float>(window_height));
            
            if (m_scene) {
                m_scene->GetCamera()->PushCommand(CameraCommand {
                    .command = CameraCommand::CAMERA_COMMAND_MAG,
                    .mag_data = {
                        .mouse_x = mouse_x,
                        .mouse_y = mouse_y,
                        .mx = mx,
                        .my = my
                    }
                });
            }

            break;
        }
        default:
            break;
    }
}

} // namespace hyperion::v2
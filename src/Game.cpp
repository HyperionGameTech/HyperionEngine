#include <Game.hpp>
#include <Engine.hpp>
#include <Threads.hpp>

#include <system/Debug.hpp>

namespace hyperion::v2 {

Game::Game(RC<Application> application)
    : m_application(application),
      m_is_init(false),
      m_input_manager(new InputManager())
{
}

Game::~Game()
{
    AssertExitMsg(
        !m_is_init,
        "Expected Game to have called Teardown() before destructor call"
    );
}

void Game::Init()
{
    Threads::AssertOnThread(THREAD_MAIN);

    AssertThrowMsg(m_application != nullptr, "No valid Application instance was provided to Game constructor!");
    if (m_application->GetCurrentWindow()) {
        m_input_manager->SetWindow(m_application->GetCurrentWindow());
    }

    m_scene = CreateObject<Scene>(
        Handle<Camera>(),
        Scene::InitInfo {
            .flags = Scene::InitInfo::SCENE_FLAGS_HAS_TLAS // default it to having a top level acceleration structure for RT
        }
    );

    InitObject(m_scene);
    g_engine->GetWorld()->AddScene(m_scene);

    InitRender();

    m_is_init = true;
}

void Game::Update(GameCounter::TickUnit delta)
{
    EntityManager::GetInstance().Update(delta);

    g_engine->GetComponents().Update(delta);
    g_engine->GetWorld()->Update(delta);

    Logic(delta);
}

void Game::InitGame()
{
    Threads::AssertOnThread(THREAD_GAME);

    m_ui.Init();
}

void Game::InitRender()
{
    Threads::AssertOnThread(THREAD_RENDER);
}

void Game::Teardown()
{
    if (m_scene) {
        g_engine->GetWorld()->RemoveScene(m_scene->GetID());
        m_scene.Reset();
    }

    g_engine->GetWorld().Reset();

    m_is_init = false;
}

void Game::HandleEvent(SystemEvent &&event)
{
    if (m_input_manager == nullptr) {
        return;
    }

    m_input_manager->CheckEvent(&event);

    switch (event.GetType()) {
    case SystemEventType::EVENT_SHUTDOWN:
        g_engine->RequestStop();

        break;
    default:
        if (g_engine->game_thread->IsRunning()) {
            g_engine->game_thread->GetScheduler().Enqueue([this, event = std::move(event)](...) mutable {
                OnInputEvent(event);

                HYPERION_RETURN_OK;
            });
        }

        break;
    }
}

void Game::OnInputEvent(const SystemEvent &event)
{
    Threads::AssertOnThread(THREAD_GAME);

    // forward to UI
    if (m_ui.OnInputEvent(m_input_manager.Get(), event)) {
        // ui handled the event
        return;
    }

    switch (event.GetType()) {
    case SystemEventType::EVENT_MOUSESCROLL:
    {
        if (m_scene && m_scene->GetCamera()) {
            int wheel_x,
                wheel_y;

            event.GetMouseWheel(&wheel_x, &wheel_y);

            if (auto *controller = m_scene->GetCamera()->GetCameraController()) {
                controller->PushCommand(CameraCommand {
                    .command = CameraCommand::CAMERA_COMMAND_SCROLL,
                    .scroll_data = {
                        .wheel_x = wheel_x,
                        .wheel_y = wheel_y
                    }
                });
            }
        }

        break;
    }
    case SystemEventType::EVENT_MOUSEMOTION:
    {
        if (m_input_manager->GetWindow()->HasMouseFocus()) {
            const auto &mouse_position = m_input_manager->GetMousePosition();

            const Int mouse_x = mouse_position.x.load(),
                mouse_y = mouse_position.y.load();

            const auto extent = m_input_manager->GetWindow()->GetExtent();

            const Float mx = (Float(mouse_x) - Float(extent.width) * 0.5f) / (Float(extent.width));
            const Float my = (Float(mouse_y) - Float(extent.height) * 0.5f) / (Float(extent.height));
            
            if (m_scene) {
                if (auto *controller = m_scene->GetCamera()->GetCameraController()) {
                    controller->PushCommand(CameraCommand {
                        .command = CameraCommand::CAMERA_COMMAND_MAG,
                        .mag_data = {
                            .mouse_x = mouse_x,
                            .mouse_y = mouse_y,
                            .mx = mx,
                            .my = my
                        }
                    });
                }
            }
        }

        break;
    }
    case SystemEventType::EVENT_FILE_DROP:
    {

        break;
    }
    default:
        break;
    }
}

void Game::OnFrameBegin(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    g_engine->GetRenderState().AdvanceFrameCounter();
    g_engine->GetRenderState().BindScene(m_scene.Get());

    if (m_scene->GetCamera()) {
        g_engine->GetRenderState().BindCamera(m_scene->GetCamera().Get());
    }
}

void Game::OnFrameEnd(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    if (m_scene->GetCamera()) {
        g_engine->GetRenderState().UnbindCamera();
    }

    g_engine->GetRenderState().UnbindScene();
}

} // namespace hyperion::v2
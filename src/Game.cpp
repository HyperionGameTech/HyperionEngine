/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <Game.hpp>

#include <Engine.hpp>

#include <core/threading/GameThread.hpp>
#include <core/threading/Threads.hpp>
#include <core/system/SystemEvent.hpp>
#include <core/system/Debug.hpp>

#include <dotnet/DotNetSystem.hpp>
#include <dotnet/Class.hpp>
#include <dotnet/runtime/ManagedHandle.hpp>
#include <dotnet/core/RefCountedPtrBindings.hpp>

namespace hyperion {

Game::Game()
    : m_is_init(false),
      m_input_manager(new InputManager()),
      m_ui_stage(new UIStage())
{
}

Game::Game(Optional<ManagedGameInfo> managed_game_info)
    : m_is_init(false),
      m_input_manager(new InputManager()),
      m_managed_game_info(std::move(managed_game_info)),
      m_ui_stage(new UIStage())
{
}

Game::~Game()
{
    AssertExitMsg(
        !m_is_init,
        "Expected Game to have called Teardown() before destructor call"
    );
}

void Game::Init_Internal()
{
    Threads::AssertOnThread(ThreadName::THREAD_MAIN);

    AssertThrowMsg(m_game_thread == nullptr, "Game thread already initialized!");
    m_game_thread.Reset(new GameThread);

    AssertThrowMsg(m_app_context != nullptr, "No valid Application instance was provided to Game constructor!");
    
    Extent2D window_size;

    if (m_app_context->GetCurrentWindow()) {
        m_input_manager->SetWindow(m_app_context->GetCurrentWindow());

        window_size = m_app_context->GetCurrentWindow()->GetDimensions();
    }

    if (m_managed_game_info.HasValue()) {
        if ((m_managed_assembly = dotnet::DotNetSystem::GetInstance().LoadAssembly(m_managed_game_info->assembly_name.Data()))) {
            if (dotnet::Class *class_ptr = m_managed_assembly->GetClassObjectHolder().FindClassByName(m_managed_game_info->class_name.Data())) {
                m_managed_game_object = class_ptr->NewObject();
            }
        }
    }

    m_scene = CreateObject<Scene>(
        Handle<Camera>(),
        Scene::InitInfo {
            .flags = Scene::InitInfo::SCENE_FLAGS_HAS_TLAS // default it to having a top level acceleration structure for RT
        }
    );

    m_scene->SetCamera(CreateObject<Camera>(
        70.0f,
        window_size.width, window_size.height,
        0.01f, 30000.0f
    ));

    m_scene->SetIsAudioListener(true);

    InitObject(m_scene);
    g_engine->GetWorld()->AddScene(m_scene);

    // Init game thread (calls Init() on game thread)
    m_game_thread->Start(this);

    m_is_init = true;
}

void Game::Update(GameCounter::TickUnit delta)
{
    SystemEvent event;

    Logic(delta);
    
    if (m_ui_stage) {
        m_ui_stage->Update(delta);
    }

    if (m_managed_game_object) {
        m_managed_game_object->InvokeMethodByName<void, float>("Update", float(delta));
    }

    g_engine->GetWorld()->Update(delta);
}

void Game::Init()
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    m_ui_stage->Init();

    if (m_managed_game_object) {
        m_managed_game_object->InvokeMethodByName<void, ManagedHandle, void *, void *, ManagedRefCountedPtr>(
            "BeforeInit",
            CreateManagedHandleFromHandle(m_scene),
            m_input_manager.Get(),
            g_asset_manager,
            CreateManagedRefCountedPtr(m_ui_stage)
        );

        m_managed_game_object->InvokeMethodByName<void>("Init");
    }
}

void Game::Teardown()
{
    if (m_scene) {
        g_engine->GetWorld()->RemoveScene(m_scene->GetID());
        m_scene.Reset();
    }

    m_is_init = false;
}

void Game::RequestStop()
{
    Threads::AssertOnThread(~ThreadName::THREAD_GAME);

    // Stop game thread and wait for it to finish
    if (m_game_thread != nullptr) {
        DebugLog(
            LogType::Debug,
            "Stopping game thread\n"
        );

        m_game_thread->Stop();

        while (m_game_thread->IsRunning()) {
            DebugLog(
                LogType::Debug,
                "Waiting for game thread to stop\n"
            );

            Threads::Sleep(1);
        }

        m_game_thread->Join();
    }

    g_engine->RequestStop();
}

void Game::HandleEvent(SystemEvent &&event)
{
    Threads::AssertOnThread(ThreadName::THREAD_INPUT);

    if (m_input_manager == nullptr) {
        return;
    }

    m_input_manager->CheckEvent(&event);

    OnInputEvent(std::move(event));
}

void Game::PushEvent(SystemEvent &&event)
{
    Threads::AssertOnThread(ThreadName::THREAD_MAIN);

    if (event.GetType() == SystemEventType::EVENT_SHUTDOWN) {
        RequestStop();

        return;
    }

    if (m_game_thread->IsRunning()) {
        m_game_thread->GetScheduler().Enqueue([this, event = std::move(event)](...) mutable
        {
            HandleEvent(std::move(event));
        });
    }
}

void Game::OnInputEvent(const SystemEvent &event)
{
    Threads::AssertOnThread(ThreadName::THREAD_INPUT);
    
    // forward to UI
    if (m_ui_stage->OnInputEvent(m_input_manager.Get(), event) & UIEventHandlerResult::STOP_BUBBLING) {
        // ui handled the event
        return;
    }

    return; // temp

    switch (event.GetType()) {
    case SystemEventType::EVENT_MOUSESCROLL:
    {
        if (m_scene && m_scene->GetCamera()) {
            int wheel_x;
            int wheel_y;

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
            const Vec2i mouse_position = m_input_manager->GetMousePosition();
            const Vec2u window_size = m_input_manager->GetWindow()->GetDimensions();

            const float mx = (float(mouse_position.x) - float(window_size.x) * 0.5f) / (float(window_size.x));
            const float my = (float(mouse_position.y) - float(window_size.y) * 0.5f) / (float(window_size.y));
            
            if (m_scene) {
                if (auto *controller = m_scene->GetCamera()->GetCameraController()) {
                    controller->PushCommand(CameraCommand {
                        .command = CameraCommand::CAMERA_COMMAND_MAG,
                        .mag_data = {
                            .mouse_x    = mouse_position.x,
                            .mouse_y    = mouse_position.y,
                            .mx         = mx,
                            .my         = my
                        }
                    });

                    if (controller->IsMouseLocked()) {
                        m_input_manager->SetMousePosition(Vec2i { int(window_size.x / 2), int(window_size.y / 2) });
                    }
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
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    g_engine->GetRenderState().AdvanceFrameCounter();
    g_engine->GetRenderState().BindScene(m_scene.Get());

    if (m_scene->GetCamera()) {
        g_engine->GetRenderState().BindCamera(m_scene->GetCamera().Get());
    }
}

void Game::OnFrameEnd(Frame *frame)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    if (m_scene->GetCamera()) {
        g_engine->GetRenderState().UnbindCamera();
    }

    g_engine->GetRenderState().UnbindScene();
}

} // namespace hyperion
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <Game.hpp>
#include <GameThread.hpp>

#include <asset/Assets.hpp>

#include <core/threading/Threads.hpp>

#include <core/debug/Debug.hpp>

#include <core/logging/Logger.hpp>

#include <scene/World.hpp>

#include <scene/camera/Camera.hpp>

#include <ui/UISubsystem.hpp>
#include <ui/UIStage.hpp>

#include <rendering/RenderCamera.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/RenderScene.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <dotnet/DotNetSystem.hpp>
#include <dotnet/Class.hpp>
#include <dotnet/Object.hpp>
#include <dotnet/Assembly.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <scripting/ScriptingService.hpp>

#include <system/SystemEvent.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(GameThread);

Game::Game()
    : m_is_init(false),
      m_render_scene(nullptr),
      m_managed_game_object(nullptr)
{
}

Game::Game(Optional<ManagedGameInfo> managed_game_info)
    : m_is_init(false),
      m_managed_game_info(std::move(managed_game_info)),
      m_managed_game_object(nullptr),
      m_render_scene(nullptr)
{
}

Game::~Game()
{
    AssertThrowMsg(
        !m_is_init,
        "Expected Game to have called Teardown() before destructor call");

    delete m_managed_game_object;
}

void Game::Init_Internal()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_main_thread);

    AssertThrowMsg(m_game_thread == nullptr, "Game thread already initialized!");
    m_game_thread = MakeUnique<GameThread>();

    AssertThrowMsg(m_app_context != nullptr, "No valid Application instance was provided to Game constructor!");

    Vec2i window_size;

    if (m_app_context->GetMainWindow())
    {
        window_size = m_app_context->GetMainWindow()->GetDimensions();
    }

    Task<void> future;

    m_game_thread->GetScheduler().Enqueue(
        HYP_STATIC_MESSAGE("Initialize game"),
        [this, window_size, promise = future.Promise()]() -> void
        {
            if (m_managed_game_info.HasValue())
            {
                if (RC<dotnet::Assembly> managed_assembly = dotnet::DotNetSystem::GetInstance().LoadAssembly(m_managed_game_info->assembly_name.Data()))
                {
                    if (RC<dotnet::Class> class_ptr = managed_assembly->FindClassByName(m_managed_game_info->class_name.Data()))
                    {
                        m_managed_game_object = class_ptr->NewObject();
                    }

                    m_managed_assembly = std::move(managed_assembly);
                }
            }

            const Handle<World>& world = g_engine->GetWorld();
            AssertThrow(world.IsValid());
            InitObject(world);

            m_scene = CreateObject<Scene>(SceneFlags::FOREGROUND);
            m_scene->SetName(NAME("Scene_Main"));
            m_scene->SetIsAudioListener(true);

            world->AddScene(m_scene);

            InitObject(m_scene);

            RC<UIStage> ui_stage = MakeRefCountedPtr<UIStage>(g_game_thread);
            m_ui_subsystem = world->AddSubsystem<UISubsystem>(ui_stage);

            // Call Init method (overridden)
            Init();

            promise->Fulfill();
        },
        TaskEnqueueFlags::FIRE_AND_FORGET);

    m_game_thread->Start(this);

    future.Await();

    m_is_init = true;
}

void Game::Update(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    struct RENDER_COMMAND(UpdateGameSceneRenderResource)
        : public renderer::RenderCommand
    {
        Game& game;
        TResourceHandle<RenderScene> render_scene;

        RENDER_COMMAND(UpdateGameSceneRenderResource)(Game& game, const TResourceHandle<RenderScene>& render_scene)
            : game(game),
              render_scene(render_scene)
        {
        }

        virtual ~RENDER_COMMAND(UpdateGameSceneRenderResource)() override = default;

        virtual RendererResult operator()() override
        {
            game.m_render_scene_handle = render_scene;

            HYPERION_RETURN_OK;
        }
    };

    if (m_scene.IsValid() && m_scene->IsReady() && &m_scene->GetRenderResource() != m_render_scene)
    {
        PUSH_RENDER_COMMAND(UpdateGameSceneRenderResource, *this, TResourceHandle<RenderScene>(m_scene->GetRenderResource()));

        m_render_scene = &m_scene->GetRenderResource();
    }
    else if ((!m_scene.IsValid() || !m_scene->IsReady()) && m_render_scene != nullptr)
    {
        PUSH_RENDER_COMMAND(UpdateGameSceneRenderResource, *this, TResourceHandle<RenderScene>());

        m_render_scene = nullptr;
    }

    g_engine->GetScriptingService()->Update();

    Logic(delta);

    if (m_managed_game_object && m_managed_game_object->IsValid())
    {
        m_managed_game_object->InvokeMethodByName<void, float>("Update", float(delta));
    }

    g_engine->GetWorld()->Update(delta);
}

void Game::Init()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread);

    if (m_managed_game_object && m_managed_game_object->IsValid())
    {
        m_managed_game_object->InvokeMethodByName<void>(
            "BeforeInit",
            m_scene,
            m_app_context->GetInputManager(),
            AssetManager::GetInstance(),
            m_ui_subsystem->GetUIStage());

        m_managed_game_object->InvokeMethodByName<void>("Init");
    }
}

void Game::Teardown()
{
    HYP_SCOPE;

    HYP_SYNC_RENDER(); // prevent dangling references to this

    if (m_scene)
    {
        g_engine->GetWorld()->RemoveScene(m_scene);
        m_scene.Reset();
    }

    m_is_init = false;
}

void Game::RequestStop()
{
    HYP_SCOPE;

    Threads::AssertOnThread(~g_game_thread);

    // Stop game thread and wait for it to finish
    if (m_game_thread != nullptr)
    {
        HYP_LOG(GameThread, Debug, "Stopping game thread");

        m_game_thread->Stop();

        while (m_game_thread->IsRunning())
        {
            HYP_LOG(GameThread, Debug, "Waiting for game thread to stop");

            Threads::Sleep(1);
        }

        m_game_thread->Join();
    }

    g_engine->RequestStop();
}

void Game::HandleEvent(SystemEvent&& event)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread);

    if (!m_app_context->GetInputManager().IsValid())
    {
        return;
    }

    m_app_context->GetInputManager()->CheckEvent(&event);

    OnInputEvent(std::move(event));
}

void Game::PushEvent(SystemEvent&& event)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_main_thread);

    if (event.GetType() == SystemEventType::EVENT_SHUTDOWN)
    {
        RequestStop();

        return;
    }

    if (m_game_thread->IsRunning())
    {
        m_game_thread->GetScheduler().Enqueue(
            HYP_STATIC_MESSAGE("HandleEvent"),
            [this, event = std::move(event)]() mutable -> void
            {
                HandleEvent(std::move(event));
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }
}

void Game::OnInputEvent(const SystemEvent& event)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread);

    // forward to UI
    if (m_ui_subsystem->GetUIStage()->OnInputEvent(m_app_context->GetInputManager().Get(), event) & UIEventHandlerResult::STOP_BUBBLING)
    {
        // ui handled the event
        return;
    }

    return; // temp

    switch (event.GetType())
    {
    case SystemEventType::EVENT_MOUSESCROLL:
    {
        if (m_scene.IsValid())
        {
            if (const Handle<Camera>& primary_camera = m_scene->GetPrimaryCamera())
            {
                int wheel_x;
                int wheel_y;

                event.GetMouseWheel(&wheel_x, &wheel_y);

                if (const Handle<CameraController>& controller = primary_camera->GetCameraController())
                {
                    controller->PushCommand(CameraCommand {
                        .command = CameraCommand::CAMERA_COMMAND_SCROLL,
                        .scroll_data = {
                            .wheel_x = wheel_x,
                            .wheel_y = wheel_y } });
                }
            }
        }

        break;
    }
    case SystemEventType::EVENT_MOUSEMOTION:
    {
        if (m_app_context->GetInputManager()->GetWindow()->HasMouseFocus())
        {
            const Vec2i mouse_position = m_app_context->GetInputManager()->GetMousePosition();
            const Vec2i window_size = m_app_context->GetInputManager()->GetWindow()->GetDimensions();

            const float mx = (float(mouse_position.x) - float(window_size.x) * 0.5f) / (float(window_size.x));
            const float my = (float(mouse_position.y) - float(window_size.y) * 0.5f) / (float(window_size.y));

            if (m_scene.IsValid())
            {
                if (const Handle<Camera>& primary_camera = m_scene->GetPrimaryCamera())
                {
                    if (const Handle<CameraController>& controller = primary_camera->GetCameraController())
                    {
                        controller->PushCommand(CameraCommand {
                            .command = CameraCommand::CAMERA_COMMAND_MAG,
                            .mag_data = {
                                .mouse_x = mouse_position.x,
                                .mouse_y = mouse_position.y,
                                .mx = mx,
                                .my = my } });

                        if (controller->IsMouseLockRequested())
                        {
                            m_app_context->GetInputManager()->SetMousePosition(Vec2i { int(window_size.x / 2), int(window_size.y / 2) });
                        }
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

} // namespace hyperion
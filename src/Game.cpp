/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <Game.hpp>
#include <GameThread.hpp>

#include <asset/Assets.hpp>

#include <core/threading/Threads.hpp>

#include <core/debug/Debug.hpp>

#include <core/logging/Logger.hpp>

#include <scene/World.hpp>

#include <scene/camera/Camera.hpp>

#include <rendering/RenderCamera.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/RenderScene.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <dotnet/DotNetSystem.hpp>
#include <dotnet/Class.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <scripting/ScriptingService.hpp>

#include <system/SystemEvent.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(GameThread);

Game::Game()
    : m_is_init(false),
      m_scene_render_resource(nullptr)
{
}

Game::Game(Optional<ManagedGameInfo> managed_game_info)
    : m_is_init(false),
      m_managed_game_info(std::move(managed_game_info)),
      m_scene_render_resource(nullptr)
{
}

Game::~Game()
{
    AssertThrowMsg(
        !m_is_init,
        "Expected Game to have called Teardown() before destructor call"
    );
}

void Game::Init_Internal()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_main_thread);

    AssertThrowMsg(m_game_thread == nullptr, "Game thread already initialized!");
    m_game_thread = MakeUnique<GameThread>();

    AssertThrowMsg(m_app_context != nullptr, "No valid Application instance was provided to Game constructor!");

    Vec2i window_size;

    if (m_app_context->GetMainWindow()) {
        window_size = m_app_context->GetMainWindow()->GetDimensions();
    }

    m_game_thread->GetScheduler().Enqueue(
        HYP_STATIC_MESSAGE("Initialize game"),
        [this, window_size]() -> void
        {
            if (m_managed_game_info.HasValue()) {
                if ((m_managed_assembly = dotnet::DotNetSystem::GetInstance().LoadAssembly(m_managed_game_info->assembly_name.Data()))) {
                    if (dotnet::Class *class_ptr = m_managed_assembly->GetClassObjectHolder().FindClassByName(m_managed_game_info->class_name.Data())) {
                        m_managed_game_object = class_ptr->NewObject();
                    }
                }
            }

            Handle<Camera> camera = CreateObject<Camera>(
                70.0f,
                window_size.x, window_size.y,
                0.01f, 30000.0f
            );

            InitObject(camera);

            m_scene = CreateObject<Scene>(
                SceneFlags::FOREGROUND | SceneFlags::HAS_TLAS // default it to having a top level acceleration structure for RT
            );

            m_scene->SetName(NAME("Scene_Main"));
            m_scene->SetCamera(camera);

            m_scene->SetIsAudioListener(true);

            g_engine->GetWorld()->AddScene(m_scene);
            InitObject(m_scene);
            
            m_ui_stage.Emplace(g_game_thread);

            // Call Init method (overridden)
            Init();
        },
        TaskEnqueueFlags::FIRE_AND_FORGET
    );

    m_game_thread->Start(this);

    m_is_init = true;
}

void Game::Update(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    struct RENDER_COMMAND(UpdateGameSceneRenderResource) : public renderer::RenderCommand
    {
        Game                                    &game;
        TResourceHandle<SceneRenderResource>    render_resource_handle;

        RENDER_COMMAND(UpdateGameSceneRenderResource)(Game &game, const TResourceHandle<SceneRenderResource> &render_resource_handle)
            : game(game),
              render_resource_handle(render_resource_handle)
        {
        }

        virtual ~RENDER_COMMAND(UpdateGameSceneRenderResource)() override = default;

        virtual RendererResult operator()() override
        {
            game.m_scene_render_resource_handle = render_resource_handle;

            HYPERION_RETURN_OK;
        }
    };

    if (m_scene.IsValid() && m_scene->IsReady() && &m_scene->GetRenderResource() != m_scene_render_resource) {
        PUSH_RENDER_COMMAND(UpdateGameSceneRenderResource, *this, TResourceHandle<SceneRenderResource>(m_scene->GetRenderResource()));

        m_scene_render_resource = &m_scene->GetRenderResource();
    } else if ((!m_scene.IsValid() || !m_scene->IsReady()) && m_scene_render_resource != nullptr) {
        PUSH_RENDER_COMMAND(UpdateGameSceneRenderResource, *this, TResourceHandle<SceneRenderResource>());

        m_scene_render_resource = nullptr;
    }

    g_engine->GetScriptingService()->Update();
    
    if (m_ui_stage) {
        m_ui_stage->Update(delta);
    }

    Logic(delta);

    if (m_managed_game_object.IsValid()) {
        m_managed_game_object.InvokeMethodByName<void, float>("Update", float(delta));
    }

    g_engine->GetWorld()->Update(delta);
}

void Game::Init()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread);

    m_ui_stage->Init();

    if (m_managed_game_object.IsValid()) {
        m_managed_game_object.InvokeMethodByName<void>(
            "BeforeInit",
            m_scene,
            m_app_context->GetInputManager(),
            AssetManager::GetInstance(),
            m_ui_stage
        );

        m_managed_game_object.InvokeMethodByName<void>("Init");
    }
}

void Game::Teardown()
{
    HYP_SCOPE;

    HYP_SYNC_RENDER(); // prevent dangling references to this

    if (m_scene) {
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
    if (m_game_thread != nullptr) {
        HYP_LOG(GameThread, Debug, "Stopping game thread");

        m_game_thread->Stop();

        while (m_game_thread->IsRunning()) {
            HYP_LOG(GameThread, Debug, "Waiting for game thread to stop");

            Threads::Sleep(1);
        }

        m_game_thread->Join();
    }

    g_engine->RequestStop();
}

void Game::HandleEvent(SystemEvent &&event)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread);

    if (!m_app_context->GetInputManager().IsValid()) {
        return;
    }

    m_app_context->GetInputManager()->CheckEvent(&event);

    OnInputEvent(std::move(event));
}

void Game::PushEvent(SystemEvent &&event)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_main_thread);

    if (event.GetType() == SystemEventType::EVENT_SHUTDOWN) {
        RequestStop();

        return;
    }

    if (m_game_thread->IsRunning()) {
        m_game_thread->GetScheduler().Enqueue(
            HYP_STATIC_MESSAGE("HandleEvent"),
            [this, event = std::move(event)]() mutable -> void
            {
                HandleEvent(std::move(event));
            },
            TaskEnqueueFlags::FIRE_AND_FORGET
        );
    }
}

void Game::OnInputEvent(const SystemEvent &event)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread);
    
    // forward to UI
    if (m_ui_stage->OnInputEvent(m_app_context->GetInputManager().Get(), event) & UIEventHandlerResult::STOP_BUBBLING) {
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

            if (const RC<CameraController> &controller = m_scene->GetCamera()->GetCameraController()) {
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
        if (m_app_context->GetInputManager()->GetWindow()->HasMouseFocus()) {
            const Vec2i mouse_position = m_app_context->GetInputManager()->GetMousePosition();
            const Vec2i window_size = m_app_context->GetInputManager()->GetWindow()->GetDimensions();

            const float mx = (float(mouse_position.x) - float(window_size.x) * 0.5f) / (float(window_size.x));
            const float my = (float(mouse_position.y) - float(window_size.y) * 0.5f) / (float(window_size.y));
            
            if (m_scene) {
                if (const RC<CameraController> &controller = m_scene->GetCamera()->GetCameraController()) {
                    controller->PushCommand(CameraCommand {
                        .command = CameraCommand::CAMERA_COMMAND_MAG,
                        .mag_data = {
                            .mouse_x    = mouse_position.x,
                            .mouse_y    = mouse_position.y,
                            .mx         = mx,
                            .my         = my
                        }
                    });

                    if (controller->IsMouseLockRequested()) {
                        m_app_context->GetInputManager()->SetMousePosition(Vec2i { int(window_size.x / 2), int(window_size.y / 2) });
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
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    if (!m_scene_render_resource_handle) {
        return;
    }

    SceneRenderResource &scene_render_resource = static_cast<SceneRenderResource &>(*m_scene_render_resource_handle);

    g_engine->GetRenderState()->SetActiveScene(scene_render_resource.GetScene());

    // @FIXME: CameraRenderResource should be held on SceneRenderResource so we don't need to read from the Scene*
    if (scene_render_resource.GetScene()->GetCamera().IsValid()) {
        g_engine->GetRenderState()->BindCamera(scene_render_resource.GetScene()->GetCamera().Get());
    }
}

void Game::OnFrameEnd(Frame *frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    if (!m_scene_render_resource_handle) {
        return;
    }

    SceneRenderResource &scene_render_resource = static_cast<SceneRenderResource &>(*m_scene_render_resource_handle);

    g_engine->GetRenderState()->UnsetActiveScene();

    // @FIXME: CameraRenderResource should be held on SceneRenderResource so we don't need to read from the Scene*
    if (scene_render_resource.GetScene()->GetCamera().IsValid()) {
        g_engine->GetRenderState()->UnbindCamera(scene_render_resource.GetScene()->GetCamera().Get());
    }
}

} // namespace hyperion
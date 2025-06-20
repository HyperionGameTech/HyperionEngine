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
    : m_managed_game_object(nullptr)
{
}

Game::Game(Optional<ManagedGameInfo> managed_game_info)
    : m_managed_game_info(std::move(managed_game_info)),
      m_managed_game_object(nullptr)
{
}

Game::~Game()
{
    delete m_managed_game_object;
}

void Game::Update(float delta)
{
    HYP_SCOPE;

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

    Handle<UIStage> ui_stage = CreateObject<UIStage>(g_game_thread);
    m_ui_subsystem = world->AddSubsystem<UISubsystem>(ui_stage);

    if (m_managed_game_object && m_managed_game_object->IsValid())
    {
        m_managed_game_object->InvokeMethodByName<void>(
            "BeforeInit",
            world,
            m_app_context->GetInputManager(),
            AssetManager::GetInstance(),
            m_ui_subsystem->GetUIStage());

        m_managed_game_object->InvokeMethodByName<void>("Init");
    }
}

void Game::HandleEvent(SystemEvent&& event)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    OnInputEvent(std::move(event));
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

#if 0
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
#endif
}

} // namespace hyperion
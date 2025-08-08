/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <game/Game.hpp>
#include <game/GameThread.hpp>

#include <asset/Assets.hpp>

#include <core/threading/Threads.hpp>

#include <core/debug/Debug.hpp>

#include <core/logging/Logger.hpp>

#include <scene/World.hpp>

#include <scene/camera/Camera.hpp>

#include <ui/UISubsystem.hpp>
#include <ui/UIStage.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <dotnet/DotNetSystem.hpp>
#include <dotnet/Class.hpp>
#include <dotnet/Object.hpp>
#include <dotnet/Assembly.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <scripting/ScriptingService.hpp>

#include <system/SystemEvent.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(GameThread);

Game::Game()
    : m_managedGameObject(nullptr)
{
}

Game::Game(Optional<ManagedGameInfo> managedGameInfo)
    : m_managedGameInfo(std::move(managedGameInfo)),
      m_managedGameObject(nullptr)
{
}

Game::~Game()
{
    delete m_managedGameObject;
}

void Game::Update(float delta)
{
    HYP_SCOPE;
    
    g_engineDriver->SetCurrentWorld(m_world);

    g_engineDriver->GetScriptingService()->Update();

    Logic(delta);

    if (m_managedGameObject && m_managedGameObject->IsValid())
    {
        m_managedGameObject->InvokeMethodByName<void, float>("Update", float(delta));
    }

    m_world->Update(delta);
}

void Game::Init()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    if (m_managedGameInfo.HasValue())
    {
        if (RC<dotnet::Assembly> managedAssembly = dotnet::DotNetSystem::GetInstance().LoadAssembly(m_managedGameInfo->assemblyName.Data()))
        {
            if (RC<dotnet::Class> classPtr = managedAssembly->FindClassByName(m_managedGameInfo->className.Data()))
            {
                m_managedGameObject = classPtr->NewObject();
            }

            m_managedAssembly = std::move(managedAssembly);
        }
    }
    
    m_world = CreateObject<World>();
    InitObject(m_world);

    Handle<UIStage> uiStage = CreateObject<UIStage>(g_gameThread);
    
    m_uiSubsystem = m_world->AddSubsystem(CreateObject<UISubsystem>(uiStage));

    if (m_managedGameObject && m_managedGameObject->IsValid())
    {
        m_managedGameObject->InvokeMethodByName<void>(
            "BeforeInit",
            m_world,
            m_appContext->GetInputManager(),
            AssetManager::GetInstance(),
            m_uiSubsystem->GetUIStage());

        m_managedGameObject->InvokeMethodByName<void>("Init");
    }
}

void Game::HandleEvent(SystemEvent&& event)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    OnInputEvent(std::move(event));
}

void Game::OnInputEvent(const SystemEvent& event)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_gameThread);

    // forward to UI
    if (m_uiSubsystem->GetUIStage()->OnInputEvent(m_appContext->GetInputManager().Get(), event) & UIEventHandlerResult::STOP_BUBBLING)
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
            if (const Handle<Camera>& primaryCamera = m_scene->GetPrimaryCamera())
            {
                int wheelX;
                int wheelY;

                event.GetMouseWheel(&wheelX, &wheelY);

                if (const Handle<CameraController>& controller = primaryCamera->GetCameraController())
                {
                    controller->PushCommand(CameraCommand {
                        .command = CameraCommand::CAMERA_COMMAND_SCROLL,
                        .scrollData = {
                            .wheelX = wheelX,
                            .wheelY = wheelY } });
                }
            }
        }

        break;
    }
    case SystemEventType::EVENT_MOUSEMOTION:
    {
        if (m_appContext->GetInputManager()->GetWindow()->HasMouseFocus())
        {
            const Vec2i mousePosition = m_appContext->GetInputManager()->GetMousePosition();
            const Vec2i windowSize = m_appContext->GetInputManager()->GetWindow()->GetDimensions();

            const float mx = (float(mousePosition.x) - float(windowSize.x) * 0.5f) / (float(windowSize.x));
            const float my = (float(mousePosition.y) - float(windowSize.y) * 0.5f) / (float(windowSize.y));

            if (m_scene.IsValid())
            {
                if (const Handle<Camera>& primaryCamera = m_scene->GetPrimaryCamera())
                {
                    if (const Handle<CameraController>& controller = primaryCamera->GetCameraController())
                    {
                        controller->PushCommand(CameraCommand {
                            .command = CameraCommand::CAMERA_COMMAND_MAG,
                            .magData = {
                                .mouseX = mousePosition.x,
                                .mouseY = mousePosition.y,
                                .mx = mx,
                                .my = my } });

                        if (controller->IsMouseLockRequested())
                        {
                            m_appContext->GetInputManager()->SetMousePosition(Vec2i { int(windowSize.x / 2), int(windowSize.y / 2) });
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

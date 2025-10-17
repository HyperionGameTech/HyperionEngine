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
#include <dotnet/ManagedClass.hpp>
#include <dotnet/ManagedObject.hpp>
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
            if (RC<dotnet::ManagedClass> classPtr = managedAssembly->FindClassByName(m_managedGameInfo->className.Data()))
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
            g_appContext->GetInputManager(),
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

    m_uiSubsystem->GetUIStage()->OnInputEvent(g_appContext->GetInputManager().Get(), event);
}

} // namespace hyperion

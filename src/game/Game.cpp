/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

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
#include <system/AppContext.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#include <Game.generated.inl>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(GameThread);

Game::Game()
{
}

Game::~Game() = default;

void Game::Update(float delta)
{
    HYP_SCOPE;

    g_engineDriver->SetCurrentWorld(m_world);

    g_engineDriver->GetScriptingService()->Update();

    Logic(delta);

    m_world->Update(delta);
}

void Game::Init()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    m_world = CreateObject<World>();
    InitObject(m_world);

    Handle<UIStage> uiStage = CreateObject<UIStage>(g_gameThread);

    m_uiSubsystem = m_world->AddSubsystem(CreateObject<UISubsystem>(uiStage));
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

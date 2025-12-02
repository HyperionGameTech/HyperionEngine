/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <game/Game.hpp>

#include <asset/Assets.hpp>

#include <core/threading/Threads.hpp>

#include <core/debug/Debug.hpp>

#include <core/logging/Logger.hpp>

#include <scene/World.hpp>

#include <scene/camera/Camera.hpp>

#include <ui/UISubsystem.hpp>
#include <ui/UIStage.hpp>

#include <engine/DebugDrawer.hpp>

#include <dotnet/DotNETHost.hpp>
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

static const Name s_nameMainWorld = NAME("MainWorld");

Game::Game()
    : m_isLaunched(false)
{
}

Game::~Game()
{
    if (m_world)
    {
        g_engineDriver->RemoveWorld(m_world);
    }
}

void Game::Init()
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    if (!m_world)
    {
        m_world = CreateObject<World>(s_nameMainWorld, WorldFlags::DEFAULT);
    }

    InitObject(m_world);
    g_engineDriver->AddWorld(m_world);

    Handle<UIStage> uiStage = CreateObject<UIStage>(g_gameThread);

    m_uiSubsystem = m_world->AddSubsystem(CreateObject<UISubsystem>(uiStage));
}

void Game::HandleEvent(SystemEvent&& event)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    OnInputEvent(std::move(event));
}

void Game::OnInputEvent(const SystemEvent& event)
{
    HYP_SCOPE;

    AssertOnThread(g_gameThread);

    m_uiSubsystem->GetUIStage()->OnInputEvent(g_appContext->GetInputManager().Get(), event);
}

} // namespace hyperion

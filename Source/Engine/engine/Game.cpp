/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <engine/Game.hpp>
#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>
#include <rendering/DebugDrawer.hpp>

#include <asset/Assets.hpp>

#include <Core/threading/Threads.hpp>

#include <Core/debug/Debug.hpp>

#include <scene/World.hpp>

#include <scene/camera/Camera.hpp>

#include <ui/UISubsystem.hpp>
#include <ui/UIStage.hpp>

#include <dotnet/DotNETHost.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/Assembly.hpp>

#include <scripting/ScriptingService.hpp>

#include <input/Event.hpp>

#include <Game.generated.inl>

namespace Hyperion {

static const Name s_nameMainWorld = NAME("World");

Game::Game()
    : m_isLaunched(false)
{
}

Game::~Game()
{
    if (m_world)
    {
        m_world->m_gameInstance = nullptr;

        g_engineDriver->RemoveWorld(m_world);
    }
}

void Game::Init()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (!m_world)
    {
        m_world = MakeHandle<World>(s_nameMainWorld, WorldFlags::DEFAULT);
    }

    AssertDebug(m_world->m_gameInstance == nullptr || m_world->m_gameInstance == this);
    m_world->m_gameInstance = this;

    InitObject(m_world);
    g_engineDriver->AddWorld(m_world);

    // Handle<UIStage> uiStage = MakeHandle<UIStage>(g_simThread);

    // m_uiSubsystem = m_world->AddSubsystem(MakeHandle<UISubsystem>(uiStage));
}

void Game::HandleEvent(Event&& event)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    OnInputEvent(event);
}

void Game::OnInputEvent(const Event& event)
{
    HYP_SCOPE;

    AssertOnThread(g_simThread);

    if (UISubsystem* uiSubsystem = m_world->GetSubsystem<UISubsystem>())
    {
        if (uiSubsystem->GetUIStage()->OnInputEvent(event))
        {
            return;
        }
    }
}

void Game::StartSimulating()
{
    HYP_SCOPE;

    if (m_gameState.mode == GameStateMode::SIMULATING)
    {
        return;
    }

    const GameStateMode previousGameStateMode = m_gameState.mode;

    if (previousGameStateMode != GameStateMode::PAUSED)
    {
        m_gameState.gameTime = 0.0f;
        m_gameState.deltaTime = 0.0f;
    }

    m_gameState.mode = GameStateMode::SIMULATING;

    OnGameStateChange(this, previousGameStateMode, GameStateMode::SIMULATING);
}

void Game::StopSimulating()
{
    HYP_SCOPE;

    const GameStateMode previousGameStateMode = m_gameState.mode;

    if (previousGameStateMode == GameStateMode::STOPPED)
    {
        return;
    }

    m_gameState.gameTime = 0.0f;
    m_gameState.deltaTime = 0.0f;
    m_gameState.mode = GameStateMode::STOPPED;

    OnGameStateChange(this, previousGameStateMode, GameStateMode::STOPPED);
}

void Game::PauseSimulation()
{
    HYP_SCOPE;

    if (m_gameState.mode != GameStateMode::SIMULATING)
    {
        return;
    }

    const GameStateMode previousGameStateMode = m_gameState.mode;

    m_gameState.mode = GameStateMode::PAUSED;

    OnGameStateChange(this, previousGameStateMode, GameStateMode::PAUSED);
}

#if HYP_EDITOR

void Game::SetToEditMode()
{
    HYP_SCOPE;

    if (m_gameState.mode == GameStateMode::EDIT_MODE)
    {
        return;
    }

    const GameStateMode previousGameStateMode = m_gameState.mode;

    m_gameState.gameTime = 0.0f;
    m_gameState.deltaTime = 0.0f;
    m_gameState.mode = GameStateMode::EDIT_MODE;

    OnGameStateChange(this, previousGameStateMode, GameStateMode::EDIT_MODE);

    HYP_LOG(Engine, Verbose, "Game set to Edit Mode");
}

#endif

} // namespace Hyperion

/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorSubsystem.hpp>

#include <scene/Scene.hpp>

#include <scene/ecs/EntityManager.hpp>

#include <core/HypClassUtils.hpp>

#include <core/logging/Logger.hpp>

#include <math/MathUtil.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region EditorSubsystem

HYP_DEFINE_CLASS(EditorSubsystem);

EditorSubsystem::EditorSubsystem()
{
}

EditorSubsystem::~EditorSubsystem()
{
}

void EditorSubsystem::Initialize()
{
    HYP_SCOPE;
}

void EditorSubsystem::Shutdown()
{
    HYP_SCOPE;
}

void EditorSubsystem::Update(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_GAME | ThreadName::THREAD_TASK);
}

void EditorSubsystem::OnSceneAttached(const Handle<Scene> &scene)
{
    HYP_SCOPE;
}

void EditorSubsystem::OnSceneDetached(const Handle<Scene> &scene)
{
    HYP_SCOPE;
}

#pragma endregion EditorSubsystem

} // namespace hyperion

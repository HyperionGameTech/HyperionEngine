/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/threading/TaskThread.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/ConditionVariable.hpp>

#include <core/utilities/Span.hpp>

#include <core/memory/allocator/ArenaAllocator.hpp>

#include <scene/EntitySet.hpp>

#include <core/utilities/ClockTimer.hpp>

#include <engine/EngineMemory.hpp>

#include <semaphore>

namespace Hyperion {

class AppContextBase;
class Game;
class Entity;
class View;

struct VisibilityStateComponent;

class VisThread final : public TaskThread
{
public:
    using EntitySetType = EntitySet<VisibilityStateComponent, TagComponent<EntityTag::UpdateVisibility>>;

    VisThread();
    ~VisThread();

    bool Start();
    void Stop() override;

    void AddViewToProcess(View* view);

    void OnFrameStart(uint32 frameCounter);
    void OnFrameEnd(Array<Entity*, SceneTempAllocator>& outProcessedEntities);

    void Process();

private:
    virtual void operator()() override;

    Arena m_tempAllocator;

    Mutex m_mtx;
    ConditionVariable m_cv;

    std::binary_semaphore m_visSemaphore;
    std::binary_semaphore m_simSemaphore;

    Array<View*> m_views;
    Array<Entity*> m_processedEntities;

    uint32 m_frameCounter;
};

} // namespace Hyperion

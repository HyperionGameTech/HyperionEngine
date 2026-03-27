/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/threading/TaskThread.hpp>

#include <Core/threading/Mutex.hpp>
#include <Core/threading/ConditionVariable.hpp>

#include <semaphore>

namespace Hyperion {

class AppContextBase;
class Game;
class Entity;
class View;

struct VisibilityStateComponent;

class TileWorker final : public TaskThread
{
public:
    TileWorker();
    ~TileWorker();

    bool Start();
    void Stop() override;

    void AddViewToProcess(View* view);
    void WaitUntilProcessed(View* view);

    void OnFrameStart(uint32 frameCounter);
    void OnFrameEnd();

    void Process();

private:
    virtual void operator()() override;

    Arena m_tempAllocator;

    Mutex m_mtx;
    ConditionVariable m_cv;

    std::binary_semaphore m_workerSemaphore;
    std::binary_semaphore m_renderSemaphore;

    Array<View*> m_views;

    uint32 m_frameCounter;
};

} // namespace Hyperion

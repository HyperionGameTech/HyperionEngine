/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/threading/TaskThread.hpp>

#include <Core/threading/Mutex.hpp>
#include <Core/threading/ConditionVariable.hpp>

#include <Core/utilities/Span.hpp>

#include <Core/memory/allocator/ArenaAllocator.hpp>

#include <Scene/EntitySet.hpp>

#include <Core/utilities/ClockTimer.hpp>

#include <Framework/EngineMemory.hpp>

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
    void OnFrameEnd(Array<Entity*, SceneAllocator>& outProcessedEntities);

    void Process();

private:
    virtual void operator()() override;

    Arena m_tempAllocator;

    std::binary_semaphore m_visSemaphore;
    std::binary_semaphore m_simSemaphore;

    Array<View*> m_views;
    Array<Entity*> m_processedEntities;

    uint32 m_frameCounter;
};

} // namespace Hyperion

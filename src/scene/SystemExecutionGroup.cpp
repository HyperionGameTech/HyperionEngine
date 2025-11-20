/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <scene/SystemExecutionGroup.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

#define HYP_SYSTEMS_PARALLEL_EXECUTION
// #define HYP_SYSTEMS_LAG_SPIKE_DETECTION
//#define HYP_SYSTEM_LOG_PERFORMANCE

SystemExecutionGroup::SystemExecutionGroup(bool requiresGameThread, bool allowUpdate)
    : m_requiresGameThread(requiresGameThread),
      m_allowUpdate(allowUpdate),
      m_taskBatch(MakeUnique<TaskBatch>())
{
}

SystemExecutionGroup::~SystemExecutionGroup()
{
}

bool SystemExecutionGroup::IsValidForSystem(const SystemBase* systemPtr) const
{
    Assert(systemPtr != nullptr);

    // If the system does not allow update calls, and we don't as well, return true as there will be no overlap.
    if (!AllowUpdate())
    {
        return !systemPtr->AllowUpdate();
    }

    // If the system requires to execute on game thread and the SystemExecutionGroup does not, it is not valid
    // and if the system does not require to execute on game thread and the SystemExecutionGroup does, it is not valid (it could be better parallelized)
    if (systemPtr->RequiresGameThread() != RequiresGameThread())
    {
        return false;
    }

    const Array<TypeId>& componentTypeIds = systemPtr->GetComponentTypeIds();

    for (const auto& it : m_systems)
    {
        const SystemBase* otherSystem = it.second.Get();

        for (TypeId componentTypeId : componentTypeIds)
        {
            const ComponentInfo& componentInfo = systemPtr->GetComponentInfo(componentTypeId);

            if (componentInfo.rwFlags & ComponentRWFlags::WRITE)
            {
                if (otherSystem->HasComponentTypeId(componentTypeId, true))
                {
                    return false;
                }
            }
            else
            {
                // This System is read-only for this component, so it can be processed with other Systems
                if (otherSystem->HasComponentTypeId(componentTypeId, false))
                {
                    return false;
                }
            }
        }
    }

    return true;
}

SystemBase* SystemExecutionGroup::AddSystem(const Handle<SystemBase>& system)
{
    Assert(system.IsValid());
    Assert(IsValidForSystem(system.Get()), "System is not valid for this SystemExecutionGroup");

    const TypeId typeId = GetTypeIdForClass(system->InstanceClass());

    auto it = m_systems.Find(typeId);
    Assert(it == m_systems.End(), "System already exists");

    auto insertResult = m_systems.Set(typeId, system);

    return insertResult.first->second;
}

void SystemExecutionGroup::StartProcessing(float delta, Span<Handle<Scene>> scenes)
{
    HYP_SCOPE;

    AssertDebug(AllowUpdate());

#ifdef HYP_DEBUG_MODE
    m_performanceClock.Start();

    for (auto& it : m_systems)
    {
        SystemBase* system = it.second.Get();

        m_performanceClocks.Set(system, PerformanceClock());
    }
#endif

#if defined(HYP_DEBUG_MODE) && defined(HYP_SYSTEM_LOG_PERFORMANCE)
    HYP_LOG(Entity, Debug, "Starting SystemExecutionGroup processing with {} systems", m_systems.Size());
#endif

    for (auto& it : m_systems)
    {
        SystemBase* system = it.second.Get();

#if defined(HYP_DEBUG_MODE) && defined(HYP_SYSTEM_LOG_PERFORMANCE)
        HYP_LOG(Entity, Debug, "\t\tSystem: {}", system->GetName());
#endif

        m_taskBatch->AddTask([this, system, scenes, delta]
            {
                HYP_NAMED_SCOPE_FMT("Processing system {}", system->GetName());

#ifdef HYP_DEBUG_MODE
                PerformanceClock& performanceClock = m_performanceClocks[system];
                performanceClock.Start();
#endif

                system->Process(delta, scenes);

#ifdef HYP_DEBUG_MODE
                performanceClock.Stop();
#endif
            });
    }
}

void SystemExecutionGroup::FinishProcessing(bool executeBlocking)
{
    AssertDebug(AllowUpdate());

#ifdef HYP_SYSTEMS_PARALLEL_EXECUTION
    if (executeBlocking)
    {
        m_taskBatch->ExecuteBlocking(/* executeDependentBatches */ true);
    }
    else
    {
        m_taskBatch->AwaitCompletion();
    }
#else
    m_taskBatch->ExecuteBlocking(/* executeDependentBatches */ true);
#endif

    for (auto& it : m_systems)
    {
        SystemBase* system = it.second.Get();

        if (system->m_afterProcessProcs.Any())
        {
            for (auto& proc : system->m_afterProcessProcs)
            {
                proc();
            }

            system->m_afterProcessProcs.Clear();
        }
    }

#ifdef HYP_DEBUG_MODE
    m_performanceClock.Stop();
#endif
}

} // namespace hyperion

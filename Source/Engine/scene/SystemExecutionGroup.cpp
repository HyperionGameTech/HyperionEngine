/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ScenePch.hpp>

#include <scene/SystemExecutionGroup.hpp>

#include <Core/threading/TaskSystem.hpp>

namespace Hyperion {

#define HYP_SYSTEMS_PARALLEL_EXECUTION
// #define HYP_SYSTEMS_LAG_SPIKE_DETECTION
// #define HYP_SYSTEM_LOG_PERFORMANCE

SystemExecutionGroup::SystemExecutionGroup(bool requiresSimThread, bool allowUpdate)
    : m_requiresSimThread(requiresSimThread),
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

    // If the system requires to execute on sim thread and the SystemExecutionGroup does not, it is not valid
    // and if the system does not require to execute on sim thread and the SystemExecutionGroup does, it is not valid (it could be better parallelized)
    if (systemPtr->RequiresSimThread() != RequiresSimThread())
    {
        return false;
    }

    const Array<TypeId>& componentTypeIds = systemPtr->GetComponentTypeIds();

    for (const auto& it : m_systems)
    {
        SystemBase* otherSystem = it.second;

        for (TypeId componentTypeId : componentTypeIds)
        {
            const ComponentInfo& componentInfo = systemPtr->GetComponentInfo(componentTypeId);

            if (componentInfo.access & ComponentAccess::WRITE)
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

SystemBase* SystemExecutionGroup::AddSystem(SystemBase* system)
{
    if (!system)
    {
        return nullptr;
    }

    Assert(IsValidForSystem(system), "System is not valid for this SystemExecutionGroup");

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
        SystemBase* system = it.second;

        m_performanceClocks.Set(system, PerformanceClock());
    }
#endif

#if defined(HYP_DEBUG_MODE) && defined(HYP_SYSTEM_LOG_PERFORMANCE)
    HYP_LOG(Entity, Verbose, "Starting SystemExecutionGroup processing with {} systems", m_systems.Size());
#endif

    for (auto& it : m_systems)
    {
        SystemBase* system = it.second;

#if defined(HYP_DEBUG_MODE) && defined(HYP_SYSTEM_LOG_PERFORMANCE)
        HYP_LOG(Entity, Verbose, "\t\tSystem: {}", system->GetName());
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
        SystemBase* system = it.second;

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

} // namespace Hyperion

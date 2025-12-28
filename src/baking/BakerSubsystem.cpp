/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <baking/BakerSubsystem.hpp>
#include <baking/Baker.hpp>

#include <baking/lightmap_volume/LightmapVolumeBaker.hpp>
#include <baking/reflection_probe/ReflectionProbeBaker.hpp>
#include <baking/fog_volume/FogVolumeBaker.hpp>

#include <rendering/RenderConfig.hpp>

#include <scene/EnvProbe.hpp>
#include <scene/FogVolume.hpp>
#include <scene/LightmapVolume.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/math/BoundingBox.hpp>

#include <system/AppContext.hpp>

#include <engine/EngineDriver.hpp>

#include <BakerSubsystem.generated.inl>

namespace Hyperion {

using namespace Baking;

#pragma region BakerSubsystem

BakerSubsystem::BakerSubsystem()
{
}

void BakerSubsystem::OnAddedToWorld()
{
    AssertOnThread(g_simThread);
}

void BakerSubsystem::OnRemovedFromWorld()
{
    AssertOnThread(g_simThread);

    m_bakers.Clear();
}

void BakerSubsystem::Update(float delta)
{
    AssertOnThread(g_simThread);

    HashSet<Task<void>*> erasedTasks; // to ensure we remove pointers after we remove tasks!

    for (auto it = m_tasks.Begin(); it != m_tasks.End();)
    {
        if (it->IsCompleted())
        {
            erasedTasks.Insert(&(*it));
            it = m_tasks.Erase(it);
        }
        else
        {
            ++it;
        }
    }

    Array<ObjectBase*> keysToRemove;

    for (auto& it : m_bakers)
    {
        it.second->Update(delta);

        if (it.second->IsComplete())
        {
            keysToRemove.PushBack(it.first);
        }
    }

    for (ObjectBase* obj : keysToRemove)
    {
        m_bakers.Erase(obj);

        auto activeTasksIt = m_activeTasks.Find(obj);
        AssertDebug(activeTasksIt != m_activeTasks.End());

        if (activeTasksIt != m_activeTasks.End())
        {
            erasedTasks.Erase(activeTasksIt->second);
            m_activeTasks.Erase(activeTasksIt);
        }
    }

    // Ensure no invalid pointers exist in active tasks!
    if (erasedTasks.Any())
    {
        for (Task<void>* taskPtr : erasedTasks)
        {
            for (auto activeIt = m_activeTasks.Begin(); activeIt != m_activeTasks.End();)
            {
                if (activeIt->second == taskPtr)
                {
                    activeIt = m_activeTasks.Erase(activeIt);
                }
                else
                {
                    ++activeIt;
                }
            }
        }
    }
}

template <>
Task<void>* BakerSubsystem::EnqueueBake(const Handle<LightmapVolume>& source)
{
    return EnqueueBake_Internal(source);
}

template <>
Task<void>* BakerSubsystem::EnqueueBake(const Handle<ReflectionProbe>& source)
{
    return EnqueueBake_Internal(source);
}

template <>
Task<void>* BakerSubsystem::EnqueueBake(const Handle<FogVolume>& source)
{
    return EnqueueBake_Internal(source);
}

template <class T, class... Args>
Task<void>* BakerSubsystem::EnqueueBake_Internal(const Handle<T>& source, Args&&... args)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (!source)
    {
        return nullptr;
    }

    auto it = m_bakers.Find(source.Get());

    if (it != m_bakers.End())
    {
        // already running
        auto taskIt = m_activeTasks.Find(source.Get());
        AssertDebug(taskIt != m_activeTasks.End());

        return taskIt->second;
    }

    Handle<BakerBase> lightmapper = CreateObject<Baker<T>>(LightmapperConfig::FromConfig(), source, std::forward<Args>(args)...);
    InitObject(lightmapper);

    Task<void>& task = m_tasks.EmplaceBack();

    lightmapper->OnComplete
        .Bind([promise = task.Promise()]()
            {
                promise->Fulfill();
            })
        .Detach();

    lightmapper->Initialize();

    m_bakers.Insert(source.Get(), std::move(lightmapper));
    m_activeTasks.Insert(source.Get(), &task);

    return &task;
}

#pragma endregion BakerSubsystem

} // namespace Hyperion

/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <lightmapper/LightmapperSubsystem.hpp>
#include <lightmapper/Lightmapper.hpp>
#include <lightmapper/LightmapPathTraceCpu.hpp>
#include <lightmapper/LightmapPathTraceGpu.hpp>
#include <lightmapper/LightmapVolume.hpp>

#include <rendering/RenderConfig.hpp>

#include <scene/EnvProbe.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/math/BoundingBox.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <system/AppContext.hpp>

#include <engine/EngineDriver.hpp>

#include <LightmapperSubsystem.generated.inl>

namespace hyperion {

#pragma region LightmapperSubsystem

LightmapperSubsystem::LightmapperSubsystem()
{
}

void LightmapperSubsystem::OnAddedToWorld()
{
    Threads::AssertOnThread(g_gameThread);
}

void LightmapperSubsystem::OnRemovedFromWorld()
{
    Threads::AssertOnThread(g_gameThread);

    m_lightmappers.Clear();
}

void LightmapperSubsystem::Update(float delta)
{
    Threads::AssertOnThread(g_gameThread);

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

    Array<ObjectBase*> lightmappersToRemove;

    for (auto& it : m_lightmappers)
    {
        it.second->Update(delta);

        if (it.second->IsComplete())
        {
            lightmappersToRemove.PushBack(it.first);
        }
    }

    for (ObjectBase* obj : lightmappersToRemove)
    {
        m_lightmappers.Erase(obj);

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

Task<void>* LightmapperSubsystem::GenerateLightmaps(const Handle<LightmapVolume>& volume)
{
    return GenerateLightmaps_Internal(volume);
}

Task<void>* LightmapperSubsystem::GenerateLightmaps(const Handle<EnvProbe>& envProbe)
{
    return GenerateLightmaps_Internal(envProbe);
}

template <class T, class... Args>
Task<void>* LightmapperSubsystem::GenerateLightmaps_Internal(const Handle<T>& source, Args&&... args)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    if (!source)
    {
        return nullptr;
    }

    auto it = m_lightmappers.Find(source.Get());

    if (it != m_lightmappers.End())
    {
        // already running
        auto taskIt = m_activeTasks.Find(source.Get());
        AssertDebug(taskIt != m_activeTasks.End());

        return taskIt->second;
    }

    Handle<LightmapperBase> lightmapper = CreateObject<Lightmapper<T>>(LightmapperConfig::FromConfig(), source, std::forward<Args>(args)...);
    InitObject(lightmapper);

    Task<void>& task = m_tasks.EmplaceBack();

    lightmapper->OnComplete
        .Bind([promise = task.Promise()]()
            {
                promise->Fulfill();
            })
        .Detach();

    lightmapper->Initialize();

    m_lightmappers.Insert(source.Get(), std::move(lightmapper));
    m_activeTasks.Insert(source.Get(), &task);

    return &task;
}

#pragma endregion LightmapperSubsystem

} // namespace hyperion

/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <ScenePch.hpp>

#include <scene/DetachedScene.hpp>
#include <scene/Scene.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/Thread.hpp>
#include <core/threading/util/ThreadId.hpp>

#include <engine/EngineDriver.hpp>

namespace Hyperion {

class DetachedScenes
{
public:
    DetachedScenes()
    {
        onShutdownHandle = g_engineDriver->GetDelegates().OnShutdown.Bind([this]()
            {
                Mutex::Guard guard(m_mutex);
                m_scenes.Clear();

                onShutdownHandle.Reset();
            });
    }

    Scene* GetDetachedScene(const ThreadId& threadId)
    {
        Mutex::Guard guard(m_mutex);

        auto it = m_scenes.Find(threadId);

        if (it == m_scenes.End())
        {
            it = m_scenes.Insert({ threadId, CreateSceneForThread(threadId) }).first;
        }

        return it->second.Get();
    }

    DelegateHandler onShutdownHandle;

private:
    Handle<Scene> CreateSceneForThread(const ThreadId& threadId)
    {
        Handle<Scene> scene = MakeHandle<Scene>(NAME_FMT("DetachedSceneForThread_{}", threadId.GetName()), threadId, SceneFlags::DETACHED);
        InitObject(scene);

        return scene;
    }

    HashMap<ThreadId, Handle<Scene>> m_scenes;
    Mutex m_mutex;
};

static thread_local Scene* s_sceneForCurrentThread;

static DetachedScenes& GetDetachedScenes()
{
    static DetachedScenes s_detachedScenes;
    return s_detachedScenes;
}

Scene* GetDetachedSceneForCurrentThread()
{
    if (!s_sceneForCurrentThread)
    {
        s_sceneForCurrentThread = GetDetachedScenes().GetDetachedScene(CurrentThreadId());
    }

    return s_sceneForCurrentThread;
}

Scene* GetDetachedSceneForThread(const ThreadId& threadId)
{
    return GetDetachedScenes().GetDetachedScene(threadId);
}

} // namespace Hyperion

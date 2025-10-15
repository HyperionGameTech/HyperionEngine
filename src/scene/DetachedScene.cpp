/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <scene/DetachedScene.hpp>
#include <scene/Scene.hpp>

#include <core/containers/HashMap.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/Thread.hpp>
#include <core/threading/ThreadId.hpp>

namespace hyperion {

class DetachedScenes
{
public:
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

private:
    Handle<Scene> CreateSceneForThread(const ThreadId& threadId)
    {
        Handle<Scene> scene = CreateObject<Scene>(nullptr, threadId, SceneFlags::DETACHED);
        scene->SetName(CreateNameFromDynamicString(ANSIString("DetachedSceneForThread_") + *threadId.GetName()));

        InitObject(scene);

        return scene;
    }

    HashMap<ThreadId, Handle<Scene>> m_scenes;
    Mutex m_mutex;
};

static thread_local Scene* s_pScene;

static DetachedScenes& GetDetachedScenes()
{
    static DetachedScenes s_detachedScenes;
    return s_detachedScenes;
}

Scene* GetDetachedSceneForCurrentThread()
{
    if (!s_pScene)
    {
        s_pScene = GetDetachedScenes().GetDetachedScene(Threads::CurrentThreadId());
    }

    return s_pScene;
}

Scene* GetDetachedSceneForThread(const ThreadId& threadId)
{
    return GetDetachedScenes().GetDetachedScene(threadId);
}

} // namespace hyperion
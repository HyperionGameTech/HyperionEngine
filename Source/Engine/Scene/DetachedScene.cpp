/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/DetachedScene.hpp>
#include <Scene/Scene.hpp>

#include <Core/Threading/Mutex.hpp>
#include <Core/Threading/Thread.hpp>
#include <Core/Threading/AtomicFlag.hpp>

#include <Core/Threading/Util/ThreadId.hpp>

#include <Framework/EngineDriver.hpp>

namespace Hyperion {

class DetachedScenes
{
public:
    DetachedScenes()
    {
        // likely we won't ever need more than this, but just to prevent allocs:
        m_scenes.Reserve(16);
    }

    Scene& GetDetachedScene(const ThreadId& threadId)
    {
        Mutex::Guard guard(m_mutex);

        auto it = m_scenes.Find(threadId);

        if (it == m_scenes.End())
        {
            it = m_scenes.Insert({ threadId, CreateSceneForThread(threadId) }).first;
        }

        return *it->second;
    }

    void DestroyAll()
    {
        Mutex::Guard guard(m_mutex);

        for (auto& pair : m_scenes)
        {
            if (pair.second != nullptr)
            {
                pair.second->Release();
                pair.second = nullptr;
            }
        }
    }

private:
    HYP_NODISCARD Handle<Scene> CreateSceneForThread(const ThreadId& threadId)
    {
        Handle<Scene> scene = MakeHandle<Scene>(NAME_FMT("DetachedSceneForThread_{}", threadId.GetName()), threadId, SceneFlags::DETACHED);
        scene->Initialize();

        return scene;
    }

    Map<ThreadId, Handle<Scene>, DynamicAllocator, HashTablePolicy::NotPooled> m_scenes;
    Mutex m_mutex;
};

// Thread local cached scene.
thread_local Scene* t_detachedScene;

static DetachedScenes& GetDetachedScenes()
{
    static DetachedScenes s_detachedScenes;
    return s_detachedScenes;
}

void DestroyDetachedScenes()
{
    GetDetachedScenes().DestroyAll();
}

Scene& GetDetachedSceneForCurrentThread()
{
    if (!t_detachedScene)
    {
        t_detachedScene = &GetDetachedScenes().GetDetachedScene(CurrentThreadId());
    }

    return *t_detachedScene;
}

Scene& GetDetachedSceneForThread(const ThreadId& threadId)
{
    return GetDetachedScenes().GetDetachedScene(threadId);
}

} // namespace Hyperion

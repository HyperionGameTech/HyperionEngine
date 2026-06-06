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
        m_scenes.Reserve(16);
    }

    Scene*& GetDetachedScene(const ThreadId& threadId)
    {
        Mutex::Guard guard(m_mutex);

        auto it = m_scenes.Find(threadId);

        if (it == m_scenes.End())
        {
            it = m_scenes.Insert({ threadId, CreateSceneForThread(threadId) }).first;
        }

        return it->second;
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
    Scene* CreateSceneForThread(const ThreadId& threadId)
    {
        Scene* scene = new Scene(NAME_FMT("DetachedSceneForThread_{}", threadId.GetName()), threadId, SceneFlags::DETACHED);
        scene->Initialize();

        return scene;
    }

    TMap<ThreadId, Scene*, DynamicAllocator, HashTablePolicy::NotPooled> m_scenes;
    Mutex m_mutex;
};

static thread_local Scene** s_ppDetachedScene;

static DetachedScenes& GetDetachedScenes()
{
    static DetachedScenes s_detachedScenes;
    return s_detachedScenes;
}

void DestroyDetachedScenes()
{
    GetDetachedScenes().DestroyAll();
}

Scene* GetDetachedSceneForCurrentThread()
{
    if (!s_ppDetachedScene)
    {
        s_ppDetachedScene = &GetDetachedScenes().GetDetachedScene(CurrentThreadId());
        Assert(*s_ppDetachedScene != nullptr);
    }

    return *s_ppDetachedScene;
}

Scene* GetDetachedSceneForThread(const ThreadId& threadId)
{
    return GetDetachedScenes().GetDetachedScene(threadId);
}

} // namespace Hyperion

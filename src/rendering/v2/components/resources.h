#ifndef HYPERION_V2_RESOURCES_H
#define HYPERION_V2_RESOURCES_H

#include "containers.h"
#include "shader.h"
#include "framebuffer.h"
#include "compute.h"
#include "graphics.h"
#include "material.h"
#include "texture.h"
#include "texture_atlas.h"
#include "mesh.h"
#include "skeleton.h"
#include "scene.h"

#include <mutex>
#include <thread>

namespace hyperion::v2 {

class Engine;

struct Resources {
    using Callbacks = EngineCallbacks;

    RefCounter<Shader,   Callbacks>         shaders;
    RefCounter<Texture,  Callbacks>         textures;
    RefCounter<Framebuffer, Callbacks>      framebuffers;
    RefCounter<RenderPass, Callbacks>       render_passes;
    RefCounter<Material, Callbacks>         materials;
    ObjectHolder<ComputePipeline>           compute_pipelines{.defer_create = true};
    
    RefCounter<Spatial,  Callbacks>         spatials;
    RefCounter<Mesh,     Callbacks>         meshes;
    RefCounter<Skeleton, Callbacks>         skeletons;

    RefCounter<Scene,    Callbacks>         scenes;

    Resources(Engine *);
    Resources(const Resources &other) = delete;
    Resources &operator=(const Resources &other) = delete;
    ~Resources();

    void Create(Engine *engine);
    void Destroy(Engine *engine);

    /*! \brief Guard the given lambda with a lock/unlock of the resources'
     *  internal mutex for the purposes of asset loading.
     *  @param lambda The lambda function to execute within the guard
     */
    template <class LambdaFunction>
    void Lock(LambdaFunction lambda)
    {
        const auto thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());

        DebugLog(LogType::Debug, "Locking resource mutex in thread %llu\n", thread_id);
        m_mtx.lock();
        DebugLog(LogType::Debug, "Locked resource mutex in thread %llu\n", thread_id);
        
        lambda(*this);

        DebugLog(LogType::Debug, "Unlocking resource mutex in thread %llu\n", thread_id);
        m_mtx.unlock();
        DebugLog(LogType::Debug, "Unlocked resource mutex in thread %llu\n", thread_id);
    }

private:
    std::mutex m_mtx;
};

} // namespace hyperion::v2

#endif
#ifndef HYPERION_V2_RESOURCES_H
#define HYPERION_V2_RESOURCES_H

#include <core/Containers.hpp>
#include "Shader.hpp"
#include "Framebuffer.hpp"
#include "Compute.hpp"
#include "Graphics.hpp"
#include "Material.hpp"
#include "Texture.hpp"
#include "Mesh.hpp"
#include "Light.hpp"
#include <ui/UiScene.hpp>
#include "../animation/Skeleton.hpp"
#include "../scene/Scene.hpp"
#include <rendering/rt/Blas.hpp>
#include <rendering/rt/Tlas.hpp>

#include <mutex>
#include <thread>

namespace hyperion::v2 {

class Engine;

struct Resources {
    using Callbacks = EngineCallbacks;

#define HYP_DEF_REF_COUNTED(class_name, member_name) \
    RefCounter<class_name, Callbacks> member_name

    HYP_DEF_REF_COUNTED(Shader,            shaders);
    HYP_DEF_REF_COUNTED(Texture,           textures);
    HYP_DEF_REF_COUNTED(Framebuffer,       framebuffers);
    HYP_DEF_REF_COUNTED(RenderPass,        render_passes);
    HYP_DEF_REF_COUNTED(Material,          materials);
    HYP_DEF_REF_COUNTED(Light,             lights);
    HYP_DEF_REF_COUNTED(GraphicsPipeline,  graphics_pipelines);
    HYP_DEF_REF_COUNTED(ComputePipeline,   compute_pipelines);
    HYP_DEF_REF_COUNTED(Spatial,           spatials);
    HYP_DEF_REF_COUNTED(Mesh,              meshes);
    HYP_DEF_REF_COUNTED(Skeleton,          skeletons);
    HYP_DEF_REF_COUNTED(Scene,             scenes);
    HYP_DEF_REF_COUNTED(Blas,              blas);
    //HYP_DEF_REF_COUNTED(UIObject,          ui_objects);

#undef HYP_DEF_REF_COUNTED

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
        std::lock_guard guard(m_mtx);

        lambda(*this);
    }

private:
    std::mutex m_mtx;
};

} // namespace hyperion::v2

#endif
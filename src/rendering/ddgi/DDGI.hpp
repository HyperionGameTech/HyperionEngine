#ifndef HYPERION_V2_DDGI_H
#define HYPERION_V2_DDGI_H

#include <Constants.hpp>

#include <rendering/ProbeSystem.hpp>

#include <rendering/RenderComponent.hpp>
#include <rendering/Texture.hpp>
#include <rendering/Renderer.hpp>
#include <rendering/Compute.hpp>
#include <rendering/Framebuffer.hpp>
#include <rendering/Shader.hpp>
#include <rendering/RenderPass.hpp>
#include <scene/Scene.hpp>

#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererFrame.hpp>

#include <rendering/rt/TLAS.hpp>
#include <rendering/rt/BLAS.hpp>

#include <core/lib/FlatMap.hpp>

namespace hyperion::v2 {

using renderer::CommandBuffer;
using renderer::StorageBuffer;
using renderer::Frame;

class Engine;

class DDGI : public EngineComponentBase<STUB_CLASS(DDGI)>, public RenderComponent<DDGI> {
public:
    static constexpr RenderComponentName component_name = RENDER_COMPONENT_DDGI;

    struct Params {
        BoundingBox aabb;
    };

    DDGI(Params &&params);
    DDGI(const DDGI &other) = delete;
    DDGI &operator=(const DDGI &other) = delete;
    ~DDGI();

    void Init(Engine *engine);
    void InitGame(Engine *engine); // init on game thread

    void OnUpdate(Engine *engine, GameCounter::TickUnit delta);
    void OnRender(Engine *engine, Frame *frame);

private:
    void CreateDescriptors(Engine *engine);

    virtual void OnEntityAdded(Ref<Entity> &entity) override;
    virtual void OnEntityRemoved(Ref<Entity> &entity) override;
    virtual void OnEntityRenderableAttributesChanged(Ref<Entity> &entity) override;
    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override;

    Params                m_params;

    ProbeGrid             m_probe_grid;
    TLAS                  m_tlas;

    Ref<Scene>            m_scene;
    Ref<RendererInstance> m_renderer_instance;
};

} // namespace hyperion::v2

#endif
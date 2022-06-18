#ifndef HYPERION_V2_SHADOWS_H
#define HYPERION_V2_SHADOWS_H

#include "base.h"
#include "post_fx.h"
#include "renderer.h"
#include "light.h"
#include "render_component.h"

#include <rendering/backend/renderer_frame.h>

#include <math/bounding_box.h>
#include <scene/scene.h>
#include <camera/camera.h>
#include <types.h>

namespace hyperion::v2 {

using renderer::Frame;

class ShadowPass : public FullScreenPass {
public:
    ShadowPass();
    ShadowPass(const ShadowPass &other) = delete;
    ShadowPass &operator=(const ShadowPass &other) = delete;
    ~ShadowPass();

    Scene *GetScene() const                 { return m_scene.ptr; }

    Ref<Light> &GetLight()                  { return m_light; }
    const Ref<Light> &GetLight() const      { return m_light; }
    void SetLight(Ref<Light> &&light)       { m_light = std::move(light); }

    void SetParentScene(Scene::ID id);

    const Vector3 &GetOrigin() const        { return m_origin; }
    void SetOrigin(const Vector3 &origin)   { m_origin = origin; }

    float GetMaxDistance() const            { return m_max_distance; }
    void SetMaxDistance(float max_distance) { m_max_distance = max_distance; }

    BoundingBox GetAabb() const
    {
        return {
            MathUtil::Round(m_origin - m_max_distance * 0.5f),
            MathUtil::Round(m_origin + m_max_distance * 0.5f)
        };
    }

    uint GetShadowMapIndex() const          { return m_shadow_map_index; }
    void SetShadowMapIndex(uint index)      { m_shadow_map_index = index; }

    void CreateShader(Engine *engine);
    void CreateRenderPass(Engine *engine);
    void CreatePipeline(Engine *engine);
    void CreateDescriptors(Engine *engine);
    void Create(Engine *engine);

    void Destroy(Engine *engine);
    void Render(Engine *engine, Frame *frame);

private:
    Ref<Scene>                                               m_scene;
    Ref<Light>                                               m_light;
    std::vector<ObserverRef<Ref<GraphicsPipeline>>>          m_pipeline_observers;
    FlatMap<GraphicsPipeline::ID, ObserverRef<Spatial *>>    m_spatial_observers;
    Scene::ID                                                m_parent_scene_id;
    Vector3                                                  m_origin;
    float                                                    m_max_distance;
    uint                                                     m_shadow_map_index;
};

class ShadowRenderer : public EngineComponentBase<STUB_CLASS(ShadowRenderer)>, public RenderComponent<ShadowRenderer> {
public:
    ShadowRenderer(Ref<Light> &&light);
    ShadowRenderer(Ref<Light> &&light, const Vector3 &origin, float max_distance);
    ShadowRenderer(const ShadowRenderer &other) = delete;
    ShadowRenderer &operator=(const ShadowRenderer &other) = delete;
    virtual ~ShadowRenderer();

    ShadowPass &GetEffect()             { return m_shadow_pass; }
    const ShadowPass &GetEffect() const { return m_shadow_pass; }

    Scene *GetScene() const               { return m_shadow_pass.GetScene(); }

    const Vector3 &GetOrigin() const      { return m_shadow_pass.GetOrigin(); }
    void SetOrigin(const Vector3 &origin) { m_shadow_pass.SetOrigin(origin); }

    void SetParentScene(const Ref<Scene> &parent_scene)
    {
        if (parent_scene != nullptr) {
            m_shadow_pass.SetParentScene(parent_scene->GetId());
        } else {
            m_shadow_pass.SetParentScene(Scene::empty_id);
        }
    }

    void Init(Engine *engine);

    void OnUpdate(Engine *engine, GameCounter::TickUnit delta);
    void OnRender(Engine *engine, Frame *frame);

private:
    void UpdateSceneCamera(Engine *engine);
    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override;

    ShadowPass m_shadow_pass;
};


} // namespace hyperion::v2

#endif

#ifndef HYPERION_V2_SHADOWS_H
#define HYPERION_V2_SHADOWS_H

#include "../Base.hpp"
#include "../PostFX.hpp"
#include "../Renderer.hpp"
#include "../Light.hpp"
#include "../RenderComponent.hpp"

#include <rendering/backend/RendererFrame.hpp>

#include <math/BoundingBox.hpp>
#include <scene/Scene.hpp>
#include <camera/Camera.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

using renderer::Frame;

class ShadowPass : public FullScreenPass {
public:
    ShadowPass();
    ShadowPass(const ShadowPass &other) = delete;
    ShadowPass &operator=(const ShadowPass &other) = delete;
    ~ShadowPass();

    Ref<Scene> &GetScene()                  { return m_scene; }
    const Ref<Scene> &GetScene() const      { return m_scene; }

    Ref<Light> &GetLight()                  { return m_light; }
    const Ref<Light> &GetLight() const      { return m_light; }

    void SetLight(Ref<Light> &&light)
    {
        m_light = std::move(light);

        if (m_light != nullptr) {
            m_light->SetShadowMapIndex(m_shadow_map_index);
        }
    }

    void SetParentScene(Scene::ID id);

    const Vector3 &GetOrigin() const        { return m_origin; }
    void SetOrigin(const Vector3 &origin)   { m_origin = origin; }

    float GetMaxDistance() const            { return m_max_distance; }
    void SetMaxDistance(float max_distance) { m_max_distance = max_distance; }

    BoundingBox GetAABB() const
    {
        return {
            MathUtil::Round(m_origin - m_max_distance * 0.5f),
            MathUtil::Round(m_origin + m_max_distance * 0.5f)
        };
    }

    UInt GetShadowMapIndex() const          { return m_shadow_map_index; }

    void SetShadowMapIndex(UInt index)
    {
        m_shadow_map_index = index;

        if (m_light != nullptr) {
            m_light->SetShadowMapIndex(index);
        }
    }

    void CreateShader(Engine *engine);
    void CreateRenderPass(Engine *engine);
    void CreateRendererInstance(Engine *engine);
    void CreateDescriptors(Engine *engine);
    void Create(Engine *engine);

    void Destroy(Engine *engine);
    void Render(Engine *engine, Frame *frame);

private:
    Ref<Scene>                                               m_scene;
    Ref<Light>                                               m_light;
    Scene::ID                                                m_parent_scene_id;
    Vector3                                                  m_origin;
    float                                                    m_max_distance;
    UInt                                                     m_shadow_map_index;
    Extent2D                                                 m_dimensions;
};

class ShadowRenderer : public EngineComponentBase<STUB_CLASS(ShadowRenderer)>, public RenderComponent<ShadowRenderer> {
public:
    static constexpr RenderComponentName component_name = RENDER_COMPONENT_SHADOWS;

    ShadowRenderer(Ref<Light> &&light);
    ShadowRenderer(Ref<Light> &&light, const Vector3 &origin, float max_distance);
    ShadowRenderer(const ShadowRenderer &other) = delete;
    ShadowRenderer &operator=(const ShadowRenderer &other) = delete;
    virtual ~ShadowRenderer();

    ShadowPass &GetEffect()               { return m_shadow_pass; }
    const ShadowPass &GetEffect() const   { return m_shadow_pass; }

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

    void Init(Engine *engine);     // init on render thread
    void InitGame(Engine *engine); // init on game thread

    void OnUpdate(Engine *engine, GameCounter::TickUnit delta);
    void OnRender(Engine *engine, Frame *frame);

private:
    void UpdateSceneCamera(Engine *engine);

    virtual void OnEntityAdded(Ref<Entity> &entity) override;
    virtual void OnEntityRemoved(Ref<Entity> &entity) override;
    virtual void OnEntityRenderableAttributesChanged(Ref<Entity> &entity) override;
    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override;

    ShadowPass m_shadow_pass;
};


} // namespace hyperion::v2

#endif

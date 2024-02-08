#ifndef HYPERION_V2_DIRECTIONAL_LIGHT_SHADOW_RENDERER_HPP
#define HYPERION_V2_DIRECTIONAL_LIGHT_SHADOW_RENDERER_HPP

#include <scene/Controller.hpp>

#include <rendering/FullScreenPass.hpp>
#include <core/Base.hpp>
#include <rendering/PostFX.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/Light.hpp>
#include <rendering/RenderComponent.hpp>
#include <rendering/EntityDrawCollection.hpp>
#include <rendering/Compute.hpp>

#include <rendering/backend/RendererFrame.hpp>

#include <math/BoundingBox.hpp>
#include <scene/Scene.hpp>
#include <scene/camera/Camera.hpp>
#include <Types.hpp>

#include <mutex>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Image;
using renderer::ImageView;
using renderer::DescriptorSet;

enum class ShadowMode
{
    STANDARD,
    PCF,
    CONTACT_HARDENED,
    VSM
};

struct ShadowMapCameraData
{
    Matrix4 view;
    Matrix4 projection;
    BoundingBox aabb;
};

struct RenderCommand_CreateShadowMapDescriptors;

class ShadowPass : public FullScreenPass
{
    friend struct RenderCommand_CreateShadowMapDescriptors;

public:
    ShadowPass(const Handle<Scene> &parent_scene, Extent2D resolution);
    ShadowPass(const ShadowPass &other) = delete;
    ShadowPass &operator=(const ShadowPass &other) = delete;
    virtual ~ShadowPass();

    const Handle<Camera> &GetCamera() const { return m_camera; }

    const Handle<Light> &GetLight() const { return m_light; }

    RenderList &GetRenderList() { return m_render_list; }
    const RenderList &GetRenderList() const { return m_render_list; }

    void SetLight(const Handle<Light> &light)
    {
        m_light = light;

        if (m_light.IsValid()) {
            m_light->SetShadowMapIndex(m_shadow_map_index);
        }
    }

    ShadowMode GetShadowMode() const { return m_shadow_mode; }
    void SetShadowMode(ShadowMode shadow_mode) { m_shadow_mode = shadow_mode; }

    const Vector3 &GetOrigin() const { return m_origin; }
    void SetOrigin(const Vector3 &origin) { m_origin = origin; }

    Extent2D GetResolution() const
        { return m_resolution; }

    UInt GetShadowMapIndex() const
        { return m_shadow_map_index; }

    void SetShadowMapIndex(UInt index)
    {
        m_shadow_map_index = index;

        if (m_light.IsValid()) {
            m_light->SetShadowMapIndex(index);
        }
    }

    const Handle<Texture> &GetShadowMap() const
        { return m_shadow_map; }

    void CreateShader();
    virtual void CreateFramebuffer() override;
    virtual void CreateDescriptors() override;

    virtual void Create() override;
    virtual void Destroy() override;
    virtual void Render(Frame *frame) override;

private:
    void CreateShadowMap();
    void CreateComputePipelines();

    Handle<Scene>                   m_parent_scene;
    Handle<Light>                   m_light;
    ShadowMode                      m_shadow_mode;
    Handle<Camera>                  m_camera;
    RenderList                      m_render_list;
    Vector3                         m_origin;
    UInt                            m_shadow_map_index;
    Extent2D                        m_resolution;

    Handle<Texture>                 m_shadow_map;

    Handle<ComputePipeline>         m_blur_shadow_map;
    FixedArray<DescriptorSetRef, 2> m_blur_descriptor_sets;
};

class DirectionalLightShadowRenderer : public RenderComponent<DirectionalLightShadowRenderer>
{
public:
    DirectionalLightShadowRenderer(Name name, Extent2D resolution);
    DirectionalLightShadowRenderer(const DirectionalLightShadowRenderer &other) = delete;
    DirectionalLightShadowRenderer &operator=(const DirectionalLightShadowRenderer &other) = delete;
    virtual ~DirectionalLightShadowRenderer();

    ShadowPass *GetPass() const { return m_shadow_pass.Get(); }

    void SetCameraData(const ShadowMapCameraData &camera_data);

    void Init();     // init on render thread
    void InitGame(); // init on game thread

    void OnUpdate(GameCounter::TickUnit delta);
    void OnRender(Frame *frame);

private:
    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override;

    UniquePtr<ShadowPass>   m_shadow_pass;
    Extent2D                m_resolution;
};

} // namespace hyperion::v2

#endif

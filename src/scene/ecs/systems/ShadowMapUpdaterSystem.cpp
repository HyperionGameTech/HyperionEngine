#include <scene/ecs/systems/ShadowMapUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <scene/camera/Camera.hpp>
#include <scene/camera/OrthoCamera.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/EntityDrawCollection.hpp>
#include <math/MathUtil.hpp>
#include <Threads.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

class ShadowMapRenderer
{
public:
    ShadowMapRenderer(Scene *scene, ShadowMapFilter shadow_map_filter)
        : m_scene(scene),
          m_shadow_map_filter(shadow_map_filter)
    {
        AssertThrow(scene != nullptr);

        // Create shader
        auto shader = g_shader_manager->GetOrCreate(
            HYP_NAME(Shadows),
            GetShaderProperties()
        );

        InitObject(shader);

        { // Create pass
            m_pass.Reset(new FullScreenPass(
                shader,
                InternalFormat::RG32F
            ));

            m_pass->Create();
        }

        { // Create camera
            m_camera = CreateObject<Camera>(2048, 2048);
            m_camera->SetCameraController(RC<OrthoCameraController>::Construct());
            m_camera->SetFramebuffer(m_pass->GetFramebuffer());

            InitObject(m_camera);

            m_render_list.SetCamera(m_camera);
        }
    }

    ShadowMapRenderer(const ShadowMapRenderer &other)               = delete;
    ShadowMapRenderer &operator=(const ShadowMapRenderer &other)    = delete;
    ShadowMapRenderer(ShadowMapRenderer &&other)                    = default;
    ShadowMapRenderer &operator=(ShadowMapRenderer &&other)         = default;

    ~ShadowMapRenderer()
    {
        m_pass->Destroy();
    }

    const Handle<Camera> &GetCamera() const
        { return m_camera; }

    void Render(Frame *frame)
    {
        Threads::AssertOnThread(THREAD_RENDER);

        const ImageRef &shadow_map = m_pass->GetFramebuffer()->GetAttachmentUsages()[0]->GetAttachment()->GetImage();

        g_engine->GetRenderState().BindScene(m_scene);

        {
            m_render_list.CollectDrawCalls(
                frame,
                Bitset((1 << BUCKET_OPAQUE)),
                nullptr
            );

            m_render_list.ExecuteDrawCalls(
                frame,
                Bitset((1 << BUCKET_OPAQUE)),
                nullptr
            );
        }

        g_engine->GetRenderState().UnbindScene();
    }

private:
    ShaderProperties GetShaderProperties() const
    {
        ShaderProperties properties;
        properties.SetRequiredVertexAttributes(renderer::static_mesh_vertex_attributes);

        switch (m_shadow_map_filter) {
        case ShadowMapFilter::SHADOW_MAP_FILTER_VSM:
            properties.Set("MODE_VSM");
            break;
        case ShadowMapFilter::SHADOW_MAP_FILTER_CONTACT_HARDENED:
            properties.Set("MODE_CONTACT_HARDENED");
            break;
        case ShadowMapFilter::SHADOW_MAP_FILTER_PCF:
            properties.Set("MODE_PCF");
            break;
        case ShadowMapFilter::SHADOW_MAP_FILTER_NONE: // fallthrough
        default:
            properties.Set("MODE_STANDARD");
            break;
        }

        return properties;
    }

    Scene                       *m_scene;
    ShadowMapFilter             m_shadow_map_filter;
    UniquePtr<FullScreenPass>   m_pass;
    Handle<Camera>              m_camera;
    RenderList                  m_render_list;
};

#pragma region Render commands

struct RENDER_COMMAND(RenderShadowMap) : renderer::RenderCommand
{
    RC<ShadowMapRenderer> shadow_map_renderer;

    RENDER_COMMAND(RenderShadowMap)(RC<ShadowMapRenderer> shadow_map_renderer)
        : shadow_map_renderer(std::move(shadow_map_renderer))
    {
    }

    virtual Result operator()()
    {
        AssertThrow(shadow_map_renderer != nullptr);

        const FrameRef &frame = g_engine->GetGPUInstance()->GetFrameHandler()->GetCurrentFrame();

        shadow_map_renderer->Render(frame);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UpdateShadowMapData) : renderer::RenderCommand
{
    UInt                index;
    ShadowShaderData    data;

    RENDER_COMMAND(UpdateShadowMapData)(
        UInt index,
        ShadowShaderData data
    ) : index(index),
        data(data)
    {
    }

    virtual Result operator()() override
    {
        g_engine->GetRenderData()->shadow_map_data.Set(
            index,
            data
        );

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

void ShadowMapUpdaterSystem::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    UInt index = 0;

    for (auto [entity_id, shadow_map_component, light_component, transform_component] : entity_manager.GetEntitySet<ShadowMapComponent, LightComponent, TransformComponent>()) {
        if (!light_component.light) {
            continue;
        }

        if (!shadow_map_component.shadow_map_renderer) {
            shadow_map_component.shadow_map_renderer.Reset(new ShadowMapRenderer(entity_manager.GetScene(), shadow_map_component.filter));
        }

        // only update shadow map every 10 ticks
        if (shadow_map_component.update_counter++ % 10 != 0) {
            continue;
        }

        const Vec3f &center = transform_component.transform.GetTranslation();

        shadow_map_component.aabb = BoundingBox(center - shadow_map_component.radius, center + shadow_map_component.radius);

        const Vec3f light_direction = light_component.light->GetPosition().Normalized() * -1.0f;

        const Handle<Camera> &shadow_camera = shadow_map_component.shadow_map_renderer->GetCamera();

        if (!shadow_camera) {
            continue;
        }

        shadow_camera->SetTranslation(center + light_direction);
        shadow_camera->SetTarget(center);

        light_component.light->SetShadowMapIndex(index);

        FixedArray<Vec3f, 8> corners = shadow_map_component.aabb.GetCorners();

        for (Vec3f &corner : corners) {
            corner = shadow_camera->GetViewMatrix() * corner;

            shadow_map_component.aabb.max = MathUtil::Max(shadow_map_component.aabb.max, corner);
            shadow_map_component.aabb.min = MathUtil::Min(shadow_map_component.aabb.min, corner);
        }

        shadow_map_component.aabb.max.z = shadow_map_component.radius;
        shadow_map_component.aabb.min.z = -shadow_map_component.radius;

        { // update render data
            ShadowFlags flags = SHADOW_FLAGS_NONE;

            switch (shadow_map_component.filter) {
            case ShadowMapFilter::SHADOW_MAP_FILTER_VSM:
                flags |= SHADOW_FLAGS_VSM;
                break;
            case ShadowMapFilter::SHADOW_MAP_FILTER_CONTACT_HARDENED:
                flags |= SHADOW_FLAGS_CONTACT_HARDENED;
                break;
            case ShadowMapFilter::SHADOW_MAP_FILTER_PCF:
                flags |= SHADOW_FLAGS_PCF;
                break;
            case ShadowMapFilter::SHADOW_MAP_FILTER_NONE: // fallthrough
            default:
                break;
            }

            PUSH_RENDER_COMMAND(
                UpdateShadowMapData,
                index,
                ShadowShaderData {
                    shadow_camera->GetProjectionMatrix(),
                    shadow_camera->GetViewMatrix(),
                    Vector4(shadow_map_component.aabb.max, 0.0f),
                    Vector4(shadow_map_component.aabb.min, 0.0f),
                    Vec2u { 2048, 2048 },
                    flags
                }
            );
        }

        // Push command to render shadow map
        PUSH_RENDER_COMMAND(
            RenderShadowMap,
            shadow_map_component.shadow_map_renderer
        );

        index++;
    }
}

} // namespace hyperion::v2
#include <rendering/render_components/sky/SkydomeRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

#pragma region Render commands

struct RENDER_COMMAND(SetEnvironmentMap) : renderer::RenderCommand
{
    UInt            index;
    Handle<Texture> environment_map;

    RENDER_COMMAND(SetEnvironmentMap)(UInt index, Handle<Texture> environment_map)
        : index(index),
          environment_map(std::move(environment_map))
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            DescriptorSetRef descriptor_set_scene = g_engine->GetGPUInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::scene_buffer_mapping[frame_index]);

            auto *environment_maps_descriptor = descriptor_set_scene
                ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::ENVIRONMENT_MAPS);

            if (environment_map.IsValid()) {
                environment_maps_descriptor->SetElementSRV(index, environment_map->GetImageView());
            } else {
                environment_maps_descriptor->SetElementSRV(index, &g_engine->GetPlaceholderData().GetImageViewCube1x1R8());
            }
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

SkydomeRenderer::SkydomeRenderer(Extent2D dimensions)
    : RenderComponent(60),
      m_dimensions(dimensions)
{
}

void SkydomeRenderer::Init()
{
    m_camera = CreateObject<Camera>(
        90.0f,
        -Int(m_dimensions.width), Int(m_dimensions.height),
        0.1f, 10000.0f
    );

    m_camera->SetViewMatrix(Matrix4::LookAt(Vec3f::UnitZ(), Vec3f::Zero(), Vec3f::UnitY()));
    InitObject(m_camera);

    m_virtual_scene = CreateObject<Scene>(m_camera);
    InitObject(m_virtual_scene);

    m_env_probe = CreateObject<EnvProbe>(
        m_virtual_scene,
        BoundingBox(Vec3f(-1000.0f), Vec3f(1000.0f)),
        m_dimensions,
        ENV_PROBE_TYPE_REFLECTION,
        g_shader_manager->GetOrCreate({
            HYP_NAME(RenderToCubemap_Skydome),
            ShaderProperties(renderer::static_mesh_vertex_attributes)
        })
    );
    
    InitObject(m_env_probe);
    m_env_probe->EnqueueBind();

    m_cubemap = m_env_probe->GetTexture();

    if (GetComponentIndex() < max_bound_environment_maps) {
        PUSH_RENDER_COMMAND(
            SetEnvironmentMap,
            GetComponentIndex(),
            m_cubemap
        );
    }
}

void SkydomeRenderer::InitGame()
{
    auto dome_node = g_asset_manager->Load<Node>("models/inv_sphere.obj");
    m_dome = dome_node[0].GetEntity();

    if (m_dome) {
        m_dome->SetScale(Vec3f(10.0f));
        m_virtual_scene->AddEntity(m_dome);
    }
}

void SkydomeRenderer::OnRemoved()
{
    if (GetComponentIndex() < max_bound_environment_maps) {
        PUSH_RENDER_COMMAND(
            SetEnvironmentMap,
            GetComponentIndex(),
            Handle<Texture>::empty
        );
    }

    if (m_env_probe) {
        m_env_probe->EnqueueUnbind();
        m_env_probe.Reset();
    }

    m_virtual_scene.Reset();

    m_dome.Reset();
    m_camera.Reset();
    m_cubemap.Reset();
}

void SkydomeRenderer::OnUpdate(GameCounter::TickUnit delta)
{
    AssertThrow(m_virtual_scene.IsValid());
    AssertThrow(m_env_probe.IsValid());

    m_env_probe->SetNeedsUpdate(true);
    m_env_probe->SetNeedsRender(true);

    m_virtual_scene->Update(delta);
    m_env_probe->Update(delta);
}

void SkydomeRenderer::OnRender(Frame *frame)
{
    AssertThrow(m_env_probe.IsValid());
    m_env_probe->Render(frame);
}

} // namespace hyperion::v2
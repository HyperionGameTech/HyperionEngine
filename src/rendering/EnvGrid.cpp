#include <rendering/EnvGrid.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <scene/controllers/PagingController.hpp>
#include <scene/Scene.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::Image;
using renderer::Result;

static const Extent2D ambient_probe_dimensions = Extent2D { 16, 16 };

EnvGrid::EnvGrid(const BoundingBox &aabb, const Extent3D &density)
    : RenderComponent(),
      m_aabb(aabb),
      m_density(density),
      m_current_probe_index(0)
{
}

EnvGrid::~EnvGrid()
{
}

void EnvGrid::Init()
{
    Handle<Scene> scene(GetParent()->GetScene()->GetID());
    AssertThrow(scene.IsValid());

    const SizeType num_ambient_probes = m_density.Size();
    const Vector3 aabb_extent = m_aabb.GetExtent();

    if (num_ambient_probes != 0) {
        m_ambient_probes.Resize(num_ambient_probes);

        for (SizeType x = 0; x < m_density.width; x++) {
            for (SizeType y = 0; y < m_density.height; y++) {
                for (SizeType z = 0; z < m_density.depth; z++) {
                    const SizeType index = x * m_density.height * m_density.depth
                         + y * m_density.depth
                         + z;

                    AssertThrow(!m_ambient_probes[index].IsValid());

                    const BoundingBox env_probe_aabb(
                        m_aabb.min + (Vector3(Float(x), Float(y), Float(z)) * (aabb_extent / Vector3(m_density))),
                        m_aabb.min + (Vector3(Float(x + 1), Float(y + 1), Float(z + 1)) * (aabb_extent / Vector3(m_density)))
                    );

                    m_ambient_probes[index] = CreateObject<EnvProbe>(
                        scene,
                        env_probe_aabb,
                        ambient_probe_dimensions,
                        ENV_PROBE_TYPE_AMBIENT
                    );

                    InitObject(m_ambient_probes[index]);
                }
            }
        }
    }

    CreateShader();
    CreateFramebuffer();

    {
        Memory::Set(m_shader_data.probe_indices, 0, sizeof(m_shader_data.probe_indices));
        m_shader_data.center = Vector4(m_aabb.GetCenter(), 1.0f);
        m_shader_data.extent = Vector4(m_aabb.GetExtent(), 1.0f);
        m_shader_data.density = { m_density.width, m_density.height, m_density.depth, 0 };
        m_shader_data.enabled_indices_mask = 0u;
    }

    {
        m_camera = CreateObject<Camera>(
            90.0f,
            -Int(ambient_probe_dimensions.width), Int(ambient_probe_dimensions.height),
            0.15f, (m_aabb * (Vector3::one / Vector3(m_density))).GetRadius() + 0.15f//(Vector3(m_aabb.GetRadius()) / Vector3(m_density)).Max()////
        );

        m_camera->SetFramebuffer(m_framebuffer);
        InitObject(m_camera);

        m_render_list.SetCamera(m_camera);
    }

    DebugLog(LogType::Info, "Created %llu total ambient EnvProbes in grid\n", num_ambient_probes);
}

// called from game thread
void EnvGrid::InitGame()
{
    Threads::AssertOnThread(THREAD_GAME);
}

void EnvGrid::OnRemoved()
{
    m_camera.Reset();
    m_render_list.Reset();
    m_ambient_shader.Reset();

    struct RENDER_COMMAND(DestroyEnvGridFramebufferAttachments) : RenderCommand
    {
        EnvGrid &env_grid;

        RENDER_COMMAND(DestroyEnvGridFramebufferAttachments)(EnvGrid &env_grid)
            : env_grid(env_grid)
        {
        }

        virtual Result operator()()
        {
            auto result = Result::OK;

            if (env_grid.m_framebuffer != nullptr) {
                for (auto &attachment : env_grid.m_attachments) {
                    env_grid.m_framebuffer->RemoveAttachmentUsage(attachment.get());
                }
            }

            for (auto &attachment : env_grid.m_attachments) {
                HYPERION_PASS_ERRORS(attachment->Destroy(Engine::Get()->GetGPUInstance()->GetDevice()), result);
            }

            env_grid.m_attachments.clear();

            return result;
        }
    };

    PUSH_RENDER_COMMAND(DestroyEnvGridFramebufferAttachments, *this);
}

void EnvGrid::OnUpdate(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertThrow(m_camera.IsValid());

    m_camera->Update(delta);

    GetParent()->GetScene()->CollectEntities(
        m_render_list,
        m_camera,
        Bitset((1 << BUCKET_OPAQUE) | (1 << BUCKET_TRANSLUCENT)),
        RenderableAttributeSet(
            MeshAttributes { },
            MaterialAttributes {
                .bucket = BUCKET_INTERNAL,
                .cull_faces = FaceCullMode::BACK
            },
            m_ambient_shader->GetCompiledShader().GetDefinition(),
            Entity::InitInfo::ENTITY_FLAGS_INCLUDE_IN_INDIRECT_LIGHTING // override flags -- require this flag to be set
        ),
        true // skip frustum culling
    );

    m_render_list.UpdateRenderGroups();

    for (auto &env_probe : m_ambient_probes) {
        // TODO: Just update render data in Render()
        env_probe->Update(delta);
    }
}

void EnvGrid::OnRender(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
    const UInt num_ambient_probes = UInt(m_ambient_probes.Size());

    m_shader_data.enabled_indices_mask = 0u;
    
    if (num_ambient_probes != 0) {
        AssertThrow(m_current_probe_index < m_ambient_probes.Size());

        // Find a probe that needs to be updated
        UInt found_index = UInt(-1);

        for (UInt offset = 0; offset < m_ambient_probes.Size(); ++offset) {
            const UInt index = (offset + m_current_probe_index) % m_ambient_probes.Size();

            if (const Handle<EnvProbe> &env_probe = m_ambient_probes[index]) {
                if (env_probe->NeedsUpdate()) {
                    found_index = index;

                    break;
                }
            }
        }

        if (found_index != UInt(-1)) {
            auto &env_probe = m_ambient_probes[found_index];

            const BoundingBox &probe_aabb = env_probe->GetAABB();
            const Vector3 probe_center = probe_aabb.GetCenter();

            const Vector3 diff = probe_center - m_aabb.GetCenter();//probe_center - camera_position;

            const Vector3 probe_position_clamped = ((diff + (m_aabb.GetExtent() * 0.5f)) / m_aabb.GetExtent());

            const Vector3 probe_diff_units = probe_position_clamped * Vector3(m_density);

            const Int probe_index_at_point = (Int(probe_diff_units.x) * Int(m_density.height) * Int(m_density.depth))
                + (Int(probe_diff_units.y) * Int(m_density.depth))
                + Int(probe_diff_units.z) + 1;

            if (probe_index_at_point > 0 && UInt(probe_index_at_point) < max_bound_ambient_probes) {
                const EnvProbeIndex bound_index(
                    Extent3D { UInt(probe_diff_units.x), UInt(probe_diff_units.y), UInt(probe_diff_units.z) + 1 },
                    m_density
                );

                env_probe->UpdateRenderData(bound_index);

                RenderEnvProbe(frame, env_probe);
    
                env_probe->SetNeedsUpdate(false);
                
                m_shader_data.probe_indices[probe_index_at_point] = env_probe->GetID().ToIndex();
            } else {
                DebugLog(
                    LogType::Warn,
                    "Probe %u out of range of max bound env probes (index: %d, position in units: %f, %f, %f)\n",
                    env_probe->GetID().Value(),
                    probe_index_at_point,
                    probe_diff_units.x,
                    probe_diff_units.y,
                    probe_diff_units.z
                );

                env_probe->SetBoundIndex(EnvProbeIndex());
            }

            m_current_probe_index = (found_index + 1) % num_ambient_probes;
        }
            
        for (UInt index = 0; index < num_ambient_probes; index++) {
            const auto &env_probe = m_ambient_probes[index];
            const EnvProbeIndex &bound_index = env_probe->GetBoundIndex();

            if (bound_index.GetProbeIndex() < max_bound_ambient_probes) {
                m_shader_data.enabled_indices_mask |= (1u << bound_index.GetProbeIndex());
            }
        }
    }
    
    m_shader_data.extent = Vector4(m_aabb.GetExtent(), 1.0f);
    m_shader_data.center = Vector4(m_aabb.GetCenter(), 1.0f);
    m_shader_data.density = { m_density.width, m_density.height, m_density.depth, 0 };

    Engine::Get()->GetRenderData()->env_grids.Set(GetComponentIndex(), m_shader_data);
}

void EnvGrid::OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index /*prev_index*/)
{
    AssertThrowMsg(false, "Not implemented");
}

void EnvGrid::CreateShader()
{
    m_ambient_shader = Engine::Get()->GetShaderManager().GetOrCreate(
        HYP_NAME(CubemapRenderer),
        ShaderProps(renderer::static_mesh_vertex_attributes, { "MODE_AMBIENT" })
    );

    InitObject(m_ambient_shader);
}

void EnvGrid::CreateFramebuffer()
{
    m_framebuffer = CreateObject<Framebuffer>(
        ambient_probe_dimensions,
        RenderPassStage::SHADER,
        renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER,
        6
    );

    m_framebuffer->AddAttachment(
        0,
        RenderObjects::Make<Image>(renderer::FramebufferImageCube(
            ambient_probe_dimensions,
            InternalFormat::RGBA8_SRGB,
            nullptr
        )),
        RenderPassStage::SHADER,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );

    m_framebuffer->AddAttachment(
        1,
        RenderObjects::Make<Image>(renderer::FramebufferImageCube(
            ambient_probe_dimensions,
            Engine::Get()->GetDefaultFormat(TEXTURE_FORMAT_DEFAULT_DEPTH),
            nullptr
        )),
        RenderPassStage::SHADER,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );

    InitObject(m_framebuffer);
}

void EnvGrid::RenderEnvProbe(
    Frame *frame,
    Handle<EnvProbe> &probe
)
{
    auto *command_buffer = frame->GetCommandBuffer();

    auto result = Result::OK;

    {
        struct alignas(128) { UInt32 env_probe_index; } push_constants;
        push_constants.env_probe_index = probe->GetID().ToIndex();

        Engine::Get()->GetRenderState().SetActiveEnvProbe(probe->GetID());
        Engine::Get()->GetRenderState().BindScene(GetParent()->GetScene());

        m_render_list.CollectDrawCalls(
            frame,
            Bitset(1 << BUCKET_INTERNAL),
            nullptr
        );

        m_render_list.ExecuteDrawCalls(
            frame,
            Bitset(1 << BUCKET_INTERNAL),
            nullptr,
            &push_constants
        );

        Engine::Get()->GetRenderState().UnbindScene();
        Engine::Get()->GetRenderState().UnsetActiveEnvProbe();
    }
    
    Image *framebuffer_image = m_framebuffer->GetAttachmentUsages()[0]->GetAttachment()->GetImage();
    ImageView *framebuffer_image_view = m_framebuffer->GetAttachmentUsages()[0]->GetImageView();
    
    if (probe->IsAmbientProbe()) {
        probe->ComputeSH(frame, framebuffer_image, framebuffer_image_view);
    } else {
        AssertThrow(probe->GetTexture().IsValid());

        // copy current framebuffer image to EnvProbe texture, generate mipmap
        Handle<Texture> &current_cubemap_texture = probe->GetTexture();

        framebuffer_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);
        current_cubemap_texture->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);

        HYPERION_PASS_ERRORS(
            current_cubemap_texture->GetImage()->Blit(command_buffer, framebuffer_image),
            result
        );

        HYPERION_PASS_ERRORS(
            current_cubemap_texture->GetImage()->GenerateMipmaps(Engine::Get()->GetGPUDevice(), command_buffer),
            result
        );

        framebuffer_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
        current_cubemap_texture->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);

        HYPERION_ASSERT_RESULT(result);
    }
}

} // namespace hyperion::v2

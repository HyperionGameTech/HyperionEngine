#include <rendering/EnvGrid.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <scene/controllers/PagingController.hpp>
#include <scene/Scene.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::Image;
using renderer::Result;

static const Extent2D ambient_probe_dimensions = Extent2D { 16, 16 };
static constexpr UInt32 invalid_probe_index = ~0u;

#pragma region Render commands

struct RENDER_COMMAND(UpdateProbeOffsets) : RenderCommand
{
    EnvGrid *grid;
    BoundingBox grid_aabb;
    Array<ID<EnvProbe>> ambient_probe_indices;

    RENDER_COMMAND(UpdateProbeOffsets)(
        EnvGrid *grid,
        const BoundingBox &grid_aabb,
        const Array<ID<EnvProbe>> &ambient_probe_indices
    ) : grid(grid),
        grid_aabb(grid_aabb),
        ambient_probe_indices(ambient_probe_indices)
    {
    }

    virtual Result operator()() override
    {
        Array<UInt> new_indices;
        new_indices.Resize(std::size(grid->m_shader_data.probe_indices));
        
        for (SizeType index = 0; index < std::size(grid->m_shader_data.probe_indices); index++) {
            new_indices[index] = invalid_probe_index;
        }

        for (SizeType index = 0; index < ambient_probe_indices.Size(); index++) {
            const ID<EnvProbe> id = ambient_probe_indices[index];

            if (!id) {
                continue;
            }

            AssertThrow(index < grid->m_ambient_probes.Size());

            const EnvProbeIndex current_bound_index = grid->m_ambient_probes[index]->GetBoundIndex();

            if (current_bound_index.GetProbeIndex() == invalid_probe_index) {
                grid->m_ambient_probes[index]->SetNeedsRender(true);

                continue;
            }

            const Vector3 probe_center = Vector3(Engine::Get()->GetRenderData()->env_probes.Get(current_bound_index.GetProbeIndex()).world_position);

            const Vector3 diff = probe_center - grid_aabb.GetCenter();
            const Vector3 probe_position_clamped = ((diff + (grid_aabb.GetExtent() * 0.5f)) / grid_aabb.GetExtent());

            Vector3 probe_diff_units = probe_position_clamped * Vector3(grid->m_density);
            probe_diff_units = MathUtil::Max(probe_diff_units, Vector3::zero);

            EnvProbeIndex offset_index(
                Extent3D { UInt(probe_diff_units.x), UInt(probe_diff_units.y), UInt(probe_diff_units.z) + 1 },
                grid->m_density
            );

            if (offset_index != current_bound_index) {
                grid->m_ambient_probes[index]->SetNeedsRender(true);
            }

            const UInt offset_probe_index = offset_index.GetProbeIndex();

            if (offset_probe_index != invalid_probe_index && offset_probe_index < new_indices.Size()) {
                new_indices[offset_probe_index] = index;
            }
        }

        for (SizeType index = 0; index < new_indices.Size(); index++) {
            grid->m_shader_data.probe_indices[index] = new_indices[index];
        }

        AssertThrow(grid->IsValidComponent());
        Engine::Get()->GetRenderData()->env_grids.Set(grid->GetComponentIndex(), grid->m_shader_data);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

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

void EnvGrid::SetCameraData(const Vector3 &camera_position)
{
    Vector3 size_of_probe = (m_aabb.GetExtent() / Vector3(m_density));
    Vector3 position_snapped = Vector3(
        MathUtil::Floor<Float, Float>(camera_position.x / size_of_probe.x) * size_of_probe.x,
        MathUtil::Floor<Float, Float>(camera_position.y / size_of_probe.y) * size_of_probe.y,
        MathUtil::Floor<Float, Float>(camera_position.z / size_of_probe.z) * size_of_probe.z
    );

    // TODO: Ensure thread safety.
    m_aabb.SetCenter(position_snapped);

    const Vector3 aabb_extent = m_aabb.GetExtent();
    
    if (m_camera) {
        m_camera->SetTranslation(m_aabb.GetCenter());
    }

    Array<ID<EnvProbe>> probe_ids;
    probe_ids.Resize(m_ambient_probes.Size());

    if (m_ambient_probes.Any()) {
        for (SizeType x = 0; x < m_density.width; x++) {
            for (SizeType y = 0; y < m_density.height; y++) {
                for (SizeType z = 0; z < m_density.depth; z++) {
                    const SizeType index = x * m_density.height * m_density.depth
                        + y * m_density.depth
                        + z;

                    AssertThrow(m_ambient_probes[index].IsValid());

                    const BoundingBox env_probe_aabb(
                        m_aabb.min + (Vector3(Float(x), Float(y), Float(z)) * (aabb_extent / Vector3(m_density))),
                        m_aabb.min + (Vector3(Float(x + 1), Float(y + 1), Float(z + 1)) * (aabb_extent / Vector3(m_density)))
                    );

                    probe_ids[index] = m_ambient_probes[index]->GetID();

                    m_ambient_probes[index]->SetAABB(env_probe_aabb);
                    // m_ambient_probes[index]->SetNeedsRender(true);
                }
            }
        }
    }

    PUSH_RENDER_COMMAND(
        UpdateProbeOffsets,
        this,
        m_aabb,
        probe_ids
    );
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
        for (SizeType index = 0; index < std::size(m_shader_data.probe_indices); index++) {
            m_shader_data.probe_indices[index] = invalid_probe_index;
        }

        m_shader_data.center = Vector4(m_aabb.GetCenter(), 1.0f);
        m_shader_data.extent = Vector4(m_aabb.GetExtent(), 1.0f);
        m_shader_data.aabb_max = Vector4(m_aabb.max, 1.0f);
        m_shader_data.aabb_min = Vector4(m_aabb.min, 1.0f);
        m_shader_data.density = { m_density.width, m_density.height, m_density.depth, 0 };
        m_shader_data.enabled_indices_mask = { 0, 0, 0, 0 };
    }

    {
        m_camera = CreateObject<Camera>(
            90.0f,
            -Int(ambient_probe_dimensions.width), Int(ambient_probe_dimensions.height),
            0.15f, (m_aabb * (Vector3::one / Vector3(m_density))).GetRadius() + 0.15f
        );

        m_camera->SetTranslation(m_aabb.GetCenter());

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

    // @TODO: Only collect entities in next probe to render range.

    GetParent()->GetScene()->CollectEntities(
        m_render_list,
        m_camera,
        Bitset((1 << BUCKET_OPAQUE)),
        RenderableAttributeSet(
            MeshAttributes { },
            MaterialAttributes {
                .bucket = BUCKET_INTERNAL,
                .cull_faces = FaceCullMode::FRONT
            },
            m_ambient_shader->GetCompiledShader().GetDefinition(),
            Entity::InitInfo::ENTITY_FLAGS_INCLUDE_IN_INDIRECT_LIGHTING // override flags -- require this flag to be set
        ),
        true // skip frustum culling
    );

    m_render_list.UpdateRenderGroups();

    for (auto &env_probe : m_ambient_probes) {
        env_probe->Update(delta);
    }
}

void EnvGrid::OnRender(Frame *frame)
{
    static constexpr UInt max_queued_probes_for_render = 64;

    Threads::AssertOnThread(THREAD_RENDER);
    const UInt num_ambient_probes = UInt(m_ambient_probes.Size());

    m_shader_data.enabled_indices_mask = { 0, 0, 0, 0 };

    const EnvGridFlags flags = m_flags.Get(MemoryOrder::ACQUIRE);
    EnvGridFlags new_flags = flags;

    if (flags & ENV_GRID_FLAGS_RESET_REQUESTED) {
        for (SizeType index = 0; index < std::size(m_shader_data.probe_indices); index++) {
            m_shader_data.probe_indices[index] = invalid_probe_index;
        }

        new_flags &= ~ENV_GRID_FLAGS_RESET_REQUESTED;
    }

    // Debug draw
    for (const Handle<EnvProbe> &probe : m_ambient_probes) {
        Engine::Get()->GetImmediateMode().Sphere(
            probe->GetDrawProxy().world_position,
            0.15f,
            probe->NeedsUpdate() ? Color(1.0f, 0.0f, 0.0f) : Color(1.0f, 1.0f, 1.0f)
        );
    }
    
    if (num_ambient_probes != 0) {
        while (m_next_render_indices.Any()) {
            const UInt next_index = m_next_render_indices.Front();
            AssertThrow(next_index < m_ambient_probes.Size());

            Handle<EnvProbe> &env_probe = m_ambient_probes[next_index];
            AssertThrow(env_probe.IsValid());

            RenderEnvProbe(frame, env_probe);

            m_next_render_indices.Pop();
        }

        AssertThrow(m_current_probe_index < m_ambient_probes.Size());

        const Vector3 &camera_position = GetParent()->GetScene()->GetCamera()
            ? GetParent()->GetScene()->GetCamera()->GetDrawProxy().position
            : Engine::Get()->GetRenderState().GetCamera().camera.position;

        Array<Pair<UInt, Float>> indices_distances;
        indices_distances.Reserve(16);

        for (UInt offset = 0; offset < m_ambient_probes.Size(); offset++) {
            const UInt index = (offset + m_current_probe_index) % m_ambient_probes.Size();
            if (const Handle<EnvProbe> &env_probe = m_ambient_probes[index]) {
                if (env_probe->NeedsRender()) {
                    indices_distances.PushBack({ index, env_probe->GetDrawProxy().world_position.Distance(camera_position) });

                    break;
                }
            }
        }

        std::sort(indices_distances.Begin(), indices_distances.End(), [](const auto &lhs, const auto &rhs) {
            return lhs.second < rhs.second;
        });

        if (indices_distances.Any()) {
            for (auto &it : indices_distances) {
                if (m_next_render_indices.Size() < max_queued_probes_for_render) {
                    const UInt found_index = it.first;

                    Handle<EnvProbe> &env_probe = m_ambient_probes[found_index];
                
                    const BoundingBox &probe_aabb = env_probe->GetAABB();//env_probe->GetDrawProxy().aabb;
                    const Vector3 probe_center = probe_aabb.GetCenter();

                    const Vector3 diff = probe_center - m_aabb.GetCenter();
                    const Vector3 probe_position_clamped = ((diff + (m_aabb.GetExtent() * 0.5f)) / m_aabb.GetExtent());
                    const Vector3 probe_diff_units = probe_position_clamped * Vector3(m_density);

                    const Int probe_index_at_point = (Int(probe_diff_units.x) * Int(m_density.height) * Int(m_density.depth))
                        + (Int(probe_diff_units.y) * Int(m_density.depth))
                        + Int(probe_diff_units.z) + 1;

                    if (probe_index_at_point > 0 && UInt(probe_index_at_point) < max_bound_ambient_probes) {
                        const EnvProbeIndex calculated_probe_index(
                            Extent3D { UInt(probe_diff_units.x), UInt(probe_diff_units.y), UInt(probe_diff_units.z) + 1 },
                            m_density
                        );

                        env_probe->UpdateRenderData(calculated_probe_index);
                        m_shader_data.probe_indices[calculated_probe_index.GetProbeIndex()] = env_probe->GetID().ToIndex();

                        // render this probe next frame, since the data will have been updated on the gpu on start of the frame
                        m_next_render_indices.Push(found_index);
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

                        // Do not keep trying to render it now.
                        env_probe->SetNeedsRender(false);
                    }
                } else {
                    break;
                }
            }

            m_current_probe_index = (m_current_probe_index + UInt(m_next_render_indices.Size())) % num_ambient_probes;
        } else {
            m_current_probe_index = (m_current_probe_index + 1) % num_ambient_probes;
        }
    }
    
    m_shader_data.extent = Vector4(m_aabb.GetExtent(), 1.0f);
    m_shader_data.center = Vector4(m_aabb.GetCenter(), 1.0f);
    m_shader_data.aabb_max = Vector4(m_aabb.max, 1.0f);
    m_shader_data.aabb_min = Vector4(m_aabb.min, 1.0f);
    m_shader_data.density = { m_density.width, m_density.height, m_density.depth, 0 };

    for (SizeType index = 0; index < m_ambient_probes.Size(); index++) {
        const EnvProbeIndex current_bound_index = m_ambient_probes[index]->GetBoundIndex();

        if (current_bound_index.GetProbeIndex() == invalid_probe_index) {
            continue;
        }

        const Vector3 probe_center = Vector3(Engine::Get()->GetRenderData()->env_probes.Get(current_bound_index.GetProbeIndex()).world_position);

        const Vector3 diff = probe_center - m_aabb.GetCenter();
        const Vector3 probe_position_clamped = ((diff + (m_aabb.GetExtent() * 0.5f)) / m_aabb.GetExtent());

        Vector3 probe_diff_units = probe_position_clamped * Vector3(m_density);
        probe_diff_units = MathUtil::Max(probe_diff_units, Vector3::zero);

        EnvProbeIndex offset_index(
            Extent3D { UInt(probe_diff_units.x), UInt(probe_diff_units.y), UInt(probe_diff_units.z) + 1 },
            m_density
        );

        if (offset_index != current_bound_index) {
            m_ambient_probes[index]->SetNeedsRender(true);
        }

        if (offset_index.GetProbeIndex() < std::size(m_shader_data.probe_indices)) {
            m_shader_data.probe_indices[offset_index.GetProbeIndex()] = m_ambient_probes[index]->GetID().ToIndex();
        }
    }

    Engine::Get()->GetRenderData()->env_grids.Set(GetComponentIndex(), m_shader_data);

    if (flags != new_flags) {
        m_flags.Set(new_flags, MemoryOrder::RELEASE);
    }
}

void EnvGrid::OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index /*prev_index*/)
{
    AssertThrowMsg(false, "Not implemented");
}

void EnvGrid::CreateShader()
{
    m_ambient_shader = Engine::Get()->GetShaderManager().GetOrCreate(
        HYP_NAME(CubemapRenderer),
        ShaderProperties(renderer::static_mesh_vertex_attributes, { "MODE_AMBIENT" })
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
            Bitset((1 << BUCKET_OPAQUE)),
            nullptr
        );

        m_render_list.ExecuteDrawCalls(
            frame,
            Bitset((1 << BUCKET_OPAQUE)),
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

    probe->SetNeedsRender(false);
}

} // namespace hyperion::v2

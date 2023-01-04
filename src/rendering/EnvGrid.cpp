#include <rendering/EnvGrid.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <scene/controllers/PagingController.hpp>
#include <scene/Scene.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::Image;
using renderer::Result;

const Extent2D EnvGrid::reflection_probe_dimensions = Extent2D { 128, 128 };
const Extent2D EnvGrid::ambient_probe_dimensions = Extent2D { 16, 16 };
const Float EnvGrid::overlap_amount = 1.0f;

#pragma region Render commands

struct RENDER_COMMAND(CreateComputeSHClipmapDescriptorSets) : RenderCommand
{
    FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets;

    RENDER_COMMAND(CreateComputeSHClipmapDescriptorSets)(const FixedArray<DescriptorSetRef, max_frames_in_flight> &descriptor_sets)
        : descriptor_sets(descriptor_sets)
    {
    }

    virtual Result operator()()
    {
        for (auto &descriptor_set : descriptor_sets) {
            HYPERION_BUBBLE_ERRORS(descriptor_set->Create(Engine::Get()->GetGPUDevice(), &Engine::Get()->GetGPUInstance()->GetDescriptorPool()));
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

EnvGrid::EnvGrid(const BoundingBox &aabb, const Extent3D &density)
    : RenderComponent(3),
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

    m_reflection_probe = CreateObject<EnvProbe>(
        scene,
        m_aabb,
        reflection_probe_dimensions,
        false
    );

    InitObject(m_reflection_probe);

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
                        m_aabb.min + (Vector3(Float(x), Float(y), Float(z)) * (aabb_extent / Vector3(m_density))) - Vector3(overlap_amount),
                        m_aabb.min + (Vector3(Float(x + 1), Float(y + 1), Float(z + 1)) * (aabb_extent / Vector3(m_density))) + Vector3(overlap_amount)
                    );

                    m_ambient_probes[index] = CreateObject<EnvProbe>(
                        scene,
                        env_probe_aabb,
                        ambient_probe_dimensions,
                        true
                    );

                    InitObject(m_ambient_probes[index]);
                }
            }
        }
    }

    CreateShader();
    CreateFramebuffer();

    CreateClipmapComputeShader();

    {
        Memory::Set(m_shader_data.probe_indices, 0, sizeof(m_shader_data.probe_indices));
        m_shader_data.center = Vector4(m_aabb.GetCenter(), 1.0f);
        m_shader_data.extent = Vector4(m_aabb.GetExtent(), 1.0f);
        m_shader_data.density = { m_density.width, m_density.height, m_density.depth, 0 };
        m_shader_data.enabled_indices_mask = 0u;
    }

    {
        m_reflection_scene = CreateObject<Scene>(CreateObject<Camera>(
            90.0f,
            -Int(reflection_probe_dimensions.width), Int(reflection_probe_dimensions.height),
            0.01f, m_aabb.GetExtent().Max()
        ));

        m_reflection_scene->GetCamera()->SetFramebuffer(m_framebuffer);
        m_reflection_scene->SetName("EnvGrid Reflection Probe Scene");
        m_reflection_scene->SetOverrideRenderableAttributes(RenderableAttributeSet(
            MeshAttributes { },
            MaterialAttributes {
                .bucket = BUCKET_INTERNAL,
                .cull_faces = FaceCullMode::FRONT
            },
            m_reflection_shader->GetID()
        ));
        m_reflection_scene->SetParentScene(Handle<Scene>(GetParent()->GetScene()->GetID()));

        InitObject(m_reflection_scene);
        Engine::Get()->GetWorld()->AddScene(m_reflection_scene);
    }

    {
        m_ambient_scene = CreateObject<Scene>(CreateObject<Camera>(
            90.0f,
            -Int(ambient_probe_dimensions.width), Int(ambient_probe_dimensions.height),
            0.01f, 1250.0f //((aabb_extent / Vector3(m_density)) * 0.5f).Max() + overlap_amount
        ));

        m_ambient_scene->GetCamera()->SetFramebuffer(m_framebuffer);
        m_ambient_scene->SetName("EnvGrid Ambient Probe Scene");
        m_ambient_scene->SetOverrideRenderableAttributes(RenderableAttributeSet(
            MeshAttributes { },
            MaterialAttributes {
                .bucket = BUCKET_INTERNAL,
                .cull_faces = FaceCullMode::FRONT
            },
            m_ambient_shader->GetID()
        ));
        m_ambient_scene->SetParentScene(Handle<Scene>(GetParent()->GetScene()->GetID()));

        InitObject(m_ambient_scene);
        Engine::Get()->GetWorld()->AddScene(m_ambient_scene);
    }

    m_reflection_probe->EnqueueBind();

    for (auto &env_probe : m_ambient_probes) {
        if (InitObject(env_probe)) {
            env_probe->EnqueueBind();
        }
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
    if (m_reflection_probe) {
        m_reflection_probe->EnqueueUnbind();
    }

    for (auto &env_probe : m_ambient_probes) {
        env_probe->EnqueueUnbind();
    }

    Engine::Get()->GetWorld()->RemoveScene(m_reflection_scene->GetID());
    m_reflection_scene.Reset();

    Engine::Get()->GetWorld()->RemoveScene(m_ambient_scene->GetID());
    m_ambient_scene.Reset();

    m_reflection_shader.Reset();
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

    m_reflection_probe->Update();

    for (auto &env_probe : m_ambient_probes) {
        // TODO: Just update render data in Render()
        env_probe->Update();
    }
}

void EnvGrid::OnRender(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
    const UInt num_ambient_probes = UInt(m_ambient_probes.Size());

    m_shader_data.enabled_indices_mask = 0u;

    if (m_reflection_probe) {
        m_reflection_probe->UpdateRenderData(EnvProbeIndex(
            Extent3D { 0, 0, 0 },
            Extent3D { 0, 0, 0 }
        ));

        m_shader_data.probe_indices[0] = m_reflection_probe->GetID().ToIndex();
        m_shader_data.enabled_indices_mask |= (1u << 0u);
    }
    
    if (num_ambient_probes != 0) {
        AssertThrow(m_current_probe_index < m_ambient_probes.Size());

        {
            auto &env_probe = m_ambient_probes[m_current_probe_index];

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

                RenderEnvProbe(frame, m_ambient_scene, env_probe);
                
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

            ComputeClipmaps(frame);

            m_current_probe_index = (m_current_probe_index + 1) % num_ambient_probes;
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

    if (m_reflection_probe) {
        RenderEnvProbe(frame, m_reflection_scene, m_reflection_probe);
    }
}

void EnvGrid::OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index /*prev_index*/)
{
    AssertThrowMsg(false, "Not implemented");
}

void EnvGrid::CreateClipmapComputeShader()
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_compute_clipmaps_descriptor_sets[frame_index] = RenderObjects::Make<DescriptorSet>();

        m_compute_clipmaps_descriptor_sets[frame_index]->AddDescriptor<renderer::ImageDescriptor>(0)
            ->SetElementSRV(0, Engine::Get()->shader_globals->spherical_harmonics_grid.textures[0].image_view)
            ->SetElementSRV(1, Engine::Get()->shader_globals->spherical_harmonics_grid.textures[1].image_view)
            ->SetElementSRV(2, Engine::Get()->shader_globals->spherical_harmonics_grid.textures[2].image_view)
            ->SetElementSRV(3, Engine::Get()->shader_globals->spherical_harmonics_grid.textures[3].image_view)
            ->SetElementSRV(4, Engine::Get()->shader_globals->spherical_harmonics_grid.textures[4].image_view)
            ->SetElementSRV(5, Engine::Get()->shader_globals->spherical_harmonics_grid.textures[5].image_view)
            ->SetElementSRV(6, Engine::Get()->shader_globals->spherical_harmonics_grid.textures[6].image_view)
            ->SetElementSRV(7, Engine::Get()->shader_globals->spherical_harmonics_grid.textures[7].image_view)
            ->SetElementSRV(8, Engine::Get()->shader_globals->spherical_harmonics_grid.textures[8].image_view);

        m_compute_clipmaps_descriptor_sets[frame_index]->AddDescriptor<renderer::SamplerDescriptor>(1)
            ->SetElementSampler(0, &Engine::Get()->GetPlaceholderData().GetSamplerNearest());

        m_compute_clipmaps_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageImageDescriptor>(2)
            ->SetElementUAV(0, Engine::Get()->shader_globals->spherical_harmonics_grid.clipmaps[0].image_view)
            ->SetElementUAV(1, Engine::Get()->shader_globals->spherical_harmonics_grid.clipmaps[1].image_view)
            ->SetElementUAV(2, Engine::Get()->shader_globals->spherical_harmonics_grid.clipmaps[2].image_view)
            ->SetElementUAV(3, Engine::Get()->shader_globals->spherical_harmonics_grid.clipmaps[3].image_view)
            ->SetElementUAV(4, Engine::Get()->shader_globals->spherical_harmonics_grid.clipmaps[4].image_view)
            ->SetElementUAV(5, Engine::Get()->shader_globals->spherical_harmonics_grid.clipmaps[5].image_view)
            ->SetElementUAV(6, Engine::Get()->shader_globals->spherical_harmonics_grid.clipmaps[6].image_view)
            ->SetElementUAV(7, Engine::Get()->shader_globals->spherical_harmonics_grid.clipmaps[7].image_view)
            ->SetElementUAV(8, Engine::Get()->shader_globals->spherical_harmonics_grid.clipmaps[8].image_view);

        // gbuffer textures
        m_compute_clipmaps_descriptor_sets[frame_index]
            ->AddDescriptor<ImageDescriptor>(3)
            ->SetElementSRV(0, Engine::Get()->GetDeferredSystem().Get(BUCKET_OPAQUE).GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetImageView());

        // scene buffer - so we can reconstruct world positions
        m_compute_clipmaps_descriptor_sets[frame_index]
            ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(4)
            ->SetElementBuffer<SceneShaderData>(0, Engine::Get()->shader_globals->scenes.GetBuffer(frame_index).get());

        // env grid buffer (dynamic)
        m_compute_clipmaps_descriptor_sets[frame_index]
            ->AddDescriptor<renderer::DynamicUniformBufferDescriptor>(5)
            ->SetElementBuffer<EnvGridShaderData>(0, Engine::Get()->shader_globals->env_grids.GetBuffer(frame_index).get());

        // env probes buffer
        m_compute_clipmaps_descriptor_sets[frame_index]
            ->AddDescriptor<renderer::StorageBufferDescriptor>(6)
            ->SetElementBuffer(0, Engine::Get()->shader_globals->env_probes.GetBuffer(frame_index).get());
    }

    PUSH_RENDER_COMMAND(CreateComputeSHClipmapDescriptorSets, m_compute_clipmaps_descriptor_sets);

    m_compute_clipmaps = CreateObject<ComputePipeline>(
        CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("ComputeSHClipmap")),
        Array<const DescriptorSet *> { m_compute_clipmaps_descriptor_sets[0].Get() }
    );

    InitObject(m_compute_clipmaps);
}

void EnvGrid::ComputeClipmaps(Frame *frame)
{
    const auto &scene_binding = Engine::Get()->render_state.GetScene();
    const UInt scene_index = scene_binding.id.ToIndex();
    
    const auto &clipmaps = Engine::Get()->shader_globals->spherical_harmonics_grid.clipmaps;

    AssertThrow(clipmaps[0].image.IsValid());
    const Extent3D &clipmap_extent = clipmaps[0].image->GetExtent();

    struct alignas(128) {
        ShaderVec4<UInt32> clipmap_dimensions;
    } push_constants;

    push_constants.clipmap_dimensions = { clipmap_extent[0], clipmap_extent[1], clipmap_extent[2], 0 };

    for (auto &texture : Engine::Get()->shader_globals->spherical_harmonics_grid.clipmaps) {
        texture.image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);
    }

    frame->GetCommandBuffer()->BindDescriptorSet(
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
        m_compute_clipmaps->GetPipeline(),
        m_compute_clipmaps_descriptor_sets[frame->GetFrameIndex()],
        0,
        FixedArray {
            HYP_RENDER_OBJECT_OFFSET(Scene, scene_index),
            HYP_RENDER_OBJECT_OFFSET(EnvGrid, GetComponentIndex())
        }
    );

    m_compute_clipmaps->GetPipeline()->Bind(frame->GetCommandBuffer(), &push_constants, sizeof(push_constants));
    m_compute_clipmaps->GetPipeline()->Dispatch(
        frame->GetCommandBuffer(),
        Extent3D {
            (clipmap_extent[0] + 7) / 8,
            (clipmap_extent[1] + 7) / 8,
            1
        }
    );

    for (auto &texture : Engine::Get()->shader_globals->spherical_harmonics_grid.clipmaps) {
        texture.image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
    }
}

void EnvGrid::CreateShader()
{
    m_reflection_shader = CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader(
        "CubemapRenderer",
        ShaderProps({ "LIGHTING", "SHADOWS", "TONEMAP" })
    ));

    InitObject(m_reflection_shader);

    m_ambient_shader = CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader(
        "CubemapRenderer",
        ShaderProps()
    ));

    InitObject(m_ambient_shader);
}

void EnvGrid::CreateFramebuffer()
{
    m_framebuffer = CreateObject<Framebuffer>(
        reflection_probe_dimensions,
        RenderPassStage::SHADER,
        renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER,
        6
    );

    AttachmentUsage *attachment_usage;

    m_attachments.push_back(std::make_unique<Attachment>(
        std::make_unique<renderer::FramebufferImageCube>(
            reflection_probe_dimensions,
            InternalFormat::RGBA8_SRGB,
            nullptr
        ),
        RenderPassStage::SHADER
    ));

    HYPERION_ASSERT_RESULT(m_attachments.back()->AddAttachmentUsage(
        Engine::Get()->GetGPUInstance()->GetDevice(),
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE,
        &attachment_usage
    ));

    m_framebuffer->AddAttachmentUsage(attachment_usage);

    m_attachments.push_back(std::make_unique<Attachment>(
        std::make_unique<renderer::FramebufferImageCube>(
            reflection_probe_dimensions,
            Engine::Get()->GetDefaultFormat(TEXTURE_FORMAT_DEFAULT_DEPTH),
            nullptr
        ),
        RenderPassStage::SHADER
    ));

    HYPERION_ASSERT_RESULT(m_attachments.back()->AddAttachmentUsage(
        Engine::Get()->GetGPUInstance()->GetDevice(),
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE,
        &attachment_usage
    ));

    m_framebuffer->AddAttachmentUsage(attachment_usage);

    // attachment should be created in render thread?
    for (auto &attachment : m_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(Engine::Get()->GetGPUInstance()->GetDevice()));
    }

    InitObject(m_framebuffer);
}

void EnvGrid::RenderEnvProbe(
    Frame *frame,
    Handle<Scene> &scene,
    Handle<EnvProbe> &probe
)
{
    auto *command_buffer = frame->GetCommandBuffer();

    auto result = Result::OK;
    
    struct alignas(128) { UInt32 env_probe_index; } push_constants;
    push_constants.env_probe_index = probe->GetID().ToIndex();
    
    Image *framebuffer_image = m_framebuffer->GetAttachmentUsages()[0]->GetAttachment()->GetImage();
    ImageView *framebuffer_image_view = m_framebuffer->GetAttachmentUsages()[0]->GetImageView();

    scene->Render(frame, &push_constants, sizeof(push_constants));
    
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

/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/Shadows.hpp>

#include <rendering/backend/RenderingAPI.hpp>
#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererBuffer.hpp>

#include <scene/Texture.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

static const InternalFormat reflection_probe_format = InternalFormat::R10G10B10A2;
static const InternalFormat shadow_probe_format = InternalFormat::RG32F;

static FixedArray<Matrix4, 6> CreateCubemapMatrices(const BoundingBox &aabb, const Vec3f &origin)
{
    FixedArray<Matrix4, 6> view_matrices;

    for (uint32 i = 0; i < 6; i++) {
        view_matrices[i] = Matrix4::LookAt(
            origin,
            origin + Texture::cubemap_directions[i].first,
            Texture::cubemap_directions[i].second
        );
    }

    return view_matrices;
}

#pragma region EnvProbeRenderResource

EnvProbeRenderResource::EnvProbeRenderResource(EnvProbe *env_probe)
    : m_env_probe(env_probe),
      m_buffer_data { },
      m_texture_slot(~0u)
{
    if (!m_env_probe->IsControlledByEnvGrid()) {
        CreateShader();
        CreateFramebuffer();
        CreateTexture();

        m_render_collector.SetOverrideAttributes(RenderableAttributeSet(
            MeshAttributes { },
            MaterialAttributes {
                .shader_definition  = m_shader->GetCompiledShader()->GetDefinition(),
                .blend_function     = BlendFunction::AlphaBlending(),
                .cull_faces         = FaceCullMode::NONE
            }
        ));
    }
}

EnvProbeRenderResource::~EnvProbeRenderResource()
{
    m_scene_resource_handle.Reset();
    m_camera_resource_handle.Reset();

    SafeRelease(std::move(m_shader));
    SafeRelease(std::move(m_framebuffer));
}

void EnvProbeRenderResource::SetTextureSlot(uint32_t texture_slot)
{
    HYP_SCOPE;

    Execute([this, texture_slot]()
    {
        m_texture_slot = texture_slot;

        SetNeedsUpdate();
    });
}

void EnvProbeRenderResource::SetBufferData(const EnvProbeShaderData &buffer_data)
{
    HYP_SCOPE;

    Execute([this, buffer_data]()
    {
        // TEMP hack: save previous texture_index and position_in_grid
        const uint32 texture_index = m_buffer_data.texture_index;
        const Vec4i position_in_grid = m_buffer_data.position_in_grid;

        m_buffer_data = buffer_data;

        // restore previous texture_index and position_in_grid
        m_buffer_data.texture_index = texture_index;
        m_buffer_data.position_in_grid = position_in_grid;

        SetNeedsUpdate();
    });
}

void EnvProbeRenderResource::SetCameraResourceHandle(TResourceHandle<CameraRenderResource> &&camera_resource_handle)
{
    HYP_SCOPE;

    Execute([this, camera_resource_handle = std::move(camera_resource_handle)]()
    {
        if (m_camera_resource_handle) {
            m_camera_resource_handle->SetFramebuffer(nullptr);
        }

        m_camera_resource_handle = std::move(camera_resource_handle);

        if (m_camera_resource_handle) {
            m_camera_resource_handle->SetFramebuffer(m_framebuffer);
        }
    });
}

void EnvProbeRenderResource::SetSceneResourceHandle(TResourceHandle<SceneRenderResource> &&scene_resource_handle)
{
    HYP_SCOPE;

    Execute([this, scene_resource_handle = std::move(scene_resource_handle)]()
    {
        m_scene_resource_handle = std::move(scene_resource_handle);
    });
}

void EnvProbeRenderResource::CreateShader()
{
    if (m_env_probe->IsControlledByEnvGrid()) {
        return;
    }

    if (m_env_probe->IsReflectionProbe()) {
        m_shader = g_shader_manager->GetOrCreate(
            NAME("RenderToCubemap"),
            ShaderProperties(static_mesh_vertex_attributes, { "WRITE_NORMALS", "WRITE_MOMENTS" })
        );
    } else if (m_env_probe->IsSkyProbe()) {
        m_shader = g_shader_manager->GetOrCreate(
            NAME("RenderSky"),
            ShaderProperties(static_mesh_vertex_attributes)
        );
    } else if (m_env_probe->IsShadowProbe()) {
        m_shader = g_shader_manager->GetOrCreate(
            NAME("RenderToCubemap"),
            ShaderProperties(static_mesh_vertex_attributes, { "MODE_SHADOWS" })
        );
    } else {
        HYP_UNREACHABLE();
    }

    AssertThrow(m_shader.IsValid());
}

void EnvProbeRenderResource::CreateFramebuffer()
{
    m_framebuffer = g_rendering_api->MakeFramebuffer(m_env_probe->GetDimensions(), 6);

    InternalFormat format = InternalFormat::NONE;

    if (m_env_probe->IsReflectionProbe() || m_env_probe->IsSkyProbe()) {
        format = reflection_probe_format;
    } else if (m_env_probe->IsShadowProbe()) {
        format = shadow_probe_format;
    }

    AssertThrowMsg(format != InternalFormat::NONE, "Invalid attachment format for EnvProbe");

    AttachmentRef color_attachment = m_framebuffer->AddAttachment(
        0,
        format,
        ImageType::TEXTURE_TYPE_CUBEMAP,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );

    if (m_env_probe->IsShadowProbe()) {
        // For shadows, we want to make sure we clear the color to infinity
        // (the color attachment is used for moments)
        color_attachment->SetClearColor(MathUtil::Infinity<Vec4f>());

        m_framebuffer->AddAttachment(
            1,
            g_rendering_api->GetDefaultFormat(renderer::DefaultImageFormatType::DEPTH),
            ImageType::TEXTURE_TYPE_CUBEMAP,
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE
        );
    } else if (m_env_probe->IsReflectionProbe()) {
        AttachmentRef normals_attachment = m_framebuffer->AddAttachment(
            1,
            InternalFormat::RG16F,
            ImageType::TEXTURE_TYPE_CUBEMAP,
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE
        );

        AttachmentRef moments_attachment = m_framebuffer->AddAttachment(
            2,
            InternalFormat::RG16F,
            ImageType::TEXTURE_TYPE_CUBEMAP,
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE
        );
        moments_attachment->SetClearColor(MathUtil::Infinity<Vec4f>());

        m_framebuffer->AddAttachment(
            3,
            g_rendering_api->GetDefaultFormat(renderer::DefaultImageFormatType::DEPTH),
            ImageType::TEXTURE_TYPE_CUBEMAP,
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE
        );
    }

    DeferCreate(m_framebuffer);
}

void EnvProbeRenderResource::CreateTexture()
{
    if (!m_env_probe->IsControlledByEnvGrid()) {
        if (m_env_probe->IsShadowProbe()) {
            m_texture = CreateObject<Texture>(
                TextureDesc {
                    ImageType::TEXTURE_TYPE_CUBEMAP,
                    shadow_probe_format,
                    Vec3u { m_env_probe->GetDimensions(), 1 },
                    FilterMode::TEXTURE_FILTER_NEAREST,
                    FilterMode::TEXTURE_FILTER_NEAREST
                }
            );
        } else {
            m_texture = CreateObject<Texture>(
                TextureDesc {
                    ImageType::TEXTURE_TYPE_CUBEMAP,
                    reflection_probe_format,
                    Vec3u { m_env_probe->GetDimensions(), 1 },
                    FilterMode::TEXTURE_FILTER_LINEAR,
                    FilterMode::TEXTURE_FILTER_LINEAR
                }
            );
        }

        AssertThrow(InitObject(m_texture));

        m_texture->SetPersistentRenderResourceEnabled(true);
    }
}

void EnvProbeRenderResource::UpdateRenderData(bool set_texture)
{
    Threads::AssertOnThread(g_render_thread);

    AssertThrow(m_bound_index.GetProbeIndex() != ~0u);

    if (m_env_probe->IsControlledByEnvGrid()) {
        AssertThrow(m_env_probe->m_grid_slot != ~0u);
    }

    const uint32 texture_slot = m_env_probe->IsControlledByEnvGrid() ? ~0u :m_bound_index.GetProbeIndex();

    UpdateRenderData(
        texture_slot,
        m_env_probe->IsControlledByEnvGrid() ? m_env_probe->m_grid_slot : ~0u,
        m_env_probe->IsControlledByEnvGrid() ? m_bound_index.grid_size : Vec3u { }
    );

    // update cubemap texture in array of bound env probes
    if (set_texture) {
        AssertThrow(texture_slot != ~0u);

        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            if (m_env_probe->IsShadowProbe()) {
                AssertThrow(m_texture.IsValid());

                g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Scene"), frame_index)
                    ->SetElement(NAME("PointLightShadowMapTextures"), texture_slot, m_texture->GetRenderResource().GetImageView());
            } else if (ShouldComputePrefilteredEnvMap()) {
                AssertThrow(m_prefiltered_image.IsValid());
                AssertThrow(m_prefiltered_image_view.IsValid());

                g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Scene"), frame_index)
                    ->SetElement(NAME("EnvProbeTextures"), texture_slot, m_prefiltered_image_view);
            } else {
                HYP_UNREACHABLE();
            }
        }
    }
}

void EnvProbeRenderResource::UpdateRenderData(uint32 texture_slot, uint32 grid_slot, const Vec3u &grid_size)
{
    HYP_SCOPE;

    Execute([this, texture_slot, grid_slot, grid_size]()
    {
        m_buffer_data.position_in_grid = grid_slot != ~0u
            ? Vec4i {
                int32(grid_slot % grid_size.x),
                int32((grid_slot % (grid_size.x * grid_size.y)) / grid_size.x),
                int32(grid_slot / (grid_size.x * grid_size.y)),
                int32(grid_slot)
            }
            : Vec4i::Zero();

        m_buffer_data.texture_index = texture_slot;

        SetNeedsUpdate();
    });
}

void EnvProbeRenderResource::Initialize_Internal()
{
    HYP_SCOPE;

    if (ShouldComputePrefilteredEnvMap()) {
        if (!m_prefiltered_image) {
            m_prefiltered_image = g_rendering_api->MakeImage(TextureDesc {
                ImageType::TEXTURE_TYPE_2D,
                InternalFormat::RGBA16F,
                Vec3u { 1024, 1024, 1 },
                FilterMode::TEXTURE_FILTER_LINEAR,
                FilterMode::TEXTURE_FILTER_LINEAR,
                WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
                1,
                ImageFormatCapabilities::STORAGE | ImageFormatCapabilities::SAMPLED
            });
            HYPERION_ASSERT_RESULT(m_prefiltered_image->Create());
        }

        if (!m_prefiltered_image_view) {
            m_prefiltered_image_view = g_rendering_api->MakeImageView();
            HYPERION_ASSERT_RESULT(m_prefiltered_image_view->Create(m_prefiltered_image, 0, 1, 0, 1));
        }
    }

    if (ShouldComputeIrradianceMap()) {
        if (!m_irradiance_image) {
            m_irradiance_image = g_rendering_api->MakeImage(TextureDesc {
                ImageType::TEXTURE_TYPE_2D,
                InternalFormat::RGBA16F,
                Vec3u { 128, 128, 1 },
                FilterMode::TEXTURE_FILTER_LINEAR,
                FilterMode::TEXTURE_FILTER_LINEAR,
                WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
                1,
                ImageFormatCapabilities::STORAGE | ImageFormatCapabilities::SAMPLED
            });
            HYPERION_ASSERT_RESULT(m_irradiance_image->Create());
        }

        if (!m_irradiance_image_view) {
            m_irradiance_image_view = g_rendering_api->MakeImageView();
            HYPERION_ASSERT_RESULT(m_irradiance_image_view->Create(m_irradiance_image, 0, 1, 0, 1));
        }
    }
}

void EnvProbeRenderResource::Destroy_Internal()
{
    HYP_SCOPE;

    m_render_collector.ClearState();

    SafeRelease(std::move(m_prefiltered_image));
    SafeRelease(std::move(m_prefiltered_image_view));
    SafeRelease(std::move(m_irradiance_image));
    SafeRelease(std::move(m_irradiance_image_view));
}

void EnvProbeRenderResource::Update_Internal()
{
    HYP_SCOPE;

    UpdateBufferData();
}

void EnvProbeRenderResource::EnqueueBind()
{
    HYP_SCOPE;

    Execute([this]()
    {
        g_engine->GetRenderState()->BindEnvProbe(m_env_probe->GetEnvProbeType(), TResourceHandle<EnvProbeRenderResource>(*this));
    }, /* force_render_thread */ true);
}

void EnvProbeRenderResource::EnqueueUnbind()
{
    HYP_SCOPE;

    Execute([this]()
    {
        g_engine->GetRenderState()->UnbindEnvProbe(m_env_probe->GetEnvProbeType(), this);
    }, /* force_render_thread */ true);
}

GPUBufferHolderBase *EnvProbeRenderResource::GetGPUBufferHolder() const
{
    return g_engine->GetRenderData()->env_probes;
}

bool EnvProbeRenderResource::ShouldComputePrefilteredEnvMap() const
{
    if (m_env_probe->IsControlledByEnvGrid()) {
        return false;
    }

    if (m_env_probe->IsReflectionProbe() || m_env_probe->IsSkyProbe()) {
        return m_env_probe->GetDimensions().Volume() > 1;
    }

    return false;
}

bool EnvProbeRenderResource::ShouldComputeIrradianceMap() const
{
    if (m_env_probe->IsControlledByEnvGrid()) {
        return false;
    }

    if (m_env_probe->IsSkyProbe()) {
        return m_env_probe->GetDimensions().Volume() > 1;
    }

    return false;
}

void EnvProbeRenderResource::UpdateBufferData()
{
    HYP_SCOPE;

    const BoundingBox aabb = BoundingBox(m_buffer_data.aabb_min.GetXYZ(), m_buffer_data.aabb_max.GetXYZ());
    const Vec3f world_position = m_buffer_data.world_position.GetXYZ();
    const FixedArray<Matrix4, 6> view_matrices = CreateCubemapMatrices(aabb, world_position);

    Memory::MemCpy(&m_buffer_data.face_view_matrices, view_matrices.Data(), sizeof(Matrix4) * 6);

    *static_cast<EnvProbeShaderData *>(m_buffer_address) = m_buffer_data;

    GetGPUBufferHolder()->MarkDirty(m_buffer_index);
}

void EnvProbeRenderResource::BindToIndex(const EnvProbeIndex &probe_index)
{
    bool set_texture = true;

    if (m_env_probe->IsControlledByEnvGrid()) {
        if (probe_index.GetProbeIndex() >= max_bound_ambient_probes) {
            HYP_LOG(EnvProbe, Warning, "Probe index ({}) out of range of max bound ambient probes", probe_index.GetProbeIndex());
        }

        set_texture = false;
    } else if (m_env_probe->IsReflectionProbe() || m_env_probe->IsSkyProbe()) {
        if (probe_index.GetProbeIndex() >= max_bound_reflection_probes) {
            HYP_LOG(EnvProbe, Warning, "Probe index ({}) out of range of max bound reflection probes", probe_index.GetProbeIndex());

            set_texture = false;
        }
    } else if (m_env_probe->IsShadowProbe()) {
        if (probe_index.GetProbeIndex() >= max_bound_point_shadow_maps) {
            HYP_LOG(EnvProbe, Warning, "Probe index ({}) out of range of max bound shadow map probes", probe_index.GetProbeIndex());

            set_texture = false;
        }
    }

    m_bound_index = probe_index;

    Execute([this, set_texture]()
    {
        UpdateRenderData(set_texture);
    }, /* force_owner_thread */ true);
}

void EnvProbeRenderResource::Render(FrameBase *frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    if (m_env_probe->IsControlledByEnvGrid()) {
        HYP_LOG(EnvProbe, Warning, "EnvProbe #{} is controlled by an EnvGrid, but Render() is being called!", m_env_probe->GetID().Value());

        return;
    }

    if (!m_env_probe->NeedsRender()) {
    //    return;
    }

    if (!m_scene_resource_handle || !m_camera_resource_handle) {
        return;
    }

    HYP_LOG(EnvProbe, Debug, "Rendering EnvProbe #{} (type: {})", m_env_probe->GetID().Value(), m_env_probe->GetEnvProbeType());

    const uint32 frame_index = frame->GetFrameIndex();

    EnvProbeIndex probe_index;

    if (m_texture_slot == ~0u) {
        HYP_LOG(EnvProbe, Warning, "EnvProbe #{} (type: {}) has no value set for texture slot!",
            m_env_probe->GetID().Value(), m_env_probe->GetEnvProbeType());

        return;
    }
    
    // don't care about position in grid if it is not controlled by one.
    // set it such that the uint32 value of probe_index
    // would be the texture slot.
    probe_index = EnvProbeIndex(
        Vec3u { 0, 0, m_texture_slot },
        Vec3u { 0, 0, 0 }
    );

    BindToIndex(probe_index);
    
    TResourceHandle<LightRenderResource> *light_render_resource_handle = nullptr;

    if (m_env_probe->IsSkyProbe() || m_env_probe->IsReflectionProbe()) {
        // @TODO Support selecting a specific light for the EnvProbe in the case of a sky probe.
        // For reflection probes, it would be ideal to bind a number of lights that can be used in forward rendering.
        auto &directional_lights = g_engine->GetRenderState()->bound_lights[uint32(LightType::DIRECTIONAL)];

        if (directional_lights.Any()) {
            light_render_resource_handle = &directional_lights[0];
        }

        if (!light_render_resource_handle) {
            HYP_LOG(EnvProbe, Warning, "No directional light found for Sky EnvProbe #{}", m_env_probe->GetID().Value());
        }
    }

    {
        g_engine->GetRenderState()->SetActiveEnvProbe(TResourceHandle<EnvProbeRenderResource>(*this));

        if (light_render_resource_handle != nullptr) {
            g_engine->GetRenderState()->SetActiveLight(*light_render_resource_handle);
        }

        g_engine->GetRenderState()->SetActiveScene(m_scene_resource_handle->GetScene());

        g_engine->GetRenderState()->BindCamera(m_camera_resource_handle);

        m_render_collector.CollectDrawCalls(
            frame,
            Bitset((1 << BUCKET_OPAQUE) | (1 << BUCKET_TRANSLUCENT)),
            nullptr
        );

        m_render_collector.ExecuteDrawCalls(
            frame,
            m_camera_resource_handle,
            Bitset((1 << BUCKET_OPAQUE) | (1 << BUCKET_TRANSLUCENT))
        );

        g_engine->GetRenderState()->UnbindCamera(m_camera_resource_handle.Get());

        g_engine->GetRenderState()->UnsetActiveScene();

        if (light_render_resource_handle != nullptr) {
            g_engine->GetRenderState()->UnsetActiveLight();
        }

        g_engine->GetRenderState()->UnsetActiveEnvProbe();
    }

    const ImageRef &framebuffer_image = m_framebuffer->GetAttachment(0)->GetImage();

    frame->GetCommandList().Add<InsertBarrier>(framebuffer_image, renderer::ResourceState::COPY_SRC);
    frame->GetCommandList().Add<InsertBarrier>(m_texture->GetRenderResource().GetImage(), renderer::ResourceState::COPY_DST);
    frame->GetCommandList().Add<Blit>(framebuffer_image, m_texture->GetRenderResource().GetImage());
    frame->GetCommandList().Add<InsertBarrier>(framebuffer_image, renderer::ResourceState::SHADER_RESOURCE);
    frame->GetCommandList().Add<InsertBarrier>(m_texture->GetRenderResource().GetImage(), renderer::ResourceState::SHADER_RESOURCE);

    if (ShouldComputePrefilteredEnvMap()) {
        Convolve(frame, EnvProbeConvolveMode::PREFILTERED_ENV_MAP);
    }

    if (ShouldComputeIrradianceMap()) {
        Convolve(frame, EnvProbeConvolveMode::IRRADIANCE_MAP);
    }

    // Temp; refactor
    m_env_probe->SetNeedsRender(false);
}

void EnvProbeRenderResource::Convolve(FrameBase *frame, EnvProbeConvolveMode convolve_mode)
{
    HYP_SCOPE;

    HYP_LOG(EnvProbe, Debug, "Convolving EnvProbe #{} (type: {})", m_env_probe->GetID().Value(), m_env_probe->GetEnvProbeType());

    struct ConvolveProbeUniforms
    {
        Vec2u               out_image_dimensions;
        Vec4f               world_position;
        uint32              num_bound_lights;
        alignas(16) uint32  light_indices[16];
    };

    ImageRef convolved_image;
    ImageViewRef convolved_image_view;

    ShaderProperties shader_properties;

    switch (convolve_mode) {
    case EnvProbeConvolveMode::PREFILTERED_ENV_MAP:
        convolved_image = m_prefiltered_image;
        convolved_image_view = m_prefiltered_image_view;
        break;
    case EnvProbeConvolveMode::IRRADIANCE_MAP:
        convolved_image = m_irradiance_image;
        convolved_image_view = m_irradiance_image_view;

        shader_properties.Set("MODE_IRRADIANCE");
        break;
    default:
        HYP_FAIL("Invalid EnvProbe convolve mode");
}

    if (m_env_probe->IsReflectionProbe()) {
        shader_properties.Set("LIGHTING");
    }

    ShaderRef convolve_probe_shader = g_shader_manager->GetOrCreate(
        NAME("ConvolveProbe"),
        shader_properties
    );

    if (!convolve_probe_shader) {
        HYP_FAIL("Failed to create ConvolveProbe shader");
    }

    AssertThrow(convolved_image.IsValid());
    AssertThrow(convolved_image_view.IsValid());

    ConvolveProbeUniforms uniforms;
    uniforms.out_image_dimensions = convolved_image->GetExtent().GetXY();
    uniforms.world_position = m_buffer_data.world_position;
    
    const uint32 max_bound_lights = MathUtil::Min(g_engine->GetRenderState()->NumBoundLights(), ArraySize(uniforms.light_indices));
    uint32 num_bound_lights = 0;

    for (uint32 light_type = 0; light_type < uint32(LightType::MAX); light_type++) {
        if (num_bound_lights >= max_bound_lights) {
            break;
        }

        for (const auto &it : g_engine->GetRenderState()->bound_lights[light_type]) {
            if (num_bound_lights >= max_bound_lights) {
                break;
            }

            uniforms.light_indices[num_bound_lights++] = it->GetBufferIndex();
        }
    }

    uniforms.num_bound_lights = num_bound_lights;

    GPUBufferRef uniform_buffer = g_rendering_api->MakeGPUBuffer(GPUBufferType::CONSTANT_BUFFER, sizeof(uniforms));
    HYPERION_ASSERT_RESULT(uniform_buffer->Create());
    uniform_buffer->Copy(sizeof(uniforms), &uniforms);

    AttachmentBase *color_attachment = m_framebuffer->GetAttachment(0);
    AttachmentBase *normals_attachment = nullptr;
    AttachmentBase *moments_attachment = nullptr;

    if (m_env_probe->IsReflectionProbe()) {
        normals_attachment = m_framebuffer->GetAttachment(1);
        AssertThrow(normals_attachment != nullptr);
        
        moments_attachment = m_framebuffer->GetAttachment(2);
        AssertThrow(moments_attachment != nullptr);
    }

    renderer::DescriptorTableDeclaration descriptor_table_decl = convolve_probe_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

    DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(NAME("ConvolveProbeDescriptorSet"), frame_index);
        AssertThrow(descriptor_set.IsValid());
        
        descriptor_set->SetElement(NAME("UniformBuffer"), uniform_buffer);
        descriptor_set->SetElement(NAME("ColorTexture"), color_attachment ? color_attachment->GetImageView() : g_engine->GetPlaceholderData()->GetImageViewCube1x1R8());
        descriptor_set->SetElement(NAME("NormalsTexture"), normals_attachment ? normals_attachment->GetImageView() : g_engine->GetPlaceholderData()->GetImageViewCube1x1R8());
        descriptor_set->SetElement(NAME("MomentsTexture"), moments_attachment ? moments_attachment->GetImageView() : g_engine->GetPlaceholderData()->GetImageViewCube1x1R8());
        descriptor_set->SetElement(NAME("SamplerLinear"), g_engine->GetPlaceholderData()->GetSamplerLinear());
        descriptor_set->SetElement(NAME("SamplerNearest"), g_engine->GetPlaceholderData()->GetSamplerNearest());
        descriptor_set->SetElement(NAME("OutImage"), convolved_image_view);
    }

    DeferCreate(descriptor_table);

    ComputePipelineRef convolve_probe_compute_pipeline = g_rendering_api->MakeComputePipeline(convolve_probe_shader, descriptor_table);
    HYPERION_ASSERT_RESULT(convolve_probe_compute_pipeline->Create());

    // Use sky as fallback
    TResourceHandle<EnvProbeRenderResource> sky_env_probe_resource_handle;

    if (g_engine->GetRenderState()->bound_env_probes[ENV_PROBE_TYPE_SKY].Any()) {
        sky_env_probe_resource_handle = g_engine->GetRenderState()->bound_env_probes[ENV_PROBE_TYPE_SKY].Front();
    }

    frame->GetCommandList().Add<InsertBarrier>(convolved_image, renderer::ResourceState::UNORDERED_ACCESS);

    frame->GetCommandList().Add<BindComputePipeline>(convolve_probe_compute_pipeline);

    frame->GetCommandList().Add<BindDescriptorTable>(
        descriptor_table,
        convolve_probe_compute_pipeline,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            {
                NAME("Scene"),
                {
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(sky_env_probe_resource_handle ? sky_env_probe_resource_handle->GetBufferIndex() : 0) }
                }
            }
        },
        frame->GetFrameIndex()
    );

    frame->GetCommandList().Add<DispatchCompute>(
        convolve_probe_compute_pipeline,
        Vec3u {
            (convolved_image->GetExtent().x + 7) / 8,
            (convolved_image->GetExtent().y + 7) / 8,
            1
        }
    );

    if (convolved_image->GetTextureDesc().HasMipmaps()) {
        frame->GetCommandList().Add<InsertBarrier>(convolved_image, renderer::ResourceState::COPY_DST);
        frame->GetCommandList().Add<GenerateMipmaps>(convolved_image);
    }

    frame->GetCommandList().Add<InsertBarrier>(convolved_image, renderer::ResourceState::SHADER_RESOURCE);

    SafeRelease(std::move(uniform_buffer));
    SafeRelease(std::move(convolve_probe_compute_pipeline));
    SafeRelease(std::move(descriptor_table));
}

#pragma endregion EnvProbeRenderResource

} // namespace hyperion

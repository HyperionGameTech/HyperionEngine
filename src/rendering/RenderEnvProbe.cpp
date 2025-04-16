/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/Shadows.hpp>

#include <rendering/backend/RenderingAPI.hpp>
#include <rendering/backend/RendererFrame.hpp>

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
    if (m_shader.IsValid()) {
        return;
    }

    switch (m_env_probe->GetEnvProbeType()) {
    case EnvProbeType::ENV_PROBE_TYPE_REFLECTION:
        m_shader = g_shader_manager->GetOrCreate({
            NAME("RenderToCubemap"),
            ShaderProperties(static_mesh_vertex_attributes, { "MODE_REFLECTION" })
        });

        break;
    case EnvProbeType::ENV_PROBE_TYPE_SKY:
        m_shader = g_shader_manager->GetOrCreate({
            NAME("RenderSky"),
            ShaderProperties(static_mesh_vertex_attributes)
        });

        break;
    case EnvProbeType::ENV_PROBE_TYPE_SHADOW:
        m_shader = g_shader_manager->GetOrCreate({
            NAME("RenderToCubemap"),
            ShaderProperties(static_mesh_vertex_attributes, { "MODE_SHADOWS" })
        });

        break;
    case EnvProbeType::ENV_PROBE_TYPE_AMBIENT:
        // Do nothing
        return;
    default:
        HYP_UNREACHABLE();
    }
}

void EnvProbeRenderResource::CreateFramebuffer()
{
    m_framebuffer = MakeRenderObject<Framebuffer>(
        m_env_probe->GetDimensions(),
        renderer::RenderPassStage::SHADER,
        renderer::RenderPassMode::RENDER_PASS_INLINE,
        6
    );

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
        renderer::RenderPassStage::SHADER,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );

    if (m_env_probe->IsShadowProbe()) {
        color_attachment->SetClearColor(MathUtil::Infinity<Vec4f>());
    }

    m_framebuffer->AddAttachment(
        1,
        g_rendering_api->GetDefaultFormat(renderer::DefaultImageFormatType::DEPTH),
        ImageType::TEXTURE_TYPE_CUBEMAP,
        renderer::RenderPassStage::SHADER,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );

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
                    Vec3u { m_env_probe->GetDimensions(), 1},
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
                    m_env_probe->IsReflectionProbe() ? FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP : FilterMode::TEXTURE_FILTER_LINEAR,
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
        AssertThrow(m_texture.IsValid());

        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            switch (m_env_probe->GetEnvProbeType()) {
            case ENV_PROBE_TYPE_REFLECTION:
            case ENV_PROBE_TYPE_SKY: // fallthrough
                g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Scene"), frame_index)
                    ->SetElement(NAME("EnvProbeTextures"), texture_slot, m_texture->GetRenderResource().GetImageView());

                break;
            case ENV_PROBE_TYPE_SHADOW:
                g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Scene"), frame_index)
                    ->SetElement(NAME("PointLightShadowMapTextures"), texture_slot, m_texture->GetRenderResource().GetImageView());

                break;
            default:
                break;
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
}

void EnvProbeRenderResource::Destroy_Internal()
{
    HYP_SCOPE;

    m_render_collector.ClearState();
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

void EnvProbeRenderResource::Render(IFrame *frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    if (m_env_probe->IsControlledByEnvGrid()) {
        HYP_LOG(EnvProbe, Warning, "EnvProbe #{} is controlled by an EnvGrid, but Render() is being called!", m_env_probe->GetID().Value());

        return;
    }

    if (!m_env_probe->NeedsRender()) {
        return;
    }

    if (!m_scene_resource_handle || !m_camera_resource_handle) {
        return;
    }

    HYP_LOG(EnvProbe, Debug, "Rendering EnvProbe #{} (type: {})", m_env_probe->GetID().Value(), m_env_probe->GetEnvProbeType());

    AssertThrow(m_texture.IsValid());

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

    if (m_texture->HasMipmaps()) {
        frame->GetCommandList().Add<GenerateMipmaps>(m_texture->GetRenderResource().GetImage());
    }

    frame->GetCommandList().Add<InsertBarrier>(framebuffer_image, renderer::ResourceState::SHADER_RESOURCE);
    frame->GetCommandList().Add<InsertBarrier>(m_texture->GetRenderResource().GetImage(), renderer::ResourceState::SHADER_RESOURCE);

    // Temp; refactor
    m_env_probe->SetNeedsRender(false);
}

#pragma endregion EnvProbeRenderResource

} // namespace hyperion

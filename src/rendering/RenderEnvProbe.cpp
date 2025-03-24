/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/Shadows.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(EnvProbe);

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

EnvProbeRenderResource::EnvProbeRenderResource(EnvProbe *env_probe, const ResourceHandle &camera_resource_handle)
    : m_env_probe(env_probe),
      m_camera_resource_handle(camera_resource_handle),
      m_buffer_data { },
      m_texture_slot(~0u),
      m_needs_render(true)
{
}

EnvProbeRenderResource::~EnvProbeRenderResource() = default;

void EnvProbeRenderResource::SetTextureSlot(uint32_t texture_slot)
{
    HYP_SCOPE;

    Execute([this, texture_slot]()
    {
        m_texture_slot = texture_slot;
    });
}

void EnvProbeRenderResource::Initialize_Internal()
{
    HYP_SCOPE;

    UpdateBufferData();
}

void EnvProbeRenderResource::Destroy_Internal()
{
    HYP_SCOPE;
}

void EnvProbeRenderResource::Update_Internal()
{
    HYP_SCOPE;
}

void EnvProbeRenderResource::Render(Frame *frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    if (m_env_probe->IsControlledByEnvGrid()) {
        HYP_LOG(EnvProbe, Warning, "EnvProbe #{} is controlled by an EnvGrid, but Render() is being called!", m_env_probe->GetID().Value());

        return;
    }
    
    //if (!m_needs_render) {
    //    return;
    //}

    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();
    const uint32 frame_index = frame->GetFrameIndex();

    RendererResult result;

    EnvProbeIndex probe_index;

    if (m_texture_slot == ~0u) {
        HYP_LOG(EnvProbe, Warning, "Env Probe #{} (type: {}) has no value set for texture slot!",
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

    CameraRenderResource &camera_render_resource = static_cast<CameraRenderResource &>(*m_camera_resource_handle);
    
    TResourceHandle<LightRenderResource> *light_render_resource_handle = nullptr;

    if (m_env_probe->IsSkyProbe()) {
        // Find a directional light to use for the sky
        // @TODO Support selecting a specific light for the EnvProbe

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
            g_engine->GetRenderState()->SetActiveLight(**light_render_resource_handle);
        }

        g_engine->GetRenderState()->SetActiveScene(m_env_probe->GetParentScene().Get());
        g_engine->GetRenderState()->BindCamera(camera_render_resource.GetCamera());

        m_render_collector.CollectDrawCalls(
            frame,
            Bitset((1 << BUCKET_OPAQUE) | (1 << BUCKET_TRANSLUCENT)),
            nullptr
        );

        m_render_collector.ExecuteDrawCalls(
            frame,
            camera_render_resource,
            Bitset((1 << BUCKET_OPAQUE) | (1 << BUCKET_TRANSLUCENT))
        );

        g_engine->GetRenderState()->UnbindCamera(camera_render_resource.GetCamera());
        g_engine->GetRenderState()->UnsetActiveScene();

        if (light_render_resource_handle != nullptr) {
            g_engine->GetRenderState()->UnsetActiveLight();
        }

        g_engine->GetRenderState()->UnsetActiveEnvProbe();
    }

    const ImageRef &framebuffer_image = m_env_probe->GetFramebuffer()->GetAttachment(0)->GetImage();

    framebuffer_image->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);
    m_env_probe->GetTexture()->GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);

    m_env_probe->GetTexture()->GetImage()->Blit(command_buffer, framebuffer_image);

    if (m_env_probe->GetTexture()->HasMipmaps()) {
        HYPERION_PASS_ERRORS(
            m_env_probe->GetTexture()->GetImage()->GenerateMipmaps(g_engine->GetGPUDevice(), command_buffer),
            result
        );
    }

    framebuffer_image->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
    m_env_probe->GetTexture()->GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);

    HYPERION_ASSERT_RESULT(result);

    m_needs_render = false;
}

void EnvProbeRenderResource::BindToIndex(const EnvProbeIndex &probe_index)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    m_probe_index = probe_index;
    
    if (IsInitialized()) {
        SetNeedsUpdate();
    }
}

GPUBufferHolderBase *EnvProbeRenderResource::GetGPUBufferHolder() const
{
    return g_engine->GetRenderData()->env_probes.Get();
}

void EnvProbeRenderResource::SetBufferData(const EnvProbeShaderData &buffer_data)
{
    HYP_SCOPE;

    Execute([this, buffer_data]()
    {
        m_buffer_data = buffer_data;
        
        if (IsInitialized()) {
            SetNeedsUpdate();
        }
    });
}

void EnvProbeRenderResource::UpdateBufferData()
{
    HYP_SCOPE;

    bool should_set_texture = true;

    if (m_env_probe->IsControlledByEnvGrid()) {
        if (m_probe_index.GetProbeIndex() >= max_bound_ambient_probes) {
            HYP_LOG(EnvProbe, Warning, "Probe index ({}) out of range of max bound ambient probes", m_probe_index.GetProbeIndex());
        }

        should_set_texture = false;
    } else if (m_env_probe->IsReflectionProbe() || m_env_probe->IsSkyProbe()) {
        if (m_probe_index.GetProbeIndex() >= max_bound_reflection_probes) {
            HYP_LOG(EnvProbe, Warning, "Probe index ({}) out of range of max bound reflection probes", m_probe_index.GetProbeIndex());

            should_set_texture = false;
        }
    } else if (m_env_probe->IsShadowProbe()) {
        if (m_probe_index.GetProbeIndex() >= max_bound_point_shadow_maps) {
            HYP_LOG(EnvProbe, Warning, "Probe index ({}) out of range of max bound shadow map probes", m_probe_index.GetProbeIndex());

            should_set_texture = false;
        }
    }

    const CameraRenderResource *camera_render_resource = static_cast<CameraRenderResource *>(m_camera_resource_handle.Get());
    
    uint32 flags = 0;

    const uint32 texture_slot = m_env_probe->IsControlledByEnvGrid() ? ~0u : m_probe_index.GetProbeIndex();

    { // build proxy flags
        const EnumFlags<ShadowFlags> shadow_flags = m_env_probe->IsShadowProbe() ? ShadowFlags::VSM : ShadowFlags::NONE;

        if (m_needs_render) {
            flags |= uint32(EnvProbeFlags::DIRTY);
        }

        flags |= uint32(shadow_flags) << 3;
    }

    const BoundingBox aabb = BoundingBox(m_buffer_data.aabb_min.GetXYZ(), m_buffer_data.aabb_max.GetXYZ());
    const Vec3f world_position = m_buffer_data.world_position.GetXYZ();

    const FixedArray<Matrix4, 6> view_matrices = CreateCubemapMatrices(aabb, world_position);

    const uint32 grid_slot = m_env_probe->IsControlledByEnvGrid() ? m_env_probe->m_grid_slot : ~0u;
    const Vec3u grid_size = m_env_probe->IsControlledByEnvGrid() ? m_probe_index.grid_size : Vec3u { };

    Memory::MemCpy(m_buffer_data.face_view_matrices, view_matrices.Data(), sizeof(Matrix4) * 6);
    m_buffer_data.texture_index = texture_slot;
    m_buffer_data.flags = flags;
    m_buffer_data.camera_near = camera_render_resource ? camera_render_resource->GetBufferData().camera_near : 0.0f;
    m_buffer_data.camera_far = camera_render_resource ? camera_render_resource->GetBufferData().camera_far : 0.0f;
    m_buffer_data.dimensions = m_env_probe->GetDimensions();
    m_buffer_data.position_in_grid = grid_slot != ~0u && grid_size.Max() != 0
        ? Vec4i {
              int32(grid_slot % grid_size.x),
              int32((grid_slot % (grid_size.x * grid_size.y)) / grid_size.x),
              int32(grid_slot / (grid_size.x * grid_size.y)),
              int32(grid_slot)
          }
        : Vec4i::Zero();

    // update cubemap texture in array of bound env probes
    if (should_set_texture) {
        AssertThrow(texture_slot != ~0u);

        AssertThrow(m_env_probe->GetTexture().IsValid());
        AssertThrow(m_env_probe->GetTexture()->IsReady());

        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            switch (m_env_probe->GetEnvProbeType()) {
            case ENV_PROBE_TYPE_REFLECTION:
            case ENV_PROBE_TYPE_SKY: // fallthrough
                g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Scene"), frame_index)
                    ->SetElement(NAME("EnvProbeTextures"), texture_slot, m_env_probe->GetTexture()->GetImageView());

                break;
            case ENV_PROBE_TYPE_SHADOW:
                g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Scene"), frame_index)
                    ->SetElement(NAME("PointLightShadowMapTextures"), texture_slot, m_env_probe->GetTexture()->GetImageView());

                break;
            default:
                break;
            }
        }
    }

    *static_cast<EnvProbeShaderData *>(m_buffer_address) = m_buffer_data;

    GetGPUBufferHolder()->MarkDirty(m_buffer_index);
}

#pragma endregion EnvProbeRenderResource

} // namespace hyperion

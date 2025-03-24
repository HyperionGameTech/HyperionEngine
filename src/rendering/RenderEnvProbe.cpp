/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/ShaderGlobals.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(EnvProbe);

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

    m_env_probe->m_bound_index = probe_index;

    UpdateRenderData(set_texture);
}

void EnvProbeRenderResource::UpdateRenderData(bool set_texture)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    AssertThrow(m_bound_index.GetProbeIndex() != ~0u);

    if (IsControlledByEnvGrid()) {
        AssertThrow(m_grid_slot != ~0u);
    }

    const uint32 texture_slot = IsControlledByEnvGrid() ? ~0u : m_bound_index.GetProbeIndex();

    { // build proxy flags
        const EnumFlags<ShadowFlags> shadow_flags = IsShadowProbe() ? ShadowFlags::VSM : ShadowFlags::NONE;

        if (NeedsRender()) {
            m_proxy.flags |= EnvProbeFlags::DIRTY;
        }

        m_proxy.flags |= uint32(shadow_flags) << 3;
    }

    UpdateRenderData(
        texture_slot,
        IsControlledByEnvGrid() ? m_grid_slot : ~0u,
        IsControlledByEnvGrid() ? m_bound_index.grid_size : Vec3u { }
    );

    // update cubemap texture in array of bound env probes
    if (set_texture) {
        AssertThrow(texture_slot != ~0u);
        AssertThrow(m_texture.IsValid());

        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            switch (GetEnvProbeType()) {
            case ENV_PROBE_TYPE_REFLECTION:
            case ENV_PROBE_TYPE_SKY: // fallthrough
                g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Scene"), frame_index)
                    ->SetElement(NAME("EnvProbeTextures"), texture_slot, m_texture->GetImageView());

                break;
            case ENV_PROBE_TYPE_SHADOW:
                g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Scene"), frame_index)
                    ->SetElement(NAME("PointLightShadowMapTextures"), texture_slot, m_texture->GetImageView());

                break;
            default:
                break;
            }
        }
    }
}

GPUBufferHolderBase *EnvProbeRenderResource::GetGPUBufferHolder() const
{
    return g_engine->GetRenderData()->env_probes.Get();
}

void EnvProbeRenderResource::UpdateBufferData()
{
    HYP_SCOPE;

    *static_cast<EnvProbeShaderData *>(m_buffer_address) = m_buffer_data;

    GetGPUBufferHolder()->MarkDirty(m_buffer_index);
}

#pragma endregion EnvProbeRenderResource

} // namespace hyperion

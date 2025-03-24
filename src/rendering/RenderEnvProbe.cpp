/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/Shadows.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

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
}

EnvProbeRenderResource::~EnvProbeRenderResource() = default;

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

void EnvProbeRenderResource::UpdateRenderData(bool set_texture)
{
    Threads::AssertOnThread(g_render_thread);

    AssertThrow(m_env_probe->GetBoundIndex().GetProbeIndex() != ~0u);

    if (m_env_probe->IsControlledByEnvGrid()) {
        AssertThrow(m_env_probe->m_grid_slot != ~0u);
    }

    const uint32 texture_slot = m_env_probe->IsControlledByEnvGrid() ? ~0u : m_env_probe->GetBoundIndex().GetProbeIndex();

    UpdateRenderData(
        texture_slot,
        m_env_probe->IsControlledByEnvGrid() ? m_env_probe->m_grid_slot : ~0u,
        m_env_probe->IsControlledByEnvGrid() ? m_env_probe->GetBoundIndex().grid_size : Vec3u { }
    );

    // update cubemap texture in array of bound env probes
    if (set_texture) {
        AssertThrow(texture_slot != ~0u);
        AssertThrow(m_env_probe->GetTexture().IsValid());

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

    UpdateBufferData();
}

void EnvProbeRenderResource::Destroy_Internal()
{
    HYP_SCOPE;
}

void EnvProbeRenderResource::Update_Internal()
{
    HYP_SCOPE;

    UpdateBufferData();
}

GPUBufferHolderBase *EnvProbeRenderResource::GetGPUBufferHolder() const
{
    return g_engine->GetRenderData()->env_probes.Get();
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

#pragma endregion EnvProbeRenderResource

} // namespace hyperion

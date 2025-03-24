/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/ShaderGlobals.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

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

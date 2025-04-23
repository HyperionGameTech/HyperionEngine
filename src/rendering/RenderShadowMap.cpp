/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderShadowMap.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/ShaderGlobals.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region ShadowMapRenderResource

ShadowMapRenderResource::ShadowMapRenderResource(ShadowMode shadow_mode, Vec2u extent)
    : m_shadow_mode(shadow_mode),
      m_extent(extent),
      m_buffer_data { }
{
}

ShadowMapRenderResource::ShadowMapRenderResource(ShadowMapRenderResource &&other) noexcept
    : RenderResourceBase(static_cast<RenderResourceBase &&>(other)),
      m_shadow_mode(other.m_shadow_mode),
      m_extent(other.m_extent),
      m_buffer_data(std::move(other.m_buffer_data))
{
}

ShadowMapRenderResource::~ShadowMapRenderResource() = default;

void ShadowMapRenderResource::Initialize_Internal()
{
    HYP_SCOPE;
    
    UpdateBufferData();
}

void ShadowMapRenderResource::Destroy_Internal()
{
    HYP_SCOPE;
}

void ShadowMapRenderResource::Update_Internal()
{
    HYP_SCOPE;
}

GPUBufferHolderBase *ShadowMapRenderResource::GetGPUBufferHolder() const
{
    return g_engine->GetRenderData()->shadow_map_data;
}

void ShadowMapRenderResource::UpdateBufferData()
{
    HYP_SCOPE;

    AssertThrow(m_buffer_index != ~0u);

    m_buffer_data.dimensions = m_extent;
    m_buffer_data.flags = uint32(ShadowFlags::NONE);

    switch (m_shadow_mode) {
    case ShadowMode::VSM:
        m_buffer_data.flags |= uint32(ShadowFlags::VSM);
        break;
    case ShadowMode::CONTACT_HARDENED:
        m_buffer_data.flags |= uint32(ShadowFlags::CONTACT_HARDENED);
        break;
    case ShadowMode::PCF:
        m_buffer_data.flags |= uint32(ShadowFlags::PCF);
        break;
    default:
        break;
    }

    *static_cast<ShadowMapShaderData *>(m_buffer_address) = m_buffer_data;
    GetGPUBufferHolder()->MarkDirty(m_buffer_index);
}

void ShadowMapRenderResource::SetBufferData(const ShadowMapShaderData &buffer_data)
{
    HYP_SCOPE;

    Execute([this, buffer_data]()
    {
        m_buffer_data = buffer_data;

        if (IsInitialized()) {
            UpdateBufferData();
        }
    });
}

#pragma endregion ShadowMapRenderResource

namespace renderer {

HYP_DESCRIPTOR_SRV(Scene, ShadowMapsTextureArray, 1);
HYP_DESCRIPTOR_SSBO(Scene, ShadowMapsBuffer, 1, sizeof(ShadowMapShaderData) * max_shadow_maps, false);

} // namespace renderer

} // namespace hyperion
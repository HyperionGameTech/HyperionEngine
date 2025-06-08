/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderShadowMap.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/ShaderGlobals.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

#pragma region RenderShadowMap

RenderShadowMap::RenderShadowMap(ShadowMapType type, ShadowMapFilterMode filter_mode, const ShadowMapAtlasElement& atlas_element, const ImageViewRef& image_view)
    : m_type(type),
      m_filter_mode(filter_mode),
      m_atlas_element(atlas_element),
      m_image_view(image_view),
      m_buffer_data {}
{
    HYP_LOG(Rendering, Debug, "Creating shadow map for atlas element, (atlas: {}, offset: {}, dimensions: {}, scale: {})",
        atlas_element.atlas_index,
        atlas_element.offset_coords,
        atlas_element.dimensions,
        atlas_element.scale);
}

RenderShadowMap::RenderShadowMap(RenderShadowMap&& other) noexcept
    : RenderResourceBase(static_cast<RenderResourceBase&&>(other)),
      m_type(other.m_type),
      m_filter_mode(other.m_filter_mode),
      m_atlas_element(other.m_atlas_element),
      m_image_view(std::move(other.m_image_view)),
      m_buffer_data(std::move(other.m_buffer_data))
{
}

RenderShadowMap::~RenderShadowMap()
{
    SafeRelease(std::move(m_image_view));
}

void RenderShadowMap::Initialize_Internal()
{
    HYP_SCOPE;

    UpdateBufferData();
}

void RenderShadowMap::Destroy_Internal()
{
    HYP_SCOPE;
}

void RenderShadowMap::Update_Internal()
{
    HYP_SCOPE;
}

GPUBufferHolderBase* RenderShadowMap::GetGPUBufferHolder() const
{
    return g_engine->GetRenderData()->shadow_map_data;
}

void RenderShadowMap::SetBufferData(const ShadowMapShaderData& buffer_data)
{
    HYP_SCOPE;

    Execute([this, buffer_data]()
        {
            m_buffer_data = buffer_data;

            if (IsInitialized())
            {
                UpdateBufferData();
            }
        });
}

void RenderShadowMap::UpdateBufferData()
{
    HYP_SCOPE;

    AssertThrow(m_buffer_index != ~0u);

    m_buffer_data.dimensions_scale = Vec4f(Vec2f(m_atlas_element.dimensions), m_atlas_element.scale);
    m_buffer_data.offset_uv = m_atlas_element.offset_uv;
    m_buffer_data.layer_index = m_type == ShadowMapType::POINT_SHADOW_MAP ? m_atlas_element.point_light_index : m_atlas_element.atlas_index;
    m_buffer_data.flags = uint32(ShadowFlags::NONE);

    switch (m_filter_mode)
    {
    case ShadowMapFilterMode::VSM:
        m_buffer_data.flags |= uint32(ShadowFlags::VSM);
        break;
    case ShadowMapFilterMode::CONTACT_HARDENED:
        m_buffer_data.flags |= uint32(ShadowFlags::CONTACT_HARDENED);
        break;
    case ShadowMapFilterMode::PCF:
        m_buffer_data.flags |= uint32(ShadowFlags::PCF);
        break;
    default:
        break;
    }

    *static_cast<ShadowMapShaderData*>(m_buffer_address) = m_buffer_data;
    GetGPUBufferHolder()->MarkDirty(m_buffer_index);
}

#pragma endregion RenderShadowMap

namespace renderer {

HYP_DESCRIPTOR_SRV(Global, ShadowMapsTextureArray, 1);
HYP_DESCRIPTOR_SRV(Global, PointLightShadowMapsTextureArray, 1);
HYP_DESCRIPTOR_SSBO(Global, ShadowMapsBuffer, 1, ~0u, false);

} // namespace renderer

} // namespace hyperion
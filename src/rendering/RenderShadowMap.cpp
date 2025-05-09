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

#pragma region ShadowMapRenderResource

ShadowMapRenderResource::ShadowMapRenderResource(ShadowMapType type, ShadowMapFilterMode filter_mode, const ShadowMapAtlasElement &atlas_element, const ImageViewRef &image_view)
    : m_type(type),
      m_filter_mode(filter_mode),
      m_atlas_element(atlas_element),
      m_image_view(image_view),
      m_buffer_data { }
{
    HYP_LOG(Rendering, Debug, "Creating shadow map for atlas element, (atlas: {}, offset: {}, dimensions: {}, scale: {})",
        atlas_element.atlas_index,
        atlas_element.offset_coords,
        atlas_element.dimensions,
        atlas_element.scale);
}

ShadowMapRenderResource::ShadowMapRenderResource(ShadowMapRenderResource &&other) noexcept
    : RenderResourceBase(static_cast<RenderResourceBase &&>(other)),
      m_type(other.m_type),
      m_filter_mode(other.m_filter_mode),
      m_atlas_element(other.m_atlas_element),
      m_image_view(std::move(other.m_image_view)),
      m_buffer_data(std::move(other.m_buffer_data))
{
}

ShadowMapRenderResource::~ShadowMapRenderResource()
{
    SafeRelease(std::move(m_image_view));
}

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

    m_buffer_data.dimensions_scale = Vec4f(Vec2f(m_atlas_element.dimensions), m_atlas_element.scale);
    m_buffer_data.offset_uv = m_atlas_element.offset_uv;
    m_buffer_data.atlas_index = m_atlas_element.atlas_index;
    m_buffer_data.flags = uint32(ShadowFlags::NONE);

    switch (m_filter_mode) {
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
HYP_DESCRIPTOR_SRV(Scene, PointLightShadowMapsTextureArray, 1);
HYP_DESCRIPTOR_SSBO(Scene, ShadowMapsBuffer, 1, sizeof(ShadowMapShaderData) * max_shadow_maps, false);

} // namespace renderer

} // namespace hyperion
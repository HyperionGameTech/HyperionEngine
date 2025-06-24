/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderShadowMap.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/PlaceholderData.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

#pragma region ShadowMapAtlas

bool ShadowMapAtlas::AddElement(const Vec2u& element_dimensions, ShadowMapAtlasElement& out_element)
{
    if (!AtlasPacker<ShadowMapAtlasElement>::AddElement(element_dimensions, out_element))
    {
        return false;
    }

    out_element.atlas_index = atlas_index;

    return true;
}

#pragma endregion ShadowMapAtlas

#pragma region ShadowMapAllocator

ShadowMapAllocator::ShadowMapAllocator()
    : m_atlas_dimensions(2048, 2048)
{
    m_atlases.Reserve(max_shadow_maps);

    for (SizeType i = 0; i < max_shadow_maps; i++)
    {
        m_atlases.PushBack(ShadowMapAtlas(uint32(i), m_atlas_dimensions));
    }
}

ShadowMapAllocator::~ShadowMapAllocator()
{
    SafeRelease(std::move(m_atlas_image));
    SafeRelease(std::move(m_atlas_image_view));

    SafeRelease(std::move(m_point_light_shadow_map_image));
    SafeRelease(std::move(m_point_light_shadow_map_image_view));
}

void ShadowMapAllocator::Initialize()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    m_atlas_image = g_rendering_api->MakeImage(TextureDesc {
        TT_TEX2D_ARRAY,
        TF_RG32F,
        Vec3u { m_atlas_dimensions, 1 },
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        uint32(m_atlases.Size()),
        IU_SAMPLED | IU_STORAGE });

    HYPERION_ASSERT_RESULT(m_atlas_image->Create());

    m_atlas_image_view = g_rendering_api->MakeImageView(m_atlas_image);
    HYPERION_ASSERT_RESULT(m_atlas_image_view->Create());

    m_point_light_shadow_map_image = g_rendering_api->MakeImage(TextureDesc {
        TT_CUBEMAP_ARRAY,
        TF_RG32F,
        Vec3u { 512, 512, 1 },
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        max_bound_point_shadow_maps * 6,
        IU_SAMPLED | IU_STORAGE });

    HYPERION_ASSERT_RESULT(m_point_light_shadow_map_image->Create());

    m_point_light_shadow_map_image_view = g_rendering_api->MakeImageView(m_point_light_shadow_map_image);
    HYPERION_ASSERT_RESULT(m_point_light_shadow_map_image_view->Create());
}

void ShadowMapAllocator::Destroy()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    for (ShadowMapAtlas& atlas : m_atlases)
    {
        atlas.Clear();
    }

    SafeRelease(std::move(m_atlas_image));
    SafeRelease(std::move(m_atlas_image_view));

    SafeRelease(std::move(m_point_light_shadow_map_image));
    SafeRelease(std::move(m_point_light_shadow_map_image_view));
}

RenderShadowMap* ShadowMapAllocator::AllocateShadowMap(ShadowMapType shadow_map_type, ShadowMapFilter filter_mode, const Vec2u& dimensions)
{
    if (shadow_map_type == SMT_OMNI)
    {
        const uint32 point_light_index = m_point_light_shadow_map_id_generator.NextID() - 1;

        // Cannot allocate if we ran out of IDs
        if (point_light_index >= max_bound_point_shadow_maps)
        {
            m_point_light_shadow_map_id_generator.FreeID(point_light_index + 1);

            return nullptr;
        }

        const ShadowMapAtlasElement atlas_element {
            .atlas_index = ~0u,
            .point_light_index = point_light_index,
            .offset_uv = Vec2f::Zero(),
            .offset_coords = Vec2u::Zero(),
            .dimensions = dimensions,
            .scale = Vec2f::One()
        };

        RenderShadowMap* shadow_map = AllocateResource<RenderShadowMap>(
            shadow_map_type,
            filter_mode,
            atlas_element,
            m_point_light_shadow_map_image_view);

        return shadow_map;
    }

    for (ShadowMapAtlas& atlas : m_atlases)
    {
        ShadowMapAtlasElement atlas_element;

        if (atlas.AddElement(dimensions, atlas_element))
        {
            ImageViewRef atlas_image_view = m_atlas_image->MakeLayerImageView(atlas_element.atlas_index);
            DeferCreate(atlas_image_view);

            RenderShadowMap* shadow_map = AllocateResource<RenderShadowMap>(
                shadow_map_type,
                filter_mode,
                atlas_element,
                atlas_image_view);

            return shadow_map;
        }
    }

    return nullptr;
}

bool ShadowMapAllocator::FreeShadowMap(RenderShadowMap* shadow_map)
{
    if (!shadow_map)
    {
        return false;
    }

    const ShadowMapAtlasElement& atlas_element = shadow_map->GetAtlasElement();

    bool result = false;

    if (atlas_element.atlas_index != ~0u)
    {
        AssertThrow(atlas_element.atlas_index < m_atlases.Size());

        ShadowMapAtlas& atlas = m_atlases[atlas_element.atlas_index];
        result = atlas.RemoveElement(atlas_element);

        if (!result)
        {
            HYP_LOG(Rendering, Error, "Failed to free shadow map from atlas (atlas index: {})", atlas_element.atlas_index);
        }
    }
    else if (atlas_element.point_light_index != ~0u)
    {
        m_point_light_shadow_map_id_generator.FreeID(atlas_element.point_light_index + 1);

        result = true;
    }
    else
    {
        HYP_LOG(Rendering, Error, "Failed to free shadow map: invalid atlas index and point light index");
    }

    FreeResource(shadow_map);

    return result;
}

#pragma endregion ShadowMapAllocator

#pragma region RenderShadowMap

RenderShadowMap::RenderShadowMap(ShadowMapType type, ShadowMapFilter filter_mode, const ShadowMapAtlasElement& atlas_element, const ImageViewRef& image_view)
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
    return g_render_global_state->ShadowMaps;
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
    m_buffer_data.layer_index = m_type == SMT_OMNI ? m_atlas_element.point_light_index : m_atlas_element.atlas_index;
    m_buffer_data.flags = uint32(SF_NONE);

    switch (m_filter_mode)
    {
    case SMF_VSM:
        m_buffer_data.flags |= uint32(SF_VSM);
        break;
    case SMF_CONTACT_HARDENED:
        m_buffer_data.flags |= uint32(SF_CONTACT_HARDENED);
        break;
    case SMF_PCF:
        m_buffer_data.flags |= uint32(SF_PCF);
        break;
    default:
        break;
    }

    *static_cast<ShadowMapShaderData*>(m_buffer_address) = m_buffer_data;
    GetGPUBufferHolder()->MarkDirty(m_buffer_index);
}

#pragma endregion RenderShadowMap

HYP_DESCRIPTOR_SRV(Global, ShadowMapsTextureArray, 1);
HYP_DESCRIPTOR_SRV(Global, PointLightShadowMapsTextureArray, 1);
HYP_DESCRIPTOR_SSBO(Global, ShadowMapsBuffer, 1, ~0u, false);

} // namespace hyperion
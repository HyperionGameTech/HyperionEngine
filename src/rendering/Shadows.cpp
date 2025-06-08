/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Shadows.hpp>
#include <rendering/RenderShadowMap.hpp>
#include <rendering/PlaceholderData.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <Engine.hpp>

namespace hyperion {

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

#pragma region ShadowMapManager

ShadowMapManager::ShadowMapManager()
    : m_atlas_dimensions(2048, 2048)
{
    m_atlases.Reserve(max_shadow_maps);

    for (SizeType i = 0; i < max_shadow_maps; i++)
    {
        m_atlases.PushBack(ShadowMapAtlas(uint32(i), m_atlas_dimensions));
    }
}

ShadowMapManager::~ShadowMapManager()
{
    SafeRelease(std::move(m_atlas_image));
    SafeRelease(std::move(m_atlas_image_view));

    SafeRelease(std::move(m_point_light_shadow_map_image));
    SafeRelease(std::move(m_point_light_shadow_map_image_view));
}

void ShadowMapManager::Initialize()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    m_atlas_image = g_rendering_api->MakeImage(TextureDesc {
        ImageType::TEXTURE_TYPE_2D_ARRAY,
        InternalFormat::RG32F,
        Vec3u { m_atlas_dimensions, 1 },
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        uint32(m_atlases.Size()),
        ImageFormatCapabilities::SAMPLED | ImageFormatCapabilities::STORAGE });

    HYPERION_ASSERT_RESULT(m_atlas_image->Create());

    m_atlas_image_view = g_rendering_api->MakeImageView(m_atlas_image);
    HYPERION_ASSERT_RESULT(m_atlas_image_view->Create());

    m_point_light_shadow_map_image = g_rendering_api->MakeImage(TextureDesc {
        ImageType::TEXTURE_TYPE_CUBEMAP_ARRAY,
        InternalFormat::RG32F,
        Vec3u { 512, 512, 1 },
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        max_bound_point_shadow_maps * 6,
        ImageFormatCapabilities::SAMPLED | ImageFormatCapabilities::STORAGE });

    HYPERION_ASSERT_RESULT(m_point_light_shadow_map_image->Create());

    m_point_light_shadow_map_image_view = g_rendering_api->MakeImageView(m_point_light_shadow_map_image);
    HYPERION_ASSERT_RESULT(m_point_light_shadow_map_image_view->Create());

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("ShadowMapsTextureArray"), m_atlas_image_view);
        g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("PointLightShadowMapsTextureArray"), m_point_light_shadow_map_image_view);
    }
}

void ShadowMapManager::Destroy()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    for (ShadowMapAtlas& atlas : m_atlases)
    {
        atlas.Clear();
    }

    // unset the shadow map texture array in the global descriptor set
    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("ShadowMapsTextureArray"), g_engine->GetPlaceholderData()->GetImageView2D1x1R8Array());
        g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("PointLightShadowMapsTextureArray"), g_engine->GetPlaceholderData()->GetImageViewCube1x1R8Array());
    }

    SafeRelease(std::move(m_atlas_image));
    SafeRelease(std::move(m_atlas_image_view));

    SafeRelease(std::move(m_point_light_shadow_map_image));
    SafeRelease(std::move(m_point_light_shadow_map_image_view));
}

RenderShadowMap* ShadowMapManager::AllocateShadowMap(ShadowMapType shadow_map_type, ShadowMapFilterMode filter_mode, const Vec2u& dimensions)
{
    if (shadow_map_type == ShadowMapType::POINT_SHADOW_MAP)
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
            .offset_coords = Vec2u::Zero(),
            .offset_uv = Vec2f::Zero(),
            .dimensions = dimensions,
            .scale = Vec2f::One()
        };

        RenderShadowMap* shadow_render_map = AllocateResource<RenderShadowMap>(
            shadow_map_type,
            filter_mode,
            atlas_element,
            m_point_light_shadow_map_image_view);

        return shadow_render_map;
    }

    for (ShadowMapAtlas& atlas : m_atlases)
    {
        ShadowMapAtlasElement atlas_element;

        if (atlas.AddElement(dimensions, atlas_element))
        {
            ImageViewRef atlas_image_view = m_atlas_image->MakeLayerImageView(atlas_element.atlas_index);
            DeferCreate(atlas_image_view);

            RenderShadowMap* shadow_render_map = AllocateResource<RenderShadowMap>(
                shadow_map_type,
                filter_mode,
                atlas_element,
                atlas_image_view);

            return shadow_render_map;
        }
    }

    return nullptr;
}

bool ShadowMapManager::FreeShadowMap(RenderShadowMap* shadow_render_map)
{
    if (!shadow_render_map)
    {
        return false;
    }

    const ShadowMapAtlasElement& atlas_element = shadow_render_map->GetAtlasElement();

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

    FreeResource(shadow_render_map);

    return result;
}

#pragma endregion ShadowMapManager

} // namespace hyperion
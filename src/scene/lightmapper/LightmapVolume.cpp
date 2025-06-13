/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/lightmapper/LightmapVolume.hpp>
#include <scene/Texture.hpp>

#include <rendering/RenderTexture.hpp>
#include <rendering/lightmapper/RenderLightmapVolume.hpp>
#include <rendering/lightmapper/LightmapUVBuilder.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/threading/Threads.hpp>

#include <core/algorithm/AnyOf.hpp>

#include <Engine.hpp>

namespace hyperion {

LightmapVolume::LightmapVolume()
    : LightmapVolume(BoundingBox::Empty())
{
}

LightmapVolume::LightmapVolume(const BoundingBox& aabb)
    : m_render_resource(nullptr),
      m_aabb(aabb),
      m_atlas(Vec2u(4096, 4096))
{
    m_atlas_textures = {
        { LightmapElementTextureType::RADIANCE, Handle<Texture>::empty },
        { LightmapElementTextureType::IRRADIANCE, Handle<Texture>::empty }
    };
}

LightmapVolume::~LightmapVolume()
{
    if (IsInitCalled())
    {
        SetReady(false);

        if (m_render_resource != nullptr)
        {
            FreeResource(m_render_resource);
        }
    }
}

bool LightmapVolume::AddElement(const LightmapUVMap& uv_map, LightmapElement& out_element, bool shrink_to_fit, float downscale_limit)
{
    Threads::AssertOnThread(g_game_thread);

    const Vec2u element_dimensions = { uv_map.width, uv_map.height };

    return m_atlas.AddElement(element_dimensions, out_element, shrink_to_fit, downscale_limit);
}

const LightmapElement* LightmapVolume::GetElement(uint32 index) const
{
    Threads::AssertOnThread(g_game_thread);

    if (index >= m_atlas.elements.Size())
    {
        return nullptr;
    }

    return &m_atlas.elements[index];
}

bool LightmapVolume::BuildElementTextures(const LightmapUVMap& uv_map, uint32 index)
{
    Threads::AssertOnThread(g_game_thread);

    if (index >= m_atlas.elements.Size())
    {
        return false;
    }

    LightmapElement& element = m_atlas.elements[index];

    const Vec2u element_dimensions = element.dimensions;

    FixedArray<Bitmap<4, float>, uint32(LightmapElementTextureType::MAX)> bitmaps = {
        uv_map.ToBitmapRadiance(),  /* RADIANCE */
        uv_map.ToBitmapIrradiance() /* IRRADIANCE */
    };

    element.entries.Resize(uint32(LightmapElementTextureType::MAX));

    static const char* texture_type_names[uint32(LightmapElementTextureType::MAX)] = { "R", "I" };

    for (uint32 i = 0; i < uint32(LightmapElementTextureType::MAX); i++)
    {
        element.entries[i].type = LightmapElementTextureType(i);

        Handle<Texture> texture = CreateObject<Texture>(TextureData {
            TextureDesc {
                ImageType::TEXTURE_TYPE_2D,
                InternalFormat::RGBA32F,
                Vec3u { element_dimensions, 1 },
                FilterMode::TEXTURE_FILTER_LINEAR,
                FilterMode::TEXTURE_FILTER_LINEAR,
                WrapMode::TEXTURE_WRAP_REPEAT },
            ByteBuffer(bitmaps[i].GetUnpackedFloats().ToByteView()) });

        texture->SetName(NAME_FMT("LightmapVolumeTexture_{}_{}_{}", m_uuid, element.index, texture_type_names[i]));

        element.entries[i].texture = std::move(texture);
    }

    if (IsInitCalled())
    {
        // Initalize textures for the entries in the element
        for (LightmapElementTextureEntry& entry : element.entries)
        {
            if (entry.texture.IsValid())
            {
                InitObject(entry.texture);
            }
        }

        UpdateAtlasTextures();
    }

    return true;
}

void LightmapVolume::Init()
{
    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind(
        [this]()
        {
            if (m_render_resource != nullptr)
            {
                FreeResource(m_render_resource);

                m_render_resource = nullptr;
            }
        }));

    m_render_resource = AllocateResource<RenderLightmapVolume>(this);

    for (LightmapElement& element : m_atlas.elements)
    {
        if (!element.IsValid())
        {
            continue;
        }

        for (LightmapElementTextureEntry& entry : element.entries)
        {
            InitObject(entry.texture);
        }
    }

    UpdateAtlasTextures();

    SetReady(true);
}

void LightmapVolume::UpdateAtlasTextures()
{
    HYP_LOG(Lightmap, Debug, "Updating atlas textures for LightmapVolume {}", m_uuid);

    static const char* texture_type_names[uint32(LightmapElementTextureType::MAX)] = { "R", "I" };

    Array<TResourceHandle<RenderTexture>> render_textures;
    render_textures.Resize(uint32(LightmapElementTextureType::MAX));

    for (uint32 i = 0; i < uint32(LightmapElementTextureType::MAX); i++)
    {
        LightmapElementTextureType texture_type = LightmapElementTextureType(i);

        Handle<Texture>& atlas_texture = m_atlas_textures[texture_type];

        if (!atlas_texture.IsValid())
        {
            atlas_texture = CreateObject<Texture>(TextureDesc {
                ImageType::TEXTURE_TYPE_2D,
                InternalFormat::RGBA8,
                Vec3u { m_atlas.atlas_dimensions, 1 },
                FilterMode::TEXTURE_FILTER_LINEAR,
                FilterMode::TEXTURE_FILTER_LINEAR,
                WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE });

            atlas_texture->SetName(NAME_FMT("LightmapVolumeAtlasTexture_{}_{}_{}", m_uuid, uint32(texture_type), texture_type_names[i]));
        }

        InitObject(atlas_texture);

        render_textures[i] = TResourceHandle<RenderTexture>(atlas_texture->GetRenderResource());
    }

    if (IsInitCalled())
    {
        m_render_resource->BuildAtlasTextures(std::move(render_textures), m_atlas.elements);
    }
}

} // namespace hyperion

/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/lightmapper/LightmapVolume.hpp>
#include <scene/Texture.hpp>

#include <rendering/RenderTexture.hpp>
#include <rendering/RenderProxy.hpp>

#include <rendering/rhi/CmdList.hpp>

#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/RendererHelpers.hpp>

#include <rendering/lightmapper/RenderLightmapVolume.hpp>
#include <rendering/lightmapper/LightmapUVBuilder.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/threading/Threads.hpp>

#include <core/algorithm/AnyOf.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(BakeLightmapVolumeTexture)
    : RenderCommand
{
    WeakHandle<LightmapVolume> lightmap_volume_weak;
    Array<LightmapElement> lightmap_elements;
    Array<Handle<Texture>> atlas_textures;

    RENDER_COMMAND(BakeLightmapVolumeTexture)(const WeakHandle<LightmapVolume>& lightmap_volume_weak, const Array<LightmapElement>& lightmap_elements, const Array<Handle<Texture>>& atlas_textures)
        : lightmap_volume_weak(lightmap_volume_weak),
          lightmap_elements(lightmap_elements),
          atlas_textures(atlas_textures)
    {
    }

    virtual ~RENDER_COMMAND(BakeLightmapVolumeTexture)() override
    {
    }

    virtual RendererResult operator()() override
    {
        // Ensure the array of atlas textures are resized to the correct count
        AssertThrow(atlas_textures.Size() == uint32(LTT_MAX));

        Array<Array<Pair<const LightmapElement*, Handle<Texture>>>> element_textures;
        element_textures.Resize(uint32(LTT_MAX));

        for (const LightmapElement& element : lightmap_elements)
        {
            for (const LightmapElementTextureEntry& entry : element.entries)
            {
                if (!atlas_textures[uint32(entry.type)])
                {
                    HYP_LOG(Lightmap, Warning, "Atlas texture for type {} is not set, skipping element {}", uint32(entry.type), element.index);

                    continue;
                }

                if (!entry.texture.IsValid())
                {
                    continue;
                }

                AssertThrow(entry.texture->IsReady());

                element_textures[uint32(entry.type)].EmplaceBack(&element, entry.texture);
            }
        }

        SingleTimeCommands commands;

        commands.Push([&](CmdList& cmd)
            {
                for (uint32 texture_type_index = 0; texture_type_index < uint32(LTT_MAX); texture_type_index++)
                {
                    if (!atlas_textures[texture_type_index])
                    {
                        continue;
                    }

                    const Handle<Texture>& atlas_texture = atlas_textures[texture_type_index];

                    cmd.Add<InsertBarrier>(atlas_texture->GetRenderResource().GetImage(), RS_COPY_DST);

                    for (const auto& element_texture_pair : element_textures[texture_type_index])
                    {
                        const LightmapElement* element = element_texture_pair.first;
                        const Handle<Texture>& element_texture = element_texture_pair.second;

                        HYP_LOG(Lightmap, Debug, "Blitting element {} (name: {}) to atlas texture {} (dim: {}), at offset {}, dimensions {}",
                            element->index,
                            element_texture->GetName(),
                            atlas_texture->GetName(),
                            element_texture->GetRenderResource().GetImage()->GetExtent(),
                            element->offset_coords,
                            element->dimensions);

                        AssertDebug(element->offset_coords.x < atlas_texture->GetExtent().x);
                        AssertDebug(element->offset_coords.y < atlas_texture->GetExtent().y);
                        AssertDebug(element->offset_coords.x + element->dimensions.x <= atlas_texture->GetExtent().x);
                        AssertDebug(element->offset_coords.y + element->dimensions.y <= atlas_texture->GetExtent().y);

                        cmd.Add<InsertBarrier>(element_texture->GetRenderResource().GetImage(), RS_COPY_SRC);

                        cmd.Add<Blit>(
                            element_texture->GetRenderResource().GetImage(),
                            atlas_texture->GetRenderResource().GetImage(),
                            Rect<uint32> {
                                .x0 = 0,
                                .y0 = 0,
                                .x1 = element_texture->GetRenderResource().GetImage()->GetExtent().x,
                                .y1 = element_texture->GetRenderResource().GetImage()->GetExtent().y },
                            Rect<uint32> {
                                .x0 = element->offset_coords.x,
                                .y0 = element->offset_coords.y,
                                .x1 = element->offset_coords.x + element->dimensions.x,
                                .y1 = element->offset_coords.y + element->dimensions.y },
                            0, /* src_mip */
                            0, /* dst_mip */
                            0, /* src_face */
                            0  /* dst_face */
                        );

                        cmd.Add<InsertBarrier>(element_texture->GetRenderResource().GetImage(), RS_SHADER_RESOURCE);
                    }

                    cmd.Add<InsertBarrier>(atlas_texture->GetRenderResource().GetImage(), RS_SHADER_RESOURCE);
                }
            });

        HYPERION_ASSERT_RESULT(commands.Execute());

        // DEBUGGING: Save each atlas texture to a file for debugging purposes
        HYP_LOG(Lightmap, Info, "Saving atlas textures to disk for debugging");
        for (uint32 texture_type_index = 0; texture_type_index < uint32(LTT_MAX); texture_type_index++)
        {
            if (!atlas_textures[texture_type_index])
            {
                continue;
            }

            const Handle<Texture>& atlas_texture = atlas_textures[texture_type_index];

            ByteBuffer data;
            atlas_texture->GetRenderResource().Readback(data);

            if (data.Empty())
            {
                HYP_LOG(Lightmap, Warning, "Atlas texture {} is empty, skipping save", atlas_texture->GetName());
                continue;
            }

            Bitmap<4> bitmap(
                atlas_texture->GetExtent().x,
                atlas_texture->GetExtent().y);

            bitmap.SetPixels(data);

            const String filename = HYP_FORMAT("lightmap_atlas_texture_{}_{}.bmp",
                atlas_texture->GetName(),
                uint32(LightmapTextureType(texture_type_index)));

            HYP_LOG(Lightmap, Info, "Writing atlas texture {} to file {}", atlas_texture->GetName(), filename);

            bool res = bitmap.Write(filename.Data());

            if (!res)
            {
                HYP_LOG(Lightmap, Error, "Failed to write atlas texture {} to file", atlas_texture->GetName());
            }
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

LightmapVolume::LightmapVolume()
    : LightmapVolume(BoundingBox::Empty())
{
}

LightmapVolume::LightmapVolume(const BoundingBox& aabb)
    : m_aabb(aabb),
      m_atlas(Vec2u(4096, 4096))
{
    m_atlas_textures = {
        { LTT_RADIANCE, Handle<Texture>::empty },
        { LTT_IRRADIANCE, Handle<Texture>::empty }
    };
}

LightmapVolume::~LightmapVolume()
{
    if (IsInitCalled())
    {
        SetReady(false);
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

    FixedArray<Bitmap<4, float>, uint32(LTT_MAX)> bitmaps = {
        uv_map.ToBitmapRadiance(),  /* RADIANCE */
        uv_map.ToBitmapIrradiance() /* IRRADIANCE */
    };

    element.entries.Resize(uint32(LTT_MAX));

    static const char* texture_type_names[uint32(LTT_MAX)] = { "R", "I" };

    for (uint32 i = 0; i < uint32(LTT_MAX); i++)
    {
        element.entries[i].type = LightmapTextureType(i);

        Handle<Texture> texture = CreateObject<Texture>(TextureData {
            TextureDesc {
                TT_TEX2D,
                TF_RGBA32F,
                Vec3u { element_dimensions, 1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_REPEAT },
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

void LightmapVolume::UpdateRenderProxy(IRenderProxy* proxy)
{
    RenderProxyLightmapVolume* proxy_casted = static_cast<RenderProxyLightmapVolume*>(proxy);

    proxy_casted->lightmap_volume = WeakHandle<LightmapVolume>(WeakHandleFromThis());

    proxy_casted->buffer_data.aabb_max = Vec4f(m_aabb.max, 1.0f);
    proxy_casted->buffer_data.aabb_min = Vec4f(m_aabb.min, 1.0f);
    proxy_casted->buffer_data.texture_index = ~0u; // @TODO: Set the correct texture index based on the element
}

void LightmapVolume::UpdateAtlasTextures()
{
    HYP_LOG(Lightmap, Debug, "Updating atlas textures for LightmapVolume {}", m_uuid);

    static const char* texture_type_names[uint32(LTT_MAX)] = { "R", "I" };

    Array<Handle<Texture>> element_textures;
    element_textures.Resize(uint32(LTT_MAX));

    for (uint32 i = 0; i < uint32(LTT_MAX); i++)
    {
        LightmapTextureType texture_type = LightmapTextureType(i);

        Handle<Texture>& texture = m_atlas_textures[texture_type];

        if (!texture.IsValid())
        {
            texture = CreateObject<Texture>(TextureDesc {
                TT_TEX2D,
                TF_RGBA8,
                Vec3u { m_atlas.atlas_dimensions, 1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE });

            texture->SetName(NAME_FMT("LightmapVolumeAtlasTexture_{}_{}_{}", m_uuid, uint32(texture_type), texture_type_names[i]));
        }

        InitObject(texture);

        element_textures[i] = texture;
    }

    if (IsInitCalled())
    {
        PUSH_RENDER_COMMAND(
            BakeLightmapVolumeTexture,
            WeakHandle<LightmapVolume>(WeakHandleFromThis()),
            m_atlas.elements,
            element_textures);
    }
}

} // namespace hyperion

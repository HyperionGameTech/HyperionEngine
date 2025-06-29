/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/lightmapper/RenderLightmapVolume.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/backend/RendererHelpers.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/rhi/RHICommandList.hpp>

#include <scene/lightmapper/LightmapVolume.hpp>

#include <scene/Texture.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

// temp
#include <util/img/Bitmap.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region RenderLightmapVolume

RenderLightmapVolume::RenderLightmapVolume(LightmapVolume* lightmap_volume)
    : m_lightmap_volume(lightmap_volume)
{
    m_buffer_data = LightmapVolumeShaderData {};
    m_buffer_data.aabb_max = Vec4f(lightmap_volume->GetAABB().GetMax(), 1.0f);
    m_buffer_data.aabb_min = Vec4f(lightmap_volume->GetAABB().GetMin(), 1.0f);
    m_buffer_data.texture_index = ~0u; // @TODO
}

RenderLightmapVolume::~RenderLightmapVolume() = default;

void RenderLightmapVolume::Initialize_Internal()
{
    HYP_SCOPE;

    AssertThrow(m_lightmap_volume != nullptr);

    UpdateBufferData();
}

void RenderLightmapVolume::Destroy_Internal()
{
    HYP_SCOPE;
}

void RenderLightmapVolume::Update_Internal()
{
    HYP_SCOPE;

    AssertThrow(m_lightmap_volume != nullptr);
}

GPUBufferHolderBase* RenderLightmapVolume::GetGPUBufferHolder() const
{
    return g_render_global_state->gpu_buffers[GRB_LIGHTMAP_VOLUMES];
}

void RenderLightmapVolume::SetBufferData(const LightmapVolumeShaderData& buffer_data)
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

void RenderLightmapVolume::UpdateBufferData()
{
    HYP_SCOPE;

    *static_cast<LightmapVolumeShaderData*>(m_buffer_address) = m_buffer_data;
    GetGPUBufferHolder()->MarkDirty(m_buffer_index);
}

void RenderLightmapVolume::BuildAtlasTextures(Array<TResourceHandle<RenderTexture>>&& atlas_textures, const Array<LightmapElement>& elements)
{
    HYP_SCOPE;

    Execute([this, atlas_textures = std::move(atlas_textures), elements = elements]()
        {
            m_atlas_textures.Clear();
            m_atlas_textures = std::move(atlas_textures);

            if (elements.Empty())
            {
                HYP_LOG(Lightmap, Warning, "No elements to build atlas textures for, skipping");

                return;
            }

            // Ensure the array of atlas textures are resized to the correct count
            AssertThrow(m_atlas_textures.Size() == uint32(LightmapElementTextureType::MAX));

            Array<Array<Pair<const LightmapElement*, TResourceHandle<RenderTexture>>>> element_textures;
            element_textures.Resize(uint32(LightmapElementTextureType::MAX));

            for (const LightmapElement& element : elements)
            {
                for (const LightmapElementTextureEntry& entry : element.entries)
                {
                    if (!m_atlas_textures[uint32(entry.type)])
                    {
                        HYP_LOG(Lightmap, Warning, "Atlas texture for type {} is not set, skipping element {}", uint32(entry.type), element.index);

                        continue;
                    }

                    if (!entry.texture.IsValid())
                    {
                        continue;
                    }

                    AssertThrow(entry.texture->IsReady());

                    TResourceHandle<RenderTexture> render_texture(entry.texture->GetRenderResource());

                    element_textures[uint32(entry.type)].EmplaceBack(&element, std::move(render_texture));
                }
            }

            SingleTimeCommands commands;

            commands.Push([&](RHICommandList& cmd)
                {
                    for (uint32 texture_type_index = 0; texture_type_index < uint32(LightmapElementTextureType::MAX); texture_type_index++)
                    {
                        if (!m_atlas_textures[texture_type_index])
                        {
                            continue;
                        }

                        TResourceHandle<RenderTexture>& atlas_texture = m_atlas_textures[texture_type_index];

                        cmd.Add<InsertBarrier>(atlas_texture->GetImage(), RS_COPY_DST);

                        for (const auto& element_texture_pair : element_textures[texture_type_index])
                        {
                            const LightmapElement* element = element_texture_pair.first;
                            const TResourceHandle<RenderTexture>& render_texture = element_texture_pair.second;

                            HYP_LOG(Lightmap, Debug, "Blitting element {} (name: {}) to atlas texture {} (dim: {}), at offset {}, dimensions {}",
                                element->index,
                                render_texture->GetTexture()->GetName(),
                                atlas_texture->GetTexture()->GetName(),
                                render_texture->GetImage()->GetExtent(),
                                element->offset_coords,
                                element->dimensions);

                            AssertDebug(element->offset_coords.x < atlas_texture->GetImage()->GetExtent().x);
                            AssertDebug(element->offset_coords.y < atlas_texture->GetImage()->GetExtent().y);
                            AssertDebug(element->offset_coords.x + element->dimensions.x <= atlas_texture->GetImage()->GetExtent().x);
                            AssertDebug(element->offset_coords.y + element->dimensions.y <= atlas_texture->GetImage()->GetExtent().y);

                            cmd.Add<InsertBarrier>(render_texture->GetImage(), RS_COPY_SRC);

                            cmd.Add<Blit>(
                                render_texture->GetImage(),
                                atlas_texture->GetImage(),
                                Rect<uint32> {
                                    .x0 = 0,
                                    .y0 = 0,
                                    .x1 = render_texture->GetImage()->GetExtent().x,
                                    .y1 = render_texture->GetImage()->GetExtent().y },
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

                            cmd.Add<InsertBarrier>(render_texture->GetImage(), RS_SHADER_RESOURCE);
                        }

                        cmd.Add<InsertBarrier>(atlas_texture->GetImage(), RS_SHADER_RESOURCE);
                    }
                });

            HYPERION_ASSERT_RESULT(commands.Execute());

            // DEBUGGING: Save each atlas texture to a file for debugging purposes
            HYP_LOG(Lightmap, Info, "Saving atlas textures to disk for debugging");
            for (uint32 texture_type_index = 0; texture_type_index < uint32(LightmapElementTextureType::MAX); texture_type_index++)
            {
                if (!m_atlas_textures[texture_type_index])
                {
                    continue;
                }

                const TResourceHandle<RenderTexture>& atlas_texture = m_atlas_textures[texture_type_index];

                ByteBuffer data;
                atlas_texture->Readback(data);

                if (data.Empty())
                {
                    HYP_LOG(Lightmap, Warning, "Atlas texture {} is empty, skipping save", atlas_texture->GetTexture()->GetName());
                    continue;
                }

                Bitmap<4> bitmap(
                    atlas_texture->GetImage()->GetExtent().x,
                    atlas_texture->GetImage()->GetExtent().y);

                bitmap.SetPixels(data);

                const String filename = HYP_FORMAT("lightmap_atlas_texture_{}_{}.bmp",
                    atlas_texture->GetTexture()->GetName(),
                    uint32(LightmapElementTextureType(texture_type_index)));

                HYP_LOG(Lightmap, Info, "Writing atlas texture {} to file {}", atlas_texture->GetTexture()->GetName(), filename);

                bool res = bitmap.Write(filename.Data());

                if (!res)
                {
                    HYP_LOG(Lightmap, Error, "Failed to write atlas texture {} to file", atlas_texture->GetTexture()->GetName());
                }
            }
        },
        /* force_owner_thread */ true);
}

#pragma endregion RenderLightmapVolume

HYP_DESCRIPTOR_SSBO(Global, LightmapVolumesBuffer, 1, ~0u, false);

} // namespace hyperion
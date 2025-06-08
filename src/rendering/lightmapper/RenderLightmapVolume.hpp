/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERING_LIGHTMAP_VOLUME_HPP
#define HYPERION_RENDERING_LIGHTMAP_VOLUME_HPP

#include <core/containers/Array.hpp>

#include <rendering/RenderResource.hpp>

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RenderObject.hpp>

namespace hyperion {

class LightmapVolume;
struct LightmapElement;
class RenderTexture;

struct LightmapVolumeShaderData
{
    Vec4f aabb_max;
    Vec4f aabb_min;

    uint32 texture_index;
    uint32 _pad0;
    uint32 _pad1;
    uint32 _pad2;
};

class RenderLightmapVolume final : public RenderResourceBase
{
public:
    RenderLightmapVolume(LightmapVolume* lightmap_volume);
    virtual ~RenderLightmapVolume() override;

    HYP_FORCE_INLINE LightmapVolume* GetLightmapVolume() const
    {
        return m_lightmap_volume;
    }

    void SetBufferData(const LightmapVolumeShaderData& buffer_data);

    void BuildAtlasTextures(Array<TResourceHandle<RenderTexture>>&& atlas_textures, const Array<LightmapElement>& elements);

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

    virtual GPUBufferHolderBase* GetGPUBufferHolder() const override;

private:
    void UpdateBufferData();

    LightmapVolume* m_lightmap_volume;
    LightmapVolumeShaderData m_buffer_data;
    Array<TResourceHandle<RenderTexture>> m_atlas_textures;
};

} // namespace hyperion

#endif

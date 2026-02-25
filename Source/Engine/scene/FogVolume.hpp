/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <scene/Volume.hpp>

namespace Hyperion {

class Texture;

HYP_CLASS()
class HYP_API FogVolume final : public VolumeBase
{
    HYP_OBJECT_BODY(FogVolume);

public:
    static constexpr uint32 MaxVolumeTextureExtent = 32;

    FogVolume();

    explicit FogVolume(const BoundingBox& localBounds);

    FogVolume(const FogVolume&) = delete;
    FogVolume& operator=(const FogVolume&) = delete;

    ~FogVolume() override;

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Texture>& GetVolumeTexture() const
    {
        return m_volumeTexture;
    }

    HYP_METHOD()
    void SetTextures(
        const Handle<Texture>& volumeTexture,
        const Handle<Texture>& noiseTexture);

    void UpdateRenderProxy(class RenderProxyFogVolume* proxy);

private:
    void Init() override;

    HYP_FIELD()
    Handle<Texture> m_volumeTexture;

    HYP_FIELD()
    Handle<Texture> m_noiseTexture;
};

} // namespace Hyperion

/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/Volume.hpp>

namespace hyperion {

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
    void SetVolumeTexture(const Handle<Texture>& texture);

    void UpdateRenderProxy(class RenderProxyFogVolume* proxy);

private:
    void Init() override;

    HYP_FIELD()
    Handle<Texture> m_volumeTexture;
};

} // namespace hyperion

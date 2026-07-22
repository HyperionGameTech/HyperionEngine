/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/Volume.hpp>

namespace Hyperion {

class Texture;

HYP_CLASS()
class ENGINE_API FogVolume final : public VolumeBase
{
    HYP_OBJECT_BODY(FogVolume);

public:
    static constexpr uint32 MaxVolumeTextureExtent = 64;
    static constexpr uint32 MaxNoiseTextureExtent = 32;


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

    void UpdateRenderProxy(struct RenderProxyFogVolume* proxy);

#ifdef HYP_EDITOR
    HYP_METHOD(EditorOnly, EditAction = "Rebake")
    void Rebake();
#endif

private:
    void Init() override;

    HYP_FIELD()
    Handle<Texture> m_volumeTexture;

    HYP_FIELD()
    Handle<Texture> m_noiseTexture;
};

} // namespace Hyperion

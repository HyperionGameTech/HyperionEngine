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

    HYP_METHOD(Property = "VolumeTexture")
    HYP_FORCE_INLINE const Handle<Texture>& GetVolumeTexture() const
    {
        return m_volumeTexture;
    }

    HYP_METHOD(Property = "VolumeTexture")
    void SetVolumeTexture(const Handle<Texture>& volumeTexture);

    HYP_METHOD(Property = "NoiseTexture")
    HYP_FORCE_INLINE const Handle<Texture>& GetNoiseTexture() const
    {
        return m_noiseTexture;
    }

    HYP_METHOD(Property = "NoiseTexture")
    void SetNoiseTexture(const Handle<Texture>& noiseTexture);

    HYP_METHOD()
    void SetTextures(
        const Handle<Texture>& volumeTexture,
        const Handle<Texture>& noiseTexture);

    // Medium

    /*! \brief Extinction per metre where the noise map is 1. */
    HYP_METHOD(Property = "Density")
    HYP_FORCE_INLINE float GetDensity() const
    {
        return m_density;
    }

    HYP_METHOD(Property = "Density")
    void SetDensity(float density);

    /*! \brief Fraction of the extinction that scatters rather than absorbs, per channel. */
    HYP_METHOD(Property = "Albedo")
    HYP_FORCE_INLINE const Vec3f& GetAlbedo() const
    {
        return m_albedo;
    }

    HYP_METHOD(Property = "Albedo")
    void SetAlbedo(const Vec3f& albedo);

    /*! \brief Henyey-Greenstein g of the forward lobe (bright looking toward the sun). */
    HYP_METHOD(Property = "PhaseForward")
    HYP_FORCE_INLINE float GetPhaseForward() const
    {
        return m_phaseForward;
    }

    HYP_METHOD(Property = "PhaseForward")
    void SetPhaseForward(float phaseForward);

    /*! \brief Henyey-Greenstein g of the backward lobe. */
    HYP_METHOD(Property = "PhaseBackward")
    HYP_FORCE_INLINE float GetPhaseBackward() const
    {
        return m_phaseBackward;
    }

    HYP_METHOD(Property = "PhaseBackward")
    void SetPhaseBackward(float phaseBackward);

    /*! \brief How much of the scattering goes to the backward lobe. */
    HYP_METHOD(Property = "PhaseBlend")
    HYP_FORCE_INLINE float GetPhaseBlend() const
    {
        return m_phaseBlend;
    }

    HYP_METHOD(Property = "PhaseBlend")
    void SetPhaseBlend(float phaseBlend);

    /*! \brief Scale on the sun's in-scattering. */
    HYP_METHOD(Property = "SunIntensity")
    HYP_FORCE_INLINE float GetSunIntensity() const
    {
        return m_sunIntensity;
    }

    HYP_METHOD(Property = "SunIntensity")
    void SetSunIntensity(float sunIntensity);

    /*! \brief Scale on the ambient in-scattering (Glimmer's irradiance where it reaches, else a flat grey). */
    HYP_METHOD(Property = "AmbientIntensity")
    HYP_FORCE_INLINE float GetAmbientIntensity() const
    {
        return m_ambientIntensity;
    }

    HYP_METHOD(Property = "AmbientIntensity")
    void SetAmbientIntensity(float ambientIntensity);

    /*! \brief Metres over which the density fades out toward the volume's box. */
    HYP_METHOD(Property = "EdgeFade")
    HYP_FORCE_INLINE float GetEdgeFade() const
    {
        return m_edgeFade;
    }

    HYP_METHOD(Property = "EdgeFade")
    void SetEdgeFade(float edgeFade);

    void UpdateRenderProxy(struct RenderProxyFogVolume* proxy);

    ///Per-swatch stuff

    static Name GetVolumeTexturePropertyName()
    {
        return NAME("VolumeTexture");
    }

    static Name GetNoiseTexturePropertyName()
    {
        return NAME("NoiseTexture");
    }

    static Name BuildVolumeTextureName(Name volumeName, Name swatchName);
    static Name BuildNoiseTextureName(Name volumeName, Name swatchName);

    Handle<Texture> GetVolumeTextureForSwatch(Name swatchName) const;
    Handle<Texture> GetNoiseTextureForSwatch(Name swatchName) const;

    void SetTexturesForSwatch(
        const Handle<Texture>& volumeTexture,
        const Handle<Texture>& noiseTexture,
        Name swatchName);

#ifdef HYP_EDITOR
    HYP_METHOD(EditorOnly)
    Array<Name> GetBakedSwatchNames() const;
#endif // HYP_EDITOR

#ifdef HYP_EDITOR
    HYP_METHOD(EditorOnly, EditorAction = "Bake Fog Texture")
    void Rebake();
#endif

private:
    HYP_FIELD(Property = "VolumeTexture")
    Handle<Texture> m_volumeTexture;

    HYP_FIELD(Property = "NoiseTexture")
    Handle<Texture> m_noiseTexture;

    HYP_FIELD(Property = "Density")
    float m_density = 0.04f;

    HYP_FIELD(Property = "Albedo")
    Vec3f m_albedo = Vec3f(0.5f);

    HYP_FIELD(Property = "PhaseForward")
    float m_phaseForward = 0.8f;

    HYP_FIELD(Property = "PhaseBackward")
    float m_phaseBackward = -0.3f;

    HYP_FIELD(Property = "PhaseBlend")
    float m_phaseBlend = 0.2f;

    HYP_FIELD(Property = "SunIntensity")
    float m_sunIntensity = 1.4f;

    HYP_FIELD(Property = "AmbientIntensity")
    float m_ambientIntensity = 0.6f;

    HYP_FIELD(Property = "EdgeFade")
    float m_edgeFade = 6.0f;
};

} // namespace Hyperion

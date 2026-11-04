/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/Volume.hpp>

namespace Hyperion {

HYP_CLASS()
class ENGINE_API FogVolume final : public VolumeBase
{
    HYP_OBJECT_BODY(FogVolume);

public:
    FogVolume();

    explicit FogVolume(const BoundingBox& localBounds);

    FogVolume(const FogVolume&) = delete;
    FogVolume& operator=(const FogVolume&) = delete;

    ~FogVolume() override;

    HYP_METHOD(Property = "Density")
    HYP_FORCE_INLINE float GetDensity() const
    {
        return m_density;
    }

    HYP_METHOD(Property = "Density")
    void SetDensity(float density);

    HYP_METHOD(Property = "Albedo")
    HYP_FORCE_INLINE const Vec3f& GetAlbedo() const
    {
        return m_albedo;
    }

    HYP_METHOD(Property = "Albedo")
    void SetAlbedo(const Vec3f& albedo);

    HYP_METHOD(Property = "PhaseForward")
    HYP_FORCE_INLINE float GetPhaseForward() const
    {
        return m_phaseForward;
    }

    HYP_METHOD(Property = "PhaseForward")
    void SetPhaseForward(float phaseForward);

    HYP_METHOD(Property = "PhaseBackward")
    HYP_FORCE_INLINE float GetPhaseBackward() const
    {
        return m_phaseBackward;
    }

    HYP_METHOD(Property = "PhaseBackward")
    void SetPhaseBackward(float phaseBackward);

    HYP_METHOD(Property = "PhaseBlend")
    HYP_FORCE_INLINE float GetPhaseBlend() const
    {
        return m_phaseBlend;
    }

    HYP_METHOD(Property = "PhaseBlend")
    void SetPhaseBlend(float phaseBlend);

    HYP_METHOD(Property = "SunIntensity")
    HYP_FORCE_INLINE float GetSunIntensity() const
    {
        return m_sunIntensity;
    }

    HYP_METHOD(Property = "SunIntensity")
    void SetSunIntensity(float sunIntensity);

    HYP_METHOD(Property = "AmbientIntensity")
    HYP_FORCE_INLINE float GetAmbientIntensity() const
    {
        return m_ambientIntensity;
    }

    HYP_METHOD(Property = "AmbientIntensity")
    void SetAmbientIntensity(float ambientIntensity);

    HYP_METHOD(Property = "EdgeFade")
    HYP_FORCE_INLINE float GetEdgeFade() const
    {
        return m_edgeFade;
    }

    HYP_METHOD(Property = "EdgeFade")
    void SetEdgeFade(float edgeFade);

    void UpdateRenderProxy(struct RenderProxyFogVolume* proxy);

private:
    HYP_FIELD(Property = "Density")
    float m_density = 0.04f;

    HYP_FIELD(Property = "Albedo")
    Vec3f m_albedo = Vec3f(0.95f);

    HYP_FIELD(Property = "PhaseForward")
    float m_phaseForward = 0.8f;

    HYP_FIELD(Property = "PhaseBackward")
    float m_phaseBackward = -0.3f;

    HYP_FIELD(Property = "PhaseBlend")
    float m_phaseBlend = 0.2f;

    HYP_FIELD(Property = "SunIntensity")
    float m_sunIntensity = 1.0f;

    HYP_FIELD(Property = "AmbientIntensity")
    float m_ambientIntensity = 1.0f;

    HYP_FIELD(Property = "EdgeFade")
    float m_edgeFade = 6.0f;
};

} // namespace Hyperion

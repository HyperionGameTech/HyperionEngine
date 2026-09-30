/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/Glimmer/GlimmerSurfaceCache.hpp>
#include <Rendering/Glimmer/GlimmerSpanCache.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>

namespace Hyperion {

class Texture;
class EnvProbe;
class GlimmerBLASCache;
class GlimmerTLAS;
class GlimmerFootprintMask;
class GlimmerSHVolume;

// the near field only: 2 m and 4 m columns, so out to +-64 m; the SH volume covers what's past that
static constexpr uint32 GlimmerProbeCascades = 2;
static constexpr uint32 GlimmerProbeGrid = 32;
static constexpr uint32 GlimmerProbeLayers = 4;
static constexpr uint32 GlimmerProbesPerCascade = GlimmerProbeGrid * GlimmerProbeGrid * GlimmerProbeLayers;
static constexpr uint32 GlimmerProbeRays = 16;

// Must match GlimmerProbeCascade in Shaders/Glimmer/SWRT/GlimmerProbeTypes.hlsli
struct GlimmerProbeCascadeShaderData
{
    Vec4i gridOrigin; // xy = absolute column of the grid's first column, z = 1 once the cascade has been traced
    Vec4f params;     // x = column spacing, y = layer scale, z = hysteresis
};

// Must match GlimmerProbeVolume in Shaders/Glimmer/SWRT/GlimmerProbeTypes.hlsli
struct GlimmerProbeVolumeShaderData
{
    GlimmerProbeCascadeShaderData cascades[GlimmerProbeCascades];
    Vec4u info;        // x = number of cascades, y = rays per probe, z = frame, w = 1 when the volume can be sampled
    Vec4f rayRotation; // quaternion applied to this frame's ray directions
    Vec4f params;      // x = base height where there's no ground, y = unused, z = escape radiance clamp, w = max ray distance
    Vec4f nearField;   // x = cascades traced with SWRT, y = SWRT reach in spacings, z = SWRT instances, w = ground albedo
};

struct GlimmerSWRTProbeUpdateInputs
{
    Vec3f viewerPosition;
    const GlimmerSurfaceCache* surfaceCache = nullptr;
    const GlimmerSpanCache* spanCache = nullptr;
    const GlimmerBLASCache* blasCache = nullptr;
    const GlimmerTLAS* tlas = nullptr;
    const GlimmerFootprintMask* footprintMask = nullptr; // of the tlas; tells the trace where SWRT has anything to hit
    const GlimmerSHVolume* shVolume = nullptr;           // the far field, for the bounce at hits past the probes; may be nullptr
    EnvProbe* skyProbe = nullptr;
};

/*! \brief Glimmer's near field: a terrain following probe clipmap, cascades of columns that scroll with the viewer, each column
 *  holding a few probes stacked up from the ground. Probes are traced against the SWRT scene and the heightfield and store L1 irradiance.
 *  Render thread only. */
class GlimmerSWRTProbeVolume
{
public:
    GlimmerSWRTProbeVolume();
    GlimmerSWRTProbeVolume(const GlimmerSWRTProbeVolume& other) = delete;
    GlimmerSWRTProbeVolume& operator=(const GlimmerSWRTProbeVolume& other) = delete;
    ~GlimmerSWRTProbeVolume();

    void Update(Frame* frame, const GlimmerSWRTProbeUpdateInputs& inputs);

    HYP_FORCE_INLINE const GlimmerProbeVolumeShaderData& GetShaderData() const
    {
        return m_shaderData;
    }

    HYP_FORCE_INLINE bool IsReady() const
    {
        return m_shaderData.info.w != 0;
    }

    /*! \brief L1 per colour channel, a slab each stacked along z (see GlimmerProbeTypes.hlsli) */
    const GpuImageViewRef& GetSHImageView() const;
    const GpuImageViewRef& GetStateImageView() const;
    const GpuImageViewRef& GetBaseImageView() const;

private:
    void CreateResources();
    void ScrollCascades(const Vec3f& viewerPosition, uint32& outScrolledMask);
    float UpdateFallbackBase(float viewerHeight);

    Handle<Texture> m_shTexture;
    Handle<Texture> m_stateTexture;
    Handle<Texture> m_trendTexture; // luminance over each probe's last few updates, to tell real change from noise
    Handle<Texture> m_baseTexture;

    GpuBufferRef m_raysBuffer;
    GpuBufferRef m_rayHitsBuffer; // between the trace and shade passes

    Vec2i m_gridOrigins[GlimmerProbeCascades];
    bool m_hasGridOrigins;

    float m_fallbackBase;
    bool m_hasFallbackBase;

    uint32 m_frameIndex;

    GlimmerProbeVolumeShaderData m_shaderData;
};

} // namespace Hyperion

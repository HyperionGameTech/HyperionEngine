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

static constexpr uint32 GlimmerProbeCascades = 6;
static constexpr uint32 GlimmerProbeGrid = 32;
static constexpr uint32 GlimmerProbeLayers = 4;
static constexpr uint32 GlimmerProbesPerCascade = GlimmerProbeGrid * GlimmerProbeGrid * GlimmerProbeLayers;
static constexpr uint32 GlimmerProbeRays = 32;

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
    EnvProbe* skyProbe = nullptr;
};

/*! \brief Glimmer's terrain following probe clipmap: cascades of columns that scroll with the viewer, each column holding
 *  a few probes stacked up from the ground. Probes are traced against the SWRT scene and the heightfield and store L1 irradiance.
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

    const GpuImageViewRef& GetSHImageView(uint32 channel) const;
    const GpuImageViewRef& GetStateImageView() const;
    const GpuImageViewRef& GetBaseImageView() const;

private:
    void CreateResources();
    void ScrollCascades(const Vec3f& viewerPosition, uint32& outScrolledMask);

    Handle<Texture> m_shTextures[3];
    Handle<Texture> m_stateTexture;
    Handle<Texture> m_baseTexture;

    GpuBufferRef m_raysBuffer;

    Vec2i m_gridOrigins[GlimmerProbeCascades];
    bool m_hasGridOrigins;

    uint32 m_frameIndex;

    GlimmerProbeVolumeShaderData m_shaderData;
};

} // namespace Hyperion

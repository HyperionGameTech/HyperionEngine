/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderProxy.hpp>

namespace Hyperion {

class Texture;
class RenderProxyList;
class CBufferAllocator;

// matches CloudWeatherMap in Shaders/Include/Clouds.hlsli
struct CloudWeatherMapShaderData
{
    // cloud-space XZ (world position plus the weather wind offset) of the map's min corner
    Vec2f origin;
    float worldExtent;
    uint32 dimensions;

    // origin in weather noise cells, wrapped to the noise period in double precision so the noise stays precise far from the world origin
    Vec2f noiseOrigin;
    uint32 weatherNoisePeriodCells;
    uint32 evolutionNoisePeriodCells;

    // individual clouds get their own lattice, sized to repeat exactly when the weather noise does
    Vec2f cellNoiseOrigin;
    uint32 cellNoisePeriodCells;
    float cellWorldSize;

    float evolutionSecondsPerCell;
    uint32 debugShadows;

    // the map is a two slice array of evolution keyframes; samplers blend from the previous slice to the next
    uint32 previousKeyframeSlice;
    uint32 nextKeyframeSlice;

    float keyframeBlend;

    // only meaningful for the dispatch that fills generateSlice
    float generateEvolutionTime;
    uint32 generateSlice;
    float _pad0;
};

static_assert(sizeof(CloudWeatherMapShaderData) == 80);

// matches CloudShadowMap in Shaders/Include/Clouds.hlsli
struct CloudShadowMapShaderData
{
    // world XZ of the map's min corner, on the y = 0 plane
    Vec2f origin;
    float worldExtent;
    uint32 dimensions;

    // xyz = the sun direction the map is built for, w unused (Vec3f is 16 bytes, which would break the shader layout)
    Vec4f directionToSun;

    uint32 isValid;

    // only meaningful for the dispatch filling rows [generateRowStart, generateRowStart + generateRowCount)
    uint32 generateRowStart;
    uint32 generateRowCount;
    uint32 _pad0;
};

static_assert(sizeof(CloudShadowMapShaderData) == 48);

/*! \brief Render thread cloud resources shared by every view. Holds the weather map: a camera-centered 2D map of
 *  coverage (r), cloud type (g) and density (b), used for cloud shadows and later as the input to the cloud raymarch. */
class CloudResources
{
public:
    static constexpr uint32 WeatherMapDimensions = 512;
    static constexpr float WeatherMapWorldExtent = 64000.0f;

    // the map recenters in steps of this many texels. A power of two keeps mips up to log2 of it aligned across the shift,
    // so blurred lookups (shadow softness) sample identical data before and after
    static constexpr uint32 WeatherMapOriginSnapTexels = 16;

    static constexpr uint32 NumWeatherKeyframes = 2;

    // evolution seconds between keyframes. Shapes change slowly enough that a crossfade over this is invisible
    static constexpr float WeatherKeyframeSeconds = 8.0f;

    // tileable 3D noise: shape is Perlin-Worley (r) plus Worley fbm at three frequencies (gba), detail is Worley fbm (rgb)
    static constexpr uint32 ShapeNoiseDimensions = 128;
    static constexpr uint32 DetailNoiseDimensions = 32;

    // the whole shape volume in one go is seconds of GPU time, so it's filled a few z slices per frame
    static constexpr uint32 ShapeNoiseSlicesPerFrame = 4;

    // top-down cloud transmittance around the camera, for ground and fog shadows
    static constexpr uint32 ShadowMapDimensions = 512;
    static constexpr float ShadowMapWorldExtent = 16000.0f;

    // recentering rebuilds the whole map in one frame, so it only happens in steps this large
    static constexpr uint32 ShadowMapOriginSnapTexels = 32;

    // rows refreshed per frame, so the map follows evolution and wind within ShadowMapDimensions / this frames
    static constexpr uint32 ShadowMapRowsPerFrame = 64;

    // a sun direction change bigger than this (about 1.5 degrees) rebuilds the whole map
    static constexpr float ShadowMapSunRebuildDot = 0.9997f;

    // the sky probe is small and convolved afterwards, so it gets by with a much cheaper march than the sky
    static constexpr uint32 SkyProbeTraceSteps = 24;
    static constexpr uint32 SkyProbeLightSteps = 4;

    CloudResources();

    CloudResources(const CloudResources&) = delete;
    CloudResources& operator=(const CloudResources&) = delete;

    ~CloudResources();

    void Shutdown();

    /*! \brief Advances the weather map keyframes, generating a slice when evolution reaches the next keyframe or rebuilding
     *  both when the map moved or its settings changed, then refreshes the cloud shadow map. Call once per frame on the render
     *  thread, before any pass samples them. \p cloudVolumeProxy may be null; a zero \p directionToSun means no sun. */
    void Update(Frame* frame, const RenderProxyEffectVolume* cloudVolumeProxy, const Vec3f& cameraPosition, const Vec3f& directionToSun);

    /*! \brief The weather map (Texture2DArray), or a placeholder array while clouds are inactive. */
    const GpuImageViewRef& GetWeatherMapView() const;

    /*! \brief The cloud shadow map (Texture2D, r = transmittance), or a placeholder while it isn't valid. */
    const GpuImageViewRef& GetShadowMapView() const;

    /*! \brief Writes the cloud volume's bufferData, then the weather map and shadow map placements - the layout of
     *  CloudVolume + CloudWeatherMap + CloudShadowMap in Clouds.hlsli.
     *  Writes zeroed volume data (clouds disabled) when \p rpl has no cloud volume or clouds are inactive. */
    void WriteShaderData(CBufferAllocator& cbufferAllocator, RenderProxyList& rpl) const;

    /*! \brief Composites clouds over a sky probe capture, traced from the camera and cloud volume of the last Update().
     *  \p skyTexture is the cloud-free capture (a cubemap), \p skyProbeTexture the capture cubemap to write into.
     *  Records into its own command recorder, so it runs after anything recorded before the call. */
    void CompositeSkyProbe(Texture* skyTexture, Texture* skyProbeTexture, const EnvProbeShaderData& skyProbeShaderData, const LightShaderData* sunShaderData);

    /*! \brief True when the last Update() found enabled clouds and the weather map has content. */
    HYP_FORCE_INLINE bool IsActive() const
    {
        return m_isActive;
    }

    /*! \brief True once both noise volumes are fully generated, mips included. Until then the noise views are placeholders. */
    HYP_FORCE_INLINE bool IsNoiseReady() const
    {
        return m_noiseReady;
    }

    /*! \brief The shape noise (Texture3D), or a placeholder until IsNoiseReady(). */
    const GpuImageViewRef& GetShapeNoiseView() const;

    /*! \brief The detail noise (Texture3D), or a placeholder until IsNoiseReady(). */
    const GpuImageViewRef& GetDetailNoiseView() const;

private:
    void CreateWeatherMap();
    void CreateNoiseTextures();

    /*! \brief Continues noise generation: the detail volume and the first shape slices on the first call, more shape slices
     *  on each call after, then the mip chains. No-op once the noise is ready. */
    void UpdateNoise(Frame* frame);

    void GenerateNoiseSlices(Frame* frame, Texture* noiseTexture, bool isShapeNoise, uint32 sliceStart, uint32 sliceCount);

    /*! \brief Box filters each mip from the one above it. The engine's GenerateMipmaps is 2D only on DX12. */
    void GenerateNoiseMips(Frame* frame, Texture* noiseTexture);

    /*! \brief Fills one keyframe slice with the weather at \p evolutionTime. Caller regenerates mips after its last slice. */
    void GenerateWeatherMapSlice(Frame* frame, const RenderProxyEffectVolume& cloudVolumeProxy, uint32 slice, float evolutionTime);

    bool NeedsFullWeatherMapRebuild(const CloudVolumeShaderData& cloudData, const Vec2f& origin) const;

    void CreateShadowMap();

    /*! \brief Rebuilds the whole shadow map when it moved or the sun turned, otherwise refreshes the next band of rows. */
    void UpdateShadowMap(Frame* frame, const RenderProxyEffectVolume& cloudVolumeProxy, const Vec3f& cameraPosition, const Vec3f& directionToSun);

    void GenerateShadowMapRows(Frame* frame, const RenderProxyEffectVolume& cloudVolumeProxy, uint32 rowStart, uint32 rowCount);

    Handle<Texture> m_weatherMap;

    Handle<Texture> m_shadowMap;
    CloudShadowMapShaderData m_shadowMapShaderData;
    uint32 m_shadowMapNextRow;

    Handle<Texture> m_shapeNoise;
    Handle<Texture> m_detailNoise;

    uint32 m_shapeNoiseSlicesGenerated;
    bool m_noiseReady;

    CloudWeatherMapShaderData m_weatherMapShaderData;

    // the cloud parameters the weather map was last fully rebuilt with
    CloudVolumeShaderData m_generatedCloudData;

    // evolution time the previous keyframe slice holds; the next slice holds this plus WeatherKeyframeSeconds
    float m_previousKeyframeEvolutionTime;

    uint32 m_lastActiveFrame;

    // from the last active Update(), for passes that render without the camera's proxy list
    EffectVolumeShaderData m_cloudVolumeShaderData;
    Vec3f m_cameraPosition;

    bool m_hasGeneratedWeatherMap;
    bool m_isActive;
};

} // namespace Hyperion

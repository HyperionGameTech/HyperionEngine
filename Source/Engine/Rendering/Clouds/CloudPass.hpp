/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>
#include <Core/Math/Mat4f.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderProxy.hpp>

namespace Hyperion {

class Texture;
class Mesh;
class CBufferAllocator;
struct RenderSetup;
struct RenderProxyCamera;

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

/*! \brief A GBuffer view's volumetric clouds, owned by its DeferredPassData.
 *  Update() keeps a camera centered weather map and cloud shadow map current for lighting, fog and the sky probe.
 *  Render() traces one pixel of every 2x2 half resolution block per frame, reconstructs the rest from last frame's
 *  reprojected result, and composites it over the sky pixels. */
class CloudPass
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

    // a view change bigger than this makes last frame's clouds useless to reproject
    static constexpr float CameraCutMinDirectionDot = 0.9f;
    static constexpr float CameraCutMaxMoveDistance = 50.0f;

    explicit CloudPass(const Vec2u& extent);

    CloudPass(const CloudPass&) = delete;
    CloudPass& operator=(const CloudPass&) = delete;

    ~CloudPass();

    void Create();

    /*! \brief Recreates the trace and history textures for a new view size. The weather map, shadow map and noise are kept. */
    void Resize(const Vec2u& extent);

    /*! \brief Advances the noise, weather map and cloud shadow map for renderSetup.view. Call once per frame, before lighting,
     *  fog or the sky probe sample them. */
    void Update(Frame* frame, const RenderSetup& renderSetup);

    /*! \brief Traces and reconstructs renderSetup.view's clouds, then blends them over the sky pixels of renderSetup.framebuffer.
     *  renderSetup.passData must be the view's DeferredPassData. */
    void Render(Frame* frame, const RenderSetup& renderSetup);

    /*! \brief Writes the cloud volume's bufferData, then the weather map and shadow map placements - the layout of
     *  CloudVolume + CloudWeatherMap + CloudShadowMap in Clouds.hlsli. Zeroed volume data (clouds disabled) while inactive. */
    void WriteShaderData(CBufferAllocator& cbufferAllocator) const;

    /*! \brief The weather map (Texture2DArray), or a placeholder array while inactive. */
    const GpuImageViewRef& GetWeatherMapView() const;

    /*! \brief The cloud shadow map (Texture2D, r = transmittance), or a placeholder while it isn't valid. */
    const GpuImageViewRef& GetShadowMapView() const;

    /*! \brief True when the last Update() found enabled clouds and the weather map has content. */
    HYP_FORCE_INLINE bool IsActive() const
    {
        return m_isActive;
    }

    /*! \brief True when active and the noise is ready, so CompositeSkyProbe() has something to draw. */
    HYP_FORCE_INLINE bool CanCompositeSkyProbe() const
    {
        return m_isActive && m_noiseReady;
    }

    /*! \brief Composites clouds over a sky probe capture, traced from this view's camera. \p skyTexture is the cloud-free
     *  capture (a cubemap), \p skyProbeTexture the capture cubemap to write into.
     *  Records into its own command recorder, so it runs after anything recorded before the call. */
    void CompositeSkyProbe(Texture* skyTexture, Texture* skyProbeTexture, const EnvProbeShaderData& skyProbeShaderData, const LightShaderData* sunShaderData);

private:
    void CreateTraceTextures();

    void CreateNoiseTextures();

    /*! \brief Continues noise generation: the detail volume and the first shape slices on the first call, more shape slices
     *  on each call after, then the mip chains. No-op once the noise is ready. */
    void UpdateNoise(Frame* frame);

    void GenerateNoiseSlices(Frame* frame, Texture* noiseTexture, bool isShapeNoise, uint32 sliceStart, uint32 sliceCount);

    /*! \brief Box filters each mip from the one above it. The engine's GenerateMipmaps is 2D only on DX12. */
    void GenerateNoiseMips(Frame* frame, Texture* noiseTexture);

    const GpuImageViewRef& GetShapeNoiseView() const;
    const GpuImageViewRef& GetDetailNoiseView() const;

    void UpdateWeatherMap(Frame* frame, const RenderProxyEffectVolume& cloudVolumeProxy);

    bool NeedsFullWeatherMapRebuild(const CloudVolumeShaderData& cloudData, const Vec2f& origin) const;

    /*! \brief Fills one keyframe slice with the weather at \p evolutionTime. Caller regenerates mips after its last slice. */
    void GenerateWeatherMapSlice(Frame* frame, const RenderProxyEffectVolume& cloudVolumeProxy, uint32 slice, float evolutionTime);

    /*! \brief Rebuilds the whole shadow map when it moved or the sun turned, otherwise refreshes the next band of rows. */
    void UpdateShadowMap(Frame* frame, const RenderProxyEffectVolume& cloudVolumeProxy, const Vec3f& directionToSun);

    void GenerateShadowMapRows(Frame* frame, const RenderProxyEffectVolume& cloudVolumeProxy, uint32 rowStart, uint32 rowCount);

    /*! \brief Returns false when there was nothing to trace (no camera yet), in which case there's nothing to composite. */
    bool Trace(Frame* frame, const RenderSetup& renderSetup);
    void Reconstruct(Frame* frame, const RenderProxyCamera& cameraProxy, bool isHistoryValid, const Vec2u& traceOffset);
    void Composite(Frame* frame, const RenderSetup& renderSetup);

    Vec2u m_extent;
    Vec2u m_historyExtent;
    Vec2u m_traceExtent;

    // rgb = premultiplied cloud light, a = transmittance
    Handle<Texture> m_traceTexture;

    // transmittance weighted distance to the clouds along each ray, used to reproject
    Handle<Texture> m_traceDistanceTexture;

    // ping-ponged half resolution reconstruction; m_historyIndex is the most recently written
    Handle<Texture> m_historyTextures[2];
    Handle<Texture> m_historyDistanceTextures[2];
    uint32 m_historyIndex;

    Mat4f m_previousViewProjection;
    Vec3f m_previousCameraPosition;
    Vec3f m_previousCameraDirection;

    uint32 m_lastRenderedFrame;

    Handle<Mesh> m_quadMesh;

    Handle<Texture> m_shapeNoise;
    Handle<Texture> m_detailNoise;

    uint32 m_shapeNoiseSlicesGenerated;
    bool m_noiseReady;

    Handle<Texture> m_weatherMap;
    CloudWeatherMapShaderData m_weatherMapShaderData;

    // the cloud parameters the weather map was last fully rebuilt with
    CloudVolumeShaderData m_generatedCloudData;

    // evolution time the previous keyframe slice holds; the next slice holds this plus WeatherKeyframeSeconds
    float m_previousKeyframeEvolutionTime;

    bool m_hasGeneratedWeatherMap;

    Handle<Texture> m_shadowMap;
    CloudShadowMapShaderData m_shadowMapShaderData;
    uint32 m_shadowMapNextRow;

    // the cloud volume and camera as of the last Update()
    EffectVolumeShaderData m_cloudVolumeShaderData;
    Vec3f m_cameraPosition;

    bool m_isActive;
};

} // namespace Hyperion

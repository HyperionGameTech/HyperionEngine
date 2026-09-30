/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/Glimmer/GlimmerChannel.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/FixedArray.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector4.hpp>

namespace Hyperion {

class Texture;

// Must match GlimmerGroundLevel in Shaders/Glimmer/GlimmerCommon.hlsli
struct GlimmerGroundLevelShaderData
{
    Vec4i validRect; // absolute texels, xy = min, zw = max (exclusive)
    Vec4f params;    // x = texel size, y = 1 / texel size
};

struct GlimmerGroundShaderData
{
    GlimmerGroundLevelShaderData levels[GlimmerGroundLevels];
};

static_assert(sizeof(GlimmerGroundShaderData) == 32 * GlimmerGroundLevels);

// Must match GlimmerTerrainPatch in Shaders/Glimmer/GlimmerCommon.hlsli
struct GlimmerTerrainPatchShaderData
{
    Vec4f worldToObject0;
    Vec4f worldToObject2;
    Vec4f boundsXZ; // xy = world xz min, zw = max
    Vec4u data;     // x = material index
};

static_assert(sizeof(GlimmerTerrainPatchShaderData) == 64);

static constexpr uint32 GlimmerMaxTerrainPatches = 512;

/*! \brief Render side of Glimmer's heightfield surface cache: wrapped clipmap textures centred on the viewer.
 *  Holds the ground heights, filled from exact terrain heights sampled on the sim thread, and the ground albedo,
 *  filled on the GPU from whichever terrain patches the renderer has seen. Render thread only. */
class GlimmerSurfaceCache
{
public:
    GlimmerSurfaceCache();
    GlimmerSurfaceCache(const GlimmerSurfaceCache& other) = delete;
    GlimmerSurfaceCache& operator=(const GlimmerSurfaceCache& other) = delete;
    ~GlimmerSurfaceCache();

    /*! \brief Writes the queued ground uploads into the clipmap, adopts the published windows,
     *  and refreshes the ground albedo from the given terrain patches. */
    void Update(Frame* frame, const GlimmerChannelState& state, Span<const GlimmerGroundUpload> groundUploads, Span<const GlimmerTerrainPatchShaderData> terrainPatches);

    HYP_FORCE_INLINE const GlimmerGroundShaderData& GetGroundShaderData() const
    {
        return m_groundShaderData;
    }

    /*! \brief Texture2DArray<float>, one layer per level, addressed with absolute texel & (resolution - 1). */
    const GpuImageViewRef& GetGroundImageView() const;

    /*! \brief Texture2DArray<float4> laid out like the ground heights. Alpha is 1 where a terrain patch covered the texel,
     *  and texels stay once written, so ground only the main camera ever saw keeps its albedo. */
    const GpuImageViewRef& GetGroundAlbedoImageView() const;

private:
    struct AlbedoLevel
    {
        bool hasFilled = false;
        Vec2i filledOrigin;
    };

    void CreateTextures();
    void UploadTerrainPatches(Frame* frame, Span<const GlimmerTerrainPatchShaderData> terrainPatches);
    void UpdateGroundCover(Frame* frame, const GlimmerChannelState& state);
    void UpdateGroundAlbedo(Frame* frame, const GlimmerChannelState& state);

    Handle<Texture> m_ground;
    Handle<Texture> m_groundAlbedo;

    GpuBufferRef m_terrainPatchesBuffer;

    // per splat layer, the mean albedo of the plants its ground cover grows (a = 1 once known)
    GpuBufferRef m_groundCoverAlbedoBuffer;
    bool m_hasClearedGroundCover;
    Vec4f m_groundCoverCoverage;

    Array<GlimmerTerrainPatchShaderData> m_uploadedTerrainPatches;

    GlimmerGroundShaderData m_groundShaderData;

    FixedArray<AlbedoLevel, GlimmerGroundLevels> m_albedoLevels;
    uint32 m_albedoGeneration;
    uint32 m_albedoFillCounter;
};

} // namespace Hyperion

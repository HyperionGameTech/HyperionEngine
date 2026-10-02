/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/Glimmer/GlimmerChannel.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/FixedArray.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector4.hpp>

namespace Hyperion {

class Texture;

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

struct GlimmerTerrainPatchShaderData
{
    Vec4f worldToObject0;
    Vec4f worldToObject2;
    Vec4f boundsXZ; // xy = world xz min, zw = max
    Vec4u data;     // x = material index
};

static_assert(sizeof(GlimmerTerrainPatchShaderData) == 64);

static constexpr uint32 GlimmerMaxTerrainPatches = 512;

struct GlimmerGroundCoverConstantsKey
{
    Vec4u materials[GlimmerGroundCoverLayers];
    Vec4f weights[GlimmerGroundCoverLayers];
    Vec4f coverage;
};

class GlimmerSurfaceCache final
{
public:
    GlimmerSurfaceCache();

    GlimmerSurfaceCache(const GlimmerSurfaceCache& other) = delete;
    GlimmerSurfaceCache& operator=(const GlimmerSurfaceCache& other) = delete;

    ~GlimmerSurfaceCache();

    void Update(
        Frame* frame,
        const GlimmerChannelState& state,
        Span<const GlimmerGroundUpload> groundUploads,
        Span<const GlimmerTerrainPatchShaderData> terrainPatches);

    HYP_FORCE_INLINE const GlimmerGroundShaderData& GetGroundShaderData() const
    {
        return m_groundShaderData;
    }

    const GpuImageViewRef& GetGroundImageView() const;
    const GpuImageViewRef& GetGroundAlbedoImageView() const;

    HYP_FORCE_INLINE const GlimmerTexelRect& GetUploadedRect(uint32 level) const
    {
        return m_uploadedRects[level];
    }

private:
    struct AlbedoLevel
    {
        bool hasFilled = false;
        Vec2i filledOrigin;
        GlimmerTexelRect unfilledUploads {}; // uploaded heights not yet valid when they arrived
    };

    void CreateTextures();
    void UploadTerrainPatches(Frame* frame, Span<const GlimmerTerrainPatchShaderData> terrainPatches);
    void UpdateGroundCover(Frame* frame, const GlimmerChannelState& state);
    void UpdateGroundAlbedo(Frame* frame, const GlimmerChannelState& state);

    Handle<Texture> m_ground;
    Handle<Texture> m_groundAlbedo;

    GpuBufferRef m_terrainPatchesBuffer;

    GpuBufferRef m_groundCoverAlbedoBuffer;
    bool m_hasClearedGroundCover;
    Vec4f m_groundCoverCoverage;

    Array<GlimmerTerrainPatchShaderData> m_uploadedTerrainPatches;

    GlimmerGroundShaderData m_groundShaderData;
    FixedArray<GlimmerTexelRect, GlimmerGroundLevels> m_uploadedRects;

    FixedArray<AlbedoLevel, GlimmerGroundLevels> m_albedoLevels;
    Array<Vec4f> m_albedoDirtyWorldRects;
    GlimmerGroundCoverConstantsKey m_groundCoverKey;
    bool m_hasGroundCoverChanged;
    uint32 m_albedoGeneration;
    uint32 m_albedoFillCounter;
};

} // namespace Hyperion

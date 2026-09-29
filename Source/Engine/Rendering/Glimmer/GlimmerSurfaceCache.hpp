/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/Glimmer/GlimmerChannel.hpp>

#include <Core/Reflection/Handle.hpp>

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

/*! \brief Render side of Glimmer's heightfield surface cache: wrapped clipmap textures centred on the viewer.
 *  For now it holds the ground level, filled from exact terrain heights sampled on the sim thread. Render thread only. */
class GlimmerSurfaceCache
{
public:
    GlimmerSurfaceCache();
    GlimmerSurfaceCache(const GlimmerSurfaceCache& other) = delete;
    GlimmerSurfaceCache& operator=(const GlimmerSurfaceCache& other) = delete;
    ~GlimmerSurfaceCache();

    /*! \brief Writes the queued ground uploads into the clipmap and adopts the published windows. */
    void Update(Frame* frame, const GlimmerChannelState& state, Span<const GlimmerGroundUpload> groundUploads);

    HYP_FORCE_INLINE const GlimmerGroundShaderData& GetGroundShaderData() const
    {
        return m_groundShaderData;
    }

    /*! \brief Texture2DArray<float>, one layer per level, addressed with absolute texel & (resolution - 1). */
    const GpuImageViewRef& GetGroundImageView() const;

private:
    void CreateTextures();

    Handle<Texture> m_ground;

    GlimmerGroundShaderData m_groundShaderData;
};

} // namespace Hyperion

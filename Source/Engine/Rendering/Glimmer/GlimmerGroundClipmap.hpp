/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/Glimmer/GlimmerChannel.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/FixedArray.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>

namespace Hyperion {

class World;
class TerrainWorldGridLayer;

/*! \brief Sim side of Glimmer's ground heightfield: keeps one window of terrain heights per level centred on the viewer,
 *  sampling the terrain's exact surface for texels as they scroll in, and slowly re-sampling everything to pick up edits.
 *  Sim thread only. */
class GlimmerGroundClipmap
{
public:
    GlimmerGroundClipmap();

    /*! \brief Moves the windows with the viewer and samples up to the frame's budget. */
    void Update(World* world, const Vec3f& viewerPosition, Array<GlimmerGroundUpload>& outUploads);

    /*! \brief Also fills in the terrain's ground cover. */
    void FillState(GlimmerChannelState& outState) const;

    HYP_FORCE_INLINE uint32 GetGeneration() const
    {
        return m_generation;
    }

private:
    using Rect = GlimmerTexelRect;

    struct Level
    {
        bool hasWindow = false;
        Vec2i windowOrigin;
        Rect valid;
        Array<Rect> pending;
        int32 refreshRow = 0;
    };

    void MoveWindow(uint32 levelIndex, const Vec2i& desiredOrigin);
    int32 SampleRect(TerrainWorldGridLayer* terrain, uint32 levelIndex, const Rect& rect, Array<GlimmerGroundUpload>& outUploads) const;

    FixedArray<Level, GlimmerGroundLevels> m_levels;
    TerrainWorldGridLayer* m_terrain;
    uint32 m_generation;
};

} // namespace Hyperion

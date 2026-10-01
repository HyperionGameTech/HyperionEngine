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

class GlimmerGroundClipmap final
{
public:
    GlimmerGroundClipmap();

    void Update(World* world, const Vec3f& viewerPosition, Array<GlimmerGroundUpload>& outUploads);

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

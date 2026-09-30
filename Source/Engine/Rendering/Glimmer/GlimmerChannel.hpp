/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Containers/FixedArray.hpp>

#include <Core/Memory/SharedPtr.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>

namespace Hyperion {

class World;
class Material;

static constexpr uint32 GlimmerGroundLevels = 4;
static constexpr uint32 GlimmerGroundResolution = 256;
static constexpr float GlimmerGroundTexelSize = 2.0f; // level 0, doubling per level
static constexpr float GlimmerNoGroundHeight = -60000.0f;

HYP_FORCE_INLINE float GetGlimmerGroundTexelSize(uint32 level)
{
    return GlimmerGroundTexelSize * float(1u << level);
}

struct GlimmerGroundUpload
{
    uint32 level = 0;
    Vec2i texelMin;
    Vec2u extent;
    Array<float> heights; // row major, extent.x * extent.y
};

// one per terrain splat layer (0 grass, 1 rock, 2 dirt, 3 snow)
static constexpr uint32 GlimmerGroundCoverLayers = 4;
static constexpr uint32 GlimmerGroundCoverMaxMaterials = 4;

/*! \brief What the terrain's ground cover plants over one splat layer, so the ground albedo can take its colour. */
struct GlimmerGroundCoverLayerState
{
    // how much of the ground the plants hide from above where the splat layer is full
    float coverage = 0.0f;

    uint32 numMaterials = 0;
    FixedArray<Handle<Material>, GlimmerGroundCoverMaxMaterials> materials;
    FixedArray<float, GlimmerGroundCoverMaxMaterials> weights {};
};

struct GlimmerGroundLevelState
{
    Vec2i windowOrigin;
    Vec2i validMin;
    Vec2i validMax;
};

struct GlimmerChannelState
{
    Vec3f viewerPosition;
    bool hasViewer = false;

    FixedArray<GlimmerGroundLevelState, GlimmerGroundLevels> groundLevels {};

    // bumped whenever the terrain source is swapped or reset, so the render side drops what it has
    uint32 groundGeneration = 0;

    FixedArray<GlimmerGroundCoverLayerState, GlimmerGroundCoverLayers> groundCover;
};

/*! \brief Carries per world Glimmer data from GlimmerSystem (sim thread) to GlimmerPass (render thread). */
class ENGINE_API GlimmerChannel
{
public:
    static SharedPtr<GlimmerChannel> Get(const World* world);
    static void Register(const World* world, const SharedPtr<GlimmerChannel>& channel);
    static void Unregister(const World* world);

    void Publish(const GlimmerChannelState& state, Array<GlimmerGroundUpload>&& groundUploads);
    void Consume(GlimmerChannelState& outState, Array<GlimmerGroundUpload>& outGroundUploads);

private:
    Mutex m_mutex;
    GlimmerChannelState m_state;
    Array<GlimmerGroundUpload> m_pendingGroundUploads;
};

} // namespace Hyperion

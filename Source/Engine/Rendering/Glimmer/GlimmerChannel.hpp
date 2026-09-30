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
#include <Core/Math/Vector4.hpp>

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

/*! \brief One SWRT probe as read back for Rendering.Glimmer.SWRT.DebugProbes, a record per probe of each cascade in turn, layer major
 *  then z then x within a cascade. Must match GlimmerProbeDebugRecord in Shaders/Glimmer/SWRT/GlimmerSWRTProbeDebug.hlsl */
struct GlimmerProbeDebugRecord
{
    Vec4f position; // xyz = where the probe was last traced, or where it will be when it hasn't been yet
    Vec4u info;     // x = 1 once traced for its column, y = rays that hit a back face, z = rays that started under the ground, w = updates
    Vec4f sh[3];    // L1 irradiance / pi per colour channel
};

static_assert(sizeof(GlimmerProbeDebugRecord) == 80);

/*! \brief Carries per world Glimmer data from GlimmerSystem (sim thread) to GlimmerPass (render thread), and debug readbacks back. */
class ENGINE_API GlimmerChannel
{
public:
    static SharedPtr<GlimmerChannel> Get(const World* world);
    static void Register(const World* world, const SharedPtr<GlimmerChannel>& channel);
    static void Unregister(const World* world);

    void Publish(const GlimmerChannelState& state, Array<GlimmerGroundUpload>&& groundUploads);
    void Consume(GlimmerChannelState& outState, Array<GlimmerGroundUpload>& outGroundUploads);

    /*! \brief Render thread: hands over the latest probe readback, replacing one the sim hasn't taken yet. */
    void PublishProbeDebug(Array<GlimmerProbeDebugRecord>&& records);

    /*! \brief Sim thread: takes the latest probe readback. \return false if there's been none since the last call. */
    bool ConsumeProbeDebug(Array<GlimmerProbeDebugRecord>& outRecords);

private:
    Mutex m_mutex;
    GlimmerChannelState m_state;
    Array<GlimmerGroundUpload> m_pendingGroundUploads;

    Array<GlimmerProbeDebugRecord> m_probeDebugRecords;
    bool m_hasProbeDebugRecords = false;
};

} // namespace Hyperion

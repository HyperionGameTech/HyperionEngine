/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/Glimmer/GlimmerMemory.hpp>

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
    Array<float, GlimmerAllocator> heights; // row major, extent.x * extent.y
};

// one per terrain splat layer (0 grass, 1 rock, 2 dirt, 3 snow)
static constexpr uint32 GlimmerGroundCoverLayers = 4;
static constexpr uint32 GlimmerGroundCoverMaxMaterials = 4;

struct GlimmerGroundCoverLayerState
{
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

static constexpr uint32 GlimmerMaxLocalLights = 32; // GLIMMER_MAX_LOCAL_LIGHTS

struct GlimmerLocalLightState
{
    uint64 key = 0;

    /// position in xyz
    /// radius in the w
    Vec4f positionRadius;

    /// premult by intensity
    Vec4f color;

    /// if it is indeed spot light then w will be 1.0
    Vec4f spotDirection;

    /// x = cos of the outer angle
    /// y = cos of the inner angle.
    Vec4f spotAngles;
};

struct GlimmerChannelState
{
    Vec3f viewerPosition;
    bool hasViewer = false;
    // where a viewer is headed to predict where we need to load.
    Vec3f lookAhead;
    float budgetScale = 1.0f;

    FixedArray<GlimmerGroundLevelState, GlimmerGroundLevels> groundLevels {};

    uint32 groundGeneration = 0;

    FixedArray<GlimmerGroundCoverLayerState, GlimmerGroundCoverLayers> groundCover;

    FixedArray<GlimmerLocalLightState, GlimmerMaxLocalLights> localLights {};
    uint32 numLocalLights = 0;
};

class ENGINE_API GlimmerChannel final
{
public:
    static SharedPtr<GlimmerChannel> Get(const World* world);
    
    static void Register(const World* world, const SharedPtr<GlimmerChannel>& channel);
    static void Unregister(const World* world);

    void Publish(const GlimmerChannelState& state, Array<GlimmerGroundUpload, GlimmerAllocator>&& groundUploads);
    void Consume(GlimmerChannelState& outState, Array<GlimmerGroundUpload, GlimmerAllocator>& outGroundUploads);

private:
    Mutex m_mutex;
    GlimmerChannelState m_state;
    Array<GlimmerGroundUpload, GlimmerAllocator> m_pendingGroundUploads;
};

} // namespace Hyperion

/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>

#include <Core/Containers/FixedArray.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Constants.hpp>

#include <Core/Math/Mat4f.hpp>
#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>

namespace Hyperion {

class Texture;
struct GlimmerDebugViewContext;

static constexpr uint32 GlimmerReferenceClasses = 5;
static constexpr uint32 GlimmerReferenceClassValues = 6;
static constexpr uint32 GlimmerReferenceStatsStride = 32;

/// a progressive path tracer over Glimmer's own scene (its BVH, heightfield and canopy), to measure Glimmer's irradiance against
class GlimmerReference final
{
public:
    GlimmerReference();

    GlimmerReference(const GlimmerReference& other) = delete;
    GlimmerReference& operator=(const GlimmerReference& other) = delete;

    ~GlimmerReference();

    bool Render(const GlimmerDebugViewContext& context);

    // rgb = sum of the frames' irradiance / pi
    const Handle<Texture>& GetReferenceTexture() const
    {
        return m_referenceTexture;
    }

    uint32 GetNumFrames() const
    {
        return m_numFrames;
    }

private:
    struct Settings
    {
        Mat4f viewMatrix;
        Vec2u extent;
        Vec3f toSun;
        Vec3f sunIrradiance;
        uint32 skyTextureIndex = ~0u;
        float skyScale = 0.0f;
        bool isSkyFromSH = false;
        bool isFurnace = false;
        uint32 paths = 0;
        uint32 stride = 1;
        uint32 offsetCentimetres = 0;
        uint32 bounces = 0;
        float foliageExtinction = 0.0f;
        uint32 sceneGeneration = 0;
        uint32 numTracedInstances = 0;
    };

    void CreateResources(const Vec2u& extent);
    void ReadStats(Frame* frame);
    void DispatchStats(const GlimmerDebugViewContext& context, const void* constants, size_t constantsSize, uint32 numGroups);

    static bool IsSameSettings(const Settings& a, const Settings& b);

    Handle<Texture> m_referenceTexture; // rgb = sum of the frames' irradiance / pi, a = sum of their luminance squared
    Handle<Texture> m_glimmerTexture;   // the same of Glimmer's
    Handle<Texture> m_shadedReferenceTexture; // rgb = sum of the frames' diffuse shading, the sun and the reference's irradiance
    Handle<Texture> m_shadedGlimmerTexture;   // the same with Glimmer's irradiance
    GpuBufferRef m_partialsBuffer;
    GpuBufferRef m_statsBuffer;
    GpuBufferRef m_placeholderBuffer;

    FixedArray<GpuBufferRef, NumFramesInFlight> m_statsReadbackBuffers;
    FixedArray<uint32, NumFramesInFlight> m_statsReadbackFrames {}; // frames accumulated when the stats were taken, 0 when none are on their way

    Settings m_settings;
    bool m_hasSettings;

    uint32 m_numFrames;
    uint32 m_nextStatsFrames;
    uint32 m_seed;
};

} // namespace Hyperion

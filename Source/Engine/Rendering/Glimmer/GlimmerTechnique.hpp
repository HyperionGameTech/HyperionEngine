/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/CommandRecorder.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Math/BoundingBox.hpp>
#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>

namespace Hyperion {

class CloudPass;

class World;
class View;
class EnvProbe;
class RenderProxyList;
class CBufferAllocator;
class GlimmerSurfaceCache;
class GlimmerBLASCache;
class GlimmerTLAS;
class GlimmerSpanCache;
class GlimmerFootprintMask;
class GlimmerSWRTProbeVolume;
class GlimmerSWRTProbeDebug;
class GlimmerChannel;
class GlimmerSHOccupancy;
class GlimmerSHVolume;
class GlimmerRelight;
struct GlimmerChannelState;

struct GlimmerSceneRegionParams
{
    float radius = 0.0f;           // horizontal half extent
    float recenterDistance = 0.0f; // the region follows the viewer in steps of about this
    float snap = 1.0f;             // the region's center snaps to multiples of this
};

struct GlimmerTechniqueUpdateContext
{
    Frame* frame = nullptr;
    World* world = nullptr;
    EnvProbe* skyProbe = nullptr;

    RenderProxyList* sceneProxies = nullptr;
    BoundingBox region;

    GlimmerBLASCache* blasCache = nullptr;
    const GlimmerTLAS* tlas = nullptr;
    bool tlasSwapped = false; // a newly built TLAS was swapped in this frame, or its active region moved

    const CloudPass* cloudPass = nullptr; // its cloud shadows dim the sun at probe hits; may be nullptr

    // nullptr until the world's GlimmerSystem has published - the caches are up to date for this frame when set
    const GlimmerChannelState* channelState = nullptr;
    GlimmerChannel* channel = nullptr; // where debug readbacks go back to the sim
    const GlimmerSurfaceCache* surfaceCache = nullptr;
    const GlimmerSpanCache* spanCache = nullptr;

    bool updateLighting = false;
};

struct GlimmerDebugViewContext
{
    Frame* frame = nullptr;
    View* view = nullptr;
    Framebuffer* gbufferFramebuffer = nullptr;

    const GlimmerBLASCache* blasCache = nullptr;
    const GlimmerTLAS* tlas = nullptr;
    const GlimmerSurfaceCache* surfaceCache = nullptr;
    const GlimmerSpanCache* spanCache = nullptr;

    GpuImageViewRef outputImageView;
    Vec2u extent;

    // GlimmerDebugView::TechniqueFirst + 1
    int techniqueView = 0;
};

///  @TODO Refactor, rename? GlimmerRenderer? We only have one technique now.
class GlimmerTechnique final
{
public:
    static GlimmerSceneRegionParams GetSceneRegionParams();

    static void WriteApplyShaderData(CBufferAllocator& cbufferAllocator, const GlimmerTechnique* technique);
    static uint32 BindApplyResources(CommandRecorder& cr, uint32 uniformIndex, const GlimmerTechnique* technique);

    GlimmerTechnique();

    GlimmerTechnique(const GlimmerTechnique& other) = delete;
    GlimmerTechnique& operator=(const GlimmerTechnique& other) = delete;

    ~GlimmerTechnique();

    bool IsReady() const;

    BoundingBox GetTracedRegion(const BoundingBox& sceneRegion) const;

    void Update(const GlimmerTechniqueUpdateContext& context);

    bool RenderDebugView(const GlimmerDebugViewContext& context);

private:
    UniquePtr<GlimmerFootprintMask> m_footprintMask;
    UniquePtr<GlimmerSWRTProbeVolume> m_probeVolume;
    UniquePtr<GlimmerSWRTProbeDebug> m_probeDebug;
    UniquePtr<GlimmerSHOccupancy> m_shOccupancy;
    UniquePtr<GlimmerSHVolume> m_shVolume;
    UniquePtr<GlimmerRelight> m_relight;

    // true when the lighting changed enough to wake the caches; outSunChanged when the sun did, not only the sky
    bool UpdateLightingSignature(EnvProbe* skyProbe, bool& outSunChanged);

    uint32 m_maskGeneration;

    // the lighting the caches were last woken for
    Vec3f m_litSunDirection;
    Vec3f m_litSunRadiance;
    float m_litSkyLight;
    float m_litSkyLuminance;
    bool m_hasLitSignature;
};

} // namespace Hyperion

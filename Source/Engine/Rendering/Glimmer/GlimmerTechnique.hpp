/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/RawBuffer.hpp>
#include <Rendering/CommandRecorder.hpp>

#include <Rendering/Glimmer/GlimmerHelpers.hpp>
#include <Rendering/Glimmer/GlimmerMemory.hpp>

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
class GlimmerTriangleCache;
class GlimmerSceneInstances;
class GlimmerSpanCache;
class GlimmerProbeVolume;
class GlimmerProbeDebug;
class GlimmerChannel;
class GlimmerSHOccupancy;
class GlimmerSHVolume;
class GlimmerRelight;
class GlimmerLocalLights;
class GlimmerBrickWorld;
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

    GlimmerTriangleCache* triangleCache = nullptr;
    const GlimmerSceneInstances* sceneInstances = nullptr;

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

    const GlimmerTriangleCache* triangleCache = nullptr;
    const GlimmerSceneInstances* sceneInstances = nullptr;
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

    void Update(const GlimmerTechniqueUpdateContext& context);

    void ResetLighting();

    bool RenderDebugView(const GlimmerDebugViewContext& context);

    HYP_FORCE_INLINE GlimmerRelight* GetRelight() const
    {
        return m_relight.Get();
    }

    HYP_FORCE_INLINE const GlimmerBrickWorld* GetBrickWorld() const
    {
        return m_brickWorld.Get();
    }

    bool IsSHReady() const;
    bool IsProbesReady() const;

private:
    UniquePtr<GlimmerProbeVolume, GlimmerAllocator> m_probeVolume;
    UniquePtr<GlimmerProbeDebug, GlimmerAllocator> m_probeDebug;
    UniquePtr<GlimmerSHOccupancy, GlimmerAllocator> m_shOccupancy;
    UniquePtr<GlimmerSHVolume, GlimmerAllocator> m_shVolume;
    UniquePtr<GlimmerRelight, GlimmerAllocator> m_relight;
    UniquePtr<GlimmerLocalLights, GlimmerAllocator> m_localLights;
    UniquePtr<GlimmerBrickWorld, GlimmerAllocator> m_brickWorld;

    uint32 m_relightLightsGeneration = 0;

    RWStructuredBuffer m_scratchBuffer;

    GlimmerLightShaderData m_light;
    bool m_hasLight = false;
    uint64 m_lightTime = 0;
};

} // namespace Hyperion

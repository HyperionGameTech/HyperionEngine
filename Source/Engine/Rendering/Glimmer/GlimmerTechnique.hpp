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

namespace Hyperion {

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
class GlimmerSHOccupancy;
class GlimmerSHVolume;
struct GlimmerChannelState;

struct GlimmerSceneRegionParams
{
    float radius = 0.0f;           // horizontal half extent
    float recenterDistance = 0.0f; // the region follows the viewer in steps of about this
    float snap = 1.0f;             // the region centre snaps to multiples of this
};

struct GlimmerTechniqueUpdateContext
{
    Frame* frame = nullptr;
    World* world = nullptr;
    EnvProbe* skyProbe = nullptr;

    // the Glimmer scene view's proxies, locked for reading, and the region they were collected from
    RenderProxyList* sceneProxies = nullptr;
    BoundingBox region;

    // the world's scene instances (and the BLAS pool they draw from), already updated for this frame
    GlimmerBLASCache* blasCache = nullptr;
    const GlimmerTLAS* tlas = nullptr;
    bool tlasSwapped = false; // a newly built TLAS was swapped in this frame

    // nullptr until the world's GlimmerSystem has published; the caches are up to date for this frame when set
    const GlimmerChannelState* channelState = nullptr;
    const GlimmerSurfaceCache* surfaceCache = nullptr;
    const GlimmerSpanCache* spanCache = nullptr;

    // Glimmer is enabled and there's a viewer; otherwise only the scene is kept up to date, for debug views
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

    // RGBA16F storage image of extent, already in the unordered access state
    GpuImageViewRef outputImageView;
    Vec2u extent;

    // 1 based: Rendering.Glimmer.DebugView - GlimmerDebugView::TechniqueFirst + 1
    int techniqueView = 0;
};

/*! \brief Glimmer for one world: turns the world's Glimmer scene into irradiance that lighting samples, in two parts.
 *  The near field is a terrain following clipmap of probes traced against a software BVH of the static solids around the viewer
 *  (and then the heightfield). The far field, past the probes, is a clipmap of SH voxels that are traced only as they scroll in,
 *  against an occupancy clipmap of the solids and the heightfield, and relit per pixel; nothing far away is traced every frame.
 *  The instance BVH, BLASes, ground and spans come from the world's Glimmer scene. Render thread only. */
class GlimmerTechnique final
{
public:
    /*! \brief Sim thread: how big the scene region should be. */
    static GlimmerSceneRegionParams GetSceneRegionParams();

    /*! \brief Writes the Glimmer volumes' block of the apply constants (GlimmerApply in Shaders/Glimmer/GlimmerApply.hlsli, after the part
     *  GlimmerPass writes): zeroed for a volume that isn't ready, and altogether for a nullptr technique. */
    static void WriteApplyShaderData(CBufferAllocator& cbufferAllocator, const GlimmerTechnique* technique);

    /*! \brief Binds what the apply shader samples, or placeholders where a volume isn't ready or there's no technique.
     *  \return the next uniform index. */
    static uint32 BindApplyResources(CommandRecorder& cr, uint32 uniformIndex, const GlimmerTechnique* technique);

    GlimmerTechnique();
    GlimmerTechnique(const GlimmerTechnique& other) = delete;
    GlimmerTechnique& operator=(const GlimmerTechnique& other) = delete;
    ~GlimmerTechnique();

    /*! \brief The part of the scene region whose solids get an instance BVH (GlimmerTLAS) for ray tracing. */
    BoundingBox GetTracedRegion(const BoundingBox& sceneRegion) const;

    /*! \brief Called once per frame while the Glimmer scene is required, after the scene instances, surface cache and spans. */
    void Update(const GlimmerTechniqueUpdateContext& context);

    /*! \brief Whether lighting can sample either volume yet. */
    bool IsReady() const;

    /*! \brief Renders one of the debug views into context.outputImageView. \return false if it has nothing to show. */
    bool RenderDebugView(const GlimmerDebugViewContext& context);

private:
    UniquePtr<GlimmerFootprintMask> m_footprintMask;
    UniquePtr<GlimmerSWRTProbeVolume> m_probeVolume;
    UniquePtr<GlimmerSHOccupancy> m_shOccupancy;
    UniquePtr<GlimmerSHVolume> m_shVolume;

    // the TLAS generation the footprint mask was built from
    uint32 m_maskGeneration;
};

} // namespace Hyperion

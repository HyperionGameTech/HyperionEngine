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
struct GlimmerChannelState;

/*! \brief The flavours of Glimmer. Each lives in its own subfolder of Rendering/Glimmer (and Shaders/Glimmer) and plugs into GlimmerPass
 *  through GlimmerTechnique; everything outside those subfolders (the scene view, ground heights and albedo, apply) is shared. */
enum class GlimmerTechniqueType : uint32
{
    SWRT // probes traced against a software BVH of the nearby solids and the heightfield beyond
};

/*! \brief How much static scene around the viewer the Glimmer scene view collects. Sim thread. */
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

    // nullptr until the world's GlimmerSystem has published
    const GlimmerChannelState* channelState = nullptr;

    // already up to date for this frame
    const GlimmerSurfaceCache* surfaceCache = nullptr;

    // Glimmer is enabled and there's a viewer; otherwise only the scene is kept up to date, for debug views
    bool updateLighting = false;
};

struct GlimmerDebugViewContext
{
    Frame* frame = nullptr;
    View* view = nullptr;
    Framebuffer* gbufferFramebuffer = nullptr;
    const GlimmerSurfaceCache* surfaceCache = nullptr;

    // RGBA16F storage image of extent, already in the unordered access state
    GpuImageViewRef outputImageView;
    Vec2u extent;

    // 1 based: Rendering.Glimmer.DebugView - GlimmerDebugView::TechniqueFirst + 1
    int techniqueView = 0;
};

/*! \brief One flavour of Glimmer for one world: turns the world's Glimmer scene into irradiance that lighting samples.
 *  Created by CreateGlimmerTechnique(). Render thread only. */
class GlimmerTechnique
{
public:
    virtual ~GlimmerTechnique() = default;

    virtual GlimmerTechniqueType GetType() const = 0;

    /*! \brief Called once per frame while the Glimmer scene is required. */
    virtual void Update(const GlimmerTechniqueUpdateContext& context) = 0;

    /*! \brief Whether lighting can sample the technique yet. */
    virtual bool IsReady() const = 0;

    /*! \brief Writes the technique's block of the apply constants (GlimmerTechniqueApply in its apply shader), zeroed when it isn't ready. */
    virtual void WriteApplyShaderData(CBufferAllocator& cbufferAllocator) const = 0;

    /*! \brief Binds what the technique's apply shader samples, or placeholders when it isn't ready. \return the next uniform index. */
    virtual uint32 BindApplyResources(CommandRecorder& cr, uint32 uniformIndex) const = 0;

    /*! \brief Renders one of the technique's debug views into context.outputImageView. \return false if it has nothing to show. */
    virtual bool RenderDebugView(const GlimmerDebugViewContext& context) = 0;
};

/*! \brief The technique every world uses. Must match the technique Shaders/Glimmer/GlimmerApply.hlsli includes. */
GlimmerTechniqueType GetActiveGlimmerTechniqueType();

UniquePtr<GlimmerTechnique> CreateGlimmerTechnique(GlimmerTechniqueType type);

/*! \brief Sim thread. */
GlimmerSceneRegionParams GetGlimmerSceneRegionParams(GlimmerTechniqueType type);

} // namespace Hyperion

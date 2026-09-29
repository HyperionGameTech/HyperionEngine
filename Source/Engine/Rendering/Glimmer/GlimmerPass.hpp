/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/Pass.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/Glimmer/GlimmerProbeVolume.hpp>

#include <Core/Memory/UniquePtr.hpp>
#include <Core/Memory/SharedPtr.hpp>

#include <Core/Containers/Map.hpp>

#include <Core/Math/BoundingBox.hpp>

namespace Hyperion {

class World;
class Texture;
class CBufferAllocator;
class GlimmerBLASCache;
class GlimmerTLAS;
class GlimmerFootprintMask;
class GlimmerSurfaceCache;
class GlimmerSpanCache;

// Must match GlimmerApply in Shaders/Glimmer/GlimmerApply.hlsli
struct GlimmerApplyShaderData
{
    GlimmerProbeVolumeShaderData volume;
    Vec4u params; // x = debug vis
};

/*! \brief Per world Glimmer state, keyed by the world's Glimmer scene view. */
HYP_CLASS(NoScriptBindings)
class GlimmerScenePassData : public PassData
{
    HYP_OBJECT_BODY(GlimmerScenePassData);

public:
    GlimmerScenePassData();
    virtual ~GlimmerScenePassData() override;

    SharedPtr<GlimmerBLASCache> blasCache;
    UniquePtr<GlimmerTLAS> tlas;
    UniquePtr<GlimmerFootprintMask> footprintMask;
    UniquePtr<GlimmerSurfaceCache> surfaceCache;
    UniquePtr<GlimmerSpanCache> spanCache;
    UniquePtr<GlimmerProbeVolume> probeVolume;

    World* world = nullptr;
    BoundingBox region;
    uint32 lastUpdatedFrame = ~0u;
};

/*! \brief Per GBuffer view Glimmer state (debug output and captures). */
HYP_CLASS(NoScriptBindings)
class GlimmerViewPassData : public PassData
{
    HYP_OBJECT_BODY(GlimmerViewPassData);

public:
    GlimmerViewPassData();
    virtual ~GlimmerViewPassData() override;

    Handle<Texture> debugTexture;
    Handle<Texture> captureTexture;
};

/*! \brief Glimmer GI: keeps each world's software ray tracing scene, heightfield and probe volume up to date,
 *  provides what lighting samples it through, and renders the debug views. */
class GlimmerPass final : public PassBase
{
public:
    GlimmerPass();
    virtual ~GlimmerPass() override;

    virtual void Initialize() override;
    virtual void Shutdown() override;

    /*! \brief renderSetup.view must be a world's Glimmer scene view, and renderSetup.envProbe the world's sky probe if it has one.
     *  Updates the SWRT scene, the heightfield and the probes. Call after the sky probe has rendered for the frame. */
    virtual void RenderFrame(Frame* frame, const RenderSetup& renderSetup) override;

    /*! \brief Traces the SWRT debug view for a GBuffer view, if Rendering.Glimmer.SWRT.DebugView is set.
     *  \return true with outImageView set to the result, to be shown in place of the view's final image. */
    bool RenderDebugView(Frame* frame, const RenderSetup& renderSetup, Framebuffer* gbufferFramebuffer, GpuImageViewRef& outImageView);

    /*! \brief Writes the Glimmer apply constants for the world into the CBuffer being built. Zeroed (lighting skips Glimmer) when it has none. */
    void WriteApplyShaderData(CBufferAllocator& cbufferAllocator, World* world) const;

    /*! \brief Binds the textures lighting samples Glimmer through, or placeholders. \return the next uniform index. */
    uint32 BindApplyResources(CommandRecorder& cr, uint32 uniformIndex, World* world) const;

    /*! \brief DEBUG ONLY : TO REMOVE!
     *  Copies the view's final image to a PNG when Rendering.Glimmer.CaptureFrame changes. */
    void CaptureFinalImage(Frame* frame, const RenderSetup& renderSetup, const GpuImageViewRef& finalImageView);

    /*! \brief The world's Glimmer scene, or nullptr if it has none yet. */
    GlimmerScenePassData* GetSceneForWorld(World* world) const;

    virtual void OnFrameEnd(uint32 prevFrameIndex) override;

protected:
    virtual PassData* CreateViewPassData(View* view, PassDataExt& ext) override;

private:
    void LogStats(const GlimmerScenePassData& scene) const;
    void CaptureTexture(const Handle<Texture>& texture, const char* prefix, int captureIndex);

    SharedPtr<GlimmerBLASCache> m_blasCache;
    uint32 m_lastBLASCacheUpdateFrame;
    int m_lastCaptureIndex;
    int m_lastFrameCaptureIndex;

    Map<World*, GlimmerScenePassData*> m_scenes;
};

} // namespace Hyperion

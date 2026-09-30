/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/Pass.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/Glimmer/GlimmerTechnique.hpp>

#include <Core/Memory/UniquePtr.hpp>
#include <Core/Memory/SharedPtr.hpp>

#include <Core/Containers/Map.hpp>

#include <Core/Math/BoundingBox.hpp>

namespace Hyperion {

class World;
class Texture;
class CBufferAllocator;
class GlimmerSurfaceCache;
class GlimmerBLASCache;
class GlimmerTLAS;
class GlimmerSpanCache;

// Must match GlimmerApply in Shaders/Glimmer/GlimmerApply.hlsli, less the volumes' blocks that follow it (GlimmerTechnique::WriteApplyShaderData)
struct GlimmerApplyShaderData
{
    Vec4u params;   // x = 1 to show Glimmer's irradiance on its own, y = 1 when either volume can be sampled
    Vec4f settings; // x = intensity
};

/*! \brief Per world Glimmer state, keyed by the world's Glimmer scene view. */
HYP_CLASS(NoScriptBindings)
class GlimmerScenePassData : public PassData
{
    HYP_OBJECT_BODY(GlimmerScenePassData);

public:
    GlimmerScenePassData();
    virtual ~GlimmerScenePassData() override;

    // the scene the technique builds on: instances (spans, and a BVH near the viewer), ground, spans
    SharedPtr<GlimmerBLASCache> blasCache;
    UniquePtr<GlimmerTLAS> tlas;
    UniquePtr<GlimmerSurfaceCache> surfaceCache;
    UniquePtr<GlimmerSpanCache> spanCache;

    UniquePtr<GlimmerTechnique> technique;

    World* world = nullptr;
    BoundingBox region;
    uint32 lastUpdatedFrame = ~0u;
};

/*! \brief Per GBuffer view Glimmer state (debug output). */
HYP_CLASS(NoScriptBindings)
class GlimmerViewPassData : public PassData
{
    HYP_OBJECT_BODY(GlimmerViewPassData);

public:
    GlimmerViewPassData();
    virtual ~GlimmerViewPassData() override;

    Handle<Texture> debugTexture;
};

/*! \brief Glimmer GI: keeps each world's Glimmer scene (instances, ground heights and albedo, spans) and technique up to date,
 *  provides what lighting samples it through, and renders the debug views. */
class GlimmerPass final : public PassBase
{
public:
    GlimmerPass();
    virtual ~GlimmerPass() override;

    virtual void Initialize() override;
    virtual void Shutdown() override;

    /*! \brief renderSetup.view must be a world's Glimmer scene view, and renderSetup.envProbe the world's sky probe if it has one.
     *  Updates the scene, then the world's technique. Call after the sky probe has rendered for the frame. */
    virtual void RenderFrame(Frame* frame, const RenderSetup& renderSetup) override;

    /*! \brief Renders the technique's debug view for a GBuffer view, if Rendering.Glimmer.DebugView is one of the technique's views.
     *  \return true with outImageView set to the result, to be shown in place of the view's final image. */
    bool RenderDebugView(Frame* frame, const RenderSetup& renderSetup, Framebuffer* gbufferFramebuffer, GpuImageViewRef& outImageView);

    /*! \brief Writes the Glimmer apply constants for the world into the CBuffer being built, the volumes' blocks included.
     *  Lighting skips Glimmer when the world's technique isn't ready. */
    void WriteApplyShaderData(CBufferAllocator& cbufferAllocator, World* world) const;

    /*! \brief Binds the textures lighting samples Glimmer through, or placeholders. \return the next uniform index. */
    uint32 BindApplyResources(CommandRecorder& cr, uint32 uniformIndex, World* world) const;

    /*! \brief The world's Glimmer scene, or nullptr if it has none yet. */
    GlimmerScenePassData* GetSceneForWorld(World* world) const;

    /*! \brief The technique lighting samples for the world, or nullptr while Glimmer is off or the world has no scene yet. */
    const GlimmerTechnique* GetApplyTechnique(World* world) const;

    virtual void OnFrameEnd(uint32 prevFrameIndex) override;

protected:
    virtual PassData* CreateViewPassData(View* view, PassDataExt& ext) override;

private:
    Map<World*, GlimmerScenePassData*> m_scenes;
};

} // namespace Hyperion

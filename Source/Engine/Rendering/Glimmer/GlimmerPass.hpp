/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/Pass.hpp>
#include <Rendering/RenderTypes.hpp>

#include <Rendering/Glimmer/GlimmerTechnique.hpp>
#include <Rendering/Glimmer/GlimmerChannel.hpp>

#include <Core/Memory/UniquePtr.hpp>
#include <Core/Memory/SharedPtr.hpp>

#include <Core/Utilities/EnumFlags.hpp>

#include <Core/Containers/Map.hpp>

#include <Core/Math/BoundingBox.hpp>

namespace Hyperion {

class World;
class Texture;
class CBufferAllocator;
class GlimmerSurfaceCache;
class GlimmerTriangleCache;
class GlimmerSceneInstances;
class GlimmerSpanCache;
class GlimmerReference;

struct GlimmerApplyShaderData
{
    Vec4u params;   // x = 1 to show Glimmer's irradiance on its own, y = 1 when either volume can be sampled
    Vec4f settings; // x = intensity
};

enum class GlimmerStartupStage : uint32
{
    None = 0x0,
    SceneInstances = 0x1,
    Triangles = 0x2,
    Spans = 0x4,
    Bricks = 0x8,
    BricksSettled = 0x10,
    SH = 0x20,
    Probes = 0x40,
    WarmingUp = 0x80,
    FirstUpdate = 0x100
};

HYP_MAKE_ENUM_FLAGS(GlimmerStartupStage);

enum class GlimmerRestartParts : uint32
{
    None = 0x0,
    GroundUploads = 0x1,
    GroundAlbedo = 0x2,
    SpanCache = 0x4,
    Bricks = 0x8,
    Lighting = 0x10
};

HYP_MAKE_ENUM_FLAGS(GlimmerRestartParts);

HYP_CLASS(NoScriptBindings)
class GlimmerScenePassData : public PassData
{
    HYP_OBJECT_BODY(GlimmerScenePassData);

public:
    GlimmerScenePassData();
    virtual ~GlimmerScenePassData() override;

    SharedPtr<GlimmerTriangleCache> triangleCache;
    
    UniquePtr<GlimmerSceneInstances, GlimmerAllocator> sceneInstances;
    UniquePtr<GlimmerSurfaceCache, GlimmerAllocator> surfaceCache;
    UniquePtr<GlimmerSpanCache, GlimmerAllocator> spanCache;

    UniquePtr<GlimmerTechnique, GlimmerAllocator> technique;

    World* world = nullptr;
    EnvProbe* skyProbe = nullptr; // as of the last update
    BoundingBox region;
    uint32 lastUpdatedFrame = ~0u;

    EnumFlags<GlimmerRestartParts> pendingRestart = GlimmerRestartParts::None;
    
    Array<uint32, GlimmerAllocator> seenSkippedDispatches;

    uint32 groundGeneration = 0;
    bool isReplayingGroundUploads = false;

    uint64 startTime = 0;
    uint32 startFrame = 0;
    EnumFlags<GlimmerStartupStage> loggedStages = GlimmerStartupStage::None;

    Array<GlimmerGroundUpload, GlimmerAllocator> groundUploads;
};

HYP_CLASS(NoScriptBindings)
class GlimmerViewPassData : public PassData
{
    HYP_OBJECT_BODY(GlimmerViewPassData);

public:
    GlimmerViewPassData();
    virtual ~GlimmerViewPassData() override;

    Handle<Texture> debugTexture;

    UniquePtr<GlimmerReference> reference;
};

class GlimmerPass final : public PassBase
{
public:
    GlimmerPass();
    virtual ~GlimmerPass() override;

    virtual void Initialize() override;
    virtual void Shutdown() override;

    virtual void RenderFrame(Frame* frame, const RenderSetup& renderSetup) override;

    bool RenderDebugView(Frame* frame, const RenderSetup& renderSetup, Framebuffer* gbufferFramebuffer, GpuImageViewRef& outImageView);

    // the reference's irradiance sum for the frame to be lit by, and what scales it to the mean; false unless Rendering.Glimmer.Reference is 5 and it has frames
    bool GetLitReference(View* view, Handle<Texture>& outTexture, float& outScale);

    void WriteApplyShaderData(CBufferAllocator& cbufferAllocator, World* world) const;

    uint32 BindApplyResources(CommandRecorder& cr, uint32 uniformIndex, World* world) const;

    GlimmerScenePassData* GetSceneForWorld(World* world) const;

    const GlimmerTechnique* GetApplyTechnique(World* world) const;

    virtual void OnFrameEnd(uint32 prevFrameIndex) override;

protected:
    virtual PassData* CreateViewPassData(View* view, PassDataExt& ext) override;

private:
    Map<World*, GlimmerScenePassData*> m_scenes;
};

} // namespace Hyperion

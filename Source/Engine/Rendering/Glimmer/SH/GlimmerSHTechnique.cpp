/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/SH/GlimmerSHTechnique.hpp>
#include <Rendering/Glimmer/SH/GlimmerSHVolume.hpp>
#include <Rendering/Glimmer/SH/GlimmerSHOccupancy.hpp>
#include <Rendering/Glimmer/GlimmerSurfaceCache.hpp>
#include <Rendering/Glimmer/GlimmerChannel.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/CBufferAllocator.hpp>

#include <Core/Threading/Threads.hpp>

#include <Core/Profiling/ProfileScope.hpp>

namespace Hyperion {

// the spans only need to reach as far as the volume's coarsest cascade
static constexpr float SceneRadius = 0.5f * float(GlimmerSHGridXZ) * GlimmerSHSpacing * float(1u << (GlimmerSHCascades - 1));

GlimmerSceneRegionParams GlimmerSHTechnique::GetSceneRegionParams()
{
    GlimmerSceneRegionParams params;
    params.radius = SceneRadius;
    params.recenterDistance = SceneRadius * 0.125f;
    params.snap = 1.0f;

    return params;
}

GlimmerSHTechnique::GlimmerSHTechnique()
    : m_occupancy(MakeUnique<GlimmerSHOccupancy>()),
      m_volume(MakeUnique<GlimmerSHVolume>())
{
}

GlimmerSHTechnique::~GlimmerSHTechnique()
{
}

void GlimmerSHTechnique::Update(const GlimmerTechniqueUpdateContext& context)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (context.updateLighting && context.surfaceCache && context.spanCache)
    {
        m_occupancy->Update(context.frame, context.channelState->viewerPosition, *context.tlas, *context.blasCache);

        GlimmerSHVolumeUpdateInputs inputs;
        inputs.viewerPosition = context.channelState->viewerPosition;
        inputs.surfaceCache = context.surfaceCache;
        inputs.spanCache = context.spanCache;
        inputs.occupancy = &m_occupancy->GetShaderData();
        inputs.occupancyImageView = m_occupancy->GetImageView();

        m_volume->Update(context.frame, inputs);
    }
}

bool GlimmerSHTechnique::IsReady() const
{
    return m_volume->IsReady();
}

void GlimmerSHTechnique::WriteApplyShaderData(CBufferAllocator& cbufferAllocator) const
{
    // Must match GlimmerTechniqueApply in Shaders/Glimmer/SH/GlimmerSHApply.hlsli
    GlimmerSHVolumeShaderData shaderData {};

    if (IsReady())
    {
        shaderData = m_volume->GetShaderData();
    }

    cbufferAllocator.Write(&shaderData);
}

uint32 GlimmerSHTechnique::BindApplyResources(CommandRecorder& cr, uint32 uniformIndex) const
{
    // the volume hands out placeholders until it has textures; the zeroed constants keep lighting from sampling them
    cr << SetShaderUniform(uniformIndex++, "GlimmerSHVisibilityTexture"_sh, m_volume->GetVisibilityImageView());
    cr << SetShaderUniform(uniformIndex++, "GlimmerSHBounceTexture"_sh, m_volume->GetBounceImageView());
    cr << SetShaderUniform(uniformIndex++, "GlimmerSHStateTexture"_sh, m_volume->GetStateImageView());
    cr << SetShaderUniform(uniformIndex++, "GlimmerSHDepthXTexture"_sh, m_volume->GetDepthImageView(0));
    cr << SetShaderUniform(uniformIndex++, "GlimmerSHDepthYTexture"_sh, m_volume->GetDepthImageView(1));
    cr << SetShaderUniform(uniformIndex++, "GlimmerSHDepthZTexture"_sh, m_volume->GetDepthImageView(2));

    return uniformIndex;
}

bool GlimmerSHTechnique::RenderDebugView(const GlimmerDebugViewContext& context)
{
    return false;
}

} // namespace Hyperion

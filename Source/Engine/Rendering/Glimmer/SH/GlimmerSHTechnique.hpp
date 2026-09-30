/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/Glimmer/GlimmerTechnique.hpp>

#include <Core/Memory/UniquePtr.hpp>
#include <Core/Memory/SharedPtr.hpp>

namespace Hyperion {

class GlimmerSHVolume;

/*! \brief Glimmer's SH flavour: a clipmap of sky visibility and blocker albedo, traced against the heightfield (ground and spans) only
 *  when voxels scroll in or come up for a slow refresh, and relit per pixel with the current sky and sun. Traces no BVH, so it asks the
 *  scene for none (the default GetTracedRegion()). Render thread only. */
class GlimmerSHTechnique final : public GlimmerTechnique
{
public:
    static GlimmerSceneRegionParams GetSceneRegionParams();

    GlimmerSHTechnique();
    GlimmerSHTechnique(const GlimmerSHTechnique& other) = delete;
    GlimmerSHTechnique& operator=(const GlimmerSHTechnique& other) = delete;
    virtual ~GlimmerSHTechnique() override;

    virtual GlimmerTechniqueType GetType() const override
    {
        return GlimmerTechniqueType::SH;
    }

    virtual void Update(const GlimmerTechniqueUpdateContext& context) override;

    virtual bool IsReady() const override;

    virtual void WriteApplyShaderData(CBufferAllocator& cbufferAllocator) const override;
    virtual uint32 BindApplyResources(CommandRecorder& cr, uint32 uniformIndex) const override;

    virtual bool RenderDebugView(const GlimmerDebugViewContext& context) override;

private:
    UniquePtr<GlimmerSHVolume> m_volume;
};

} // namespace Hyperion

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

class GlimmerFootprintMask;
class GlimmerSWRTProbeVolume;

/*! \brief Glimmer's SWRT flavour: a terrain following probe clipmap traced against a software BVH of the static solids near the viewer,
 *  then against the heightfield (ground, and spans of solids and foliage splatted from the same BLASes) out to the horizon.
 *  The instance BVH, BLASes and spans come from the world's Glimmer scene. Render thread only. */
class GlimmerSWRTTechnique final : public GlimmerTechnique
{
public:
    static GlimmerSceneRegionParams GetSceneRegionParams();

    GlimmerSWRTTechnique();
    GlimmerSWRTTechnique(const GlimmerSWRTTechnique& other) = delete;
    GlimmerSWRTTechnique& operator=(const GlimmerSWRTTechnique& other) = delete;
    virtual ~GlimmerSWRTTechnique() override;

    virtual GlimmerTechniqueType GetType() const override
    {
        return GlimmerTechniqueType::SWRT;
    }

    virtual BoundingBox GetTracedRegion(const BoundingBox& sceneRegion) const override;

    virtual void Update(const GlimmerTechniqueUpdateContext& context) override;

    virtual bool IsReady() const override;

    virtual void WriteApplyShaderData(CBufferAllocator& cbufferAllocator) const override;
    virtual uint32 BindApplyResources(CommandRecorder& cr, uint32 uniformIndex) const override;

    virtual bool RenderDebugView(const GlimmerDebugViewContext& context) override;

private:
    UniquePtr<GlimmerFootprintMask> m_footprintMask;
    UniquePtr<GlimmerSWRTProbeVolume> m_probeVolume;

    // the TLAS generation the footprint mask was built from
    uint32 m_maskGeneration;
};

} // namespace Hyperion

/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Baking/BakeJob.hpp>
#include <Baking/BakerMemory.hpp>

#include <Baking/FogVolume/FogVolumeBakeData.hpp>

#include <Core/Threading/AtomicVar.hpp>
#include <Core/Containers/Array.hpp>

namespace Hyperion {

class FogVolume;

namespace Baking {

template <>
class BakeJob<FogVolume> : public BakeJobBase
{
public:
    explicit BakeJob(BakeJobParams&& params, const Handle<FogVolume>& fogVolume, BakeData<FogVolume>* bakeData)
        : BakeJobBase(std::move(params)),
          m_fogVolume(fogVolume),
          m_bakeData(bakeData),
          m_gpuBakeDispatched(false),
          m_gpuBakeReady(false)
    {
    }

    virtual ~BakeJob() override;

    HYP_FORCE_INLINE const Handle<FogVolume>& GetFogVolume() const
    {
        return m_fogVolume;
    }

    virtual BakeData<FogVolume>& GetBakeData() override
    {
        return *m_bakeData;
    }

    virtual uint32 ProcessTexels(Span<LightmapTexel*> texels, uint32 texelOffset = 0) override;

    // Written to by the FogVolumeOcclusionBakeCmd render command / its OnFrameEnd readback callback,
    // read by Process_Internal/ProcessTexels on the sim thread. Public for the same reason
    // BakeJobBase::tracingCompleteSignal/readbackData are: a render-thread callback needs to reach them.
    AtomicVar<bool> m_gpuBakeDispatched;
    AtomicVar<bool> m_gpuBakeReady;
    Array<Vec4f, BakerAllocator> m_gpuResults;

protected:
    virtual void Start_Internal() override;
    virtual void Process_Internal(bool* outIsReadyToProcess) override;

    /*! \brief Dispatches the FogVolumeOcclusionBake compute shader (uploading the baked SDF grid and
     *  point light data as GPU resources) and schedules a readback of the results into m_gpuResults. */
    void DispatchOcclusionBake();

    Handle<FogVolume> m_fogVolume;
    BakeData<FogVolume>* m_bakeData;
};

} // namespace Baking

} // namespace Hyperion

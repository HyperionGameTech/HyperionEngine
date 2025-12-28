/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <baking/BakeJob.hpp>

#include <baking/fog_volume/FogVolumeBakeData.hpp>

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
          m_bakeData(bakeData)
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

protected:
    virtual void Start_Internal() override;
    virtual void Process_Internal(bool* outIsReadyToProcess) override;

    Handle<FogVolume> m_fogVolume;
    BakeData<FogVolume>* m_bakeData;
};

} // namespace Baking

} // namespace Hyperion

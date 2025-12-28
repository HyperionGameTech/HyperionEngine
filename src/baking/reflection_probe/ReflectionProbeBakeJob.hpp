/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <baking/BakeJob.hpp>

namespace Hyperion {

class ReflectionProbe;

namespace Baking {

template <>
class BakeJob<ReflectionProbe> : public BakeJobBase
{
public:
    explicit BakeJob(BakeJobParams&& params, const Handle<ReflectionProbe>& envProbe, BakeData<ReflectionProbe>* lightmapData)
        : BakeJobBase(std::move(params)),
          m_envProbe(envProbe),
          m_lightmapData(lightmapData)
    {
    }

    virtual ~BakeJob() override;

    HYP_FORCE_INLINE const Handle<ReflectionProbe>& GetEnvProbe() const
    {
        return m_envProbe;
    }

    virtual BakeData<ReflectionProbe>& GetLightmapData() override
    {
        return *m_lightmapData;
    }

protected:
    virtual void Start_Internal() override;
    virtual void Process_Internal(bool* outIsReadyToProcess) override;

    Handle<ReflectionProbe> m_envProbe;
    BakeData<ReflectionProbe>* m_lightmapData;
};

} // namespace Baking

} // namespace Hyperion

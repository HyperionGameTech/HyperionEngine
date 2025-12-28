/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <baking/Baker.hpp>

namespace Hyperion {

class FogVolume;

namespace Baking {

template <>
class Baker<FogVolume> final : public BakerBase
{
public:
    Baker(LightmapperConfig&& config, const Handle<FogVolume>& fogVolume);

    Baker(const Baker& other) = delete;
    Baker& operator=(const Baker& other) = delete;

    Baker(Baker&& other) noexcept = delete;
    Baker& operator=(Baker&& other) noexcept = delete;

    virtual ~Baker() override = default;

    virtual bool PerformsRayTracing() const override
    {
        return false;
    }

    virtual uint32 NumTexelSamples() const override
    {
        return 1;
    }

    virtual uint32 GetShadingTypesMask() const override
    {
        return 1u << int(LightmapShadingType::FULL);
    }

    virtual bool ShouldSplitIntoJobs() const override
    {
        return false;
    }

protected:
    virtual LightmapDataBase& GetLightmapData() override
    {
        return m_lightmapData;
    }

    virtual UniquePtr<BakeJobBase> CreateJob(BakeJobParams&& params) override;

    virtual Result Build_Internal() override;
    virtual void HandleCompletedJob_Internal(BakeJobBase* job) override;

    Handle<FogVolume> m_fogVolume;
    BakeData<FogVolume> m_lightmapData;
};

} // namespace Baking

} // namespace Hyperion

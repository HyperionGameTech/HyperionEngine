/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <baking/Baker.hpp>

#include <baking/lightmap_volume/LightmapVolumeBakeData.hpp>

namespace Hyperion {

class LightmapVolume;
enum class LightmapElementId : uint32;

namespace Baking {

template <>
class Baker<LightmapVolume> final : public BakerBase
{
public:
    Baker(LightmapperConfig&& config, const Handle<LightmapVolume>& volume);

    Baker(const Baker& other) = delete;
    Baker& operator=(const Baker& other) = delete;

    Baker(Baker&& other) noexcept = delete;
    Baker& operator=(Baker&& other) noexcept = delete;

    virtual ~Baker() override = default;

    virtual bool ShouldSplitIntoJobs() const override
    {
        return true;
    }

    virtual uint32 GetShadingTypesMask() const override
    {
        return (1u << int(LightmapShadingType::IRRADIANCE))
            | (1u << int(LightmapShadingType::RADIANCE));
    }

    virtual const TypeInfo& GetInnerType() const
    {
        return TypeOf<LightmapVolume>();
    }

protected:
    virtual BakeDataBase& GetBakeData() override
    {
        return m_bakeData;
    }

    virtual UniquePtr<BakeJobBase> CreateJob(BakeJobParams&& params) override;

    virtual void Initialize_Internal() override;
    virtual void HandleCompletedJob_Internal(BakeJobBase* job) override;
    virtual void Build() override;

    Handle<LightmapVolume> m_volume;
    BakeData<LightmapVolume> m_bakeData;
    LightmapElementId m_lightmapElementId;
};

} // namespace Baking

} // namespace Hyperion

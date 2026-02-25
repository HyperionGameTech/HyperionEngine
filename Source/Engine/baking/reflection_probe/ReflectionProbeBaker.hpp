/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <baking/Baker.hpp>

#include <baking/reflection_probe/ReflectionProbeBakeData.hpp>

namespace Hyperion {

class ReflectionProbe;

namespace Baking {

template <>
class Baker<ReflectionProbe> final : public BakerBase
{
public:
    Baker(LightmapperConfig&& config, const Handle<ReflectionProbe>& envProbe);

    Baker(const Baker& other) = delete;
    Baker& operator=(const Baker& other) = delete;

    Baker(Baker&& other) noexcept = delete;
    Baker& operator=(Baker&& other) noexcept = delete;

    virtual ~Baker() override = default;

    virtual bool ShouldSplitIntoJobs() const override
    {
        return false;
    }

    virtual uint32 GetShadingTypesMask() const override
    {
        return 1u << int(LightmapShadingType::FULL);
    }

    virtual const TypeInfo& GetInnerType() const
    {
        return TypeOf<ReflectionProbe>();
    }

protected:
    virtual BakeDataBase& GetBakeData() override
    {
        return m_bakeData;
    }

    virtual UniquePtr<BakeJobBase> CreateJob(BakeJobParams&& params) override;

    virtual Result Build_Internal() override;
    virtual void OnCompleted_Internal() override;

    Handle<ReflectionProbe> m_envProbe;
    BakeData<ReflectionProbe> m_bakeData;
};

} // namespace Baking

} // namespace Hyperion

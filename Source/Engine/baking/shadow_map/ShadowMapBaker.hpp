/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <baking/Baker.hpp>

#include <baking/shadow_map/ShadowMapBakeData.hpp>

namespace Hyperion {

class Light;

namespace Baking {

template <>
class Baker<Light> final : public BakerBase
{
public:
    Baker(BakerConfig&& config, const Handle<Light>& light);

    Baker(const Baker& other) = delete;
    Baker& operator=(const Baker& other) = delete;

    Baker(Baker&& other) noexcept = delete;
    Baker& operator=(Baker&& other) noexcept = delete;

    virtual ~Baker() override = default;

    virtual bool ShouldSplitIntoJobs() const override
    {
        return false;
    }

    virtual uint32 NumTexelSamples() const override
    {
        return 1;
    }

    /*! \brief Shadow map baking uses the new SHADOW shading type, which traces
     *  shadow rays from each projected texel toward static geometry to compute
     *  a fully ray-traced, static shadow map for the owning light. */
    virtual uint32 GetShadingTypesMask() const override
    {
        return 1u << int(LightmapShadingType::SHADOW);
    }

    virtual const TypeInfo& GetInnerType() const override
    {
        return TypeOf<Light>();
    }

protected:
    virtual BakeDataBase& GetBakeData() override
    {
        return m_bakeData;
    }

    virtual UniquePtr<BakeJobBase> CreateJob(BakeJobParams&& params) override;

    virtual void CreateLightmapRenderers() override;

    virtual Result Build_Internal() override;
    virtual void OnCompleted_Internal() override;

    Handle<Light> m_light;
    BakeData<Light> m_bakeData;
};

} // namespace Baking

} // namespace Hyperion

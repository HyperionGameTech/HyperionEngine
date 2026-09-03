/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Baking/Baker.hpp>

#include <Baking/FogVolume/FogVolumeBakeData.hpp>

namespace Hyperion {

class FogVolume;

namespace Baking {

template <>
class Baker<FogVolume> final : public BakerBase
{
public:
    Baker(BakerConfig&& config, BakeLayer& bakeLayer, const Handle<FogVolume>& fogVolume);

    Baker(const Baker& other) = delete;
    Baker& operator=(const Baker& other) = delete;

    Baker(Baker&& other) noexcept = delete;
    Baker& operator=(Baker&& other) noexcept = delete;

    virtual ~Baker() override
    {
        if (m_bakeDataBuildTask.IsValid() && !m_bakeDataBuildTask.IsCompleted())
        {
            m_bakeDataBuildTask.Await();
        }
    }

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

    virtual const TypeInfo& GetInnerType() const
    {
        return TypeOf<FogVolume>();
    }

protected:
    virtual BakeDataBase& GetBakeData() override
    {
        return m_bakeData;
    }

    virtual UniquePtr<BakeJobBase> CreateJob(BakeJobParams&& params) override;

    virtual void HandleCompletedJob_Internal(BakeJobBase* job) override;

    virtual bool IsBuildAsync() const override
    {
        return true;
    }

    virtual bool PollBuildReady() override
    {
        return m_bakeDataBuildTask.IsValid() && m_bakeDataBuildTask.IsCompleted();
    }

    virtual void OnBuildReady() override;

    virtual void Build() override;

    Handle<FogVolume> m_fogVolume;
    BakeData<FogVolume> m_bakeData;

    Task<BakeData<FogVolume>> m_bakeDataBuildTask;
};

} // namespace Baking

} // namespace Hyperion

/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Baking/Baker.hpp>
#include <Baking/BakerMemory.hpp>

#include <Baking/LightmapVolume/LightmapVolumeBakeData.hpp>

namespace Hyperion {

class LightmapVolume;
enum class LightmapElementId : uint32;

namespace Baking {

template <>
class Baker<LightmapVolume> final : public BakerBase
{
public:
    Baker(BakerConfig&& config, const Handle<LightmapVolume>& volume);

    Baker(const Baker& other) = delete;
    Baker& operator=(const Baker& other) = delete;

    Baker(Baker&& other) noexcept = delete;
    Baker& operator=(Baker&& other) noexcept = delete;

    virtual ~Baker() override
    {
        if (m_atlasBuildTask.IsValid() && !m_atlasBuildTask.IsCompleted())
        {
            m_atlasBuildTask.Await();
        }
    }

    virtual bool ShouldSplitIntoJobs() const override
    {
        return true;
    }

    virtual uint32 GetShadingTypesMask() const override
    {
        if (m_shadingTypesMaskOverride != 0)
        {
            return m_shadingTypesMaskOverride;
        }

        return (1u << int(LightmapShadingType::IRRADIANCE));
    }

    virtual uint32 NumTexelSamples() const override
    {
        if (m_shadingTypesMaskOverride == (1u << int(LightmapShadingType::BENT_NORMAL)))
        {
            return m_config.bentNormalSamples;
        }

        return BakerBase::NumTexelSamples();
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

    virtual void CreateLightmapRenderers() override;

    virtual void Initialize_Internal() override;
    virtual void OnCompleted_Internal() override;
    virtual void Build() override;

    virtual bool IsBuildAsync() const override
    {
        return true;
    }

    virtual bool PollBuildReady() override
    {
        return m_atlasBuildTask.IsValid() && m_atlasBuildTask.IsCompleted();
    }

    virtual void OnBuildReady() override;

    Handle<LightmapVolume> m_volume;
    BakeData<LightmapVolume> m_bakeData;
    Array<LightmapElementId, BakerAllocator> m_lightmapElementIds;

    Task<BakeData<LightmapVolume>> m_atlasBuildTask;
};

} // namespace Baking

} // namespace Hyperion

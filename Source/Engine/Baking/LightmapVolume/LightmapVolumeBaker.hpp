/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Baking/Baker.hpp>

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

    virtual void CreateLightmapRenderers() override;

    virtual void Initialize_Internal() override;
    virtual void OnCompleted_Internal() override;
    virtual void Build() override;

    Handle<LightmapVolume> m_volume;
    BakeData<LightmapVolume> m_bakeData;
    LightmapElementId m_lightmapElementId;
};

} // namespace Baking

} // namespace Hyperion

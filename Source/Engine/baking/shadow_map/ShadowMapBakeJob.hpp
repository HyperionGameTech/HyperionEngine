/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <baking/BakeJob.hpp>

#include <baking/shadow_map/ShadowMapBakeData.hpp>

namespace Hyperion {

class Light;

namespace Baking {

template <>
class BakeJob<Light> : public BakeJobBase
{
public:
    explicit BakeJob(BakeJobParams&& params, const Handle<Light>& light, BakeData<Light>* bakeData)
        : BakeJobBase(std::move(params)),
          m_light(light),
          m_bakeData(bakeData)
    {
    }

    virtual ~BakeJob() override;

    HYP_FORCE_INLINE const Handle<Light>& GetLight() const
    {
        return m_light;
    }

    virtual BakeData<Light>& GetBakeData() override
    {
        return *m_bakeData;
    }

protected:
    virtual void Start_Internal() override;
    virtual void Process_Internal(bool* outIsReadyToProcess) override;

    Handle<Light> m_light;
    BakeData<Light>* m_bakeData;
};

} // namespace Baking

} // namespace Hyperion

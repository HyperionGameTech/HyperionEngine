/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Baking/BakeJob.hpp>

#include <Baking/reflection_probe/ReflectionProbeBakeData.hpp>

namespace Hyperion {

class ReflectionProbe;

namespace Baking {

template <>
class BakeJob<ReflectionProbe> : public BakeJobBase
{
public:
    explicit BakeJob(BakeJobParams&& params, const Handle<ReflectionProbe>& envProbe, BakeData<ReflectionProbe>* bakeData)
        : BakeJobBase(std::move(params)),
          m_envProbe(envProbe),
          m_bakeData(bakeData)
    {
    }

    virtual ~BakeJob() override;

    HYP_FORCE_INLINE const Handle<ReflectionProbe>& GetEnvProbe() const
    {
        return m_envProbe;
    }

    virtual BakeData<ReflectionProbe>& GetBakeData() override
    {
        return *m_bakeData;
    }

protected:
    virtual void Start_Internal() override;
    virtual void Process_Internal(bool* outIsReadyToProcess) override;

    Handle<ReflectionProbe> m_envProbe;
    BakeData<ReflectionProbe>* m_bakeData;
};

} // namespace Baking

} // namespace Hyperion

/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Baking/BakeJob.hpp>

#include <Baking/EnvProbe/EnvProbeBakeData.hpp>

namespace Hyperion {

class EnvProbe;

namespace Baking {

template <>
class BakeJob<EnvProbe> : public BakeJobBase
{
public:
    explicit BakeJob(
        BakeJobParams&& params,
        const Handle<EnvProbe>& envProbe,
        BakeData<EnvProbe>* bakeData)
        : BakeJobBase(std::move(params)),
          m_envProbe(envProbe),
          m_bakeData(bakeData),
          m_rasterCaptureEnded(false)
    {
    }

    virtual ~BakeJob() override;

    /// Do we want to Raster - not using PT?
    bool IsRaster() const;

    HYP_FORCE_INLINE const Handle<EnvProbe>& GetEnvProbe() const
    {
        return m_envProbe;
    }

    virtual BakeData<EnvProbe>& GetBakeData() override
    {
        return *m_bakeData;
    }

    virtual bool IsCompleted() const override;

protected:
    virtual void Start_Internal() override;
    virtual void Process_Internal(bool* outIsReadyToProcess) override;

    Handle<EnvProbe> m_envProbe;
    BakeData<EnvProbe>* m_bakeData;

    /// raster only!
    bool m_rasterCaptureEnded;
};

} // namespace Baking

} // namespace Hyperion

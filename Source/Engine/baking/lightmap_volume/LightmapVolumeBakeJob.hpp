/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <baking/BakeJob.hpp>

#include <baking/lightmap_volume/LightmapVolumeBakeData.hpp>

namespace Hyperion {

class LightmapVolume;
struct LightmapElement;

namespace Baking {

template <>
class BakeJob<LightmapVolume> : public BakeJobBase
{
public:
    BakeJob(BakeJobParams&& params, const Handle<LightmapVolume>& volume, BakeData<LightmapVolume>* bakeData);
    virtual ~BakeJob() override;

    HYP_FORCE_INLINE const Handle<LightmapVolume>& GetVolume() const
    {
        return m_volume;
    }

    virtual BakeData<LightmapVolume>& GetBakeData() override
    {
        return *m_bakeData;
    }

    HYP_FORCE_INLINE LightmapElement* GetLightmapElement() const
    {
        return m_lightmapElement;
    }

protected:
    virtual void Start_Internal() override;
    virtual void Process_Internal(bool* outIsReadyToProcess) override;

    Handle<LightmapVolume> m_volume;

    BakeData<LightmapVolume>* m_bakeData;

    LightmapElement* m_lightmapElement;
};

} // namespace Baking

} // namespace Hyperion

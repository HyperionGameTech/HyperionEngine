/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <baking/lightmap_volume/LightmapVolumeBakeJob.hpp>

#include <scene/LightmapVolume.hpp>
#include <scene/Scene.hpp>

namespace Hyperion {
namespace Baking {

BakeJob<LightmapVolume>::BakeJob(BakeJobParams&& params, const Handle<LightmapVolume>& volume, BakeData<LightmapVolume>* bakeData)
    : BakeJobBase(std::move(params)),
      m_volume(volume),
      m_bakeData(bakeData),
      m_lightmapElement(nullptr)
{
    Assert(m_volume != nullptr);
    Assert(m_bakeData != nullptr);
}

BakeJob<LightmapVolume>::~BakeJob()
{
}

void BakeJob<LightmapVolume>::Start_Internal()
{
}

void BakeJob<LightmapVolume>::Process_Internal(bool* outIsReadyToProcess)
{
    if (outIsReadyToProcess)
    {
        *outIsReadyToProcess = true;
    }
}

} // namespace Baking
} // namespace Hyperion

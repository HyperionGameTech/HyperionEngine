/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Baking/ShadowMap/ShadowMapBaker.hpp>
#include <Baking/ShadowMap/ShadowMapBakeJob.hpp>

#include <Scene/Light/Light.hpp>

namespace Hyperion {

namespace Baking {

Baker<Light>::Baker(BakerConfig&& config, BakeLayer& bakeLayer, const Handle<Light>& light)
    : BakerBase(std::move(config), bakeLayer, light, MakeStrongRef(light->GetScene()), light->GetWorldBounds()),
      m_light(light)
{
}

UniquePtr<BakeJobBase> Baker<Light>::CreateJob(BakeJobParams&& params)
{
    return MakeUnique<BakeJob<Light>>(std::move(params), m_light, &m_bakeData);
}

Result Baker<Light>::Build_Internal()
{
    Assert(m_light != nullptr);
    InitObject(m_light);

    m_bakeData = BakeData<Light>(m_bakeEntities, m_light.Get());

    return m_bakeData.Build();
}

void Baker<Light>::OnCompleted_Internal()
{
}

} // namespace Baking
} // namespace Hyperion

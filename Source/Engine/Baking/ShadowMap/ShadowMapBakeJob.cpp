/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Baking/Baker.hpp>

#include <Baking/ShadowMap/ShadowMapBakeJob.hpp>

#include <Rendering/ShadowMapCaptureState.hpp>

#include <Scene/Scene.hpp>
#include <Scene/Light/Light.hpp>
#include <Scene/World.hpp>

namespace Hyperion {
namespace Baking {

BakeJob<Light>::BakeJob(BakeJobParams&& params, const Handle<Light>& light, BakeData<Light>* bakeData)
    : BakeJobBase(std::move(params)),
      m_light(light),
      m_bakeData(bakeData)
{
}

BakeJob<Light>::~BakeJob()
{
    if (m_wasStarted && m_light.IsValid() && m_shadowMapRasterCaptureState != nullptr)
    {
        m_shadowMapRasterCaptureState->End(/* commitResult */ false);
        m_shadowMapRasterCaptureState.Reset();
    }
}

bool BakeJob<Light>::IsCompleted() const
{
    if (m_light.IsValid() && m_shadowMapRasterCaptureState != nullptr)
    {
        // keep the job alive until the capture has been rendered and committed
        return false;
    }

    return BakeJobBase::IsCompleted();
}

void BakeJob<Light>::Start_Internal()
{
    Assert(m_light.IsValid());

    m_shadowMapRasterCaptureState.Reset();

    m_shadowMapRasterCaptureState = MakeUniqueWithAllocator<ShadowMapCaptureState, BakerAllocator>(
        m_light.Get(),
        m_baker->GetBakeLayerName());

    m_shadowMapRasterCaptureState->Begin();
}

void BakeJob<Light>::Process_Internal(bool* outIsReadyToProcess)
{
    Assert(m_light.IsValid());
    Assert(m_shadowMapRasterCaptureState != nullptr);

    if (World* world = m_params.scene->GetWorld())
    {
        const uint32 numFaces = m_shadowMapRasterCaptureState->GetNumFaces();

        for (uint32 faceIndex = 0; faceIndex < numFaces; faceIndex++)
        {
            if (View* view = m_shadowMapRasterCaptureState->GetView(faceIndex))
            {
                world->ProcessViewAsync(view);
            }
        }
    }

    const bool isDone = !m_light->GetWorld()
        || m_shadowMapRasterCaptureState->IsRenderComplete();

    if (isDone)
    {
        m_shadowMapRasterCaptureState->End(/* commitResult */ true);
        m_shadowMapRasterCaptureState.Reset();
    }

    if (outIsReadyToProcess)
    {
        *outIsReadyToProcess = isDone;
    }
}

} // namespace Baking
} // namespace Hyperion

/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Baking/EnvProbe/EnvProbeBakeJob.hpp>

#include <Scene/Scene.hpp>
#include <Scene/EnvProbe.hpp>

namespace Hyperion {
namespace Baking {

BakeJob<EnvProbe>::~BakeJob()
{
    if (m_wasStarted && m_envProbe.IsValid() && IsRaster() && !m_rasterCaptureEnded)
    {
        m_envProbe->EndRasterCapture();
    }
}

bool BakeJob<EnvProbe>::IsRaster() const
{
    Assert(m_envProbe.IsValid());

    return !m_envProbe->IsPathTraced();
}

void BakeJob<EnvProbe>::Start_Internal()
{
    Assert(m_envProbe.IsValid());

    m_rasterCaptureEnded = false;

    if (IsRaster())
    {
        m_envProbe->BeginRasterCapture();
    }
}

void BakeJob<EnvProbe>::Process_Internal(bool* outIsReadyToProcess)
{
    Assert(m_envProbe.IsValid());

    if (IsRaster())
    {
        const bool isDone = m_envProbe->GetWorld() == nullptr
            || (!m_envProbe->needsRender.Load() && m_envProbe->IsCaptureReadbackComplete());

        if (isDone && !m_rasterCaptureEnded)
        {
            m_rasterCaptureEnded = true;

            m_envProbe->EndRasterCapture();
        }

        if (outIsReadyToProcess)
        {
            *outIsReadyToProcess = isDone;
        }
    }
    else
    {   
        if (outIsReadyToProcess)
        {
            *outIsReadyToProcess = true;
        }
    
    }
}

} // namespace Baking
} // namespace Hyperion

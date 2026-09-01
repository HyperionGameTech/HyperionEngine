/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Baking/EnvProbe/EnvProbeBakeJob.hpp>

#include <Rendering/CommandRecorder.hpp>
#include <Rendering/CommandRecorderAllocator.hpp>
#include <Rendering/RenderInterface.hpp>

#include <Rendering/Passes/EnvProbePass.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Scene/Scene.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/World.hpp>

namespace Hyperion {
namespace Baking {

BakeJob<EnvProbe>::~BakeJob()
{
    if (IsRaster() && m_rasterStarted && !m_rasterComplete.IsSignalled())
    {
        m_rasterCancellationToken.Signal();
        m_rasterComplete.Wait();
    }
}

bool BakeJob<EnvProbe>::IsRaster() const
{
    Assert(m_envProbe.IsValid());

    return !m_envProbe->IsPathTraced();
}

bool BakeJob<EnvProbe>::IsRasterComplete() const
{
    return m_rasterStarted
        && m_rasterComplete.IsSignalled()
        && m_envProbe->IsCaptureReadbackComplete();
}

bool BakeJob<EnvProbe>::IsCompleted() const
{
    if (IsRaster() && !IsRasterComplete())
    {
        return false;
    }

    return BakeJobBase::IsCompleted();
}

void BakeJob<EnvProbe>::Start_Internal()
{
    Assert(m_envProbe.IsValid());

    m_rasterStarted = false;

    if (IsRaster())
    {
        UniquePtr<EnvProbePassBase, BakerAllocator> pass;

        switch (m_envProbe->GetEnvProbeType())
        {
        case EPT_REFLECTION:
            pass = MakeUniqueWithAllocator<ReflectionProbePass, BakerAllocator>();
            break;
        case EPT_AMBIENT:
            pass = MakeUniqueWithAllocator<IrradianceProbePass, BakerAllocator>();
            break;
        default:
            HYP_LOG(Lightmap, Warning, "Cannot raster bake env probe type {}", m_envProbe->GetEnvProbeType());
            return;
        }

        Assert(pass != nullptr);

        m_envProbePass = std::move(pass);

        // need this here, not on render thread
        m_envProbe->BeginRasterCapture();
    }
}

void BakeJob<EnvProbe>::Process_Internal(bool* outIsReadyToProcess)
{
    Assert(m_envProbe.IsValid());

    if (IsRaster())
    {
        if (World* probeWorld = m_params.scene->GetWorld())
        {
            for (uint8 viewIndex = 0; viewIndex < 6; viewIndex++)
            {
                probeWorld->ProcessViewAsync(m_envProbe->GetView(viewIndex).Get());
            }
        }

        if (!m_rasterStarted)
        {
            m_rasterComplete.Reset(0);
            m_rasterStarted = true;
            
            static void (*s_enqueueRasterCmd)(const CmdBase& cmd) = nullptr;

            // Start raster.
            class RasterEnvProbeCmd : public CmdBase
            {
            public:
                EnvProbePassBase* pass;
                EnvProbe* envProbe;
                World* world;
                View* view;

                ThreadSignal* rasterComplete;
                ThreadSignal* cancellationToken;

                explicit RasterEnvProbeCmd(
                    EnvProbePassBase* pass,
                    EnvProbe* envProbe,
                    World* world,
                    View* view,
                    ThreadSignal* rasterComplete,
                    ThreadSignal* cancellationToken)
                    : pass(pass),
                      envProbe(envProbe),
                      world(world),
                      view(view),
                      rasterComplete(rasterComplete),
                      cancellationToken(cancellationToken)
                {
                }

                static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
                {
                    RasterEnvProbeCmd* cmdCasted = static_cast<RasterEnvProbeCmd*>(cmd);

                    if (cmdCasted->cancellationToken->IsSignalled())
                    {
                        cmdCasted->rasterComplete->Signal();

                        return;
                    }

                    EnvProbe* envProbe = cmdCasted->envProbe;
                    World* world = cmdCasted->world;
                    View* view = cmdCasted->view;

                    EnvProbePassBase* pass = cmdCasted->pass;

                    Assert(envProbe && world && view && pass);

                    envProbe->needsRender.Store(true);

                    Frame* frame = RI.GetCurrentFrame();
                    Assert(frame != nullptr);

                    RenderSetup rs {};
                    rs.world = world;
                    rs.view = view;
                    rs.envProbe = envProbe;

                    pass->Initialize();
                    pass->RenderFrame(frame, rs);
                    pass->Shutdown();

                    const bool done = !envProbe->needsRender.Load();

                    if (!done)
                    {
                        if (cmdCasted->cancellationToken->IsSignalled())
                        {
                            cmdCasted->rasterComplete->Signal();

                            return;
                        }

                        // re-queue the command for next frame.
                        s_enqueueRasterCmd(*cmdCasted);

                        return;
                    }
                    
                    cmdCasted->rasterComplete->Signal();
                }
            };

            s_enqueueRasterCmd = +[](const CmdBase& cmd) -> void
                {
                    CommandRecorder& cr = RI.commandRecorderAllocator.GetCommandRecorder();
                    cr << static_cast<const RasterEnvProbeCmd&>(cmd);
                    cr.Done();
                };

            Scene* scene = m_params.scene;
            Assert(scene != nullptr);

            World* world = scene->GetWorld();
            Assert(world != nullptr);

            Assert(m_params.view.IsValid());

            s_enqueueRasterCmd(RasterEnvProbeCmd(
                m_envProbePass.Get(),
                m_envProbe.Get(),
                world,
                m_params.view.Get(), // baker view
                &m_rasterComplete,
                &m_rasterCancellationToken
            ));

            if (outIsReadyToProcess)
            {
                *outIsReadyToProcess = false;
            }

            return;
        }

        // waiting on raster to complete, including any pending async GPU readbacks
        // (SH coefficients, hit mask, visibility texture) that the render commands kicked off.
        if (!IsRasterComplete())
        {
            if (outIsReadyToProcess)
            {
                *outIsReadyToProcess = false;
            }

            return;
        }

        // done, tear down
        m_envProbePass.Reset();
        m_envProbe->EndRasterCapture();
    }

    if (outIsReadyToProcess)
    {
        *outIsReadyToProcess = true;
    }
}

} // namespace Baking
} // namespace Hyperion

/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>

#include <Core/Containers/FixedArray.hpp>

#include <Core/Constants.hpp>

namespace Hyperion {

class GlimmerSWRTProbeVolume;
class GlimmerChannel;

/*! \brief Reads the SWRT probes back for Rendering.Glimmer.SWRT.DebugProbes: packs where each probe is and what its last trace saw,
 *  copies that to a readback buffer per frame in flight, and hands each to the channel once its frame has finished on the GPU, for
 *  GlimmerSystem to draw with the DebugDrawer. Render thread only. */
class GlimmerSWRTProbeDebug
{
public:
    GlimmerSWRTProbeDebug();
    GlimmerSWRTProbeDebug(const GlimmerSWRTProbeDebug& other) = delete;
    GlimmerSWRTProbeDebug& operator=(const GlimmerSWRTProbeDebug& other) = delete;
    ~GlimmerSWRTProbeDebug();

    /*! \brief Call after the probe volume's Update(), while it's ready. */
    void Update(Frame* frame, const GlimmerSWRTProbeVolume& probeVolume, GlimmerChannel& channel);

    /*! \brief Frees the buffers while the debug view is off. */
    void Reset();

private:
    void CreateResources();

    GpuBufferRef m_recordsBuffer;
    FixedArray<GpuBufferRef, NumFramesInFlight> m_readbackBuffers;
    FixedArray<bool, NumFramesInFlight> m_isPending;
};

} // namespace Hyperion

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
class GlimmerSurfaceCache;
class GlimmerSHOccupancy;

class GlimmerSWRTProbeDebug final
{
public:
    GlimmerSWRTProbeDebug();
    
    GlimmerSWRTProbeDebug(const GlimmerSWRTProbeDebug& other) = delete;
    GlimmerSWRTProbeDebug& operator=(const GlimmerSWRTProbeDebug& other) = delete;

    ~GlimmerSWRTProbeDebug();

    void Update(
        Frame* frame,
        const GlimmerSWRTProbeVolume& probeVolume,
        const GlimmerSurfaceCache& surfaceCache,
        const GlimmerSHOccupancy& occupancy,
        GlimmerChannel& channel);

    void Reset();

private:
    void CreateResources();

    GpuBufferRef m_recordsBuffer;
    FixedArray<GpuBufferRef, NumFramesInFlight> m_readbackBuffers;
    FixedArray<bool, NumFramesInFlight> m_isPending;
};

} // namespace Hyperion

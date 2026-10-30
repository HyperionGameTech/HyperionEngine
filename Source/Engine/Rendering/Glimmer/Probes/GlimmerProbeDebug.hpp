/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/RawBuffer.hpp>

#include <Core/Containers/FixedArray.hpp>

#include <Core/Constants.hpp>

namespace Hyperion {

class GlimmerProbeVolume;
class GlimmerChannel;
class GlimmerSurfaceCache;
class GlimmerSHOccupancy;

class GlimmerProbeDebug final
{
public:
    GlimmerProbeDebug();
    
    GlimmerProbeDebug(const GlimmerProbeDebug& other) = delete;
    GlimmerProbeDebug& operator=(const GlimmerProbeDebug& other) = delete;

    ~GlimmerProbeDebug();

    void Update(
        Frame* frame,
        const GlimmerProbeVolume& probeVolume,
        const GlimmerSurfaceCache& surfaceCache,
        const GlimmerSHOccupancy& occupancy,
        GlimmerChannel& channel);

    void Reset();

private:
    void CreateResources(uint32 numRecords);

    RWStructuredBuffer m_recordsBuffer;
    uint32 m_numRecords;
    FixedArray<GpuBufferRef, NumFramesInFlight> m_readbackBuffers;
    FixedArray<bool, NumFramesInFlight> m_isPending;
};

} // namespace Hyperion

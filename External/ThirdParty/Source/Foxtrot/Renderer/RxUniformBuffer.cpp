#include "RxUniformBuffer.hpp"

#include "RxConstants.hpp"
#include "RxRenderBackend.hpp"

#include <Renderer/RxGlobals.hpp>

void RxUniforms::Create(uint32 size)
{
    Size = size;
    uint32 size_in_frames = Size * RxFramesInFlight;

    mGpuBuffer.Create(RxGpuBufferType::eUniformWithOffset, size_in_frames, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                      RxGpuBufferFlags::ePersistentMapped);
}

void RxUniforms::Rewind() { mUniformIndex = 0; }


uint32 RxUniforms::GetBaseOffset() const { return Size * gRenderer->GetFrameNumber(); }

uint8* RxUniforms::GetCurrentBuffer()
{
    // Offset by the frame currently in flight
    return reinterpret_cast<uint8*>(mGpuBuffer.pMappedBuffer) + GetBaseOffset();
}

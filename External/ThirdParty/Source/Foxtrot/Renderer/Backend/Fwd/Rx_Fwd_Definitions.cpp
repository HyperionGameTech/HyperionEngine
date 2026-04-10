#include "Rx_Fwd_AddToDeletionQueue.hpp"
#include "Rx_Fwd_GetDevice.hpp"
#include "Rx_Fwd_GetFrame.hpp"
#include "Rx_Fwd_GetGpuAllocator.hpp"
#include "Rx_Fwd_SubmitUploadGpuCmd.hpp"

#include <Renderer/RxGlobals.hpp>
#include <Renderer/RxRenderBackend.hpp>

/* Rx_Fwd_GetFrame.hpp */
RxFrameData* Rx_Fwd_GetFrame() { return gRenderer->GetFrame(); }

/* Rx_Fwd_SubmitUploadGpuCmd.hpp */
void Fx_Fwd_SubmitUploadCmd(std::function<void(RxCommandBuffer&)> func) { gRenderer->SubmitUploadCmd(func); }


VmaAllocator Fx_Fwd_GetGpuAllocator() { return gRenderer->GpuAllocator; }

RxGpuDevice* Rx_Fwd_GetDevice() { return gRenderer->GetDevice(); }

void Fx_Fwd_AddToDeletionQueue(const FxDeletionObject::FuncType& func) { gRenderer->AddToDeletionQueue(func); }

void Fx_Fwd_AddGpuBufferToDeletionQueue(const VkBuffer& buffer, const VmaAllocation& allocation)
{
    gRenderer->AddGpuBufferToDeletionQueue(buffer, allocation);
}

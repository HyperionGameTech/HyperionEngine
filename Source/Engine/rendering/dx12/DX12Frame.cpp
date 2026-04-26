/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12GpuImage.hpp>
#include <rendering/dx12/DX12Frame.hpp>
#include <rendering/dx12/DX12RenderInterface.hpp>
#include <rendering/dx12/DX12CommandBuffer.hpp>

#include <DX12Frame.generated.inl>

namespace Hyperion {

extern DX12RenderInterface* g_renderInterface;

#pragma region DX12Frame

DX12Frame::DX12Frame()
    : FrameBase(0)
{
}

DX12Frame::DX12Frame(uint32 frameIndex)
    : FrameBase(frameIndex)
{
}

DX12Frame::~DX12Frame()
{
}

bool DX12Frame::IsCreated() const
{
    return true;
}

RendererResult DX12Frame::Create()
{
    return {};
}

void DX12Frame::OnFrameStart()
{
    m_frameCounter++;
}

void DX12Frame::WriteCommandBuffer(CommandBuffer* commandBuffer)
{
    DX12CommandBuffer* dx12CommandBuffer = static_cast<DX12CommandBuffer*>(commandBuffer);

    if (dx12CommandBuffer->IsRecording())
    {
        dx12CommandBuffer->End();
    }

    DX12RenderInterface* renderInterface = g_renderInterface;
    DX12QueueData& queueData = renderInterface->GetQueueData(D3D12_COMMAND_LIST_TYPE_DIRECT);

    ID3D12CommandList* commandLists[] = { dx12CommandBuffer->GetCommandList() };
    queueData.commandQueue->ExecuteCommandLists(ArraySize(commandLists), commandLists);
}

void DX12Frame::ResetTransientStates()
{
}

#pragma endregion DX12Frame

} // namespace Hyperion

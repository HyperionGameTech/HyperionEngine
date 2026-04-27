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
    FrameBase::OnFrameStart();
}

void DX12Frame::WriteCommandBuffer(CommandBuffer* commandBuffer)
{
    AssertOnThread(g_renderThread);

    Array<CommandRecorder*, RenderAllocator> commandRecorders;
    commandRecorders.Reserve(4);

    commandRecorders.PushBack(&preRenderCommands);
    commandRecorders.PushBack(&cr);
    commandRecorders.PushBack(&g_renderInterface->commandRecorderAllocator.GetCommandRecorder());
    commandRecorders.PushBack(&postRenderCommands);
    
    for (CommandRecorder* commandRecorder : commandRecorders)
    {
        commandRecorder->Prepare(this);
    }

    if (OnPresent.AnyBound())
    {
        OnPresent(this);
        OnPresent.RemoveAllDetached();
    }

    {
        for (CommandRecorder* commandRecorder : commandRecorders)
        {
            commandRecorder->Execute(commandBuffer);
            commandRecorder->Reset(/* freeMemory */ false);
        }
    }

    if (commandBuffer->IsRecording())
    {
        commandBuffer->End();
    }

    DX12RenderInterface& ri = *g_renderInterface;

    const DX12QueueData* queueData = ri.GetQueueData(D3D12_COMMAND_LIST_TYPE_DIRECT);
    Assert(queueData != nullptr);
    
    ID3D12CommandList* commandLists[] = { commandBuffer->GetCommandList() };
    queueData->commandQueue->ExecuteCommandLists(1, commandLists);

}

void DX12Frame::ResetTransientStates()
{
}

#pragma endregion DX12Frame

} // namespace Hyperion

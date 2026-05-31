/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <DX12Pch.hpp>

#include <Rendering/dx12/DX12GpuImage.hpp>
#include <Rendering/dx12/DX12Frame.hpp>
#include <Rendering/dx12/DX12RenderInterface.hpp>
#include <Rendering/dx12/DX12CommandBuffer.hpp>

#include <DX12Frame.generated.inl>

namespace Hyperion {

extern DX12RenderInterface RI;

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
    commandRecorders.PushBack(&RI.commandRecorderAllocator.root);
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
}

void DX12Frame::ResetTransientStates()
{
}

#pragma endregion DX12Frame

} // namespace Hyperion
